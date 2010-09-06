#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)generation.cpp	1.229 04/01/27 21:46:27 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_generation.cpp.incl"

Generation::Generation(ReservedSpace rs, size_t initial_size, int level) :
  _level(level),
  _ref_processor(NULL) {
  if (!_virtual_space.initialize(rs, initial_size)) {
    vm_exit_during_initialization("Could not reserve enough space for "
		    "object heap");
  }
  _reserved = MemRegion((HeapWord*)_virtual_space.low_boundary(),
	  (HeapWord*)_virtual_space.high_boundary());

#ifdef ASSERT
  _promotion_failure_a_lot_count = PromotionFailureALotCount;
#endif
}

GenerationSpec* Generation::spec() {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  assert(0 <= level() && level() < gch->_n_gens, "Bad gen level");
  return gch->_gen_specs[level()];
}

size_t Generation::max_capacity() const {
  return reserved().byte_size();
}

void Generation::print_heap_change(size_t prev_used) const {
  if (PrintGCDetails && Verbose) {
    gclog_or_tty->print(" "  SIZE_FORMAT
                        "->" SIZE_FORMAT
                        "("  SIZE_FORMAT ")",
                        prev_used, used(), capacity());
  } else {
    gclog_or_tty->print(" "  SIZE_FORMAT "K"
                        "->" SIZE_FORMAT "K"
                        "("  SIZE_FORMAT "K)",
                        prev_used / K, used() / K, capacity() / K);
  }
}

// By default we get a single threaded default reference processor;
// generations needing multi-threaded refs discovery override this method.
void Generation::ref_processor_init() {
  assert(_ref_processor == NULL, "a reference processor already exists");
  assert(!_reserved.is_empty(), "empty generation?");
  _ref_processor =
    new ReferenceProcessor(_reserved,                  // span
                           refs_discovery_is_atomic(), // atomic_discovery
                           refs_discovery_is_mt());    // mt_discovery
  if (_ref_processor == NULL) {
    vm_exit_during_initialization("Could not allocate ReferenceProcessor object");
  }
}

void Generation::print() const { print_on(tty); }

void Generation::print_on(outputStream* st)  const {
  st->print(" %-20s", name());
  st->print(" total " SIZE_FORMAT "K, used " SIZE_FORMAT "K", 
             capacity()/K, used()/K);
  st->print_cr(" [" INTPTR_FORMAT ", " INTPTR_FORMAT ", " INTPTR_FORMAT ")",
              _virtual_space.low_boundary(),
              _virtual_space.high(),
              _virtual_space.high_boundary());
}

// Utility iterator classes

class GenerationIsInClosure : public SpaceClosure {
 public:
  const void* _p;
  Space* sp;
  virtual void do_space(Space* s) {
    if (sp == NULL) {
      if (s->contains(_p)) sp = s;
    }
  }
  GenerationIsInClosure(const void* p) : _p(p), sp(NULL) {}
};

bool Generation::is_in(const void* p) const {
  GenerationIsInClosure blk(p);
  ((Generation*)this)->space_iterate(&blk);
  return blk.sp != NULL;
}

Generation* Generation::next_gen() const {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  int next = level() + 1;
  if (next < gch->_n_gens) {
    return gch->_gens[next];
  } else {
    return NULL;
  }
}

size_t Generation::max_contiguous_available() const {
  // The largest number of contiguous free words in this or any higher generation.
  size_t max = 0;
  for (const Generation* gen = this; gen != NULL; gen = gen->next_gen()) {
    size_t avail = gen->contiguous_available();
    if (avail > max) {
      max = avail;
    }
  }
  return max;
}

bool Generation::promotion_attempt_is_safe(size_t promotion_in_bytes,
				           bool not_used) const {
  if (PrintGC && Verbose) {
    gclog_or_tty->print_cr("Generation::promotion_attempt_is_safe"
                " contiguous_available: " SIZE_FORMAT
                " promotion_in_bytes: " SIZE_FORMAT,
                max_contiguous_available(), promotion_in_bytes);
  }
  return max_contiguous_available() >= promotion_in_bytes;
}

