// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef CAPMAP_MAPPERS_H_
#define CAPMAP_MAPPERS_H_

#include "capmap-range.h"

namespace capmap {

// A map, with a custom combination function.
//
// Every time the `Mapper` finds a capability, it asks each of its mappers to
// combine it by calling `try_combine()`.
class Map {
 public:
  // Return a user-facing name for the map.
  virtual char const *name() const = 0;

  // Return a user-facing address space name for the map.
  virtual char const *address_space() const = 0;

  // All ranges included in the map.
  //
  // Whilst most maps will merge adjacent or overlapping ranges, like SparseRange,
  // some might preserve the individual ranges. This is useful for execution
  // permissions, for example.
  //
  // TODO: This doesn't actually work for Execute, because we can't store those
  // in a std::set. Differently-overlapping ranges are relevant for Execute, so
  // we need a better way to represent them. Do we need some abstract iterator
  // type?
  virtual std::set<Range> const &ranges() const = 0;

  // If the capability has the necessary permissions, add it to the map.
  //
  // The implementation may shrink the range first, for example to apply
  // alignment constraints required by the permissions being mapped.
  //
  // Returns true if the capability had the necessary permissions and was
  // combined (even if shrunk for alignment or if the resulting map already
  // included it), and false otherwise.
  virtual bool try_combine(void *__capability cap) = 0;

  virtual ~Map(){};
};

// Memory ranges from which capabilities can be loaded.
//
// This requires the necessary permissions, but it also contracts the bounds to
// ensure that they're aligned.
class LoadCapMap : public Map {
 public:
  virtual char const *name() const override { return "load capabilities"; }
  virtual char const *address_space() const override { return "virtual memory"; }
  virtual std::set<Range> const &ranges() const override { return ranges_.parts(); }

  virtual bool try_combine(void *__capability cap) override;

  virtual ~LoadCapMap() {}

  // If `addr` refers to an area of memory that might contain a capability that
  // we haven't already scanned, return false, and leave `*cont` unmodified.
  //
  // Otherwise, return true, and set `*cont` to point to the next address that a
  // scan for new capability might continue from. This will typically be the
  // first capability-aligned excluded address after `addr`, but in some cases
  // (e.g. if the map covers the whole address space), it may still be included.
  bool includes_cap(ptraddr_t addr, ptraddr_t *cont) const;

  // Return a SparseRange representing all mapped regions from which
  // capabilities could be loaded (at the page table level).
  static SparseRange vmmap();

  SparseRange const &sparse_range() const { return ranges_; }

 private:
  SparseRange ranges_;
};

// Memory ranges from which data can be loaded.
class LoadMap : public Map {
 public:
  virtual char const *name() const override { return "load"; }
  virtual char const *address_space() const override { return "virtual memory"; }
  virtual std::set<Range> const &ranges() const override { return ranges_.parts(); }
  virtual bool try_combine(void *__capability cap) override;
  virtual ~LoadMap() {}

  SparseRange const &sparse_range() const { return ranges_; }

 private:
  SparseRange ranges_;
};

// TODO: Provide other `Map` derivatives.

}  // namespace capmap
#endif
