#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)barrierSet.hpp	1.12 03/12/23 16:40:51 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This class provides the interface between a barrier implementation and
// the rest of the system.

class BarrierSet: public CHeapObj {
  friend class VMStructs;
public:
  enum Name {
    ModRef,
    CardTableModRef,
    CardTableExtension,
    Other,
    Uninit
  };

protected:
  int _max_covered_regions;
  Name _kind;
  
public:

  // To get around prohibition on RTTI.
  virtual BarrierSet::Name kind() { return _kind; }
  virtual bool is_a(BarrierSet::Name bsn) = 0;

  // These operations indicate what kind of barriers the BarrierSet has.
  virtual bool has_read_ref_barrier() = 0;
  virtual bool has_read_prim_barrier() = 0;
  virtual bool has_write_ref_barrier() = 0;
  virtual bool has_write_prim_barrier() = 0;

  // These functions indicate whether a particular access of the given
  // kinds requires a barrier.
  virtual bool read_ref_needs_barrier(oop* field) = 0;
  virtual bool read_prim_needs_barrier(HeapWord* field, size_t bytes) = 0;
  virtual bool write_ref_needs_barrier(oop* field, oop new_val) = 0;
  virtual bool write_prim_needs_barrier(HeapWord* field, size_t bytes, juint val1, juint val2) = 0;

  // The first four operations provide a direct implementation of the
  // barrier set.  An interpreter loop, for example, could call these
  // directly, as appropriate.

  // Invoke the barrier, if any, necessary when reading the given ref field.
  virtual void read_ref_field(oop* field) = 0;

  // Invoke the barrier, if any, necessary when reading the given primitive
  // "field" of "bytes" bytes in "obj".
  virtual void read_prim_field(HeapWord* field, size_t bytes) = 0;

  // Invoke the barrier, if any, necessary when writing "new_val" into the
  // ref field at "offset" in "obj".
  // (For efficiency reasons, this operation is specialized for certain
  // barrier types.  Semantically, it should be thought of as a call to the 
  // virtual "_work" function below, which must implement the barrier.)
  inline void write_ref_field(oop* field, oop new_val);
protected:
  virtual void write_ref_field_work(oop* field, oop new_val) = 0;
public:

  // Invoke the barrier, if any, necessary when writing the "bytes"-byte
  // value(s) "val1" (and "val2") into the primitive "field".
  virtual void write_prim_field(HeapWord* field, size_t bytes,
				juint val1, juint val2) = 0;

  // Operations on arrays, or general regions (e.g., for "clone") may be
  // optimized by some barriers.

  // The first six operations tell whether such an optimization exists for
  // the particular barrier.
  virtual bool has_read_ref_array_opt() = 0;
  virtual bool has_read_prim_array_opt() = 0;
  virtual bool has_write_ref_array_opt() = 0;
  virtual bool has_write_prim_array_opt() = 0;

  virtual bool has_read_region_opt() = 0;
  virtual bool has_write_region_opt() = 0;

  // These operations should assert false unless the correponding operation
  // above returns true.  Otherwise, they should perform an appropriate
  // barrier for an array whose elements are all in the given memory region.
  virtual void read_ref_array(MemRegion mr) = 0;
  virtual void read_prim_array(MemRegion mr) = 0;
  
  inline void write_ref_array(MemRegion mr);
protected:
  virtual void write_ref_array_work(MemRegion mr) = 0;
public:
  virtual void write_prim_array(MemRegion mr) = 0;

  virtual void read_region(MemRegion mr) = 0;

  // (For efficiency reasons, this operation is specialized for certain
  // barrier types.  Semantically, it should be thought of as a call to the 
  // virtual "_work" function below, which must implement the barrier.)
  inline void write_region(MemRegion mr);
protected:
  virtual void write_region_work(MemRegion mr) = 0;
public:

  // The remaining sets of operations are called by compilers or other code
  // generators to insert barriers into generated code.  There may be
  // several such code generators; the signatures of these
  // barrier-generating functions may differ from generator to generator.
  // There will be a set of four function signatures for each code
  // generator, which accomplish the generation of barriers of the four
  // kinds listed above.

#ifdef TBD
  // Generates code to invoke the barrier, if any, necessary when reading
  // the ref field at "offset" in "obj".
  virtual void gen_read_ref_field() = 0;

  // Generates code to invoke the barrier, if any, necessary when reading
  // the primitive field of "bytes" bytes at offset" in "obj".
  virtual void gen_read_prim_field() = 0;

  // Generates code to invoke the barrier, if any, necessary when writing
  // "new_val" into the ref field at "offset" in "obj".
  virtual void gen_write_ref_field() = 0;

  // Generates code to invoke the barrier, if any, necessary when writing
  // the "bytes"-byte value "new_val" into the primitive field at "offset"
  // in "obj".
  virtual void gen_write_prim_field() = 0;
#endif

  // Some barrier sets create tables whose elements correspond to parts of
  // the heap; the CardTableModRefBS is an example.  Such barrier sets will
  // normally reserve space for such tables, and commit parts of the table
  // "covering" parts of the heap that are committed.  The constructor is
  // passed the maximum number of independently committable subregions to
  // be covered, and the "resize_covoered_region" function allows the
  // sub-parts of the heap to inform the barrier set of changes of their
  // sizes.
  BarrierSet(int max_covered_regions) :
    _max_covered_regions(max_covered_regions) {}

  // Inform the BarrierSet that the the covered heap region that starts
  // with "base" has been changed to have the given size (possibly from 0,
  // for initialization.)
  virtual void resize_covered_region(MemRegion new_region) = 0;

  // If the barrier set imposes any alignment restrictions on boundaries
  // within the heap, this function tells whether they are met.
  virtual bool is_aligned(HeapWord* addr) = 0;

};
