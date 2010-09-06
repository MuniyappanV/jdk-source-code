#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)threadLocalAllocBuffer.cpp	1.42 04/02/11 12:00:10 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Thread-Local Edens support

# include "incls/_precompiled.incl"
# include "incls/_threadLocalAllocBuffer.cpp.incl"

// static member initialization
unsigned         ThreadLocalAllocBuffer::_target_refills = 0;
GlobalTLABStats* ThreadLocalAllocBuffer::_global_stats   = NULL;

void ThreadLocalAllocBuffer::clear() {
  // No waste accounting done here since this generic
  // operation is not a good predictor of future behavior.
  reset();
}

void ThreadLocalAllocBuffer::clear_before_allocation() {
  _slow_refill_waste += (unsigned)remaining();
  reset();
}

void ThreadLocalAllocBuffer::accumulate_statistics_before_gc() {
  global_stats()->initialize();

  size_t capacity = Universe::heap()->tlab_capacity() / HeapWordSize;
  size_t unused   = Universe::heap()->unsafe_max_tlab_alloc() / HeapWordSize;
  size_t used     = capacity - unused;

  // Update allocation history if a reasonable amount of eden was allocated.
  bool update_allocation_history = used > 0.5 * capacity;

  for(JavaThread *thread = Threads::first(); thread; thread = thread->next()) {
    thread->tlab().accumulate_statistics(used, update_allocation_history);
    thread->tlab().initialize_statistics();
  }

  // Publish new stats if some allocation occurred.
  if (global_stats()->allocation() != 0) {
    global_stats()->publish();
    if (PrintTLAB) {
      global_stats()->print();
    }
  }
}

void ThreadLocalAllocBuffer::accumulate_statistics(size_t used,
                                                   bool update_allocation_history) {

  _gc_waste += (unsigned)remaining();

  if (PrintTLAB && (_number_of_refills > 0 || Verbose)) {
    print_stats("gc");
  }

  if (_number_of_refills > 0) {

    if (update_allocation_history) {
      // Average the fraction of eden allocated in a tlab by this
      // thread for use in the next resize operation.
      // _gc_waste is not subtracted because it's included in
      // "used".
      size_t allocation = _number_of_refills * size();
      double alloc_frac = allocation / (double) used;
      _allocation_fraction.sample(alloc_frac);
    }
    global_stats()->update_allocating_threads();
    global_stats()->update_number_of_refills(_number_of_refills);
    global_stats()->update_allocation(_number_of_refills * size());
    global_stats()->update_gc_waste(_gc_waste);
    global_stats()->update_slow_refill_waste(_slow_refill_waste);
    global_stats()->update_fast_refill_waste(_fast_refill_waste);

  } else {
    assert(_number_of_refills == 0 && _fast_refill_waste == 0 &&
           _slow_refill_waste == 0 && _gc_waste          == 0,
           "tlab stats == 0");
  }
  global_stats()->update_slow_allocations(_slow_allocations);
}

// Fills the current tlab with a dummy filler array to create
// an illusion of a contiguous Eden and clears the tlab info
void ThreadLocalAllocBuffer::reset() {
  if (end() != NULL) {
    invariants();
    MemRegion mr(top(), hard_end());
    SharedHeap::fill_region_with_object(mr);

    set_start(NULL);
    set_top(NULL);
    set_end(NULL);
  }
  assert(start() == NULL && end() == NULL && top() == NULL,
         "TLAB must be reset");
}

void ThreadLocalAllocBuffer::resize_all_tlabs() {
  for(JavaThread *thread = Threads::first(); thread; thread = thread->next()) {
    thread->tlab().resize();
  }
}

void ThreadLocalAllocBuffer::resize() {

  if (ResizeTLAB) {
    // Compute the next tlab size using expected allocation amount
    size_t alloc = _allocation_fraction.average() *
                   (Universe::heap()->tlab_capacity() / HeapWordSize);
    size_t new_size = alloc / _target_refills;

    new_size = MIN2(MAX2(new_size, min_size()), max_size());

    size_t aligned_new_size = align_object_size(new_size);

    if (PrintTLAB && Verbose) {
      gclog_or_tty->print("TLAB new size: thread: " INTPTR_FORMAT " [id: %2d]"
                          " refills %d  alloc: %8.6f size: " SIZE_FORMAT " -> " SIZE_FORMAT "\n",
                          myThread(), myThread()->osthread()->thread_id(),
                          _target_refills, _allocation_fraction.average(), size(), aligned_new_size);
    }
    set_size(aligned_new_size);

    set_refill_waste_limit(initial_refill_waste_limit());
  }
}

void ThreadLocalAllocBuffer::initialize_statistics() {
    _number_of_refills = 0;
    _fast_refill_waste = 0;
    _slow_refill_waste = 0;
    _gc_waste          = 0;
    _slow_allocations  = 0;
}

