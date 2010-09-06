#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)markSweep.hpp	1.57 03/12/23 16:40:28 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

class ReferenceProcessor;

// MarkSweep takes care of global mark-compact garbage collection for a
// GenCollectedHeap using a four-phase pointer forwarding algorithm.  All
// generations are assumed to support marking; those that can also support
// compaction.
//
// Class unloading will only occur when a full gc is invoked.

// If VALIDATE_MARK_SWEEP is defined, the -XX:+ValidateMarkSweep flag will
// be operational, and will provide slow but comprehensive self-checks within
// the GC.  This is not enabled by default in product or release builds,
// since the extra call to track_adjusted_pointer() in _adjust_pointer()
// would be too much overhead, and would disturb performance measurement.
// However, debug builds are sometimes way too slow to run GC tests!
#ifdef ASSERT
#define VALIDATE_MARK_SWEEP 1
#endif
#ifdef VALIDATE_MARK_SWEEP
#define VALIDATE_MARK_SWEEP_ONLY(code) code
#else
#define VALIDATE_MARK_SWEEP_ONLY(code)
#endif


// declared at end
class PreservedMark;

class MarkSweep : AllStatic {
  //
  // In line closure decls
  //

  class FollowRootClosure: public OopsInGenClosure{ 
   public:
    void do_oop(oop* p) { follow_root(p); }
    virtual const bool do_nmethods() const { return true; }
  };

  class MarkAndPushClosure: public OopClosure {
   public:
    void do_oop(oop* p) { mark_and_push(p); }
    virtual const bool do_nmethods() const { return true; }
  };

  class FollowStackClosure: public VoidClosure {
   public:
    void do_void() { follow_stack(); }
  };

  class AdjustPointerClosure: public OopsInGenClosure {
    bool _is_root;
   public:
    AdjustPointerClosure(bool is_root) : _is_root(is_root) {}
    void do_oop(oop* p) { _adjust_pointer(p, _is_root); }
  };

  // Used for java/lang/ref handling
  class IsAliveClosure: public BoolObjectClosure {
   public:
    void do_object(oop p) { assert(false, "don't call"); }
    bool do_object_b(oop p) { return p->is_gc_marked(); }
  };

  class KeepAliveClosure: public OopClosure {
   public:
    void do_oop(oop* p);
  };

  //
  // Friend decls
  //

  friend class AdjustPointerClosure;
  friend class KeepAliveClosure;
  friend class VM_MarkSweep;
  friend void marksweep_init();

  //
  // Vars
  //
 protected:
  // Traversal stack used during phase1
  static GrowableArray<oop>*             _marking_stack;   
  // Stack for live klasses to revisit at end of marking phase
  static GrowableArray<Klass*>*          _revisit_klass_stack;   

  // Space for storing/restoring mark word
  static GrowableArray<markOop>*         _preserved_mark_stack;
  static GrowableArray<oop>*             _preserved_oop_stack;
  static size_t			         _preserved_count;
  static size_t			         _preserved_count_max;
  static PreservedMark*                  _preserved_marks;
  
  // Reference processing (used in ...follow_contents)
  static ReferenceProcessor*             _ref_processor;

#ifdef VALIDATE_MARK_SWEEP
  static GrowableArray<oop*>*            _root_refs_stack;
  static GrowableArray<oop> *            _live_oops;
  static GrowableArray<oop> *            _live_oops_moved_to;
  static GrowableArray<size_t>*          _live_oops_size;
  static size_t                          _live_oops_index;
  static size_t                          _live_oops_index_at_perm;
  static GrowableArray<oop*>*            _other_refs_stack;
  static GrowableArray<oop*>*            _adjusted_pointers;
  static bool                            _pointer_tracking;
  static bool                            _root_tracking;

