#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)psMarkSweep.cpp	1.64 04/03/28 16:19:30 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_psMarkSweep.cpp.incl"

elapsedTimer        PSMarkSweep::_accumulated_time;
unsigned int        PSMarkSweep::_total_invocations = 0;
jlong               PSMarkSweep::_time_of_last_gc   = 0;
CollectorCounters*  PSMarkSweep::_counters = NULL;

void PSMarkSweep::initialize() {
  MemRegion mr = Universe::heap()->reserved_region();
  _ref_processor = new ReferenceProcessor(mr,
                                          true,    // atomic_discovery
                                          false);  // mt_discovery
  _counters = new CollectorCounters("PSMarkSweep", 1);
}

// This method contains all heap specific policy for invoking mark sweep.
// PSMarkSweep::invoke_no_policy() will only attempt to mark-sweep-compact
// the heap. It will do nothing further. If we need to bail out for policy
// reasons, scavenge before full gc, or any other specialized behavior, it
// needs to be added here.
//
// Note that this method should only be called from the vm_thread while
// at a safepoint!
void PSMarkSweep::invoke(bool* notify_ref_lock, 
			 bool maximum_heap_compaction) {
  assert(SafepointSynchronize::is_at_safepoint(), "should be at safepoint");
  assert(Thread::current() == (Thread*)VMThread::vm_thread(), "should be in vm thread");
  assert(!Universe::heap()->is_gc_active(), "not reentrant");

  PSAdaptiveSizePolicy* policy = ParallelScavengeHeap::size_policy();

  // Before each allocation/collection attempt, find out from the
  // policy object if GCs are, on the whole, taking too long. If so,
  // bail out without attempting a collection.
  if (!policy->gc_time_limit_exceeded()) {
    IsGCActiveMark mark;

    if (ScavengeBeforeFullGC) {
      PSScavenge::invoke_no_policy(notify_ref_lock);
    }

    int count = (maximum_heap_compaction)?1:MarkSweepAlwaysCompactCount;
    IntFlagSetting flag_setting(MarkSweepAlwaysCompactCount, count);
    PSMarkSweep::invoke_no_policy(notify_ref_lock, maximum_heap_compaction);
  }
}

