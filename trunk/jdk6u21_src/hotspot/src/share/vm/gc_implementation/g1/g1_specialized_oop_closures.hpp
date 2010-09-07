/*
 * Copyright (c) 2001, 2009, Oracle and/or its affiliates. All rights reserved.
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

// The following OopClosure types get specialized versions of
// "oop_oop_iterate" that invoke the closures' do_oop methods
// non-virtually, using a mechanism defined in this file.  Extend these
// macros in the obvious way to add specializations for new closures.

// Forward declarations.
enum G1Barrier {
  G1BarrierNone, G1BarrierRS, G1BarrierEvac
};

template<bool do_gen_barrier, G1Barrier barrier,
         bool do_mark_forwardee>
class G1ParCopyClosure;
class G1ParScanClosure;
class G1ParPushHeapRSClosure;

typedef G1ParCopyClosure<false, G1BarrierEvac, false> G1ParScanHeapEvacClosure;

class FilterIntoCSClosure;
class FilterOutOfRegionClosure;
class FilterInHeapRegionAndIntoCSClosure;
class FilterAndMarkInHeapRegionAndIntoCSClosure;

#ifdef FURTHER_SPECIALIZED_OOP_OOP_ITERATE_CLOSURES
#error "FURTHER_SPECIALIZED_OOP_OOP_ITERATE_CLOSURES already defined."
#endif

#define FURTHER_SPECIALIZED_OOP_OOP_ITERATE_CLOSURES(f) \
      f(G1ParScanHeapEvacClosure,_nv)                   \
      f(G1ParScanClosure,_nv)                           \
      f(G1ParPushHeapRSClosure,_nv)                     \
      f(FilterIntoCSClosure,_nv)                        \
      f(FilterOutOfRegionClosure,_nv)                   \
      f(FilterInHeapRegionAndIntoCSClosure,_nv)         \
      f(FilterAndMarkInHeapRegionAndIntoCSClosure,_nv)

#ifdef FURTHER_SPECIALIZED_SINCE_SAVE_MARKS_CLOSURES
#error "FURTHER_SPECIALIZED_SINCE_SAVE_MARKS_CLOSURES already defined."
#endif

#define FURTHER_SPECIALIZED_SINCE_SAVE_MARKS_CLOSURES(f)
