// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-FileCopyrightText: Copyright 2023 The University of Glasgow
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

void* __capability cap_with_perms(size_t size, size_t perms) {
  void* ptr = calloc(1, size);
#ifdef __CHERI_PURE_CAPABILITY__
  void* cap = ptr;
#else
  void* __capability cap = cheri_address_set(cheri_ddc_get(), (ptraddr_t)ptr);
  cap = cheri_bounds_set_exact(cap, 8);
#endif
  return cheri_perms_and(cap, perms);
}

#include <sys/sysctl.h>

TEST(scan_basic_map) {
  const cheri_perms_t PERM_r = CHERI_PERM_LOAD, PERM_w = CHERI_PERM_STORE,
                      PERM_rw = (cheri_perms_t)(PERM_r | PERM_w),
                      PERM_rR = (cheri_perms_t)(CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
                      PERM_wW = (cheri_perms_t)(CHERI_PERM_STORE | CHERI_PERM_STORE_CAP),
                      PERM_rwRW = (cheri_perms_t)(PERM_rR | PERM_wW);

  void* __capability caps[8] = {
      cap_with_perms(sizeof(long), PERM_r),
      cap_with_perms(sizeof(long), PERM_w),
      cap_with_perms(sizeof(long), PERM_r | PERM_w),
      cap_with_perms(sizeof(long), PERM_rR),
      cap_with_perms(sizeof(long), PERM_wW),
      cap_with_perms(sizeof(long), PERM_rR | PERM_wW),
#ifdef __CHERI_PURE_CAPABILITY__
      (void*)&cap_with_perms
#else
      cheri_sentry_create(cheri_address_set(cheri_pcc_get(), (ptraddr_t)(void*)&cap_with_perms))
#endif
      // open slot for sealcap
  };
  size_t sz = sizeof(caps[7]);
  sysctlbyname("security.cheri.sealcap", &caps[7], &sz, NULL, 0);

  Mapper mapper;
  mapper.maps()->push_back(std::make_unique<capmap::PermissionMap>("store", "virtual memory", PERM_w));
  mapper.maps()->push_back(
      std::make_unique<capmap::PermissionMap>("store cap", "virtual memory", PERM_wW));
  mapper.maps()->push_back(
      std::make_unique<capmap::PermissionMap>("load/store", "virtual memory", PERM_rw));
  mapper.maps()->push_back(
      std::make_unique<capmap::PermissionMap>("load/store cap", "virtual memory", PERM_rwRW));
  mapper.maps()->push_back(std::make_unique<capmap::PermissionMap>("seal", "otype", CHERI_PERM_SEAL));

  mapper.scan(cap(&caps), "caps");

  if (options().verbose()) {
    mapper.print_json(stdout);
  }
  TRY(mapper.maps()->size() == 5);
  auto w_map = dynamic_cast<capmap::PermissionMap const*>(mapper.maps()->at(0).get());
  auto wW_map = dynamic_cast<capmap::PermissionMap const*>(mapper.maps()->at(1).get());
  auto rw_map = dynamic_cast<capmap::PermissionMap const*>(mapper.maps()->at(2).get());
  auto rwRW_map = dynamic_cast<capmap::PermissionMap const*>(mapper.maps()->at(3).get());
  auto seal_map = dynamic_cast<capmap::PermissionMap const*>(mapper.maps()->at(4).get());

  // construct expected outcomes for each memory map
  SparseRange w_expect(Range::from_cap(caps[1]));
  SparseRange wW_expect(Range::from_cap(caps[4]));
  SparseRange rw_expect(Range::from_cap(caps[2]));
  SparseRange rwRW_expect(Range::from_cap(caps[5]));
  SparseRange seal_expect(Range::from_cap(caps[7]));
  rwRW_expect.combine(Range::from_cap(cap(&caps)));
  rw_expect.combine(rwRW_expect);
  wW_expect.combine(rwRW_expect);
  w_expect.combine(rw_expect);
  w_expect.combine(wW_expect);

  if (options().verbose()) {
    fprintf(stdout, "w_expect : ");
    w_expect.print_json(stdout, "\t");
  }

  TRY(w_map->ranges() == w_expect.parts());
  TRY(wW_map->ranges() == wW_expect.parts());
  TRY(rw_map->ranges() == rw_expect.parts());
  TRY(rwRW_map->ranges() == rwRW_expect.parts());
  TRY(seal_map->ranges() == seal_expect.parts());

#ifdef __CHERI_PURE_CAPABILITY__
  for (int i = 0; i < 6; i++) free(caps[i]);
#else
  for (int i = 0; i < 6; i++) free((void*)cheri_address_get(caps[i]));
#endif
}

TEST(scan_branch_map) {
  const cheri_perms_t PERM_r = CHERI_PERM_LOAD, PERM_w = CHERI_PERM_STORE,
                      PERM_rw = (cheri_perms_t)(PERM_r | PERM_w),
                      PERM_rR = (cheri_perms_t)(CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
                      PERM_wW = (cheri_perms_t)(CHERI_PERM_STORE | CHERI_PERM_STORE_CAP);

  void* __capability caps[8] = {
      cap_with_perms(sizeof(long), PERM_r), cap_with_perms(sizeof(long), PERM_w),
      cap_with_perms(sizeof(long), PERM_rw), cap_with_perms(sizeof(long), PERM_rR),
      cap_with_perms(sizeof(long), PERM_wW),
      cheri_perms_and(
          cheri_pcc_get(),
          CHERI_PERM_LOAD | CHERI_PERM_EXECUTE),  // XXX example of unsealed executable, but remove
                                                  // LoadCap permissions to avoid searching further
#ifdef __CHERI_PURE_CAPABILITY__
      (void*)&cap_with_perms
#else
      cheri_sentry_create(cheri_address_set(cheri_pcc_get(), (ptraddr_t)(void*)&cap_with_perms))
#endif
      // open slot for sealcap
  };
  size_t sz = sizeof(caps[7]);
  sysctlbyname("security.cheri.sealcap", &caps[7], &sz, NULL, 0);

  Mapper mapper;
  mapper.maps()->push_back(std::make_unique<capmap::BranchMap>());

  mapper.scan(cap(&caps), "caps");

  if (options().verbose()) {
    mapper.print_json(stdout);
  }
  TRY(mapper.maps()->size() == 1);
  auto branch_map = dynamic_cast<capmap::BranchMap const*>(mapper.maps()->at(0).get());

  // construct expected outcomes for each memory map
  SparseRange branch_expect(Range::from_base_length(cheri_address_get(caps[6]), 1));
  branch_expect.combine(Range::from_cap(caps[5]));

  if (options().verbose()) {
    fprintf(stdout, "branch_expect : ");
    branch_expect.print_json(stdout, "\t");
  }

  TRY(branch_map->ranges() == branch_expect.parts());

#ifdef __CHERI_PURE_CAPABILITY__
  for (int i = 0; i < 5; i++) free(caps[i]);
#else
  for (int i = 0; i < 5; i++) free((void*)cheri_address_get(caps[i]));
#endif
}

static int pois_acc = 0;
bool pois_cb(void* __capability cap) {
  (void)cap;
  pois_acc++;
  return false;
}

TEST(scan_poison_map) {
  typedef struct node {
    struct node* __capability next;
  } node_t;
  node_t* __capability early;
  node_t* __capability poison_node;
  node_t* head = (node_t*)malloc(sizeof(node_t));

  head->next = NULL;
  for (int i = 1; i < 16; i++) {
    node_t* add = (node_t*)malloc(sizeof(node_t));
    add->next = cap<node_t>(head);
    head = add;
    if (i == 4) {
      early = add->next;
    } else if (i == 8) {
      poison_node = add->next;
    }
  }

  SparseRange poison(Range::from_cap(poison_node));
  Mapper mapper;
  mapper.maps()->push_back(std::make_unique<capmap::PoisonMap>(
      "rwpoison", "virtual memory", (cheri_perms_t)(CHERI_PERM_LOAD | CHERI_PERM_STORE), poison,
      &pois_cb));
  pois_acc = 0;

  // Scanning 'early' should not result in a poisoned access (since the list is singly-linked)
  mapper.scan(early, "early");
  TRY(!pois_acc);

  // ... but scanning 'head' should
  mapper.scan(cap(head), "head");
  TRY(pois_acc);

  while (head) {
#ifdef __CHERI_PURE_CAPABILITY__
    node_t* next = head->next;
#else
    node_t* next = (node_t*)cheri_address_get(head->next);
#endif
    free(head);
    head = next;
  }
}