// This method contains no policy. You should probably
// be calling invoke() instead. 
void PSMarkSweep::invoke_no_policy(bool* notify_ref_lock, 
				   bool clear_all_softrefs) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at a safepoint");
  assert(ref_processor() != NULL, "Sanity");

  if (GC_locker::is_active()) return;

  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  GCCause::Cause gc_cause = heap->gc_cause();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  PSYoungGen* young_gen = heap->young_gen();
  PSOldGen* old_gen = heap->old_gen();
  PSPermGen* perm_gen = heap->perm_gen();

  // Increment the invocation count
  heap->increment_total_collections();

  // We need to track unique mark sweep invocations as well.
  _total_invocations++;

  if (PrintHeapAtGC){
    gclog_or_tty->print_cr(" {Heap before GC invocations=%d:", heap->total_collections());
    Universe::print();
  }

  // Fill in TLABs
  heap->accumulate_statistics_all_tlabs();
  heap->ensure_parseability();

  if (VerifyBeforeGC && heap->total_collections() >= VerifyGCStartAt) {
    HandleMark hm;  // Discard invalid handles created during verification
    gclog_or_tty->print(" VerifyBeforeGC:");
    Universe::verify(true);
  }

  // Verify object start arrays
  if (VerifyObjectStartArray) {
    old_gen->verify_object_start_array();
    perm_gen->verify_object_start_array();
  }

  // Filled in below to track the state of the young gen after the collection.
  bool eden_empty;
  bool survivors_empty;
  bool young_gen_empty;

  {
    HandleMark hm;
    // This is useful for debugging but don't change the output
    // the the customer sees.
    char* gc_cause_string;
    char gc_cause_normal_string[] = "Full GC";
    char gc_cause_verbose_string[] = "Full GC (System)";
    if (gc_cause == GCCause::_java_lang_system_gc && 
      PrintGCDetails && Verbose) {
      gc_cause_string = gc_cause_verbose_string;
    } else {
      gc_cause_string = gc_cause_normal_string;
    }
    TraceTime t1(gc_cause_string, PrintGC, true, gclog_or_tty);
    TraceCollectorStats tcs(counters());
    TraceMemoryManagerStats tms(true /* Full GC */);

    if (TraceGen1Time) accumulated_time()->start();
  
    // Let the size policy know we're starting
    PSAdaptiveSizePolicy* size_policy = heap->size_policy();
    size_policy->major_collection_begin();

    // When collecting the permanent generation methodOops may be moving,
    // so we either have to flush all bcp data or convert it into bci.
    NOT_CORE(CodeCache::gc_prologue());
    Threads::gc_prologue();
    
    // Capture heap size before collection for printing.
    size_t prev_used = heap->used();

    // Capture perm gen size before collection for sizing.
    size_t perm_gen_prev_used = perm_gen->used_in_bytes();

    // For PrintGCDetails
    size_t old_gen_prev_used = old_gen->used_in_bytes();
    size_t young_gen_prev_used = young_gen->used_in_bytes();
    
    bool marked_for_unloading = false;
    
    allocate_stacks();
    
    NOT_PRODUCT(ref_processor()->verify_no_references_recorded());
    COMPILER2_ONLY(DerivedPointerTable::clear());
  
    ref_processor()->enable_discovery();

    mark_sweep_phase1(marked_for_unloading, clear_all_softrefs);

    mark_sweep_phase2();
    
    // Don't add any more derived pointers during phase3
    COMPILER2_ONLY(assert(DerivedPointerTable::is_active(), "Sanity"));
    COMPILER2_ONLY(DerivedPointerTable::set_active(false));
    
    mark_sweep_phase3();
    
    mark_sweep_phase4();
    
    restore_marks();
    
    deallocate_stacks();
    
    eden_empty = young_gen->eden_space()->is_empty();
    if (!eden_empty) {
      eden_empty = absorb_live_data_from_eden(size_policy, young_gen, old_gen);
    }

    // Update heap occupancy information which is used as
    // input to soft ref clearing policy at the next gc.
    Universe::update_heap_info_at_gc();

    survivors_empty = young_gen->from_space()->is_empty() && 
      young_gen->to_space()->is_empty();
    young_gen_empty = eden_empty && survivors_empty;
    
    BarrierSet* bs = heap->barrier_set();
    if (bs->is_a(BarrierSet::ModRef)) {
      ModRefBarrierSet* modBS = (ModRefBarrierSet*)bs;
      MemRegion old_mr = heap->old_gen()->reserved();
      MemRegion perm_mr = heap->perm_gen()->reserved();
      assert(perm_mr.end() <= old_mr.start(), "Generations out of order");
      
      if (young_gen_empty) {
        modBS->clear(MemRegion(perm_mr.start(), old_mr.end()));
      } else {
        modBS->invalidate(MemRegion(perm_mr.start(), old_mr.end()));
      }
    }
    
    Threads::gc_epilogue();
    NOT_CORE(CodeCache::gc_epilogue());
    
    COMPILER2_ONLY(DerivedPointerTable::update_pointers());
  
    *notify_ref_lock |= ref_processor()->enqueue_discovered_references();

    // Update time of last GC
    reset_millis_since_last_gc();

    // Let the size policy know we're done
    size_policy->major_collection_end(old_gen->used_in_bytes(), gc_cause);

    if (UseAdaptiveSizePolicy) {

      if (PrintAdaptiveSizePolicy) {
        gclog_or_tty->print("AdaptiveSizeStart: ");
        gclog_or_tty->stamp();
        gclog_or_tty->print_cr(" collection: %d ",
                       heap->total_collections());
	if (Verbose) {
	  gclog_or_tty->print("old_gen_capacity: %d young_gen_capacity: %d"
	    " perm_gen_capacity: %d ",
	    old_gen->capacity_in_bytes(), young_gen->capacity_in_bytes(), 
	    perm_gen->capacity_in_bytes());
	}
      }

      // Don't check if the size_policy is ready here.  Let
      // the size_policy check that internally.
      if (UseAdaptiveGenerationSizePolicyAtMajorCollection &&
          ((gc_cause != GCCause::_java_lang_system_gc) ||
            UseAdaptiveSizePolicyWithSystemGC)) {
        // Calculate optimal free space amounts
	assert(young_gen->max_size() > 
	  young_gen->from_space()->capacity_in_bytes() + 
	  young_gen->to_space()->capacity_in_bytes(), 
	  "Sizes of space in young gen are out-of-bounds");
	size_t max_eden_size = young_gen->max_size() - 
	  young_gen->from_space()->capacity_in_bytes() - 
	  young_gen->to_space()->capacity_in_bytes();
        size_policy->compute_generation_free_space(young_gen->used_in_bytes(),
				 young_gen->eden_space()->used_in_bytes(),
                                 old_gen->used_in_bytes(),
                                 perm_gen->used_in_bytes(),
				 young_gen->eden_space()->capacity_in_bytes(),
                                 old_gen->max_gen_size(),
                                 max_eden_size,
                                 true /* full gc*/);

        heap->resize_old_gen(size_policy->calculated_old_free_size_in_bytes());

        // Don't resize the young generation at an major collection.  A
        // desired young generation size may have been calculated but
        // resizing the young generation complicates the code because the
        // resizing of the old generation may have moved the boundary
        // between the young generation and the old generation.  Let the
        // young generation resizing happen at the minor collections.
      }
      if (PrintAdaptiveSizePolicy) {
        gclog_or_tty->print_cr("AdaptiveSizeStop: collection: %d ",
                       heap->total_collections());
      }
    }

    if (UsePerfData) {
      heap->gc_policy_counters()->update_counters();
      heap->gc_policy_counters()->update_old_capacity(
        old_gen->capacity_in_bytes());
      heap->gc_policy_counters()->update_young_capacity(
        young_gen->capacity_in_bytes());
    }

    heap->resize_all_tlabs();

    // We collected the perm gen, so we'll resize it here.
    perm_gen->compute_new_size(perm_gen_prev_used);
    
    if (TraceGen1Time) accumulated_time()->stop();

    if (PrintGC) {
      if (PrintGCDetails) {
        // Don't print a GC timestamp here.  This is after the GC so
        // would be confusing.
        young_gen->print_used_change(young_gen_prev_used);
        old_gen->print_used_change(old_gen_prev_used);
      }
      heap->print_heap_change(prev_used);
      // Do perm gen after heap becase prev_used does
      // not include the perm gen (done this way in the other
      // collectors).
      if (PrintGCDetails) {
        perm_gen->print_used_change(perm_gen_prev_used);
      }
    }

    // Track memory usage and detect low memory
    MemoryService::track_memory_usage();
    heap->update_counters();

    if (PrintGCDetails) {
      if (size_policy->print_gc_time_limit_would_be_exceeded()) {
        if (size_policy->gc_time_limit_exceeded()) {
          gclog_or_tty->print_cr("	GC time is exceeding GCTimeLimit "
	    "of %d%%", GCTimeLimit);
        } else {
          gclog_or_tty->print_cr("	GC time would exceed GCTimeLimit "
	    "of %d%%", GCTimeLimit);
	}
      }
      size_policy->set_print_gc_time_limit_would_be_exceeded(false);
    }
  }

  if (VerifyAfterGC && heap->total_collections() >= VerifyGCStartAt) {
    HandleMark hm;  // Discard invalid handles created during verification
    gclog_or_tty->print(" VerifyAfterGC:");
    Universe::verify(false);
  }

  // Re-verify object start arrays
  if (VerifyObjectStartArray) {
    old_gen->verify_object_start_array();
    perm_gen->verify_object_start_array();
  }

  NOT_PRODUCT(ref_processor()->verify_no_references_recorded());

  if (PrintHeapAtGC){
    gclog_or_tty->print_cr(" Heap after GC invocations=%d:", heap->total_collections());
    Universe::print();
    gclog_or_tty->print("} ");
  }
}

