// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdarg.h>
#include <stdio.h>

#include "include/capmap.h"
#include "tests.h"

using capmap::Mapper;
using capmap::Range;
using capmap::SparseRange;

// Return a bounded capability for the object.
//
// Bounds are not guaranteed to be exact.
//
// For array types like `array[42]`, pass e.g. `&array`, not `array`; the latter
// is interpreted as a pointer, and probably won't be given the bounds you expect.
template <typename T = void* __capability, typename S>
T* __capability cap(S* obj) {
#ifdef __CHERI_PURE_CAPABILITY__
  return reinterpret_cast<T* __capability>(obj);
#else
  auto ddc = cheri_ddc_get();
  auto size = sizeof(S);
  auto addr = reinterpret_cast<ptraddr_t>(obj);
  auto ret = cheri_bounds_set(cheri_address_set(ddc, addr), size);
  return reinterpret_cast<T* __capability>(ret);
#endif
}

// Return just the address.
//
// This is like cheri_address_get(), but this works for hybrid pointers too.
template <typename S>
ptraddr_t addr(S* cap) {
#ifdef __CHERI_PURE_CAPABILITY__
  return cheri_address_get(cap);
#else
  return reinterpret_cast<ptraddr_t>(cap);
#endif
}

TEST(scan_exclude_all) {
  // If we don't include any ranges, we'll only gather roots.
  uint64_t not_included[42] = {0};
  void* __capability buffer[4] = {
      nullptr,
      nullptr,
      cap(&not_included),
      nullptr,
  };

  Mapper mapper{SparseRange()};

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(cap(&buffer), "&buffer");

  if (options().verbose()) {
    printf("&buffer: %#lp\n", cap(&buffer));
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  TRY(ranges.size() == 1);
  TRY(ranges.begin()->base() == addr(&buffer));
  TRY(ranges.begin()->length().first == false);
  TRY(ranges.begin()->length().second == sizeof(buffer));
  // Roots do not overlap with any included memory.
  TRY(mapper.max_seen_scan_depth() == 0);
}

TEST(scan_nested_not_detected) {
  void* __capability not_detected[42] = {cheri_ddc_get()};
  void* __capability nested[42] = {cap(&not_detected)};
  void* __capability buffer[4] = {
      nullptr,
      nullptr,
      nullptr,
      cap(&nested),
  };

  SparseRange sr;
  sr.combine(Range::from_object(&buffer));
  // We'll never find a capability to not_detected; including it here shouldn't
  // cause us to scan it.
  sr.combine(Range::from_object(&not_detected));
  Mapper mapper{sr};

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(cap(&buffer), "&buffer");

  if (options().verbose()) {
    printf("&not_detected: %#lp\n", cap(&not_detected));
    printf("&nested: %#lp\n", cap(&nested));
    printf("&buffer: %#lp\n", cap(&buffer));
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  auto total_length = 0;
  for (auto& range : ranges) {
    TRY(range.length().first == false);
    total_length += range.length().second;
    TRY((range.base() == addr(&buffer)) || (range.base() == addr(&nested)));
  }
  TRY(total_length == (sizeof(buffer) + sizeof(nested)));
  TRY((ranges.size() == 1) || (ranges.size() == 2));
  TRY(!mapper.load_cap_map().sparse_range().overlaps(Range::from_object(&not_detected)));
  // Depth 1: scan &buffer, find &nested, but it isn't included.
  // We never find &detected.
  TRY(mapper.max_seen_scan_depth() == 1);
}

TEST(scan_nested_detected) {
  void* __capability detected[42] = {nullptr};
  void* __capability nested[42] = {cap(&detected)};
  void* __capability buffer[4] = {
      nullptr,
      nullptr,
      nullptr,
      cap(&nested),
  };

  SparseRange sr;
  sr.combine(Range::from_object(&nested));
  sr.combine(Range::from_object(&buffer));
  Mapper mapper{sr};

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(cap(&buffer), "&buffer");

  if (options().verbose()) {
    printf("&detected: %#lp\n", cap(&detected));
    printf("&nested: %#lp\n", cap(&nested));
    printf("&buffer: %#lp\n", cap(&buffer));
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  auto total_length = 0;
  for (auto& range : ranges) {
    TRY(range.length().first == false);
    total_length += range.length().second;
    TRY((range.base() == addr(&buffer)) || (range.base() == addr(&nested)) ||
        (range.base() == addr(&detected)));
  }
  TRY(total_length == (sizeof(buffer) + sizeof(nested) + sizeof(detected)));
  TRY((ranges.size() >= 1) && (ranges.size() <= 3));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_object(&buffer)));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_object(&nested)));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_object(&detected)));
  // Depth 1: scan &buffer, find &nested.
  // Depth 2: scan &nested, find &detected, but it isn't included.
  TRY(mapper.max_seen_scan_depth() == 2);
}

