#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)generation.hpp	1.169 04/04/19 08:35:12 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// A Generation models a heap area for similarly-aged objects.
// It will contain one ore more spaces holding the actual objects.
//
// The Generation class hierarchy:
//
// Generation                      - abstract base class
// - DefNewGeneration              - allocation area (copy collected)
//   - ParNewGeneration            - a DefNewGeneration that is collected by
//                                   several threads
// - CardGeneration                 - abstract class adding offset array behavior
//   - OneContigSpaceCardGeneration - abstract class holding a single
//                                    contiguous space with card marking
//     - TenuredGeneration         - tenured (old object) space (markSweepCompact)
//     - CompactingPermGenGen      - reflective object area (klasses, methods, symbols, ...)
//   - TrainGeneration             - tenured object area when using train gc (train copy collected)
//   - ConcurrentMarkSweepGeneration - Mostly Concurrent Mark Sweep Generation
//                                       (Detlefs-Printezis refinement of
//                                       Boehm-Demers-Schenker)
//
// The system configurations currently allowed are:
//
//   DefNewGeneration + TenuredGeneration + PermGeneration
//   DefNewGeneration + TrainGeneration   + PermGeneration
//   DefNewGeneration + ConcurrentMarkSweepGeneration + ConcurrentMarkSweepPermGen
//
//   ParNewGeneration + TenuredGeneration + PermGeneration
//   ParNewGeneration + ConcurrentMarkSweepGeneration + ConcurrentMarkSweepPermGen
//

class GenerationSpec;
class CompactibleSpace;
class ContiguousSpace;
class CompactPoint;
class OopsInGenClosure;
class OopClosure;
class ScanClosure;
class FastScanClosure;
class TrainScanClosure;
class GenCollectedHeap;
class GenRemSet;

// A "ScratchBlock" represents a block of memory in one generation usable by
// another.  It represents "num_words" free words, starting at and including
// the address of "this".
struct ScratchBlock {
  ScratchBlock* next;
  size_t num_words;
  HeapWord scratch_space[1];  // Actually, of size "num_words-2" (assuming
			      // first two fields are word-sized.)
};


class Generation: public CHeapObj {
  friend class VMStructs;
 private:
  jlong _time_of_last_gc; // time when last gc on this generation happened (ms)
  MemRegion _prev_used_region; // for collectors that want to "remember" a value for
                               // used region at some specific point during collection.

#ifdef ASSERT
  int _promotion_failure_a_lot_count;
#endif

 protected:
  // Minimum and maximum addresses for memory reserved (not necessarily
  // committed) for generation.
  // Used by card marking code. Must not overlap with address ranges of
  // other generations.
  MemRegion _reserved;

  // Memory area reserved for generation 
  VirtualSpace _virtual_space;

  // Level in the generation hierarchy.
  int _level;

  // ("Weak") Reference processing support
  ReferenceProcessor* _ref_processor;

  // Performance Counters
  CollectorCounters* _gc_counters;

  // Returns the next generation in the configuration, or else NULL if this
  // is the highest generation.
  Generation* next_gen() const;

  // Initialize the generation.
  Generation(ReservedSpace rs, size_t initial_byte_size, int level);

  // Apply "cl->do_oop" to (the address of) (exactly) all the ref fields in
  // "sp" that point into younger generations.
  // The iteration is only over objects allocated at the start of the
  // iterations; objects allocated as a result of applying the closure are
  // not included.
  void younger_refs_in_space_iterate(Space* sp, OopsInGenClosure* cl);

 public:
  // The set of possible generation kinds.
  enum Name {
    DefNew,
    ParNew,
    MarkSweepCompact,
    TrainGen,
    ConcurrentMarkSweep,
    Other
  };

  enum SomePublicConstants {
    // Generations are GenGrain-aligned and have size that are multiples of
    // GenGrain.
    LogOfGenGrain = 16,
    GenGrain = 1 << LogOfGenGrain
  };
  
  // allocate and initialize ("weak") refs processing support
  virtual void ref_processor_init();
  void set_ref_processor(ReferenceProcessor* rp) {
    assert(_ref_processor == NULL, "clobbering existing _ref_processor");
    _ref_processor = rp;
  }

  virtual Generation::Name kind() { return Generation::Other; }
  GenerationSpec* spec();