void ThreadLocalAllocBuffer::fill(HeapWord* start,
                                  HeapWord* top,
                                  size_t    new_size) {
  _number_of_refills++;
  if (PrintTLAB && Verbose) {
    print_stats("fill");
  }
  assert(top <= start + new_size - alignment_reserve(), "size too small");
  initialize(start, top, start + new_size - alignment_reserve());

  // Reset amount of internal fragmentation
  set_refill_waste_limit(initial_refill_waste_limit());
}

void ThreadLocalAllocBuffer::initialize(HeapWord* start,
                                        HeapWord* top,
                                        HeapWord* end) {
  set_start(start);
  set_top(top);
  set_end(end);
  invariants();
}

void ThreadLocalAllocBuffer::initialize() {
  initialize(NULL,                    // start
             NULL,                    // top
             NULL);                   // end
  
  set_size(initial_size());

  // Following check is needed because at startup the main (primordial)
  // thread is initialized before the heap is.  The initialization for
  // this thread is redone in startup_initialization below.
  if (Universe::heap() != NULL) {
    size_t capacity   = Universe::heap()->tlab_capacity() / HeapWordSize;
    double alloc_frac = size() * target_refills() / (double) capacity;
    _allocation_fraction.sample(alloc_frac);
  }

  set_refill_waste_limit(initial_refill_waste_limit());

  initialize_statistics();
}

void ThreadLocalAllocBuffer::startup_initialization() {

  // Assuming each thread's active tlab is, on average,
  // 1/2 full at a GC
  _target_refills = 100 / (2 * TLABWasteTargetPercent);
  _target_refills = MAX2(_target_refills, (unsigned)1U);

  _global_stats = new GlobalTLABStats();

  // During jvm startup, the main (primordial) thread is initialized
  // before the heap is initialized.  So reinitialize it now.
  guarantee(Thread::current()->is_Java_thread(), "tlab initialization thread not Java thread");
  Thread::current()->tlab().initialize();

  if (PrintTLAB && Verbose) {
    gclog_or_tty->print("TLAB min: " SIZE_FORMAT " initial: " SIZE_FORMAT " max: " SIZE_FORMAT "\n",
                        min_size(), initial_size(), max_size());
  }
}

size_t ThreadLocalAllocBuffer::initial_size() {
  size_t init_sz;

  if (TLABSize > 0) {
    init_sz = MIN2(TLABSize / HeapWordSize, max_size());
  } else if (global_stats() == NULL) {
    // Startup issue - main thread initialized before heap initialized.
    init_sz = min_size();
  } else {
    // Initial size is a function of the average number of allocating threads.
    unsigned nof_threads = global_stats()->allocating_threads_avg();

    init_sz  = (Universe::heap()->tlab_capacity() / HeapWordSize) /
                      (nof_threads * target_refills());
    init_sz = align_object_size(init_sz);
    init_sz = MIN2(MAX2(init_sz, min_size()), max_size());
  }
  return init_sz;
}

size_t ThreadLocalAllocBuffer::max_size() {

  // TLABs can't be bigger than we can fill with a int[Integer.MAX_VALUE].
  // This restriction could be removed by enabling filling with multiple arrays.
  // If we compute that the reasonable way as
  //    header_size + ((sizeof(jint) * max_jint) / HeapWordSize)
  // we'll overflow on the multiply, so we do the divide first.
  // We actually lose a little by dividing first,
  // but that just makes the TLAB  somewhat smaller than the biggest array,
  // which is fine, since we'll be able to fill that.

  return align_object_size(typeArrayOopDesc::header_size(T_INT) +
                           sizeof(jint) *
                           ((juint) max_jint / (size_t) HeapWordSize));
}

void ThreadLocalAllocBuffer::print_stats(const char* tag) {
  Thread* thrd = myThread();
  size_t waste = _gc_waste + _slow_refill_waste + _fast_refill_waste;
  size_t alloc = _number_of_refills * _size;
  double waste_percent = alloc == 0 ? 0.0 :
                      100.0 * waste / alloc;
  size_t tlab_used  = Universe::heap()->tlab_capacity() -
                      Universe::heap()->unsafe_max_tlab_alloc();
  gclog_or_tty->print("TLAB: %s thread: " INTPTR_FORMAT " [id: %2d]"
                      " size: " SIZE_FORMAT "KB"
                      " slow allocs: %d  refill waste: " SIZE_FORMAT "B"
                      " alloc:%8.5f %8.0fKB refills: %d waste %4.1f%% gc: %dB"
                      " slow: %dB fast: %dB\n",
                      tag, thrd, thrd->osthread()->thread_id(),
                      _size / (K / HeapWordSize),
                      _slow_allocations, _refill_waste_limit * HeapWordSize,
                      _allocation_fraction.average(),
                      _allocation_fraction.average() * tlab_used / K,
                      _number_of_refills, waste_percent,
                      _gc_waste * HeapWordSize,
                      _slow_refill_waste * HeapWordSize,
                      _fast_refill_waste * HeapWordSize);
}