bool PSMarkSweep::absorb_live_data_from_eden(PSAdaptiveSizePolicy* size_policy,
					     PSYoungGen* young_gen,
					     PSOldGen* old_gen) {
  MutableSpace* const eden_space = young_gen->eden_space();
  assert(!eden_space->is_empty(), "eden must be non-empty");
  assert(young_gen->virtual_space()->alignment() ==
	 old_gen->virtual_space()->alignment(), "alignments do not match");

  if (!(UseAdaptiveSizePolicy && UseAdaptiveGCBoundary)) {
    return false;
  }

  // Both generations must be completely committed.
  if (young_gen->virtual_space()->uncommitted_size() != 0) {
    return false;
  }
  if (old_gen->virtual_space()->uncommitted_size() != 0) {
    return false;
  }

  // Figure out how much to take from eden.  Include the average amount promoted
  // in the total; otherwise the next young gen GC will simply bail out to a
  // full GC.
  const size_t alignment = old_gen->virtual_space()->alignment();
  const size_t eden_used = eden_space->used_in_bytes();
  const size_t promoted = size_policy->avg_promoted()->padded_average();
  const size_t absorb_size = align_size_up(eden_used + promoted, alignment);
  const size_t eden_capacity = eden_space->capacity_in_bytes();

  if (absorb_size >= eden_capacity) {
    return false; // Must leave some space in eden.
  }

  const size_t new_young_size = young_gen->capacity_in_bytes() - absorb_size;
  if (new_young_size < young_gen->min_gen_size()) {
    return false; // Respect young gen minimum size.
  }

  if (TraceAdaptiveGCBoundary && Verbose) {
    gclog_or_tty->print(" absorbing " SIZE_FORMAT "K:  "
			"eden " SIZE_FORMAT "K->" SIZE_FORMAT "K "
			"from " SIZE_FORMAT "K, to " SIZE_FORMAT "K "
			"young_gen " SIZE_FORMAT "K->" SIZE_FORMAT "K ",
			absorb_size / K,
			eden_capacity / K, (eden_capacity - absorb_size) / K,
			young_gen->from_space()->used_in_bytes() / K,
			young_gen->to_space()->used_in_bytes() / K,
			young_gen->capacity_in_bytes() / K, new_young_size / K);
  }

  // Fill the unused part of the old gen.
  MutableSpace* const old_space = old_gen->object_space();
  MemRegion old_gen_unused(old_space->top(), old_space->end());
  if (!old_gen_unused.is_empty()) {
    SharedHeap::fill_region_with_object(old_gen_unused);
  }

  // Take the live data from eden and set both top and end in the old gen to
  // eden top.  (Need to set end because reset_after_change() mangles the region
  // from end to virtual_space->high() in debug builds).
  HeapWord* const new_top = eden_space->top();
  old_gen->virtual_space()->expand_into(young_gen->virtual_space(),
					absorb_size);
  young_gen->reset_after_change();
  old_space->set_top(new_top);
  old_space->set_end(new_top);
  old_gen->reset_after_change();

  // Update the object start array for the filler object and the data from eden.
  ObjectStartArray* const start_array = old_gen->start_array();
  HeapWord* const start = old_gen_unused.start();
  for (HeapWord* addr = start; addr < new_top; addr += oop(addr)->size()) {
    start_array->allocate_block(addr);
  }

  // Could update the promoted average here, but it is not typically updated at
  // full GCs and the value to use is unclear.  Something like
  // 
  // cur_promoted_avg + absorb_size / number_of_scavenges_since_last_full_gc.

  size_policy->set_bytes_absorbed_from_eden(absorb_size);
  return true;
}

