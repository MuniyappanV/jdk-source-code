/*
 * Copyright (c) 1997, 2009, Oracle and/or its affiliates. All rights reserved.
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

# include "incls/_precompiled.incl"
# include "incls/_sweeper.cpp.incl"

long      NMethodSweeper::_traversals = 0;   // No. of stack traversals performed
CodeBlob* NMethodSweeper::_current = NULL;   // Current nmethod
int       NMethodSweeper::_seen = 0 ;        // No. of blobs we have currently processed in current pass of CodeCache
int       NMethodSweeper::_invocations = 0;  // No. of invocations left until we are completed with this pass

jint      NMethodSweeper::_locked_seen = 0;
jint      NMethodSweeper::_not_entrant_seen_on_stack = 0;
bool      NMethodSweeper::_rescan = false;
bool      NMethodSweeper::_was_full = false;
jint      NMethodSweeper::_advise_to_sweep = 0;
jlong     NMethodSweeper::_last_was_full = 0;
uint      NMethodSweeper::_highest_marked = 0;
long      NMethodSweeper::_was_full_traversal = 0;

class MarkActivationClosure: public CodeBlobClosure {
public:
  virtual void do_code_blob(CodeBlob* cb) {
    // If we see an activation belonging to a non_entrant nmethod, we mark it.
    if (cb->is_nmethod() && ((nmethod*)cb)->is_not_entrant()) {
      ((nmethod*)cb)->mark_as_seen_on_stack();
    }
  }
};
static MarkActivationClosure mark_activation_closure;

void NMethodSweeper::sweep() {
  assert(SafepointSynchronize::is_at_safepoint(), "must be executed at a safepoint");
  if (!MethodFlushing) return;

  // No need to synchronize access, since this is always executed at a
  // safepoint.  If we aren't in the middle of scan and a rescan
  // hasn't been requested then just return.
  if (_current == NULL && !_rescan) return;

  // Make sure CompiledIC_lock in unlocked, since we might update some
  // inline caches. If it is, we just bail-out and try later.
  if (CompiledIC_lock->is_locked() || Patching_lock->is_locked()) return;

  // Check for restart
  assert(CodeCache::find_blob_unsafe(_current) == _current, "Sweeper nmethod cached state invalid");
  if (_current == NULL) {
    _seen        = 0;
    _invocations = NmethodSweepFraction;
    _current     = CodeCache::first();
    _traversals  += 1;
    if (PrintMethodFlushing) {
      tty->print_cr("### Sweep: stack traversal %d", _traversals);
    }
    Threads::nmethods_do(&mark_activation_closure);

    // reset the flags since we started a scan from the beginning.
    _rescan = false;
    _locked_seen = 0;
    _not_entrant_seen_on_stack = 0;
  }

  if (PrintMethodFlushing && Verbose) {
    tty->print_cr("### Sweep at %d out of %d. Invocations left: %d", _seen, CodeCache::nof_blobs(), _invocations);
  }

  // We want to visit all nmethods after NmethodSweepFraction invocations.
  // If invocation is 1 we do the rest
  int todo = CodeCache::nof_blobs();
  if (_invocations != 1) {
    todo = (CodeCache::nof_blobs() - _seen) / _invocations;
    _invocations--;
  }

  for(int i = 0; i < todo && _current != NULL; i++) {
    CodeBlob* next = CodeCache::next(_current); // Read next before we potentially delete current
    if (_current->is_nmethod()) {
      process_nmethod((nmethod *)_current);
    }
    _seen++;
    _current = next;
  }
  // Because we could stop on a codeBlob other than an nmethod we skip forward
  // to the next nmethod (if any). codeBlobs other than nmethods can be freed
  // async to us and make _current invalid while we sleep.
  while (_current != NULL && !_current->is_nmethod()) {
    _current = CodeCache::next(_current);
  }

  if (_current == NULL && !_rescan && (_locked_seen || _not_entrant_seen_on_stack)) {
    // we've completed a scan without making progress but there were
    // nmethods we were unable to process either because they were
    // locked or were still on stack.  We don't have to aggresively
    // clean them up so just stop scanning.  We could scan once more
    // but that complicates the control logic and it's unlikely to
    // matter much.
    if (PrintMethodFlushing) {
      tty->print_cr("### Couldn't make progress on some nmethods so stopping sweep");
    }
  }

  if (UseCodeCacheFlushing) {
    if (!CodeCache::needs_flushing()) {
      // In a safepoint, no race with setters
      _advise_to_sweep = 0;
    }

    if (was_full()) {
      // There was some progress so attempt to restart the compiler
      jlong now           = os::javaTimeMillis();
      jlong max_interval  = (jlong)MinCodeCacheFlushingInterval * (jlong)1000;
      jlong curr_interval = now - _last_was_full;
      if ((!CodeCache::needs_flushing()) && (curr_interval > max_interval)) {
        CompileBroker::set_should_compile_new_jobs(CompileBroker::run_compilation);
        set_was_full(false);

        // Update the _last_was_full time so we can tell how fast the
        // code cache is filling up
        _last_was_full = os::javaTimeMillis();

        if (PrintMethodFlushing) {
          tty->print_cr("### sweeper: Live blobs:" UINT32_FORMAT "/Free code cache:" SIZE_FORMAT " bytes, restarting compiler",
            CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
        }
        if (LogCompilation && (xtty != NULL)) {
          ttyLocker ttyl;
          xtty->begin_elem("restart_compiler live_blobs='" UINT32_FORMAT "' free_code_cache='" SIZE_FORMAT "'",
                           CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
          xtty->stamp();
          xtty->end_elem();
        }
      }
    }
  }
}


void NMethodSweeper::process_nmethod(nmethod *nm) {
  // Skip methods that are currently referenced by the VM
  if (nm->is_locked_by_vm()) {
    // But still remember to clean-up inline caches for alive nmethods
    if (nm->is_alive()) {
      // Clean-up all inline caches that points to zombie/non-reentrant methods
      nm->cleanup_inline_caches();
    } else {
      _locked_seen++;
    }
    return;
  }

  if (nm->is_zombie()) {
    // If it is first time, we see nmethod then we mark it. Otherwise,
    // we reclame it. When we have seen a zombie method twice, we know that
    // there are no inline caches that referes to it.
    if (nm->is_marked_for_reclamation()) {
      assert(!nm->is_locked_by_vm(), "must not flush locked nmethods");
      if (PrintMethodFlushing && Verbose) {
        tty->print_cr("### Nmethod %3d/" PTR_FORMAT " (marked for reclamation) being flushed", nm->compile_id(), nm);
      }
      nm->flush();
    } else {
      if (PrintMethodFlushing && Verbose) {
        tty->print_cr("### Nmethod %3d/" PTR_FORMAT " (zombie) being marked for reclamation", nm->compile_id(), nm);
      }
      nm->mark_for_reclamation();
      _rescan = true;
    }
  } else if (nm->is_not_entrant()) {
    // If there is no current activations of this method on the
    // stack we can safely convert it to a zombie method
    if (nm->can_not_entrant_be_converted()) {
      if (PrintMethodFlushing && Verbose) {
        tty->print_cr("### Nmethod %3d/" PTR_FORMAT " (not entrant) being made zombie", nm->compile_id(), nm);
      }
      nm->make_zombie();
      _rescan = true;
    } else {
      // Still alive, clean up its inline caches
      nm->cleanup_inline_caches();
      // we coudn't transition this nmethod so don't immediately
      // request a rescan.  If this method stays on the stack for a
      // long time we don't want to keep rescanning at every safepoint.
      _not_entrant_seen_on_stack++;
    }
  } else if (nm->is_unloaded()) {
    // Unloaded code, just make it a zombie
    if (PrintMethodFlushing && Verbose)
      tty->print_cr("### Nmethod %3d/" PTR_FORMAT " (unloaded) being made zombie", nm->compile_id(), nm);
    if (nm->is_osr_method()) {
      // No inline caches will ever point to osr methods, so we can just remove it
      nm->flush();
    } else {
      nm->make_zombie();
      _rescan = true;
    }
  } else {
    assert(nm->is_alive(), "should be alive");

    if (UseCodeCacheFlushing) {
      if ((nm->method()->code() != nm) && !(nm->is_locked_by_vm()) && !(nm->is_osr_method()) &&
          (_traversals > _was_full_traversal+2) && (((uint)nm->compile_id()) < _highest_marked) &&
          CodeCache::needs_flushing()) {
        // This method has not been called since the forced cleanup happened
        nm->make_not_entrant();
      }
    }

    // Clean-up all inline caches that points to zombie/non-reentrant methods
    nm->cleanup_inline_caches();
  }
}

// Code cache unloading: when compilers notice the code cache is getting full,
// they will call a vm op that comes here. This code attempts to speculatively
// unload the oldest half of the nmethods (based on the compile job id) by
// saving the old code in a list in the CodeCache. Then
// execution resumes. If a method so marked is not called by the second
// safepoint from the current one, the nmethod will be marked non-entrant and
// got rid of by normal sweeping. If the method is called, the methodOop's
// _code field is restored and the methodOop/nmethod
// go back to their normal state.
void NMethodSweeper::handle_full_code_cache(bool is_full) {
  // Only the first one to notice can advise us to start early cleaning
  if (!is_full){
    jint old = Atomic::cmpxchg( 1, &_advise_to_sweep, 0 );
    if (old != 0) {
      return;
    }
  }

  if (is_full) {
    // Since code cache is full, immediately stop new compiles
    bool did_set = CompileBroker::set_should_compile_new_jobs(CompileBroker::stop_compilation);
    if (!did_set) {
      // only the first to notice can start the cleaning,
      // others will go back and block
      return;
    }
    set_was_full(true);

    // If we run out within MinCodeCacheFlushingInterval of the last unload time, give up
    jlong now = os::javaTimeMillis();
    jlong max_interval = (jlong)MinCodeCacheFlushingInterval * (jlong)1000;
    jlong curr_interval = now - _last_was_full;
    if (curr_interval < max_interval) {
      _rescan = true;
      if (PrintMethodFlushing) {
        tty->print_cr("### handle full too often, turning off compiler");
      }
      if (LogCompilation && (xtty != NULL)) {
        ttyLocker ttyl;
        xtty->begin_elem("disable_compiler flushing_interval='" UINT64_FORMAT "' live_blobs='" UINT32_FORMAT "' free_code_cache='" SIZE_FORMAT "'",
                         curr_interval/1000, CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
        xtty->stamp();
        xtty->end_elem();
      }
      return;
    }
  }

  VM_HandleFullCodeCache op(is_full);
  VMThread::execute(&op);

  // rescan again as soon as possible
  _rescan = true;
}

void NMethodSweeper::speculative_disconnect_nmethods(bool is_full) {
  // If there was a race in detecting full code cache, only run
  // one vm op for it or keep the compiler shut off

  debug_only(jlong start = os::javaTimeMillis();)

  if ((!was_full()) && (is_full)) {
    if (!CodeCache::needs_flushing()) {
      if (PrintMethodFlushing) {
        tty->print_cr("### sweeper: Live blobs:" UINT32_FORMAT "/Free code cache:" SIZE_FORMAT " bytes, restarting compiler",
          CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
      }
      if (LogCompilation && (xtty != NULL)) {
        ttyLocker ttyl;
        xtty->begin_elem("restart_compiler live_blobs='" UINT32_FORMAT "' free_code_cache='" SIZE_FORMAT "'",
                         CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
        xtty->stamp();
        xtty->end_elem();
      }
      CompileBroker::set_should_compile_new_jobs(CompileBroker::run_compilation);
      return;
    }
  }

  // Traverse the code cache trying to dump the oldest nmethods
  uint curr_max_comp_id = CompileBroker::get_compilation_id();
  uint flush_target = ((curr_max_comp_id - _highest_marked) >> 1) + _highest_marked;
  if (PrintMethodFlushing && Verbose) {
    tty->print_cr("### Cleaning code cache: Live blobs:" UINT32_FORMAT "/Free code cache:" SIZE_FORMAT " bytes",
        CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
  }
  if (LogCompilation && (xtty != NULL)) {
    ttyLocker ttyl;
    xtty->begin_elem("start_cleaning_code_cache live_blobs='" UINT32_FORMAT "' free_code_cache='" SIZE_FORMAT "'",
                      CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
    xtty->stamp();
    xtty->end_elem();
  }

  nmethod* nm = CodeCache::alive_nmethod(CodeCache::first());
  jint disconnected = 0;
  jint made_not_entrant  = 0;
  while ((nm != NULL)){
    uint curr_comp_id = nm->compile_id();

    // OSR methods cannot be flushed like this. Also, don't flush native methods
    // since they are part of the JDK in most cases
    if (nm->is_in_use() && (!nm->is_osr_method()) && (!nm->is_locked_by_vm()) &&
        (!nm->is_native_method()) && ((curr_comp_id < flush_target))) {

      if ((nm->method()->code() == nm)) {
        // This method has not been previously considered for
        // unloading or it was restored already
        CodeCache::speculatively_disconnect(nm);
        disconnected++;
      } else if (nm->is_speculatively_disconnected()) {
        // This method was previously considered for preemptive unloading and was not called since then
        nm->method()->invocation_counter()->decay();
        nm->method()->backedge_counter()->decay();
        nm->make_not_entrant();
        made_not_entrant++;
      }

      if (curr_comp_id > _highest_marked) {
        _highest_marked = curr_comp_id;
      }
    }
    nm = CodeCache::alive_nmethod(CodeCache::next(nm));
  }

  if (LogCompilation && (xtty != NULL)) {
    ttyLocker ttyl;
    xtty->begin_elem("stop_cleaning_code_cache disconnected='" UINT32_FORMAT "' made_not_entrant='" UINT32_FORMAT "' live_blobs='" UINT32_FORMAT "' free_code_cache='" SIZE_FORMAT "'",
                      disconnected, made_not_entrant, CodeCache::nof_blobs(), CodeCache::unallocated_capacity());
    xtty->stamp();
    xtty->end_elem();
  }

  // Shut off compiler. Sweeper will run exiting from this safepoint
  // and turn it back on if it clears enough space
  if (was_full()) {
    _last_was_full = os::javaTimeMillis();
    CompileBroker::set_should_compile_new_jobs(CompileBroker::stop_compilation);
  }

  // After two more traversals the sweeper will get rid of unrestored nmethods
  _was_full_traversal = _traversals;
#ifdef ASSERT
  jlong end = os::javaTimeMillis();
  if(PrintMethodFlushing && Verbose) {
    tty->print_cr("### sweeper: unload time: " INT64_FORMAT, end-start);
  }
#endif
}