TEST(scan_nested_depth_limit) {
  // As TEST(scan_nested), but limit the depth so we don't see everything.
  void* __capability too_deep[42] = {nullptr};
  void* __capability nested[42] = {cap(&too_deep)};
  void* __capability buffer[4] = {
      nullptr,
      nullptr,
      nullptr,
      cap(&nested),
  };

  SparseRange sr;
  sr.combine(Range::from_object(&nested));
  sr.combine(Range::from_object(&buffer));
  Mapper mapper{sr};
  // Scan the root (`&buffer`) and `buffer[..]` itself, but not `nested[..]`.
  mapper.set_max_scan_depth(1);

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(cap(&buffer), "&buffer");

  if (options().verbose()) {
    printf("&too_deep: %#lp\n", cap(&too_deep));
    printf("&nested: %#lp\n", cap(&nested));
    printf("&buffer: %#lp\n", cap(&buffer));
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  auto total_length = 0;
  for (auto& range : ranges) {
    TRY(range.length().first == false);
    total_length += range.length().second;
    TRY((range.base() == addr(&buffer)) || (range.base() == addr(&nested)));
  }
  TRY(total_length == (sizeof(buffer) + sizeof(nested)));
  TRY((ranges.size() == 1) || (ranges.size() == 2));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_object(&buffer)));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_object(&nested)));
  TRY(!mapper.load_cap_map().sparse_range().overlaps(Range::from_object(&too_deep)));
  // Depth 1: scan &buffer, find &nested.
  // Depth limit prevents further scans.
  TRY(mapper.max_seen_scan_depth() == 1);
}

TEST(scan_self) {
  void* __capability a;
  a = cap(&a);

  SparseRange sr;
  sr.combine(Range::from_object(&a));
  Mapper mapper{sr};

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(a, "a");

  if (options().verbose()) {
    printf("a (&a): %#lp\n", a);
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  TRY(ranges.size() == 1);
  TRY(ranges.begin()->base() == cheri_address_get(a));
  TRY(ranges.begin()->length().first == false);
  TRY(ranges.begin()->length().second == sizeof(a));
  // Depth 1: scan a, find &a, which is already mapped.
  TRY(mapper.max_seen_scan_depth() == 1);
}

TEST(scan_loop) {
  void* __capability a;
  void* __capability b = cap(&a);
  a = cap(&b);

  SparseRange sr;
  sr.combine(Range::from_object(&a));
  sr.combine(Range::from_object(&b));
  Mapper mapper{sr};

  TRY(mapper.load_cap_map().ranges().empty());
  mapper.scan(a, "a");

  if (options().verbose()) {
    printf("a (&b): %#lp\n", a);
    printf("b (&a): %#lp\n", b);
    mapper.print_json(stdout);
  }
  auto ranges = mapper.load_cap_map().ranges();
  auto total_length = 0;
  for (auto& range : ranges) {
    TRY(range.length().first == false);
    total_length += range.length().second;
    TRY((range.base() == cheri_address_get(a)) || (range.base() == cheri_address_get(b)));
  }
  TRY(total_length == (sizeof(a) + sizeof(b)));
  TRY((ranges.size() == 1) || ranges.size() == 2);
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_cap(a)));
  TRY(mapper.load_cap_map().sparse_range().includes(Range::from_cap(b)));
  // Depth 1: scan a, find &b
  // Depth 2: scan b, find &a, which is already mapped.
  TRY(mapper.max_seen_scan_depth() == 2);
}

TEST(scan_load_vs_load_cap) {
  // `LoadMap` should always be at least as big as `LoadCapMap`.
  void* __capability load_only = map_load_only();
  Mapper mapper;
  mapper.maps()->push_back(std::make_unique<capmap::LoadMap>());
  mapper.scan(capmap::get_roots());
  // If the compiler optimises `load_only` away, it might not be reachable
  // through roots.
  mapper.scan(load_only, "load_only");

  if (options().verbose()) {
    mapper.print_json(stdout);
  }
  TRY(mapper.maps()->size() == 1);
  auto load_map = dynamic_cast<capmap::LoadMap const*>(mapper.maps()->at(0).get());
  capmap::LoadCapMap const& load_cap_map = mapper.load_cap_map();

  TRY(load_map->sparse_range().includes(load_cap_map.sparse_range()));
}

TEST(scan_depth_zero) {
  // If we limit the depth to zero, roots are never dereferenced, so we don't
  // have to worry about the memory being mapped.
  Mapper mapper{Range::full_64bit()};
  mapper.set_max_scan_depth(0);
  mapper.scan(capmap::get_roots());
  if (options().verbose()) {
    mapper.print_json(stdout);
  }
  TRY(mapper.max_seen_scan_depth() == 0);
}