void PSMarkSweep::allocate_stacks() {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  PSYoungGen* young_gen = heap->young_gen();

  MutableSpace* to_space = young_gen->to_space();
  _preserved_marks = (PreservedMark*)to_space->top();
  _preserved_count = 0;

  // We want to calculate the size in bytes first.
  _preserved_count_max  = pointer_delta(to_space->end(), to_space->top(), sizeof(jbyte));
  // Now divide by the size of a PreservedMark
  _preserved_count_max /= sizeof(PreservedMark);

  _preserved_mark_stack = NULL;
  _preserved_oop_stack = NULL;

  _marking_stack = new GrowableArray<oop>(4000, true);

  int size = SystemDictionary::number_of_classes() * 2;
  _revisit_klass_stack = new GrowableArray<Klass*>(size, true);
}


void PSMarkSweep::deallocate_stacks() {
  if (_preserved_oop_stack) {
    _preserved_mark_stack->clear_and_deallocate();
    delete _preserved_mark_stack;
    _preserved_mark_stack = NULL;
    _preserved_oop_stack->clear_and_deallocate();
    delete _preserved_oop_stack;
    _preserved_oop_stack = NULL;
  }

  _marking_stack->clear_and_deallocate();
  _revisit_klass_stack->clear_and_deallocate();
}

