// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef CAPMAP_RANGE_H_
#define CAPMAP_RANGE_H_

#if __has_feature(capabilities)
#include <cheriintrin.h>
#else
#error "capmap.h requires capabilities"
#endif

#include <assert.h>

#include <set>

namespace capmap {

// A contiguous range over some address space.
//
// Address spaces are assumed to be completely addressable by ptraddr_t, and
// the length of a range is assumed to be representable by size_t.
class Range {
 public:
  // Derive a Range from a capability's bounds.
  static Range from_cap(void *__capability cap);

  // [base,last].
  static Range from_base_last(ptraddr_t base, ptraddr_t last) {
    Range r;
    r.base_ = base;
    r.last_ = last;
    return r;
  }

  // [base,limit).
  static Range from_base_limit(ptraddr_t base, ptraddr_t limit) {
    return from_base_last(base, limit - 1);
  }

  // [base,base+length).
  static Range from_base_length(ptraddr_t base, size_t length) {
    return from_base_last(base, base + length - 1);
  }

  // The full 64-bit address space.
  static Range full_64bit() { return from_base_last(0, UINT64_MAX); }

  // A range covering the C++ object pointer to by `ptr`.
  //
  // This considers only (C++) type information, and ignores capability bounds.
  template <typename T>
  static Range from_object(T const *ptr) {
#ifdef __CHERI_PURE_CAPABILITY__
    ptraddr_t base = cheri_address_get(ptr);
#else
    ptraddr_t base = reinterpret_cast<ptraddr_t>(ptr);
#endif
    return from_base_length(base, sizeof(T));
  }

  // An arbitrary empty range.
  Range() : base_(UINT64_MAX), last_(0) {}

  // Align the base up, and the limit down, to the specified alignment.
  //
  // `multiple` must be a power of two.
  //
  // Empty ranges remain unmodified because it is not always possible to align
  // them if either bound wraps.
  void shrink_to_alignment(size_t multiple) {
    assert(__builtin_popcount(multiple) == 1);  // Power of two.
    if (!is_empty()) {
      base_ = cheri_align_up(base_, multiple);
      last_ = cheri_align_down(last_ + 1, multiple) - 1;
    }
  }

  Range shrunk_to_alignment(size_t multiple) const {
    Range r = *this;
    r.shrink_to_alignment(multiple);
    return r;
  }

  bool overlaps(Range other) const { return (base_ <= other.last_) && (last_ >= other.base_); }
  bool includes(Range other) const { return (base_ <= other.base_) && (last_ >= other.last_); }
  bool includes(ptraddr_t addr) const { return (base_ <= addr) && (last_ >= addr); }

  bool follows(Range other) const { return (base_ > 0) && (base_ == other.last_ + 1); }
  bool preceeds(Range other) const { return other.follows(*this); }

  // If {*this, other} describes a single contiguous range, modify *this to be
  // that range (and return true).
  bool try_combine(Range other);

  bool is_empty() const { return last_ < base_; }
  ptraddr_t base() const { return base_; }
  ptraddr_t last() const { return last_; }
  std::pair<bool, ptraddr_t> limit() const {
    ptraddr_t limit = last_ + 1;
    return std::make_pair(limit < last_, limit);
  }
  std::pair<bool, size_t> length() const {
    if (is_empty()) return std::make_pair(false, 0);
    bool bit64 = (base_ == 0) && (last_ == UINT64_MAX);
    return std::make_pair(bit64, last_ - base_ + 1);
  }

  bool operator==(Range other) const { return (base_ == other.base_) && (last_ == other.last_); }
  bool operator!=(Range other) const { return !(operator==(other)); }

  // Provide an ordering, for fast look-up in sorted containers.
  //
  // Sorting only on `last_` is important; we use it to simplify SparseRange
  // operations.
  //
  // Note that ranges that compare less-than might still overlap. See
  // `preceeds()` if you need to check that a range is immediately before
  // another, without overlaps.
  bool operator<(Range other) const { return last_ < other.last_; }

 private:
  ptraddr_t base_;
  ptraddr_t last_;
};

void print_json(FILE *stream, std::set<Range> const &ranges, char const *line_prefix = "");

// Zero or more non-empty, non-overlapping, non-adjacent ranges, sorted by
// address.
//
// Combining or removing ranges automatically merges or splits sub-ranges, as
// required. This is used for performing set-like operations on address spaces,
// for example to determine what to scan.
class SparseRange {
 public:
  SparseRange() {}
  SparseRange(Range range) { combine(range); }

  bool is_empty() const { return ranges_.empty(); }
  bool is_contiguous() const { return ranges_.size() == 1; }

  void combine(Range range);
  void remove(Range range);

  void combine(SparseRange ranges) {
    for (auto range : ranges.parts()) combine(range);
  }
  void remove(SparseRange ranges) { remove(ranges.parts()); }
  void remove(std::set<Range> const &ranges) {
    for (auto range : ranges) remove(range);
  }

  bool overlaps(Range other) const;
  bool includes(Range other) const;
  bool includes(ptraddr_t addr) const { return includes(Range::from_base_last(addr, addr)); }

  bool includes(SparseRange other) const {
    for (auto range : other.parts()) {
      if (!includes(range)) return false;
    }
    return true;
  }

  std::set<Range> const &parts() const { return ranges_; }

  bool operator==(const SparseRange &other) const { return ranges_ == other.ranges_; }
  bool operator!=(const SparseRange &other) const { return !(operator==(other)); }

  void print_json(FILE *stream, char const *line_prefix = "") {
    ::capmap::print_json(stream, parts(), line_prefix);
  }

 private:
  std::set<Range> ranges_;
};

}  // namespace capmap
#endif