// Ignores "ref" and calls allocate() without checking whether obj is a
// large noref object.  Generations which care about large noref objects must
// override promote().
oop Generation::promote(oop obj, size_t obj_size, oop* ref) {
  assert(obj_size == (size_t)obj->size(), "bad obj_size passed in");

#ifdef ASSERT
    // Stress testing for promotion failure handling.  Only do this
    // testing if assertions are defined so as not to slow down
    // optimized builds.
    if (PromotionFailureALot && _promotion_failure_a_lot_count-- == 0) {
      _promotion_failure_a_lot_count = PromotionFailureALotCount;
      return NULL;
    }
#endif

  HeapWord* result = allocate(obj_size, false, false);
  if (result != NULL) {
    Copy::aligned_disjoint_words((HeapWord*)obj, result, obj_size);
    return oop(result);
  } else {
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    return gch->handle_failed_promotion(this, obj, obj_size, ref);
  }
}

oop Generation::par_promote(int thread_num,
			    oop obj, markOop m, size_t word_sz) {
  // Could do a bad general impl here that gets a lock.  But no.
  ShouldNotCallThis();
  return NULL;
}

void Generation::par_promote_alloc_undo(int thread_num,
					HeapWord* obj, size_t word_sz) {
  // Could do a bad general impl here that gets a lock.  But no.
  guarantee(false, "No good general implementation.");
}

Space* Generation::space_containing(const void* p) const {
  GenerationIsInClosure blk(p);
  // Cast away const
  ((Generation*)this)->space_iterate(&blk);
  return blk.sp;
}

// Some of these are mediocre general implementations.  Should be
// overridden to get better performance.

class GenerationBlockStartClosure : public SpaceClosure {
 public:
  const void* _p;
  HeapWord* _start;
  virtual void do_space(Space* s) {
    if (_start == NULL && s->contains(_p)) {
      _start = s->block_start(_p);
    }
  }
  GenerationBlockStartClosure(const void* p) { _p = p; _start = NULL; }
};

HeapWord* Generation::block_start(const void* p) const {
  GenerationBlockStartClosure blk(p);
  // Cast away const
  ((Generation*)this)->space_iterate(&blk);
  return blk._start;
}

class GenerationBlockSizeClosure : public SpaceClosure {
 public:
  const HeapWord* _p;
  size_t size;
  virtual void do_space(Space* s) {
    if (size == 0 && s->contains(_p)) {
      size = s->block_size(_p);
    }
  }
  GenerationBlockSizeClosure(const HeapWord* p) { _p = p; size = 0; }
};

size_t Generation::block_size(const HeapWord* p) const {
  GenerationBlockSizeClosure blk(p);
  // Cast away const
  ((Generation*)this)->space_iterate(&blk);
  assert(blk.size > 0, "seems reasonable");
  return blk.size;
}

class GenerationBlockIsObjClosure : public SpaceClosure {
 public:
  const HeapWord* _p;
  bool is_obj;
  virtual void do_space(Space* s) {
    if (!is_obj && s->contains(_p)) {
      is_obj |= s->block_is_obj(_p);
    }
  }
  GenerationBlockIsObjClosure(const HeapWord* p) { _p = p; is_obj = false; }
};

bool Generation::block_is_obj(const HeapWord* p) const {
  GenerationBlockIsObjClosure blk(p);
  // Cast away const
  ((Generation*)this)->space_iterate(&blk);
  return blk.is_obj;
}

class GenerationOopIterateClosure : public SpaceClosure {
 public:
  OopClosure* cl;
  MemRegion mr;
  virtual void do_space(Space* s) {
    s->oop_iterate(mr, cl);
  }
  GenerationOopIterateClosure(OopClosure* _cl, MemRegion _mr) :
    cl(_cl), mr(_mr) {}
};