  // This properly belongs in the collector, but for now this
  // will do.
  virtual bool refs_discovery_is_atomic() const { return true;  }
  virtual bool refs_discovery_is_mt()     const { return false; }

  // Space enquiries (results in bytes)
  virtual size_t capacity() const = 0;  // The maximum number of object bytes the
                                        // generation can currently hold.
  virtual size_t used() const = 0;      // The number of used bytes in the gen.
  virtual size_t free() const = 0;      // The number of free bytes in the gen.

  // Support for java.lang.Runtime.maxMemory(); see CollectedHeap.
  virtual size_t max_capacity() const;

  // If this is a young generation, the maximum number of bytes that can be
  // allocated in this generation before a GC is triggered.
  virtual size_t capacity_before_gc() const { return 0; }

  // The largest number of contiguous free bytes in the generation,
  // including expansion  (Assumes called at a safepoint.)
  virtual size_t contiguous_available() const = 0;
  // The largest number of contiguous free bytes in this or any higher generation.
  virtual size_t max_contiguous_available() const;

  // Returns true if promotions of the specified amount can
  // be attempted safely (without a vm failure). 
  // Promotion of the full amount is not guaranteed but
  // can be attempted.  
  //   younger_handles_promotion_failure 
  // is true if the younger generation handles a promotion
  // failure.
  virtual bool promotion_attempt_is_safe(size_t promotion_in_bytes,
    bool younger_handles_promotion_failure) const;

  // Return an estimate of the maximum allocation that could be performed
  // in the generation without triggering any collection or expansion
  // activity.  It is "unsafe" because no locks are taken; the result
  // should be treated as an approximation, not a guarantee, for use in
  // heuristic resizing decisions.
  virtual size_t unsafe_max_alloc_nogc() const = 0;

  MemRegion reserved() const { return _reserved; }

  // Returns a region guaranteed to contain all the objects in the
  // generation.
  virtual MemRegion used_region() const { return _reserved; }

  MemRegion prev_used_region() const { return _prev_used_region; }
  virtual void  save_used_region()   { _prev_used_region = used_region(); }

  /* Returns "TRUE" iff "p" points into an allocated object in the generation. */
  virtual bool is_in(const void* p) const;

  /* Returns "TRUE" iff "p" points into the reserved area of the generation. */
  bool is_in_reserved(const void* p) const {
    return _reserved.contains(p);
  }
 
  // If some space in the generation contains the given "addr", return a
  // pointer to that space, else return "NULL".
  virtual Space* space_containing(const void* addr) const;

  // Iteration - do not use for time critical operations
  virtual void space_iterate(SpaceClosure* blk, bool usedOnly = false) = 0;

  // Returns the first space, if any, in the generation that can participate
  // in compaction, or else "NULL".
  virtual CompactibleSpace* first_compaction_space() const = 0;

  // Returns "true" iff this generation should be used to allocate an
  // object of the given size (and "noref"-ness).  Young generations might
  // wish to exclude very large objects, for example, since, if allocated
  // often, they would greatly increase the frequency of young-gen
  // collection.
  virtual bool should_allocate(size_t word_size, bool is_large_noref, bool is_tlab) {
    bool result = false;
    if (!is_tlab || supports_tlab_allocation()) {
      result = word_size > 0;
    }
    return result;
  }

  // Allocate and returns a block of the requested size, or returns "NULL".
  // If "is_large_noref" is true, the request is relatively large and will
  // contain no references; this may be a useful hint to some generations.
  // Assumes the caller has done any necessary locking.
  virtual HeapWord* allocate(size_t word_size, bool is_large_noref, bool is_tlab) = 0;

  // Like "allocate", but performs any necessary locking internally.
  virtual HeapWord* par_allocate(size_t word_size,
				 bool is_large_noref,
				 bool is_tlab) = 0;

  // A 'younger' gen has reached an allocation limit, and uses this to notify
  // the next older gen.  The return value is a new limit, or NULL if none.  The
  // caller must do the necessary locking.
  virtual HeapWord* allocation_limit_reached(Space* space, HeapWord* top,
					     size_t word_size) {
    return NULL;
  }

  // Some generation may offer a region for shared, contiguous allocation,
  // via inlined code (by exporting the address of the top and end fields
  // defining the extent of the contiguous allocation region.)

