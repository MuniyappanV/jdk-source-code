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

inline size_t G1RemSet::n_workers() {
  if (_g1->workers() != NULL) {
    return _g1->workers()->total_workers();
  } else {
    return 1;
  }
}

template <class T> inline void HRInto_G1RemSet::write_ref_nv(HeapRegion* from, T* p) {
  par_write_ref_nv(from, p, 0);
}

inline bool HRInto_G1RemSet::self_forwarded(oop obj) {
  bool result =  (obj->is_forwarded() && (obj->forwardee()== obj));
  return result;
}

template <class T> inline void HRInto_G1RemSet::par_write_ref_nv(HeapRegion* from, T* p, int tid) {
  oop obj = oopDesc::load_decode_heap_oop(p);
#ifdef ASSERT
  // can't do because of races
  // assert(obj == NULL || obj->is_oop(), "expected an oop");

  // Do the safe subset of is_oop
  if (obj != NULL) {
#ifdef CHECK_UNHANDLED_OOPS
    oopDesc* o = obj.obj();
#else
    oopDesc* o = obj;
#endif // CHECK_UNHANDLED_OOPS
    assert((intptr_t)o % MinObjAlignmentInBytes == 0, "not oop aligned");
    assert(Universe::heap()->is_in_reserved(obj), "must be in heap");
  }
#endif // ASSERT
  assert(from == NULL || from->is_in_reserved(p),
         "p is not in from");
  HeapRegion* to = _g1->heap_region_containing(obj);
  // The test below could be optimized by applying a bit op to to and from.
  if (to != NULL && from != NULL && from != to) {
    // There is a tricky infinite loop if we keep pushing
    // self forwarding pointers onto our _new_refs list.
    // The _par_traversal_in_progress flag is true during the collection pause,
    // false during the evacuation failure handing.
    if (_par_traversal_in_progress &&
        to->in_collection_set() && !self_forwarded(obj)) {
      _new_refs[tid]->push((void*)p);
      // Deferred updates to the Cset are either discarded (in the normal case),
      // or processed (if an evacuation failure occurs) at the end
      // of the collection.
      // See HRInto_G1RemSet::cleanup_after_oops_into_collection_set_do().
    } else {
#if G1_REM_SET_LOGGING
      gclog_or_tty->print_cr("Adding " PTR_FORMAT " (" PTR_FORMAT ") to RS"
                             " for region [" PTR_FORMAT ", " PTR_FORMAT ")",
                             p, obj,
                             to->bottom(), to->end());
#endif
      assert(to->rem_set() != NULL, "Need per-region 'into' remsets.");
      to->rem_set()->add_reference(p, tid);
    }
  }
}

template <class T> inline void UpdateRSOopClosure::do_oop_work(T* p) {
  assert(_from != NULL, "from region must be non-NULL");
  _rs->par_write_ref(_from, p, _worker_i);
}