  // The following arrays are saved since the time of the last GC and
  // assist in tracking down problems where someone has done an errant
  // store into the heap, usually to an oop that wasn't properly
  // handleized across a GC. If we crash or otherwise fail before the
  // next GC, we can query these arrays to find out the object we had
  // intended to do the store to (assuming it is still alive) and the
  // offset within that object. Covered under RecordMarkSweepCompaction.
  static GrowableArray<HeapWord*> *      _cur_gc_live_oops;
  static GrowableArray<HeapWord*> *      _cur_gc_live_oops_moved_to;
  static GrowableArray<size_t>*          _cur_gc_live_oops_size;
  static GrowableArray<HeapWord*> *      _last_gc_live_oops;
  static GrowableArray<HeapWord*> *      _last_gc_live_oops_moved_to;
  static GrowableArray<size_t>*          _last_gc_live_oops_size;
#endif


  // Non public closures
  static IsAliveClosure is_alive;
  static KeepAliveClosure keep_alive;

  // Class unloading. Update subklass/sibling/implementor links at end of marking phase.
  static void follow_weak_klass_links();

  // Debugging
  static void trace(const char* msg) PRODUCT_RETURN;

 public:
  // Public closures
  static FollowRootClosure follow_root_closure;
  static MarkAndPushClosure mark_and_push_closure;
  static FollowStackClosure follow_stack_closure;
  static AdjustPointerClosure adjust_root_pointer_closure;
  static AdjustPointerClosure adjust_pointer_closure;

  // Reference Processing
  static ReferenceProcessor* const ref_processor() { return _ref_processor; }

  // Call backs for marking
  static void mark_object(oop obj);
  static void follow_root(oop* p);        // Mark pointer and follow contents. Empty marking

                                          // stack afterwards.

  static void mark_and_follow(oop* p);    // Mark pointer and follow contents.
  static void _mark_and_push(oop* p);     // Mark pointer and push obj on
					  // marking stack.

  
  static void mark_and_push(oop* p) {     // Check mark and maybe push on
					  // marking stack
    // assert(Universe::is_reserved_heap((oop)p), "we should only be traversing objects here");
    oop m = *p;
    if (m && !m->mark()->is_marked()) {
      _mark_and_push(p);
    }
  }

  static void follow_stack();             // Empty marking stack.


  static void preserve_mark(oop p, markOop mark);       // Save the mark word so it can be restored later
  static void adjust_marks();             // Adjust the pointers in the preserved marks table
  static void restore_marks();            // Restore the marks that we saved in preserve_mark

  inline static void _adjust_pointer(oop* p, bool isroot);
  
  static void adjust_root_pointer(oop* p) { _adjust_pointer(p, true); }
  static void adjust_pointer(oop* p)      { _adjust_pointer(p, false); }

#ifdef VALIDATE_MARK_SWEEP
  static void track_adjusted_pointer(oop* p, oop newobj, bool isroot);
  static void check_adjust_pointer(oop* p);     // Adjust this pointer
  static void track_interior_pointers(oop obj);
  static void check_interior_pointers();

  static void reset_live_oop_tracking(bool at_perm);
  static void register_live_oop(oop p, size_t size);
  static void validate_live_oop(oop p, size_t size);
  static void live_oop_moved_to(HeapWord* q, size_t size, HeapWord* compaction_top);
  static void compaction_complete();

  // Querying operation of RecordMarkSweepCompaction results.
  // Finds and prints the current base oop and offset for a word
  // within an oop that was live during the last GC. Helpful for
  // tracking down heap stomps.
  static void print_new_location_of_heap_address(HeapWord* q);
#endif

  // Call backs for class unloading
  static void revisit_weak_klass_link(Klass* k);  // Update subklass/sibling/implementor links at end of marking.
};


class PreservedMark VALUE_OBJ_CLASS_SPEC {
private:
  oop _obj;
  markOop _mark;

public:
  void init(oop obj, markOop mark) {
    _obj = obj;
    _mark = mark;
  }

  void adjust_pointer() {
    MarkSweep::adjust_pointer(&_obj);
  }

  void restore() {
    _obj->set_mark(_mark);
  }
};

class JVMPI_Object_Free : public ObjectClosure {
  void do_object(oop obj) {
    if (!obj->mark()->is_marked()) {
      jvmpi::post_object_free_event(obj);
    }
  }
};

