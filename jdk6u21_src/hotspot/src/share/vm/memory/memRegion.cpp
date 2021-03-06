/*
 * Copyright (c) 2000, 2004, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

// A very simple data structure representing a contigous word-aligned
// region of address space.

#include "incls/_precompiled.incl"
#include "incls/_memRegion.cpp.incl"

MemRegion MemRegion::intersection(const MemRegion mr2) const {
  MemRegion res;
  HeapWord* res_start = MAX2(start(), mr2.start());
  HeapWord* res_end   = MIN2(end(),   mr2.end());
  if (res_start < res_end) {
    res.set_start(res_start);
    res.set_end(res_end);
  }
  return res;
}

MemRegion MemRegion::_union(const MemRegion mr2) const {
  // If one region is empty, return the other
  if (is_empty()) return mr2;
  if (mr2.is_empty()) return MemRegion(start(), end());

  // Otherwise, regions must overlap or be adjacent
  assert(((start() <= mr2.start()) && (end() >= mr2.start())) ||
         ((mr2.start() <= start()) && (mr2.end() >= start())),
             "non-adjacent or overlapping regions");
  MemRegion res;
  HeapWord* res_start = MIN2(start(), mr2.start());
  HeapWord* res_end   = MAX2(end(),   mr2.end());
  res.set_start(res_start);
  res.set_end(res_end);
  return res;
}

MemRegion MemRegion::minus(const MemRegion mr2) const {
  // There seem to be 6 cases:
  //                  |this MemRegion|
  // |strictly below|
  //   |overlap beginning|
  //                    |interior|
  //                        |overlap ending|
  //                                   |strictly above|
  //              |completely overlapping|
  // We can't deal with an interior case because it would
  // produce two disjoint regions as a result.
  // We aren't trying to be optimal in the number of tests below,
  // but the order is important to distinguish the strictly cases
  // from the overlapping cases.
  if (mr2.end() <= start()) {
    // strictly below
    return MemRegion(start(), end());
  }
  if (mr2.start() <= start() && mr2.end() <= end()) {
    // overlap beginning
    return MemRegion(mr2.end(), end());
  }
  if (mr2.start() >= end()) {
    // strictly above
    return MemRegion(start(), end());
  }
  if (mr2.start() >= start() && mr2.end() >= end()) {
    // overlap ending
    return MemRegion(start(), mr2.start());
  }
  if (mr2.start() <= start() && mr2.end() >= end()) {
    // completely overlapping
    return MemRegion();
  }
  if (mr2.start() > start() && mr2.end() < end()) {
    // interior
    guarantee(false, "MemRegion::minus, but interior");
    return MemRegion();
  }
  ShouldNotReachHere();
  return MemRegion();
}
