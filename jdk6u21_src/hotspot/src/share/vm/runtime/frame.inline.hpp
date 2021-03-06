/*
 * Copyright (c) 1997, 2006, Oracle and/or its affiliates. All rights reserved.
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

// This file holds platform-independent bodies of inline functions for frames.

// Note: The bcx usually contains the bcp; however during GC it contains the bci
//       (changed by gc_prologue() and gc_epilogue()) to be methodOop position
//       independent. These accessors make sure the correct value is returned
//       by testing the range of the bcx value. bcp's are guaranteed to be above
//       max_method_code_size, since methods are always allocated in OldSpace and
//       Eden is allocated before OldSpace.
//
//       The bcp is accessed sometimes during GC for ArgumentDescriptors; than
//       the correct translation has to be performed (was bug).

inline bool frame::is_bci(intptr_t bcx) {
#ifdef _LP64
  return ((uintptr_t) bcx) <= ((uintptr_t) max_method_code_size) ;
#else
  return 0 <= bcx && bcx <= max_method_code_size;
#endif
}

inline bool frame::is_entry_frame() const {
  return StubRoutines::returns_to_call_stub(pc());
}

inline bool frame::is_first_frame() const {
  return is_entry_frame() && entry_frame_is_first();
}

// here are the platform-dependent bodies:

# include "incls/_frame_pd.inline.hpp.incl"