void PSMarkSweep::mark_sweep_phase1( bool& marked_for_unloading, bool clear_all_softrefs) {
  // Recursively traverse all live objects and mark them
  EventMark m("1 mark object");
  TraceTime tm("phase 1", PrintGC && Verbose, true, gclog_or_tty);
  trace(" 1");

  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  // General strong roots.
  Universe::oops_do(mark_and_push_closure());
  JNIHandles::oops_do(mark_and_push_closure());   // Global (strong) JNI handles
  Threads::oops_do(mark_and_push_closure());
  ObjectSynchronizer::oops_do(mark_and_push_closure());
  FlatProfiler::oops_do(mark_and_push_closure());
  Management::oops_do(mark_and_push_closure());
  JvmtiExport::oops_do(mark_and_push_closure());
  SystemDictionary::always_strong_oops_do(mark_and_push_closure());
  vmSymbols::oops_do(mark_and_push_closure());

  // Flush marking stack.
  follow_stack();

  // Process reference objects found during marking
  {
    ReferencePolicy *soft_ref_policy;
    if (clear_all_softrefs) {
      soft_ref_policy = new AlwaysClearPolicy();
    } else {
      NOT_COMPILER2(soft_ref_policy = new LRUCurrentHeapPolicy();)
      COMPILER2_ONLY(soft_ref_policy = new LRUMaxHeapPolicy();)
    }
    assert(soft_ref_policy != NULL,"No soft reference policy");
    ReferenceProcessorSerial serial_rp(ref_processor(),
                                       soft_ref_policy,
                                       is_alive_closure(),
                                       mark_and_push_closure(),
                                       follow_stack_closure());
    serial_rp.process_discovered_references();
  }

  // Follow system dictionary roots and unload classes
  bool purged_class = SystemDictionary::do_unloading(is_alive_closure(),
                                                     mark_and_push_closure());
  follow_stack(); // Flush marking stack.

  // Follow code cache roots (has to be done after system dictionary, assumes all live klasses are marked)
  NOT_CORE(
           CodeCache::do_unloading(is_alive_closure(), mark_and_push_closure(),
                      purged_class, marked_for_unloading);// did we mark any nmethods for unloading?
           follow_stack(); // Flush marking stack.
  )

  // Update subklass/sibling/implementor links of live klasses
  follow_weak_klass_links();

  // Visit symbol and interned string tables and delete unmarked oops
  SymbolTable::unlink(is_alive_closure());
  StringTable::unlink(is_alive_closure());

  assert(_marking_stack->is_empty(), "stack should be empty by now");
}


void PSMarkSweep::mark_sweep_phase2() {
  EventMark m("2 compute new addresses");
  TraceTime tm("phase 2", PrintGC && Verbose, true, gclog_or_tty);
  trace("2");

  // Now all live objects are marked, compute the new object addresses.

  // It is imperative that we traverse perm_gen LAST. If dead space is
  // allowed a range of dead object may get overwritten by a dead int
  // array. If perm_gen is not traversed last a klassOop may get
  // overwritten. This is fine since it is dead, but if the class has dead
  // instances we have to skip them, and in order to find their size we
  // need the klassOop! 
  //
  // It is not required that we traverse spaces in the same order in
  // phase2, phase3 and phase4, but the ValidateMarkSweep live oops
  // tracking expects us to do so. See comment under phase4.

  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  PSOldGen* old_gen = heap->old_gen();
  PSPermGen* perm_gen = heap->perm_gen();

  // Begin compacting into the old gen
  PSMarkSweepDecorator::set_destination_decorator_tenured();

  // This will also compact the young gen spaces.
  old_gen->precompact();

  // Compact the perm gen into the perm gen
  PSMarkSweepDecorator::set_destination_decorator_perm_gen();

  perm_gen->precompact();
}

