// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdio.h>

#include <limits>
#include <utility>

#include "include/capmap.h"

namespace capmap {

Range Range::from_cap(void* __capability cap) {
  ptraddr_t base = cheri_base_get(cap);
  size_t length = cheri_length_get(cap);

  // As a special case: if the real length is 2^64, `cheri_length_get()`
  // saturates to (2^64)-1. This length cannot be encoded in a compressed
  // capability, so we can detect this case unambiguously.
  if ((base == 0) && (length == std::numeric_limits<size_t>::max())) {
    return Range::from_base_last(0, -1);
  }

  return Range::from_base_length(base, length);
}

bool Range::try_combine(Range other) {
  if (overlaps(other) || preceeds(other) || follows(other)) {
    base_ = std::min(base_, other.base_);
    last_ = std::max(last_, other.last_);
    return true;
  }
  return false;
}

bool SparseRange::overlaps(Range other) const {
  if (other.is_empty()) return false;
  // Sub-ranges are disjoint and non-adjacent, so if we overlap with `other` at
  // all, we must do so with either the range that ends before `other`, or the
  // range that ends with or after it.
  auto range = ranges_.lower_bound(other);
  if (range != ranges_.end()) {
    if (range->overlaps(other)) return true;
  }
  if (range != ranges_.begin()) {
    --range;
    if (range->overlaps(other)) return true;
  }
  return false;
}

bool SparseRange::includes(Range other) const {
  if (other.is_empty()) return false;
  // Sub-ranges are disjoint and non-adjacent, so if we include `other`, it is
  // with exactly one range, which must be the first one that ends on or after
  // it.
  auto range = ranges_.lower_bound(other);
  return (range != ranges_.end()) && range->includes(other);
}

void SparseRange::combine(Range other) {
  if (other.is_empty()) return;
  if (ranges_.empty()) {
    ranges_.insert(other);
    return;
  }

  // Find the first and last ranges that could be combined with `other` (if any
  // can). The iterators are bidirectional, so we can start with a fast and
  // simple look up, then search linearly.
  auto repl_start = ranges_.lower_bound(other);
  // If we can't combine with the lower_bound(), try the previous one (if there
  // is one).
  if ((repl_start == ranges_.end()) ||
      ((repl_start != ranges_.begin()) && !other.try_combine(*repl_start))) {
    --repl_start;
  }
  auto repl_end = repl_start;

  // Scan and combine in each direction.
  // - Put repl_start at the first replaceable range.
  // - Put repl_end just past the last replaceable range (or at the `end()`).
  // If no ranges are replaceable, leave `repl_end == repl_start`.
  while (repl_start != ranges_.begin()) {
    auto prev = repl_start;
    --prev;
    if (other.try_combine(*prev)) {
      repl_start = prev;
    } else {
      break;
    }
  }
  while (repl_end != ranges_.end()) {
    if (other.try_combine(*repl_end)) {
      ++repl_end;
    } else {
      break;
    }
  }

  ranges_.erase(repl_start, repl_end);
  ranges_.insert(other);
}

void SparseRange::remove(Range other) {
  if (other.is_empty()) return;
  if (ranges_.empty()) return;

  // Find the first and last ranges that overlap `other` (if any do).
  auto repl_start = ranges_.lower_bound(other);
  if ((repl_start == ranges_.end()) ||
      ((repl_start != ranges_.begin()) && !repl_start->overlaps(other))) {
    --repl_start;
  }

  if ((repl_start == ranges_.end()) || !repl_start->overlaps(other)) return;
  auto repl_end = repl_start;

  // Scan in each direction.
  // - Put repl_start at the first replaceable range.
  // - Put repl_last at the last replaceable range, and repl_end just past it.
  while (repl_start != ranges_.begin()) {
    auto prev = repl_start;
    --prev;
    if (prev->overlaps(other)) {
      repl_start = prev;
    } else {
      break;
    }
  }
  auto repl_last = repl_end++;
  while (repl_end != ranges_.end()) {
    if (!repl_end->overlaps(other)) break;
    repl_last = repl_end++;
  }

  Range l;
  Range h;
  if (repl_start->base() < other.base()) {
    l = Range::from_base_limit(repl_start->base(), other.base());
  }
  if (other.last() < repl_last->last()) {
    h = Range::from_base_last(other.last() + 1, repl_last->last());
  }

  ranges_.erase(repl_start, repl_end);
  if (!l.is_empty()) ranges_.insert(l);
  if (!h.is_empty()) ranges_.insert(h);
}

void print_json(FILE* stream, std::set<Range> const& ranges, char const* line_prefix) {
  switch (ranges.size()) {
    case 0:
      fprintf(stream, "[]");
      return;
    case 1:
      fprintf(stream, "[ { \"base\": 0x%zx, \"last\": 0x%zx } ]", ranges.begin()->base(),
              ranges.begin()->last());
      return;
  }
  fprintf(stream, "[");
  char const* sep = "\n";
  for (auto const part : ranges) {
    fprintf(stream, "%s%s    { \"base\": 0x%zx, \"last\": 0x%zx }", sep, line_prefix, part.base(),
            part.last());
    sep = ",\n";
  }
  fprintf(stream, "\n%s]", line_prefix);
}

}  // namespace capmap
