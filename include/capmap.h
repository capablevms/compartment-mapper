// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-FileCopyrightText: Copyright 2023 The University of Glasgow
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef CAPMAP_H_
#define CAPMAP_H_

#include "capmap-mappings.h"
#include "capmap-range.h"

#if __has_feature(capabilities)
#include <cheriintrin.h>
#else
#error "capmap.h requires capabilities"
#endif

#include <stdio.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace capmap {

struct Roots {
  void *__capability c[31];
  void *__capability csp;
  void *__capability ddc;
  void *__capability pcc;
  void *__capability cid_el0;
  // TODO: Are there other EL0 registers that could hold a capability?

  static char const *name_c(int i) {
    char const *const names[] = {"c0",  "c1",  "c2",  "c3",  "c4",  "c5",  "c6",  "c7",
                                 "c8",  "c9",  "c10", "c11", "c12", "c13", "c14", "c15",
                                 "c16", "c17", "c18", "c19", "c20", "c21", "c22", "c23",
                                 "c24", "c25", "c26", "c27", "c28", "c29", "c30"};
    return names[i];
  }
};

// Retrieve all register roots.
//
// This is "naked" to minimise disturbance to caller-saved registers, etc.
// However, this process is not (and cannot be) perfect; capabilities left in
// caller-saved registers may be missed.
__attribute((naked)) Roots get_roots();

// Shorthand for creating a new Mapper, scanning all roots from `get_roots()`,
// then calling `print_json()`.
//
// This is useful for simple applications and quick tests, but it only scans for
// the default permissions (notably "load capability") and doesn't allow any
// customisation.
//
// Note that argument passing and other compiler behaviours may hide
// capabilities left in caller-saved registers.
void simple_scan_and_print_json(FILE *stream);

// The primary container, and expected API entry point.
class Mapper {
 public:
  // The default Mapper will scan all accessible memory where capabilities are
  // found to it, but will not attempt to access unmapped pages, even if
  // capabilities are found.
  Mapper() : include_(LoadCapMap::vmmap()) {}

  Mapper(SparseRange include) : include_(include) {}

  // The number of dereference hops that are permitted when mapping a
  // compartment.
  //
  // For example, setting `set_max_scan_depth(0)` causes the roots themselves to
  // be incorporated, but they won't be dereferenced.
  //
  // This is available primarily as an optimisation for experimentation, since
  // sparse purecap graphs could be expensive to construct. However, it may be
  // acceptable in some applications for sensitive data to be indirectly
  // reachable, for example if strict compartmentalisation is not required.
  void set_max_scan_depth(uint64_t max) { max_scan_depth_ = max; }

  // The maximum scan depth that was actually seen.
  uint64_t max_seen_scan_depth() { return max_seen_scan_depth_; }

  // Memory ranges to scan for indirect capabilities.
  //
  // Capabilities to memory outside included ranges will still be reported, but
  // those ranges won't be examined.
  SparseRange *include() { return &include_; }

  // Scan all of the specified roots.
  //
  // The result is incorporated into the existing map.
  void scan(Roots const &roots) {
    for (size_t i = 0; i < sizeof(roots.c) / sizeof(roots.c[0]); i++) {
      scan(roots.c[i], Roots::name_c(i));
    }
    scan(roots.csp, "csp");
    scan(roots.ddc, "DDC");
    scan(roots.pcc, "PCC");
    scan(roots.cid_el0, "CID_EL0");
  }

  // Scan the specified capability.
  //
  // The result is incorporated into the existing map.
  void scan(void *__capability cap, char const *name) {
    update_self_ranges();
    if (cheri_tag_get(cap)) {
      roots_.push_back(std::make_pair(name, cap));
      try {
        scan(cap);
      } catch (int n) {
        fprintf(stderr, " from root %s at depth %d\n", name, n);
        abort();
      }
    }
  }

  void print_json(FILE *stream);

  LoadCapMap const &load_cap_map() const { return load_cap_map_; }

  std::vector<std::unique_ptr<Map>> *maps() { return &maps_; }

 private:
  void update_self_ranges();
  void scan(void *__capability cap, uint64_t depth = 0);

  SparseRange include_;

  // Memory ranges used by the mapper itself. These are updated during every
  // scan, in case the `Mapper` moves or allocates.
  SparseRange exclude_self_;

  // We always track Load + LoadCaps, because we use it to walk the graph.
  LoadCapMap load_cap_map_;

  // User-configurable maps.
  std::vector<std::unique_ptr<Map>> maps_;

  uint64_t max_scan_depth_ = UINT64_MAX;
  uint64_t max_seen_scan_depth_ = 0;

  std::vector<std::pair<char const *, void *__capability>> roots_;
};

}  // namespace capmap
#endif
