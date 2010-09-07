/*
 * Copyright (c) 2005, 2009, Oracle and/or its affiliates. All rights reserved.
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

//
// psPromotionManager is used by a single thread to manage object survival
// during a scavenge. The promotion manager contains thread local data only.
//
// NOTE! Be carefull when allocating the stacks on cheap. If you are going
// to use a promotion manager in more than one thread, the stacks MUST be
// on cheap. This can lead to memory leaks, though, as they are not auto
// deallocated.
//
// FIX ME FIX ME Add a destructor, and don't rely on the user to drain/flush/deallocate!
//

// Move to some global location
#define HAS_BEEN_MOVED 0x1501d01d
// End move to some global location


class MutableSpace;
class PSOldGen;
class ParCompactionManager;
class ObjectStartArray;
class ParallelCompactData;
class ParMarkBitMap;

// Move to it's own file if this works out.

class ParCompactionManager : public CHeapObj {
  friend class ParallelTaskTerminator;
  friend class ParMarkBitMap;
  friend class PSParallelCompact;
  friend class StealRegionCompactionTask;
  friend class UpdateAndFillClosure;
  friend class RefProcTaskExecutor;

 public:

// ------------------------  Don't putback if not needed
  // Actions that the compaction manager should take.
  enum Action {
    Update,
    Copy,
    UpdateAndCopy,
    CopyAndUpdate,
    VerifyUpdate,
    ResetObjects,
    NotValid
  };
// ------------------------  End don't putback if not needed

 private:
  static ParCompactionManager** _manager_array;
  static OopTaskQueueSet*       _stack_array;
  static ObjectStartArray*      _start_array;
  static RegionTaskQueueSet*    _region_array;
  static PSOldGen*              _old_gen;

  OopTaskQueue                  _marking_stack;
  Stack<oop>                    _overflow_stack;
  // Is there a way to reuse the _marking_stack for the
  // saving empty regions?  For now just create a different
  // type of TaskQueue.

#ifdef USE_RegionTaskQueueWithOverflow
  RegionTaskQueueWithOverflow   _region_stack;
#else
  RegionTaskQueue               _region_stack;
  Stack<size_t>                 _region_overflow_stack;
#endif

  Stack<Klass*>                 _revisit_klass_stack;
  Stack<DataLayout*>            _revisit_mdo_stack;

  static ParMarkBitMap* _mark_bitmap;

  Action _action;

  static PSOldGen* old_gen()             { return _old_gen; }
  static ObjectStartArray* start_array() { return _start_array; }
  static OopTaskQueueSet* stack_array()  { return _stack_array; }

  static void initialize(ParMarkBitMap* mbm);

 protected:
  // Array of tasks.  Needed by the ParallelTaskTerminator.
  static RegionTaskQueueSet* region_array()      { return _region_array; }
  OopTaskQueue*  marking_stack()                 { return &_marking_stack; }
  Stack<oop>* overflow_stack()                   { return &_overflow_stack; }
#ifdef USE_RegionTaskQueueWithOverflow
  RegionTaskQueueWithOverflow* region_stack()    { return &_region_stack; }
#else
  RegionTaskQueue*  region_stack()               { return &_region_stack; }
  Stack<size_t>* region_overflow_stack() { return &_region_overflow_stack; }
#endif

  // Pushes onto the marking stack.  If the marking stack is full,
  // pushes onto the overflow stack.
  void stack_push(oop obj);
  // Do not implement an equivalent stack_pop.  Deal with the
  // marking stack and overflow stack directly.

  // Pushes onto the region stack.  If the region stack is full,
  // pushes onto the region overflow stack.
  void region_stack_push(size_t region_index);
 public:

  Action action() { return _action; }
  void set_action(Action v) { _action = v; }

  inline static ParCompactionManager* manager_array(int index);

  ParCompactionManager();

  ParMarkBitMap* mark_bitmap() { return _mark_bitmap; }

  // Take actions in preparation for a compaction.
  static void reset();

  // void drain_stacks();

  bool should_update();
  bool should_copy();
  bool should_verify_only();
  bool should_reset_only();

  Stack<Klass*>* revisit_klass_stack() { return &_revisit_klass_stack; }
  Stack<DataLayout*>* revisit_mdo_stack() { return &_revisit_mdo_stack; }

  // Save oop for later processing.  Must not fail.
  void save_for_scanning(oop m);
  // Get a oop for scanning.  If returns null, no oop were found.
  oop retrieve_for_scanning();

  // Save region for later processing.  Must not fail.
  void save_for_processing(size_t region_index);
  // Get a region for processing.  If returns null, no region were found.
  bool retrieve_for_processing(size_t& region_index);

  // Access function for compaction managers
  static ParCompactionManager* gc_thread_compaction_manager(int index);

  static bool steal(int queue_num, int* seed, Task& t) {
    return stack_array()->steal(queue_num, seed, t);
  }

  static bool steal(int queue_num, int* seed, RegionTask& t) {
    return region_array()->steal(queue_num, seed, t);
  }

  // Process tasks remaining on any stack
  void drain_marking_stacks(OopClosure *blk);

  // Process tasks remaining on any stack
  void drain_region_stacks();

  // Process tasks remaining on any stack
  void drain_region_overflow_stack();
};

inline ParCompactionManager* ParCompactionManager::manager_array(int index) {
  assert(_manager_array != NULL, "access of NULL manager_array");
  assert(index >= 0 && index <= (int)ParallelGCThreads,
    "out of range manager_array access");
  return _manager_array[index];
}
