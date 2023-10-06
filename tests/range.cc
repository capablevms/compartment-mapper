// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "include/capmap-range.h"
#include "tests.h"

using capmap::Range;

TEST(range_default) {
  Range r;
  TRY(r.is_empty());
}

TEST(range_min_min) {
  Range r = Range::from_base_last(0, 0);
  TRY(!r.is_empty());
  TRY(r.base() == 0);
  TRY(r.last() == 0);
  TRY(r.limit().first == false);
  TRY(r.limit().second == 1);
  TRY(r.length().first == false);
  TRY(r.length().second == 1);
}

TEST(range_max_max) {
  Range r = Range::from_base_last(0xffffffffffffffff, 0xffffffffffffffff);
  TRY(!r.is_empty());
  TRY(r.base() == 0xffffffffffffffff);
  TRY(r.last() == 0xffffffffffffffff);
  TRY(r.limit().first == true);
  TRY(r.limit().second == 0);
  TRY(r.length().first == false);
  TRY(r.length().second == 1);
}

TEST(range_min_max) {
  Range r = Range::from_base_last(0, 0xffffffffffffffff);
  TRY(!r.is_empty());
  TRY(r.base() == 0);
  TRY(r.last() == 0xffffffffffffffff);
  TRY(r.limit().first == true);
  TRY(r.limit().second == 0);
  TRY(r.length().first == true);
  TRY(r.length().second == 0);
}

TEST(range_min_empty) {
  Range r = Range::from_base_last(1, 0);
  TRY(r.is_empty());
  TRY(r.base() == 1);
  TRY(r.last() == 0);
  TRY(r.limit().first == false);
  TRY(r.limit().second == 1);
  TRY(r.length().first == false);
  TRY(r.length().second == 0);
}

TEST(range_42_empty) {
  Range r = Range::from_base_last(42, 0);
  TRY(r.is_empty());
  TRY(r.base() == 42);
  TRY(r.last() == 0);
  TRY(r.limit().first == false);
  TRY(r.limit().second == 1);
  TRY(r.length().first == false);
  TRY(r.length().second == 0);
}

TEST(range_from_base_limit) {
  ptraddr_t values[] = {0, 1, 42, 0x8000'0000'0000, 0xff'ffff'ffff'ffff, 0xffff'ffff'ffff'fffe};
  for (auto base : values) {
    for (auto last : values) {
      TRY(Range::from_base_last(base, last) == Range::from_base_limit(base, last + 1));
    }
  }
}

TEST(range_from_base_length) {
  ptraddr_t values[] = {0, 1, 42, 0x8000'0000'0000, 0xff'ffff'ffff'ffff, 0xffff'ffff'ffff'fffe};
  for (auto base : values) {
    for (auto last : values) {
      if (last >= base) {
        TRY(Range::from_base_last(base, last) == Range::from_base_length(base, last - base + 1));
      }
    }
  }
}

TEST(range_from_pcc) {
  auto cap = cheri_pcc_get();
  Range r = Range::from_cap(cap);
  TRY(r.base() == cheri_base_get(cap));
  if (cheri_length_get(cap) < 0xffff'ffff'ffff'ffff) {
    TRY(r.length().first == false);
    TRY(r.length().second == cheri_length_get(cap));
  } else {
    TRY(r.length().first == true);
    TRY(r.length().second == 0);
  }
}

TEST(range_from_object_u8) {
  uint8_t o = 42;
  auto r = Range::from_object(&o);
  TRY(r.length().first == false);
  TRY(r.length().second == 1);
}

TEST(range_from_object_u64) {
  uint64_t o = 42;
  auto r = Range::from_object(&o);
  TRY(r.length().first == false);
  TRY(r.length().second == 8);
}

TEST(range_from_object_u64x42) {
  // This checks that array type information is propagated through the template.
  uint64_t o[42] = {0};
  auto r = Range::from_object(&o);
  TRY(r.length().first == false);
  TRY(r.length().second == (8 * 42));
}

TEST(range_shrink_to_alignment_nop) {
  Range const r = Range::from_base_limit(42, 52);
  TRY(r.shrunk_to_alignment(2) == r);
}

TEST(range_shrink_to_alignment_base) {
  Range const r = Range::from_base_limit(41, 60);
  TRY(r.shrunk_to_alignment(4) == Range::from_base_limit(44, 60));
}

TEST(range_shrink_to_alignment_limit) {
  Range const r = Range::from_base_limit(44, 63);
  TRY(r.shrunk_to_alignment(4) == Range::from_base_limit(44, 60));
}

TEST(range_shrink_to_alignment_both) {
  Range const r = Range::from_base_limit(43, 61);
  TRY(r.shrunk_to_alignment(4) == Range::from_base_limit(44, 60));
}