  // This function returns "true" iff the heap supports this kind of
  // allocation.  (More precisely, this means the style of allocation that
  // increments *top_addr()" with a CAS.) (Default is "no".)
  // A generation that supports this allocation style must use lock-free
  // allocation for *all* allocation, since there are times when lock free
  // allocation will be concurrent with plain "allocate" calls.
  virtual bool supports_inline_contig_alloc() const { return false; }

  // These functions return the addresses of the fields that define the
  // boundaries of the contiguous allocation area.  (These fields should be
  // physicall near to one another.)
  virtual HeapWord** top_addr() const { return NULL; }
  virtual HeapWord** end_addr() const { return NULL; }

  // Thread-local allocation buffers
  virtual bool supports_tlab_allocation() const { return false; }
  virtual size_t tlab_capacity() const {
    guarantee(false, "Generation doesn't support thread local allocation buffers");
    return 0;
  }
  virtual size_t unsafe_max_tlab_alloc() const {
    guarantee(false, "Generation doesn't support thread local allocation buffers");
    return 0;
  }
  
  // "obj" is the address of an object in a younger generation.  Allocate space
  // for "obj" in the current (or some higher) generation, and copy "obj" into
  // the newly allocated space, if possible, returning the result (or NULL if
  // the allocation failed).
  // 
  // The "obj_size" argument is just obj->size(), passed along so the caller can
  // avoid repeating the virtual call to retrieve it.
  // 
  // The "ref" argument, if non-NULL, is the address of some reference to "obj"
  // (that is "*ref == obj"); some generations may use this information to, for
  // example, influence placement decisions.
  // 
  // The default implementation ignores "ref" and calls allocate() without
  // checking whether obj is a large noref object.  Generations which care about
  // large noref objects must override promote().
  virtual oop promote(oop obj, size_t obj_size, oop* ref);

  // Thread "thread_num" (0 <= i < ParalleGCThreads) wants to promote
  // object "obj", whose original mark word was "m", and whose size is
  // "word_sz".  If possible, allocate space for "obj", copy obj into it
  // (taking care to copy "m" into the mark word when done, since the mark
  // word of "obj" may have been overwritten with a forwarding pointer, and
  // also taking care to copy the klass pointer *last*.  Returns the new
  // object if successful, or else NULL.
  virtual oop par_promote(int thread_num,
			  oop obj, markOop m, size_t word_sz);

  // Undo, if possible, the most recent par_promote_alloc allocation by
  // "thread_num" ("obj", of "word_sz").
  virtual void par_promote_alloc_undo(int thread_num,
				      HeapWord* obj, size_t word_sz);

  // Informs the current generation that all par_promote_alloc's in the
  // collection have been completed; any supporting data structures can be
  // reset.  Default is to do nothing.
  virtual void par_promote_alloc_done(int thread_num) {}

  // Informs the current generation that all oop_since_save_marks_iterates
  // performed by "thread_num" in the current collection, if any, have been
  // completed; any supporting data structures can be reset.  Default is to 
  // do nothing.
  virtual void par_oop_since_save_marks_iterate_done(int thread_num) {}

  // This generation will collect all younger generations
  // during a full collection.
  virtual bool full_collects_younger_generations() const { return false; }

  // Returns "true" iff collect() should subsequently be called on this
  // this generation. See comment below. 
  // This is a generic implementation which can be overridden.
  //
  // Note: in the current (1.4) implementation, when genCollectedHeap's
  // incremental_collection_will_fail flag is set, all allocations are
  // slow path (the only fast-path place to allocate is DefNew, which
  // will be full if the flag is set).
  // Thus, older generations which collect younger generations should
  // test this flag and collect if it is set.
  virtual bool should_collect(bool   full,
                              size_t word_size,
                              bool   is_large_noref,
                              bool   is_tlab) {
    return (full || should_allocate(word_size, is_large_noref, is_tlab));
  }

  // Perform a garbage collection.
  // If full is true attempt a full garbage collection of this generation.
  // Otherwise, attempting to (at least) free enough space to support an 
  // allocation of the given "word_size" and "is_large_noref" value.
  // The "is_large_noref" argument indicates that the allocated block is relatively 
  // large and will contain no references; some generations may make effective 
  // use of this hint.
  virtual void collect(bool   full,
                       bool   clear_all_soft_refs,
                       size_t word_size,
                       bool   is_large_noref, 
		       bool   is_tlab) = 0;