void Generation::oop_iterate(OopClosure* cl) {
  GenerationOopIterateClosure blk(cl, _reserved);
  space_iterate(&blk);
}

void Generation::oop_iterate(MemRegion mr, OopClosure* cl) {
  GenerationOopIterateClosure blk(cl, mr);
  space_iterate(&blk);
}

void Generation::younger_refs_in_space_iterate(Space* sp,
					       OopsInGenClosure* cl) {
  GenRemSet* rs = SharedHeap::heap()->rem_set();
  rs->younger_refs_in_space_iterate(sp, cl);
}

class GenerationObjIterateClosure : public SpaceClosure {
 private:
  ObjectClosure* _cl;
 public:
  virtual void do_space(Space* s) {
    s->object_iterate(_cl);
  }
  GenerationObjIterateClosure(ObjectClosure* cl) : _cl(cl) {}
};

void Generation::object_iterate(ObjectClosure* cl) {
  GenerationObjIterateClosure blk(cl);
  space_iterate(&blk);
}

void Generation::prepare_for_compaction(CompactPoint* cp) {
  // Generic implementation, can be specialized
  CompactibleSpace* space = first_compaction_space();
  while (space != NULL) {
    space->prepare_for_compaction(cp);
    space = space->next_compaction_space();
  }
}

class AdjustPointersClosure: public SpaceClosure {
 public:
  void do_space(Space* sp) { 
    sp->adjust_pointers(); 
  }
};

void Generation::adjust_pointers() {
  // Note that this is done over all spaces, not just the compactible
  // ones.
  AdjustPointersClosure blk;
  space_iterate(&blk, true);
}

void Generation::compact() {
  CompactibleSpace* sp = first_compaction_space();
  while (sp != NULL) {
    sp->compact();
    sp = sp->next_compaction_space();
  }
}

CardGeneration::CardGeneration(ReservedSpace rs, size_t initial_byte_size,
			       int level,
			       GenRemSet* remset) :
  Generation(rs, initial_byte_size, level), _rs(remset)
{
  HeapWord* start = (HeapWord*)rs.base();
  size_t reserved_byte_size = rs.size();
  assert((uintptr_t(start) & 3) == 0, "bad alignment");
  assert((reserved_byte_size & 3) == 0, "bad alignment");
  MemRegion reserved_mr(start, heap_word_size(reserved_byte_size));
  _bts = new BlockOffsetSharedArray(reserved_mr,
                                    heap_word_size(initial_byte_size));
  MemRegion committed_mr(start, heap_word_size(initial_byte_size));
  _rs->resize_covered_region(committed_mr);
  if (_bts == NULL)
    vm_exit_during_initialization("Could not allocate a BlockOffsetArray");

  // Verify that the start and end of this generation is the start of a card.
  // If this wasn't true, a single card could span more than on generation,
  // which would cause problems when we commit/uncommit memory, and when we
  // clear and dirty cards.
  guarantee(_rs->is_aligned(reserved_mr.start()), "generation must be card aligned");
  if (reserved_mr.end() != Universe::heap()->reserved_region().end()) {
    // Don't check at the very end of the heap as we'll assert that we're probing off
    // the end if we try.
    guarantee(_rs->is_aligned(reserved_mr.end()), "generation must be card aligned");
  }
}


// No young generation references, clear this generation's cards.
void CardGeneration::clear_remembered_set() {
  _rs->clear(reserved());
}


// Objects in this generation may have moved, invalidate this
// generation's cards.
void CardGeneration::invalidate_remembered_set() {
  _rs->invalidate(used_region());
}


// Currently nothing to do.
void CardGeneration::prepare_for_verify() {}


void OneContigSpaceCardGeneration::collect(bool   full,
                                           bool   clear_all_soft_refs,
					   size_t size,
                                           bool   is_large_noref,
                                           bool   is_tlab) {
  SpecializationStats::clear();
  // Temporarily expand the span of our ref processor, so
  // refs discovery is over the entire heap, not just this generation 
  ReferenceProcessorSpanMutator
    x(ref_processor(), GenCollectedHeap::heap()->reserved_region());
  GenMarkSweep::invoke_at_safepoint(_level, ref_processor(), clear_all_soft_refs);
  SpecializationStats::print();
}