void ThreadLocalAllocBuffer::verify() {
  HeapWord* p = start();
  HeapWord* t = top();
  HeapWord* prev_p = NULL;
  while (p < t) {
    oop(p)->verify();
    prev_p = p;
    p += oop(p)->size();
  }
  guarantee(p == top(), "end of last object must match end of space");
}

Thread* ThreadLocalAllocBuffer::myThread() {
  return (Thread*)(((char *)this) +
                   in_bytes(start_offset()) -
                   in_bytes(Thread::tlab_start_offset()));
}


GlobalTLABStats::GlobalTLABStats() :
  _allocating_threads_avg(TLABAllocationWeight) {

  initialize();

  _allocating_threads_avg.sample(1); // One allocating thread at startup

  if (UsePerfData) {

    EXCEPTION_MARK;
    ResourceMark rm;

    char* cname = PerfDataManager::counter_name("tlab", "allocThreads");
    _perf_allocating_threads =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_None, CHECK);

    cname = PerfDataManager::counter_name("tlab", "fills");
    _perf_total_refills =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_None, CHECK);

    cname = PerfDataManager::counter_name("tlab", "maxFills");
    _perf_max_refills =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_None, CHECK);

    cname = PerfDataManager::counter_name("tlab", "alloc");
    _perf_allocation =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "gcWaste");
    _perf_gc_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "maxGcWaste");
    _perf_max_gc_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "slowWaste");
    _perf_slow_refill_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "maxSlowWaste");
    _perf_max_slow_refill_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "fastWaste");
    _perf_fast_refill_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "maxFastWaste");
    _perf_max_fast_refill_waste =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_Bytes, CHECK);

    cname = PerfDataManager::counter_name("tlab", "slowAlloc");
    _perf_slow_allocations =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_None, CHECK);

    cname = PerfDataManager::counter_name("tlab", "maxSlowAlloc");
    _perf_max_slow_allocations =
      PerfDataManager::create_variable(SUN_GC, cname, PerfData::U_None, CHECK);
  }
}

void GlobalTLABStats::initialize() {
  // Clear counters summarizing info from all threads
  _allocating_threads      = 0;
  _total_refills           = 0;
  _max_refills             = 0;
  _total_allocation        = 0;
  _total_gc_waste          = 0;
  _max_gc_waste            = 0;
  _total_slow_refill_waste = 0;
  _max_slow_refill_waste   = 0;
  _total_fast_refill_waste = 0;
  _max_fast_refill_waste   = 0;
  _total_slow_allocations  = 0;
  _max_slow_allocations    = 0;
}

void GlobalTLABStats::publish() {
  _allocating_threads_avg.sample(_allocating_threads);
  if (UsePerfData) {
    _perf_allocating_threads   ->set_value(_allocating_threads);
    _perf_total_refills        ->set_value(_total_refills);
    _perf_max_refills          ->set_value(_max_refills);
    _perf_allocation           ->set_value(_total_allocation);
    _perf_gc_waste             ->set_value(_total_gc_waste);
    _perf_max_gc_waste         ->set_value(_max_gc_waste);
    _perf_slow_refill_waste    ->set_value(_total_slow_refill_waste);
    _perf_max_slow_refill_waste->set_value(_max_slow_refill_waste);
    _perf_fast_refill_waste    ->set_value(_total_fast_refill_waste);
    _perf_max_fast_refill_waste->set_value(_max_fast_refill_waste);
    _perf_slow_allocations     ->set_value(_total_slow_allocations);
    _perf_max_slow_allocations ->set_value(_max_slow_allocations);
  }
}

void GlobalTLABStats::print() {
  size_t waste = _total_gc_waste + _total_slow_refill_waste + _total_fast_refill_waste;
  double waste_percent = _total_allocation == 0 ? 0.0 :
                         100.0 * waste / _total_allocation;
  gclog_or_tty->print("TLAB totals: thrds: %d  refills: %d max: %d"
                      " slow allocs: %d max %d waste: %4.1f%%"
                      " gc: " SIZE_FORMAT "B max: " SIZE_FORMAT "B"
                      " slow: " SIZE_FORMAT "B max: " SIZE_FORMAT "B"
                      " fast: " SIZE_FORMAT "B max: " SIZE_FORMAT "B\n",
                      _allocating_threads,
                      _total_refills, _max_refills,
                      _total_slow_allocations, _max_slow_allocations,
                      waste_percent,
                      _total_gc_waste * HeapWordSize,
                      _max_gc_waste * HeapWordSize,
                      _total_slow_refill_waste * HeapWordSize,
                      _max_slow_refill_waste * HeapWordSize,
                      _total_fast_refill_waste * HeapWordSize,
                      _max_fast_refill_waste * HeapWordSize);
}