  // Perform a heap collection, attempting to create (at least) enough
  // space to support an allocation of the given "word_size".  If
  // successful, perform the allocation and return the resulting
  // "oop" (initializing the allocated block).  The "is_large_noref" argument
  // indicates that the allocated block is relatively large, and will
  // contain no references; some generations may make effective use of this
  // hint.  If the allocation is still unsuccessful, return "NULL".
  virtual HeapWord* expand_and_allocate(size_t word_size,
					bool is_large_noref, bool is_tlab,
					bool parallel = false) = 0;

  // Some generations may require some cleanup or preparation actions before
  // allowing a collection.  The default is to do nothing.
  virtual void gc_prologue(bool full) {};

  // Some generations may require some cleanup actions after a collection.
  // The default is to do nothing.
  virtual void gc_epilogue(bool full) {};

  // Some generations may need to be "fixed-up" after some allocation
  // activity to make them parseable again. The default is to do nothing.
  virtual void ensure_parseability() {};

  // Time (in ms) when we were last collected or now if a collection is
  // in progress.
  virtual jlong time_of_last_gc(jlong now) {
    // XXX See note in genCollectedHeap::millis_since_last_gc()
    NOT_PRODUCT(
      if (now < _time_of_last_gc) {
        warning("time warp: %d to %d", _time_of_last_gc, now);
      }
    )
    return _time_of_last_gc;
  }

  virtual void update_time_of_last_gc(jlong now)  {
    _time_of_last_gc = now;
  }

  // Generations may keep statistics about collection.  This
  // method updates those statistics.  current_level is
  // the level of the collection that has most recently
  // occurred.  This allows the generation to decide what
  // statistics are valid to collect.  For example, the
  // generation can decide to gather the amount of promoted data 
  // if the collection of the younger generations has completed.
  virtual void update_gc_stats(int current_level, bool full) {}

  // Mark sweep support phase2
  virtual void prepare_for_compaction(CompactPoint* cp);
  // Mark sweep support phase3
  virtual void pre_adjust_pointers() {ShouldNotReachHere();}
  virtual void adjust_pointers();
  // Mark sweep support phase4
  virtual void compact();
  virtual void post_compact() {ShouldNotReachHere();}

  // Some generations may require some cleanup actions before allowing
  // a verification.
  virtual void prepare_for_verify() {};

  // Accessing "marks".

  // This function gives a generation a chance to note a point between
  // collections.  For example, a contiguous generation might note the
  // beginning allocation point post-collection, which might allow some later
  // operations to be optimized.
  virtual void save_marks() {}

  // This function allows generations to initialize any "saved marks".  That
  // is, should only be called when the generation is empty.
  virtual void reset_saved_marks() {}

  // This function is "true" iff any no allocations have occurred in the
  // generation since the last call to "save_marks".
  virtual bool no_allocs_since_save_marks() = 0;

  // Apply "cl->apply" to (the addresses of) all reference fields in objects
  // allocated in the current generation since the last call to "save_marks".
  // If more objects are allocated in this generation as a result of applying
  // the closure, iterates over reference fields in those objects as well.
  // Calls "save_marks" at the end of the iteration.
  // General signature...
  virtual void oop_since_save_marks_iterate_v(OopsInGenClosure* cl) = 0;
  // ...and specializations for de-virtualization.  (The general
  // implemention of the _nv versions call the virtual version.
  // Note that the _nv suffix is not really semantically necessary,
  // but it avoids some not-so-useful warnings on Solaris.)
#define Generation_SINCE_SAVE_MARKS_DECL(OopClosureType, nv_suffix)		\
  virtual void oop_since_save_marks_iterate##nv_suffix(OopClosureType* cl) {	\
    oop_since_save_marks_iterate_v((OopsInGenClosure*)cl);			\
  }
  SPECIALIZED_SINCE_SAVE_MARKS_CLOSURES(Generation_SINCE_SAVE_MARKS_DECL)