// This should be moved to the shared markSweep code!
class PSAlwaysTrueClosure: public BoolObjectClosure {
public:
  void do_object(oop p) { ShouldNotReachHere(); }
  bool do_object_b(oop p) { return true; }
};
static PSAlwaysTrueClosure always_true;

void PSMarkSweep::mark_sweep_phase3() {
  // Adjust the pointers to reflect the new locations
  EventMark m("3 adjust pointers");
  TraceTime tm("phase 3", PrintGC && Verbose, true, gclog_or_tty);
  trace("3");

  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  PSYoungGen* young_gen = heap->young_gen();
  PSOldGen* old_gen = heap->old_gen();
  PSPermGen* perm_gen = heap->perm_gen();

  // General strong roots.
  Universe::oops_do(adjust_root_pointer_closure());
  JNIHandles::oops_do(adjust_root_pointer_closure());   // Global (strong) JNI handles
  Threads::oops_do(adjust_root_pointer_closure());
  ObjectSynchronizer::oops_do(adjust_root_pointer_closure());
  FlatProfiler::oops_do(adjust_root_pointer_closure());
  Management::oops_do(adjust_root_pointer_closure());
  JvmtiExport::oops_do(adjust_root_pointer_closure());
  // CSO_AllClasses
  SystemDictionary::oops_do(adjust_root_pointer_closure());
  vmSymbols::oops_do(adjust_root_pointer_closure());

  // Now adjust pointers in remaining weak roots.  (All of which should
  // have been cleared if they pointed to non-surviving objects.)
  // Global (weak) JNI handles
  JNIHandles::weak_oops_do(&always_true, adjust_root_pointer_closure());

  NOT_CORE(CodeCache::oops_do(adjust_pointer_closure()));
  SymbolTable::oops_do(adjust_root_pointer_closure());
  StringTable::oops_do(adjust_root_pointer_closure());
  ReferenceProcessor::oops_do_statics(adjust_root_pointer_closure());
  ref_processor()->oops_do(adjust_root_pointer_closure());
  PSScavenge::reference_processor()->oops_do(adjust_root_pointer_closure());

  adjust_marks();

  young_gen->adjust_pointers();
  old_gen->adjust_pointers();
  perm_gen->adjust_pointers();
}

void PSMarkSweep::mark_sweep_phase4() {
  EventMark m("4 compact heap");
  TraceTime tm("phase 4", PrintGC && Verbose, true, gclog_or_tty);
  trace("4");

  // All pointers are now adjusted, move objects accordingly

  // It is imperative that we traverse perm_gen first in phase4. All
  // classes must be allocated earlier than their instances, and traversing
  // perm_gen first makes sure that all klassOops have moved to their new
  // location before any instance does a dispatch through it's klass!
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  PSYoungGen* young_gen = heap->young_gen();
  PSOldGen* old_gen = heap->old_gen();
  PSPermGen* perm_gen = heap->perm_gen();

  perm_gen->compact();
  old_gen->compact();
  young_gen->compact();
}

jlong PSMarkSweep::millis_since_last_gc() { 
  jlong ret_val = os::javaTimeMillis() - _time_of_last_gc; 
  // XXX See note in genCollectedHeap::millis_since_last_gc().
  if (ret_val < 0) {
    NOT_PRODUCT(warning("time warp: %d", ret_val);)
    return 0;
  }
  return ret_val;
}

void PSMarkSweep::reset_millis_since_last_gc() { 
  _time_of_last_gc = os::javaTimeMillis(); 
}
