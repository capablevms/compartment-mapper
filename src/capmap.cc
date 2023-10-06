// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "include/capmap.h"

#include <inttypes.h>
#include <stdio.h>

// BSD headers for kinfo_getvmmap.
#include <libutil.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

#if __has_feature(capabilities)
#include <cheriintrin.h>
#else
#error "capmap.cc requires capabilities"
#endif

// If greater than zero, print scan-related debug message to stderr.
// This can help to diagnose why something is or isn't being found, but it
// occurs before coalescing and redundancy checks, so is very verbose.
#define SCAN_LOG_VERBOSITY 0

#if SCAN_LOG_VERBOSITY > 0
#define SCAN_LOG(verbosity, ...)             \
  do {                                       \
    if ((verbosity) <= SCAN_LOG_VERBOSITY) { \
      fprintf(stderr, __VA_ARGS__);          \
    }                                        \
  } while (0)
#else
#define SCAN_LOG(...)
#endif

namespace capmap {

#ifdef __CHERI_PURE_CAPABILITY__
#define R8 "c8"
#else
#define R8 "x8"
#endif

__attribute((naked)) Roots get_roots() {
  // AAPCS64(-cap): The caller allocates space for the large returned struct,
  // and passes a pointer in x8/c8.
  // clang-format off
  asm("1:\n"
      "stp c0, c1, [" R8 "]\n"
      "stp c2, c3, [" R8 ", #32]\n"
      "stp c4, c5, [" R8 ", #64]\n"
      "stp c6, c7, [" R8 ", #96]\n"
      "stp c8, c9, [" R8 ", #128]\n"
      "stp c10, c11, [" R8 ", #160]\n"
      "stp c12, c13, [" R8 ", #192]\n"
      "stp c14, c15, [" R8 ", #224]\n"
      "stp c16, c17, [" R8 ", #256]\n"
      "stp c18, c19, [" R8 ", #288]\n"
      "stp c20, c21, [" R8 ", #320]\n"
      "stp c22, c23, [" R8 ", #352]\n"
      "stp c24, c25, [" R8 ", #384]\n"
      "stp c26, c27, [" R8 ", #416]\n"
      "stp c28, c29, [" R8 ", #448]\n"
      "str c30, [" R8 ", #480]\n"

      "mov c0, csp\n"
      "mrs c1, ddc\n"
      "stp c0, c1, [" R8 ", #496]\n"

#ifdef __CHERI_PURE_CAPABILITY__
      "adr c0, 1b\n"  // PCC on entry
#else
      "adr x0, 1b\n"  // PCC on entry
      "cvtp c0, x0\n"
#endif
      "mrs c1, cid_el0\n"
      "stp c0, c1, [" R8 ", #528]\n"

      "ret\n");
  // clang-format on
}

void print_raw_cap(FILE* stream, void* __capability cap) {
  uint64_t parts[2];
  static_assert(sizeof(parts) == sizeof(cap), "Expected 128-bit capability");
  memcpy(parts, &cap, sizeof(parts));
  fprintf(stream, "0x%u:%" PRIx64 ":%" PRIx64, cheri_tag_get(cap), parts[1], parts[0]);
}

void simple_scan_and_print_json(FILE* stream) {
  Roots roots = get_roots();
  Mapper mapper;
  mapper.scan(roots);
  mapper.print_json(stream);
}

void Mapper::update_self_ranges() {
  // TODO: Do a better job here.
  //  - We have several heap objects, which should also be excluded.
  //  - If using scan(Roots), we might want to exclude invariant regions once,
  //    and update the exclusion ranges only when we allocate new map entries,
  //    etc.
  exclude_self_ = SparseRange();
  exclude_self_.combine(Range::from_object(this));
}

void Mapper::scan(void* __capability cap, uint64_t depth) {
  SCAN_LOG(1, "scan(%#lp, %" PRIu64 ")\n", cap, depth);
  if (depth > max_seen_scan_depth_) max_seen_scan_depth_ = depth;

  for (auto& map : maps_) map->try_combine(cap);

  // TODO: Defer this until after the depth check and the
  // load_cap_map_.try_combine() permissions check (but before the actual
  // combination).
  auto cap_range = Range::from_cap(cap);
  auto scan_ranges = SparseRange(cap_range);
  scan_ranges.remove(load_cap_map_.ranges());
  scan_ranges.remove(exclude_self_);
  // TODO: Make a SparseRange::filter() function or similar, to avoid repeating
  // this for every scan.
  auto exclude = SparseRange(Range::full_64bit());
  exclude.remove(include_);
  scan_ranges.remove(exclude);

  if (load_cap_map_.try_combine(cap) && (depth < max_scan_depth_)) {
    for (auto scan_range : scan_ranges.parts()) {
      scan_range.shrink_to_alignment(sizeof(void* __capability));
      ptraddr_t last = cheri_align_down(scan_range.last(), sizeof(void* __capability));
      for (ptraddr_t next = scan_range.base(); next <= last; next += sizeof(void* __capability)) {
        void* __capability candidate_cap;
        asm("ldr %w[candidate], [%w[addr]]\n"
            : [candidate] "=r"(candidate_cap)
            : [addr] "r"(cheri_address_set(cap, next)));
        if (cheri_tag_get(candidate_cap)) {
          SCAN_LOG(2, "Recursing at %zx: %#lp\n", next, candidate_cap);
          scan(candidate_cap, depth + 1);
        } else {
          SCAN_LOG(2, "No cap at %zx.\n", next);
        }
      }
    }
  }
}

void Mapper::print_json(FILE* stream) {
  fprintf(stream, "\"capmap\": {\n");

  {
    char const* sep = "\n        ";
    fprintf(stream, "    \"roots\": {");
    for (auto const& name_cap : roots_) {
      fprintf(stream, "%s\"%s\": \"", sep, name_cap.first);
      print_raw_cap(stream, name_cap.second);
      fprintf(stream, "\"");
      sep = ",\n        ";
    }
    fprintf(stream, "\n    }\n");
  }

  fprintf(stream, "    \"scan\": {\n");
  fprintf(stream, "        \"include\": ");
  include_.print_json(stream, "        ");
  fprintf(stream, ",\n");
  fprintf(stream, "        \"exclude\": ");
  exclude_self_.print_json(stream, "        ");
  fprintf(stream, ",\n");
  fprintf(stream, "        \"depth\": %" PRIu64 "\n    },\n", max_seen_scan_depth());

  fprintf(stream, "    \"maps\": {\n");
  fprintf(stream, "        \"%s\": {\n", load_cap_map_.name());
  fprintf(stream, "            \"address-space\": \"%s\",\n", load_cap_map_.address_space());
  fprintf(stream, "            \"ranges\": ");
  ::capmap::print_json(stream, load_cap_map_.ranges(), "            ");
  for (auto const& map : maps_) {
    fprintf(stream, ",\n        \"%s\":\n", map->name());
    fprintf(stream, "            \"address-space\": \"%s\",\n", map->address_space());
    fprintf(stream, "            \"ranges\": ");
    ::capmap::print_json(stream, map->ranges(), "            ");
  }
  fprintf(stream, "\n        }\n    }\n}\n");
}

bool LoadCapMap::try_combine(void* __capability cap) {
  if (!cheri_tag_get(cap)) return false;
  // TODO: Track sealed caps and see if we can unseal them later.
  if (cheri_is_sealed(cap)) return false;

  size_t const perms = CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP;
  if ((cheri_perms_get(cap) & perms) != perms) return false;

  ranges_.combine(Range::from_cap(cap));
  return true;
}

bool LoadCapMap::includes_cap(ptraddr_t addr, ptraddr_t* cont) const {
  // TODO: Abstract this in SparseRange somehow. This is just
  // SparseRange::includes() with some extra logic using the intermediate value.
  auto range = Range::from_base_length(addr, sizeof(void* __capability));
  auto candidate = ranges_.parts().lower_bound(range);
  if ((candidate != ranges_.parts().end()) && candidate->includes(range)) {
    // TODO: Advance better, to avoid redundant checks in large ranges.
    *cont = addr + sizeof(void* __capability);
    return true;
  }
  return false;
}

SparseRange LoadCapMap::vmmap() {
  pid_t pid = getpid();
  int count;
  kinfo_vmentry const* vm = kinfo_getvmmap(pid, &count);
  SparseRange map;
  for (int i = 0; i < count; i++) {
    auto prot = vm[i].kve_protection;
    if ((prot & KVME_PROT_READ) && (prot & KVME_PROT_READ_CAP)) {
      map.combine(Range::from_base_limit(vm[i].kve_start, vm[i].kve_end));
    }
  }
  return map;
}

bool LoadMap::try_combine(void* __capability cap) {
  if (!cheri_tag_get(cap)) return false;
  if (cheri_is_sealed(cap)) return false;

  if (cheri_perms_get(cap) & CHERI_PERM_LOAD) {
    ranges_.combine(Range::from_cap(cap));
    return true;
  }
  return false;
}

}  // namespace capmap