HeapWord*
OneContigSpaceCardGeneration::expand_and_allocate(size_t word_size,
						  bool is_large_noref,
                                                  bool is_tlab,
						  bool parallel) {
  assert(!is_tlab, "OneContigSpaceCardGeneration does not support TLAB allocation");
  if (parallel) {
    MutexLocker x(ParGCRareEvent_lock);
    expand(word_size*HeapWordSize, _min_heap_delta_bytes);
    return _the_space->par_allocate(word_size);
  } else {
    expand(word_size*HeapWordSize, _min_heap_delta_bytes);
    return _the_space->allocate(word_size);
  }
}

void OneContigSpaceCardGeneration::expand(size_t bytes, size_t expand_bytes) {
  GCMutexLocker x(ExpandHeap_lock);
  size_t aligned_bytes  = ReservedSpace::page_align_size_up(bytes);
  size_t aligned_expand_bytes = ReservedSpace::page_align_size_up(expand_bytes);
  bool success = false;
  if (aligned_expand_bytes > aligned_bytes) {
    success = grow_by(aligned_expand_bytes);
  }
  if (!success) {
    success = grow_by(aligned_bytes);
  }
  if (!success) {
    grow_to_reserved();
  }
  if (GC_locker::is_active()) {
    // Tell the GC locker that we had to expand the heap
    GC_locker::heap_expanded();
    if (PrintGC && Verbose) {
      gclog_or_tty->print_cr("Garbage collection disabled, expanded heap instead");
    }
  }
}


void OneContigSpaceCardGeneration::shrink(size_t bytes) {
  assert_locked_or_safepoint(ExpandHeap_lock);
  size_t size = ReservedSpace::page_align_size_down(bytes);
  if (size > 0) {
    shrink_by(size);
  }
}


size_t OneContigSpaceCardGeneration::capacity() const {
  return _the_space->capacity();
}


size_t OneContigSpaceCardGeneration::used() const {
  return _the_space->used();
}


size_t OneContigSpaceCardGeneration::free() const {
  return _the_space->free();
}

MemRegion OneContigSpaceCardGeneration::used_region() const {
  return the_space()->used_region();
}

size_t OneContigSpaceCardGeneration::unsafe_max_alloc_nogc() const {
  return _the_space->free();
}

size_t OneContigSpaceCardGeneration::contiguous_available() const {
  return _the_space->free() + _virtual_space.uncommitted_size();
}

bool OneContigSpaceCardGeneration::grow_by(size_t bytes) {
  assert_locked_or_safepoint(ExpandHeap_lock);
  bool result = _virtual_space.expand_by(bytes);
  if (result) {
    size_t new_word_size =
       heap_word_size(_virtual_space.committed_size());
    MemRegion mr(_the_space->bottom(), new_word_size);
    // Expand card table
    Universe::heap()->barrier_set()->resize_covered_region(mr);
    // Expand shared block offset array
    _bts->resize(new_word_size);

    // Fix for bug #4668531
    MemRegion mangle_region(_the_space->end(), (HeapWord*)_virtual_space.high());
    _the_space->mangle_region(mangle_region);

    // Expand space -- also expands space's BOT
    // (which uses (part of) shared array above)
    _the_space->set_end((HeapWord*)_virtual_space.high());

    // update the space and generation capacity counters
    update_counters();

    if (Verbose && PrintGC) {
      size_t new_mem_size = _virtual_space.committed_size();
      size_t old_mem_size = new_mem_size - bytes;
      gclog_or_tty->print_cr("Expanding %s from " SIZE_FORMAT "K by "
                      SIZE_FORMAT "K to " SIZE_FORMAT "K", 
                      name(), old_mem_size/K, bytes/K, new_mem_size/K);
    }
  }
  return result;
}