#undef Generation_SINCE_SAVE_MARKS_DECL

  // The "requestor" generation is performing some garbage collection 
  // action for which it would be useful to have scratch space.  If
  // the target is not the requestor, no gc actions will be required
  // of the target.  The requestor promises to allocate no more than
  // "max_alloc_words" in the target generation (via promotion say,
  // if the requestor is a young generation and the target is older).
  // If the target generation can provide any scratch space, it adds
  // it to "list", leaving "list" pointing to the head of the
  // augmented list.  The default is to offer no space.
  virtual void contribute_scratch(ScratchBlock*& list, Generation* requestor,
				  size_t max_alloc_words) {}

  // When an older generation has been collected, and perhaps resized,
  // this method will be invoked on all younger generations (from older to
  // younger), allowing them to resize themselves as appropriate.
  virtual void compute_new_size() = 0;

  // Oop p has been modified since the last save_marks; allow each 
  // generation a chance to examine the oop and take special action. 
  // Used for oops which cannot be scanned with an oopsInGenClosure 
  // (e.g. weak references).
  // By default, nothing is done.
  virtual void examine_modified_oop(oop* p) { return; }

  // Printing
  virtual const char* name() const = 0;
  virtual const char* short_name() const = 0;

  int level() const { return _level; }

  // Attributes

  // True iff the given generation may only be the youngest generation.
  virtual bool must_be_youngest() const = 0;
  // True iff the given generation may only be the oldest generation.
  virtual bool must_be_oldest() const = 0;

  // Reference Processing accessor
  ReferenceProcessor* const ref_processor() { return _ref_processor; }

  // Iteration.

  // Iterate over all the ref-containing fields of all objects in the
  // generation, calling "cl.do_oop" on each.
  virtual void oop_iterate(OopClosure* cl);

  // Same as above, restricted to the intersection of a memory region and
  // the generation.
  virtual void oop_iterate(MemRegion mr, OopClosure* cl);

  // Iterate over all objects in the generation, calling "cl.do_object" on
  // each.
  virtual void object_iterate(ObjectClosure* cl);

  // Iterate over all objects allocated in the generation since the last
  // collection, calling "cl.do_object" on each.  The generation must have
  // been initialized properly to support this function, or else this call
  // will fail.
  virtual void object_iterate_since_last_GC(ObjectClosure* cl) = 0;

  // Apply "cl->do_oop" to (the address of) all and only all the ref fields
  // in the current generation that contain pointers to objects in younger
  // generations. Objects allocated since the last "save_marks" call are
  // excluded.
  virtual void younger_refs_iterate(OopsInGenClosure* cl) = 0;

  // Inform a generation that it longer contains references to objects
  // in any younger generation.    [e.g. Because younger gens are empty,
  // clear the card table.]
  virtual void clear_remembered_set() { }

  // Inform a generation that some of its objects have moved.  [e.g. The
  // generation's spaces were compacted, invalidating the card table.]
  virtual void invalidate_remembered_set() { }

  // Block abstraction.

  // Returns the address of the start of the "block" that contains the
  // address "addr".  We say "blocks" instead of "object" since some heaps
  // may not pack objects densely; a chunk may either be an object or a
  // non-object. 
  virtual HeapWord* block_start(const void* addr) const;

  // Requires "addr" to be the start of a chunk, and returns its size.
  // "addr + size" is required to be the start of a new chunk, or the end
  // of the active area of the heap.
  virtual size_t block_size(const HeapWord* addr) const ;

  // Requires "addr" to be the start of a block, and returns "TRUE" iff
  // the block is an object.
  virtual bool block_is_obj(const HeapWord* addr) const;


  // PrintGC, PrintGCDetails support
  void print_heap_change(size_t prev_used) const;

  // PrintHeapAtGC support
  virtual void print() const;
  virtual void print_on(outputStream* st) const;


#ifndef PRODUCT
  virtual void verify(bool allow_dirty) = 0;
#endif

  struct StatRecord {
    int invocations;
    elapsedTimer accumulated_time;
    StatRecord() :
      invocations(0),
      accumulated_time(elapsedTimer()) {}
  };
private:
  StatRecord _stat_record;
public:
  StatRecord* stat_record() { return &_stat_record; }

  // Performance Counter support
  virtual void update_counters() = 0;
  virtual CollectorCounters* counters() { return _gc_counters; }

  // If the current generation contains "addr", return the 0-based offset
  // of the "arena" within the generation that contains "addr".  Otherwise,
  // return the negative of the number of arenas in the generation.
  virtual int addr_to_arena_id(void* addr) { return -1; }
};

