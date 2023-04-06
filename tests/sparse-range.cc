// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdlib.h>

#include "include/capmap-range.h"
#include "tests.h"

using capmap::Range;
using capmap::SparseRange;

TEST(sparse_range_simple) {
  Range ranges[] = {
      Range::from_base_last(42, 420),
      Range::full_64bit(),
      Range::from_base_last(0, 0),
      Range::from_base_last(0xffffffffffffffff, 0xffffffffffffffff),
  };
  for (auto r : ranges) {
    print_range_hex("  ", r, "\n");
    SparseRange sr(r);
    TRY(sr.overlaps(r));
    TRY(sr.includes(r));
    TRY(sr.parts().size() == 1);
    TRY(sr.parts().count(r) == 1);
  }
}

TEST(sparse_range_empty) {
  SparseRange sr;
  TRY(!sr.overlaps(Range::from_base_last(0, 0)));
  TRY(!sr.includes(Range::from_base_last(0, 0)));
  TRY(sr.parts().empty());
}

TEST(sparse_range_combine_empty) {
  Range r = Range::from_base_last(42, 420);
  SparseRange sr;
  sr.combine(r);
  TRY(sr == SparseRange(r));
}

//   [---l---]
// +             [----h----]
TEST(sparse_range_combine_disjoint) {
  Range a = Range::from_base_last(42, 420);
  Range b = Range::from_base_last(4200, 42000);
  SparseRange sr(a);
  sr.combine(b);
  TRY(sr.overlaps(a));
  TRY(sr.overlaps(b));
  TRY(sr.includes(a));
  TRY(sr.includes(b));

  TRY(sr.overlaps(Range::from_base_last(420, 421)));
  TRY(sr.overlaps(Range::from_base_last(4199, 4200)));
  TRY(!sr.overlaps(Range::from_base_last(421, 4199)));
  TRY(!sr.includes(Range::from_base_last(420, 421)));
  TRY(!sr.includes(Range::from_base_last(4199, 4200)));
  TRY(!sr.includes(Range::from_base_last(421, 4199)));
  TRY(sr.parts().size() == 2);
  TRY(*sr.parts().find(a) == a);
  TRY(*sr.parts().find(b) == b);
}

//               [----h----]
// + [---l---]
TEST(sparse_range_combine_overlap_lh) {
  Range l = Range::from_base_last(10, 50);
  Range h = Range::from_base_last(42, 420);
  print_range_dec("l = ", l, "\n");
  print_range_dec("h = ", h, "\n");

  SparseRange lh(l);
  lh.combine(h);
  print_sparse_range_dec("{h,l} = ", lh, "\n");
  TRY(lh.overlaps(l));
  TRY(lh.overlaps(h));
  TRY(lh.includes(l));
  TRY(lh.includes(h));
  TRY(lh.parts().size() == 1);
}

//         [----h----]
// + [---l---]
// = [------lh-------]
TEST(sparse_range_combine_overlap_hl) {
  Range l = Range::from_base_last(10, 50);
  Range h = Range::from_base_last(42, 420);
  print_range_dec("l = ", l, "\n");
  print_range_dec("h = ", h, "\n");

  SparseRange hl(h);
  hl.combine(l);
  print_sparse_range_dec("{h,l} = ", hl, "\n");
  TRY(hl.overlaps(l));
  TRY(hl.overlaps(h));
  TRY(hl.includes(l));
  TRY(hl.includes(h));
  TRY(hl.parts().size() == 1);
}

//            [---l---]     [----h----]
// + [--n--]
TEST(sparse_range_combine_disjoint_nlh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 98);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{n,l,h}: ", sr, "\n");
  TRY(sr.includes(n));
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 3);
}

//            [---l---]     [----h----]
// +   [--n--]
// =   [------nl------]     [----h----]
TEST(sparse_range_combine_adjacent_nlh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 99);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{n,l,h}: ", sr, "\n");
  TRY(sr.includes(n));
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//            [---l---]     [----h----]
// +   [---n---]
// =   [------nl------]     [----h----]
TEST(sparse_range_combine_overlap_nlh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 100);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{n,l,h}: ", sr, "\n");
  TRY(sr.includes(n));
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//            [---l---]     [----h----]
// +   [------n-------]
// =   [------nl------]     [----h----]
TEST(sparse_range_combine_extend_nlh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 199);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{n,l,h}: ", sr, "\n");
  TRY(sr.includes(n));
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//            [---l---]     [----h----]
// +   [--------n--------]
// =   [--------n--------]  [----h----]
TEST(sparse_range_combine_replace_nlh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 249);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{n,l,h}: ", sr, "\n");
  TRY(sr.includes(n));
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +               [--n--]
// =     [---l---] [--n--] [----h----]
TEST(sparse_range_combine_disjoint_lnh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(201, 298);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 3);
}

