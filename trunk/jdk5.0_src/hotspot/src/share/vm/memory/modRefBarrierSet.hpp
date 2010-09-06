#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)modRefBarrierSet.hpp	1.10 03/12/23 16:41:20 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This kind of "BarrierSet" allows a "CollectedHeap" to detect and
// enumerate ref fields that have been modified (since the last
// enumeration), using a card table.

class OopClosure;
class Generation;

class ModRefBarrierSet: public BarrierSet {
public:
  // Barriers only on ref writes.
  bool has_read_ref_barrier() { return false; }
  bool has_read_prim_barrier() { return false; }
  bool has_write_ref_barrier() { return true; }
  bool has_write_prim_barrier() { return false; }
  
  bool read_ref_needs_barrier(oop* field) { return false; }
  bool read_prim_needs_barrier(HeapWord* field, size_t bytes) { return false; }
  virtual bool write_ref_needs_barrier(oop* field, oop new_val) = 0;
  bool write_prim_needs_barrier(HeapWord* field, size_t bytes,
				juint val1, juint val2) { return false; }

  void write_prim_field(oop obj, size_t offset, size_t bytes,
			juint val1, juint val2) {}

  void read_ref_field(oop* field) {}
  void read_prim_field(HeapWord* field, size_t bytes) {}
protected:
  virtual void write_ref_field_work(oop* field, oop new_val) = 0;
public:
  void write_prim_field(HeapWord* field, size_t bytes,
			juint val1, juint val2) {}

  bool has_read_ref_array_opt() { return false; }
  bool has_read_prim_array_opt() { return false; }
  bool has_write_prim_array_opt() { return false; }

  bool has_read_region_opt() { return false; }


  // These operations should assert false unless the correponding operation
  // above returns true.
  void read_ref_array(MemRegion mr) {
    assert(false, "can't call");
  }
  void read_prim_array(MemRegion mr) {
    assert(false, "can't call");
  }
  void write_prim_array(MemRegion mr) {
    assert(false, "can't call");
  }
  void read_region(MemRegion mr) {
    assert(false, "can't call");
  }

  // Invoke "cl->do_oop" on (the address of) every possibly-modifed
  // reference field in objects in "sp".  If "clear" is "true", the oops
  // are no longer considered possibly modified after application of the
  // closure.  If' "before_save_marks" is true, oops in objects allocated
  // after the last call to "save_marks" on "sp" will not be considered.
  virtual void mod_oop_in_space_iterate(Space* sp, OopClosure* cl,
                                        bool clear = false,
                                        bool before_save_marks = false) = 0;

  // Causes all refs in "mr" to be assumed to be modified.
  virtual void invalidate(MemRegion mr) = 0;

  // The caller guarantees that "mr" contains no references.  (Perhaps it's 
  // objects have been moved elsewhere.)
  virtual void clear(MemRegion mr) = 0;

  // Pass along the argument to the superclass.
  ModRefBarrierSet(int max_covered_regions) :
    BarrierSet(max_covered_regions) {}

#ifndef PRODUCT
  // Verifies that the given region contains no modified references.
  virtual void verify_clean_region(MemRegion mr) = 0;
#endif

};