TEST(range_shrink_to_alignment_max) {
  Range const r = Range::from_base_last(0, 0xffff'ffff'ffff'ffff);
  TRY(r.shrunk_to_alignment(4) == r);
}

TEST(range_shrink_to_alignment_empty) {
  // Empty ranges are unmodified by alignment, because the results aren't always
  // representable.
  Range const r = Range::from_base_last(3, 2);
  TRY(r.shrunk_to_alignment(4) == r);
}

TEST(range_shrink_to_alignment_become_empty) {
  Range r = Range::from_base_last(5, 6);
  TRY(!r.is_empty());
  TRY(r.length().first == false);
  TRY(r.length().second == 2);
  r.shrink_to_alignment(4);
  TRY(r == Range::from_base_last(8, 3));
  TRY(r.is_empty());
  TRY(r.length().first == false);
  TRY(r.length().second == 0);
}

TEST(range_combination_discontiguous) {
  Range a = Range::from_base_last(42, 52);
  Range b = Range::from_base_last(54, 64);
  Range c = Range::from_base_last(66, 76);

  TRY(!a.overlaps(b));
  TRY(!a.overlaps(c));
  TRY(!b.overlaps(a));
  TRY(!b.overlaps(c));
  TRY(!c.overlaps(a));
  TRY(!c.overlaps(b));

  TRY(!a.preceeds(b));
  TRY(!a.preceeds(c));
  TRY(!b.preceeds(a));
  TRY(!b.preceeds(c));
  TRY(!c.preceeds(a));
  TRY(!c.preceeds(b));

  TRY(!a.follows(b));
  TRY(!a.follows(c));
  TRY(!b.follows(a));
  TRY(!b.follows(c));
  TRY(!c.follows(a));
  TRY(!c.follows(b));

  TRY(!a.try_combine(b));
  TRY(!a.try_combine(c));
  TRY(!b.try_combine(a));
  TRY(!b.try_combine(c));
  TRY(!c.try_combine(a));
  TRY(!c.try_combine(b));

  TRY(a == Range::from_base_last(42, 52));
  TRY(b == Range::from_base_last(54, 64));
  TRY(c == Range::from_base_last(66, 76));
}

TEST(range_combination_contiguous) {
  Range a = Range::from_base_last(42, 53);
  Range b = Range::from_base_last(54, 65);
  Range c = Range::from_base_last(66, 76);

  TRY(!a.overlaps(b));
  TRY(!a.overlaps(c));
  TRY(!b.overlaps(a));
  TRY(!b.overlaps(c));
  TRY(!c.overlaps(a));
  TRY(!c.overlaps(b));

  TRY(a.preceeds(b));
  TRY(!a.preceeds(c));
  TRY(!b.preceeds(a));
  TRY(b.preceeds(c));
  TRY(!c.preceeds(a));
  TRY(!c.preceeds(b));

  TRY(!a.follows(b));
  TRY(!a.follows(c));
  TRY(b.follows(a));
  TRY(!b.follows(c));
  TRY(!c.follows(a));
  TRY(c.follows(b));

  TRY(!a.try_combine(c));
  TRY(!c.try_combine(a));
  TRY(a.try_combine(b));
  TRY(a == Range::from_base_last(42, 65));
  TRY(c.try_combine(a));
  TRY(c == Range::from_base_last(42, 76));
  TRY(b == Range::from_base_last(54, 65));  // Unmodified.
}

TEST(range_combination_overlapping) {
  Range a = Range::from_base_last(42, 54);
  Range b = Range::from_base_last(54, 66);
  Range c = Range::from_base_last(66, 76);

  TRY(a.overlaps(b));
  TRY(!a.overlaps(c));
  TRY(b.overlaps(a));
  TRY(b.overlaps(c));
  TRY(!c.overlaps(a));
  TRY(c.overlaps(b));

  TRY(!a.preceeds(b));
  TRY(!a.preceeds(c));
  TRY(!b.preceeds(a));
  TRY(!b.preceeds(c));
  TRY(!c.preceeds(a));
  TRY(!c.preceeds(b));

  TRY(!a.follows(b));
  TRY(!a.follows(c));
  TRY(!b.follows(a));
  TRY(!b.follows(c));
  TRY(!c.follows(a));
  TRY(!c.follows(b));

  TRY(!a.try_combine(c));
  TRY(!c.try_combine(a));
  TRY(a.try_combine(b));
  TRY(a == Range::from_base_last(42, 66));
  TRY(c.try_combine(a));
  TRY(c == Range::from_base_last(42, 76));
  TRY(b == Range::from_base_last(54, 66));  // Unmodified.
}

TEST(range_overlaps_includes) {
  Range iii = Range::from_base_last(42, 420);
  Range oi = Range::from_base_last(10, 50);
  Range io = Range::from_base_last(400, 500);
  Range i = Range::from_base_last(50, 400);
  Range oiii = Range::from_base_last(41, 420);
  Range iiio = Range::from_base_last(42, 421);

  TRY(iii.overlaps(iii));
  TRY(iii.overlaps(oi));
  TRY(iii.overlaps(io));
  TRY(iii.overlaps(i));
  TRY(iii.overlaps(oiii));
  TRY(iii.overlaps(iiio));

  TRY(iii.includes(iii));
  TRY(!iii.includes(oi));
  TRY(!iii.includes(io));
  TRY(iii.includes(i));
  TRY(!iii.includes(oiii));
  TRY(!iii.includes(iiio));

  TRY(!iii.includes(41));
  TRY(iii.includes(42));
  TRY(iii.includes(420));
  TRY(!iii.includes(421));
}