//       [---l---]         [----h----]
// +              [---n--]
// =     [-------ln------] [----h----]
TEST(sparse_range_combine_adjacent_llh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(200, 298);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +            [---n----]
// =     [-------ln------] [----h----]
TEST(sparse_range_combine_overlap_llh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(199, 298);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +     [-------n-------]
// =     [-------ln------] [----h----]
TEST(sparse_range_combine_extend_llh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(100, 298);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// + [---------n---------]
// = [---------n---------] [----h----]
TEST(sparse_range_combine_replace_llh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 298);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +               [---n--]
// =     [---l---] [--------nh-------]
TEST(sparse_range_combine_adjacent_lhh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(201, 299);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +               [---n----]
// =     [---l---] [--------nh-------]
TEST(sparse_range_combine_overlap_lhh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(201, 300);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +               [--------n--------]
// =     [---l---] [--------nh-------]
TEST(sparse_range_combine_extend_lhh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(201, 399);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +               [-----------n----------]
// =     [---l---] [-----------n----------]
TEST(sparse_range_combine_replace_lhh) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(201, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 2);
}

//       [---l---]         [----h----]
// +              [---n---]
// =     [---------------------------]
TEST(sparse_range_combine_fill_adjacent_nnn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(200, 299);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 1);
}

//       [---l---]         [----h----]
// +           [------n------]
// =     [---------------------------]
TEST(sparse_range_combine_fill_overlap_nnn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(142, 342);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 1);
}

//       [---l---]         [----h----]
// +     [------------n--------------]
// =     [---------------------------]
TEST(sparse_range_combine_fill_extend_nnn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(100, 399);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  // All ranges should be merged.
  TRY(sr.parts().size() == 1);
}

//       [---l---]         [----h----]
// + [----------------n------------------]
// =     [---------------------------]
TEST(sparse_range_combine_fill_replce_nnn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(42, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,n,h}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(n));
  TRY(sr.includes(h));
  TRY(sr.parts().size() == 1);
}

//       [---l---]    [----h----]
// +                              [---n---]
// =     [---l---]    [----h----] [---n---]
TEST(sparse_range_combine_disjoint_lhn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(401, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,h,n}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.includes(n));
  TRY(sr.parts().size() == 3);
}

//       [---l---]    [----h----]
// +                             [---n---]
// =     [---l---]    [---------hn-------]
TEST(sparse_range_combine_adjacent_lhn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(400, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,h,n}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.includes(n));
  TRY(sr.parts().size() == 2);
}

//       [---l---]    [----h----]
// +                          [------n---]
// =     [---l---]    [---------hn-------]
TEST(sparse_range_combine_overlap_lhn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(399, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,h,n}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.includes(n));
  TRY(sr.parts().size() == 2);
}

//       [---l---]    [----h----]
// +                  [----------n-------]
// =     [---l---]    [---------hn-------]
TEST(sparse_range_combine_extend_lhn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(300, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,h,n}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.includes(n));
  TRY(sr.parts().size() == 2);
}

//       [---l---]    [----h----]
// +               [----------n----------]
// =     [---l---] [----------n----------]
TEST(sparse_range_combine_replace_lhn) {
  Range l = Range::from_base_last(100, 199);
  Range h = Range::from_base_last(300, 399);
  SparseRange sr;
  sr.combine(l);
  sr.combine(h);
  print_sparse_range_dec("{l,h}: ", sr, "\n");

  Range n = Range::from_base_last(242, 420);
  print_range_dec("n: ", n, "\n");
  sr.combine(n);
  print_sparse_range_dec("{l,h,n}: ", sr, "\n");
  TRY(sr.includes(l));
  TRY(sr.includes(h));
  TRY(sr.includes(n));
  TRY(sr.parts().size() == 2);
}

TEST(sparse_range_combine_remove_fuzz) {
  uint64_t reference = 0;
  SparseRange sr;

  const int iterations = 4096;
  if (options().verbosity() >= 2) print_sparse_range_bitmap("  ", sr, "\n", 64);
  for (int i = 0; i < iterations; i++) {
    size_t base = (size_t)mrand48() % 64;
    size_t len = (size_t)mrand48() % 8;
    size_t last = base + len;
    if (last > 63) last = 63;
    uint64_t base_mask = (((uint64_t)1 << base) - 1);
    uint64_t last_mask = ((((uint64_t)1 << last) << 1) - 1);

    Range r = Range::from_base_last(base, last);
    if ((i < 16) || (mrand48() % 2)) {
      sr.combine(r);
      reference |= (base_mask ^ last_mask);
    } else {
      sr.remove(r);
      reference &= ~(base_mask ^ last_mask);
    }
    if (options().verbosity() >= 2) {
      print_sparse_range_bitmap("  ", sr, "  ", 64, r);
      print_sparse_range_dec("", sr, "\n");
    }

    // Verify sub-range properties.
    Range prev;
    for (auto& range : sr.parts()) {
      if (!prev.is_empty()) {
        TRY(prev < range);
        TRY(!prev.overlaps(range));
        TRY(!prev.preceeds(range));
      }
      TRY(!range.is_empty());
      prev = range;
    }

    // Check that the bitmask matches.
    uint64_t check = 0;
    for (size_t bit = 0; bit < 64; bit++) {
      if (sr.includes(bit)) {
        check |= (uint64_t)1 << bit;
      }
    }
    TRY(check == reference);
  }
}