// Class CardGeneration is a generation that is covered by a card table,
// and uses a card-size block-offset array to implement block_start.

// class BlockOffsetArray;
// class BlockOffsetArrayContigSpace;
class BlockOffsetSharedArray;

class CardGeneration: public Generation {
  friend class VMStructs;
 protected:
  // This is shared with other generations.
  GenRemSet* _rs;
  // This is local to this generation.
  BlockOffsetSharedArray* _bts;

  CardGeneration(ReservedSpace rs, size_t initial_byte_size, int level,
		 GenRemSet* remset);
  
 public:

  virtual void clear_remembered_set();

  virtual void invalidate_remembered_set();

  virtual void prepare_for_verify();
};

// OneContigSpaceCardGeneration models a heap of old objects contained in a single
// contiguous space.
//
// Garbage collection is performed using mark-compact.

class OneContigSpaceCardGeneration: public CardGeneration {
  friend class VMStructs;
  // Abstractly, this is a subtype that gets access to protected fields.
  friend class CompactingPermGen;  
  friend class VM_PopulateDumpSharedSpace;

 protected:
  size_t     _min_heap_delta_bytes;   // Minimum amount to expand.
  ContiguousSpace*  _the_space;       // actual space holding objects
  WaterMark  _last_gc;                // watermark between objects allocated before
			              // and after last GC.

  // Grow generation with specified size (returns false if unable to grow)
  bool grow_by(size_t bytes);
  // Grow generation to reserved size.
  void grow_to_reserved();
  // Shrink generation with specified size (returns false if unable to shrink)
  void shrink_by(size_t bytes);

  // Allocation failure
  void expand(size_t bytes, size_t expand_bytes);
  void shrink(size_t bytes);

  // Accessing spaces
  ContiguousSpace* the_space() const { return _the_space; }

 public:
  OneContigSpaceCardGeneration(ReservedSpace rs, size_t initial_byte_size,
			       size_t min_heap_delta_bytes,
			       int level, GenRemSet* remset,
			       ContiguousSpace* space) :
    CardGeneration(rs, initial_byte_size, level, remset),
    _the_space(space), _min_heap_delta_bytes(min_heap_delta_bytes)
  {}

  inline bool is_in(const void* p) const;

  // Space enquiries
  size_t capacity() const;
  size_t used() const;
  size_t free() const;

  MemRegion used_region() const;

  size_t unsafe_max_alloc_nogc() const;
  size_t contiguous_available() const;

  // Iteration
  void object_iterate(ObjectClosure* blk);
  void space_iterate(SpaceClosure* blk, bool usedOnly = false);
  void object_iterate_since_last_GC(ObjectClosure* cl);

  void younger_refs_iterate(OopsInGenClosure* blk);

  inline CompactibleSpace* first_compaction_space() const;

  virtual inline HeapWord* allocate(size_t word_size, bool is_large_noref, bool is_tlab);
  virtual inline HeapWord* par_allocate(size_t word_size, bool is_large_noref, bool is_tlab);

  // Accessing marks
  inline WaterMark top_mark();
  inline WaterMark bottom_mark();

#define OneContig_SINCE_SAVE_MARKS_DECL(OopClosureType, nv_suffix)	\
  void oop_since_save_marks_iterate##nv_suffix(OopClosureType* cl);
  OneContig_SINCE_SAVE_MARKS_DECL(OopsInGenClosure,_v)
  SPECIALIZED_SINCE_SAVE_MARKS_CLOSURES(OneContig_SINCE_SAVE_MARKS_DECL)

  void save_marks();
  void reset_saved_marks();
  bool no_allocs_since_save_marks();

  inline size_t block_size(const HeapWord* addr) const;

  inline bool block_is_obj(const HeapWord* addr) const;
  
  virtual void collect(bool full,
                       bool clear_all_soft_refs,
                       size_t size, 
		       bool is_large_noref,
                       bool is_tlab);
  HeapWord* expand_and_allocate(size_t size,
				bool is_large_noref, bool is_tlab,
				bool parallel = false);

  virtual void prepare_for_verify();

  virtual void gc_epilogue(bool full);

  virtual void verify(bool allow_dirty) PRODUCT_RETURN;
  virtual void print_on(outputStream* st) const;
};
