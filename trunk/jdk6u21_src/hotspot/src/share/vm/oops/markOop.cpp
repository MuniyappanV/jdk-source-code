/*
 * Copyright (c) 1997, 2008, Oracle and/or its affiliates. All rights reserved.
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

# include "incls/_precompiled.incl"
# include "incls/_markOop.cpp.incl"


void markOopDesc::print_on(outputStream* st) const {
  if (is_locked()) {
    st->print("locked(0x%lx)->", value());
    markOop(*(markOop*)value())->print_on(st);
  } else {
    assert(is_unlocked() || has_bias_pattern(), "just checking");
    st->print("mark(");
    if (has_bias_pattern())  st->print("biased,");
    st->print("hash %#lx,", hash());
    st->print("age %d)", age());
  }
}


// Give advice about whether the oop that contains this markOop
// should be cached or not.
bool markOopDesc::should_not_be_cached() const {
  // the cast is because decode_pointer() isn't marked const
  if (is_marked() && ((markOopDesc *)this)->decode_pointer() != NULL) {
    // If the oop containing this markOop is being forwarded, then
    // we are in the middle of GC and we do not want the containing
    // oop to be added to a cache. We have no way of knowing whether
    // the cache has already been visited by the current GC phase so
    // we don't know whether the forwarded oop will be properly
    // processed in this phase. If the forwarded oop is not properly
    // processed, then we'll see strange crashes or asserts during
    // the next GC run because the markOop will contain an unexpected
    // value.
    //
    // This situation has been seen when we are GC'ing a methodOop
    // because we use the methodOop while we're GC'ing it. Scary
    // stuff. Some of the uses the methodOop cause the methodOop to
    // be added to the OopMapCache in the instanceKlass as a side
    // effect. This check lets the cache maintainer know when a
    // cache addition would not be safe.
    return true;
  }

  // caching the containing oop should be just fine
  return false;
}