void OneContigSpaceCardGeneration::grow_to_reserved() {
  assert_locked_or_safepoint(ExpandHeap_lock);
  size_t remaining_bytes = _virtual_space.uncommitted_size();
  if (remaining_bytes > 0) {
    bool success = grow_by(remaining_bytes);
    assert(success, "grow to reserved failed");
  }
}


void OneContigSpaceCardGeneration::shrink_by(size_t bytes) {
  assert_locked_or_safepoint(ExpandHeap_lock);
  // Shrink committed space
  _virtual_space.shrink_by(bytes);
  // Shrink space; this also shrinks the space's BOT
  _the_space->set_end((HeapWord*) _virtual_space.high());
  size_t new_word_size = heap_word_size(_the_space->capacity());
  // Shrink the shared block offset array
  _bts->resize(new_word_size);
  MemRegion mr(_the_space->bottom(), new_word_size);
  // Shrink the card table
  Universe::heap()->barrier_set()->resize_covered_region(mr);

  if (Verbose && PrintGC) {
    size_t new_mem_size = _virtual_space.committed_size();
    size_t old_mem_size = new_mem_size + bytes;
    gclog_or_tty->print_cr("Shrinking %s from " SIZE_FORMAT "K to " SIZE_FORMAT "K",
	     	  name(), old_mem_size/K, new_mem_size/K);
  }
}

// Currently nothing to do.
void OneContigSpaceCardGeneration::prepare_for_verify() {}


void OneContigSpaceCardGeneration::object_iterate(ObjectClosure* blk) {
  _the_space->object_iterate(blk);
}

void OneContigSpaceCardGeneration::space_iterate(SpaceClosure* blk,
						 bool usedOnly) {
  blk->do_space(_the_space);
}

void OneContigSpaceCardGeneration::object_iterate_since_last_GC(ObjectClosure* blk) {
  // Deal with delayed initialization of _the_space,
  // and lack of initialization of _last_gc.
  if (_last_gc.space() == NULL) {
    assert(the_space() != NULL, "shouldn't be NULL");
    _last_gc = the_space()->bottom_mark();
  }
  the_space()->object_iterate_from(_last_gc, blk);
}

void OneContigSpaceCardGeneration::younger_refs_iterate(OopsInGenClosure* blk) {
  blk->set_generation(this);
  younger_refs_in_space_iterate(_the_space, blk);
  blk->reset_generation();
}

void OneContigSpaceCardGeneration::save_marks() { 
  _the_space->set_saved_mark();
}


void OneContigSpaceCardGeneration::reset_saved_marks() { 
  _the_space->reset_saved_mark();
}


bool OneContigSpaceCardGeneration::no_allocs_since_save_marks() {
  return _the_space->saved_mark_at_top();
}

#define OneContig_SINCE_SAVE_MARKS_ITERATE_DEFN(OopClosureType, nv_suffix)	\
										\
void OneContigSpaceCardGeneration::						\
oop_since_save_marks_iterate##nv_suffix(OopClosureType* blk) {			\
  blk->set_generation(this);							\
  _the_space->oop_since_save_marks_iterate##nv_suffix(blk);			\
  blk->reset_generation();							\
  save_marks();									\
}

ALL_SINCE_SAVE_MARKS_CLOSURES(OneContig_SINCE_SAVE_MARKS_ITERATE_DEFN)

#undef OneContig_SINCE_SAVE_MARKS_ITERATE_DEFN


void OneContigSpaceCardGeneration::gc_epilogue(bool full) {
  _last_gc = WaterMark(the_space(), the_space()->top());

  // update the generation and space performance counters
  update_counters();
}

#ifndef PRODUCT

void OneContigSpaceCardGeneration::verify(bool allow_dirty) {
  the_space()->verify(allow_dirty);
}

#endif

void OneContigSpaceCardGeneration::print_on(outputStream* st)  const {
  Generation::print_on(st);
  st->print("   the");
  the_space()->print_on(st);
}
