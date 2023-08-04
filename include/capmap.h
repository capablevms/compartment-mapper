// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

  // Scan the specified capability.
  //
  // The result is incorporated into the existing map.
  void scan(void *__capability cap, char const *name) {
    update_self_ranges();
    if (cheri_tag_get(cap)) {
      roots_.push_back(std::make_pair(name, cap));
      scan(cap);
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
