#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)interpreterRuntime.cpp	1.451 04/06/09 09:32:59 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_interpreterRuntime.cpp.incl"

class UnlockFlagSaver {
  private:
    JavaThread* _thread;
    bool _do_not_unlock;
  public:
    UnlockFlagSaver(JavaThread* t) {
      _thread = t;
      _do_not_unlock = t->do_not_unlock_if_synchronized();
      t->set_do_not_unlock_if_synchronized(false);
    }
    ~UnlockFlagSaver() {
      _thread->set_do_not_unlock_if_synchronized(_do_not_unlock);
    }
};

//------------------------------------------------------------------------------------------------------------------------
// State accessors

void InterpreterRuntime::set_bcp_and_mdp(address bcp, JavaThread *thread) {
  last_frame(thread).interpreter_frame_set_bcp(bcp);
#ifndef CORE
  methodDataOop mdo = last_frame(thread).interpreter_frame_method()->method_data();
  if (mdo != NULL) {
    NEEDS_CLEANUP;
    last_frame(thread).interpreter_frame_set_mdp(mdo->bci_to_dp(last_frame(thread).interpreter_frame_bci()));
  }
#endif // !CORE
}

//------------------------------------------------------------------------------------------------------------------------
// Constants


IRT_ENTRY(void, InterpreterRuntime::ldc(JavaThread* thread, bool wide))
  // access constant pool
  constantPoolOop pool = method(thread)->constants();
  int index = wide ? two_byte_index(thread) : one_byte_index(thread);
  constantTag tag = pool->tag_at(index);

  if (tag.is_unresolved_klass() || tag.is_klass()) {
    klassOop klass = pool->klass_at(index, CHECK);
    oop java_class = klass->klass_part()->java_mirror();
    thread->set_vm_result(java_class);
  } else {
#ifdef ASSERT
    // If we entered this runtime routine, we believed the tag contained
    // an unresolved string, an unresolved class or a resolved class. 
    // However, another thread could have resolved the unresolved string
    // or class by the time we go there.
    assert(tag.is_unresolved_string()|| tag.is_string(), "expected string");
#endif
    oop s_oop = pool->string_at(index, CHECK);
    thread->set_vm_result(s_oop);
  }
IRT_END


//------------------------------------------------------------------------------------------------------------------------
// Allocation

IRT_ENTRY(void, InterpreterRuntime::_new(JavaThread* thread, constantPoolOop pool, int index))
  klassOop k_oop = pool->klass_at(index, CHECK);
  instanceKlassHandle klass (THREAD, k_oop);

  // Make sure we are not instantiating an abstract klass
  klass->check_valid_for_instantiation(true, CHECK);

  // Make sure klass is initialized
  klass->initialize(CHECK);    

  // At this point the class may not be fully initialized
  // because of recursive initialization. If it is fully
  // initialized & has_finalized is not set, we rewrite
  // it into its fast version (Note: no locking is needed
  // here since this is an atomic byte write and can be
  // done more than once).
  //
  // Note: In case of classes with has_finalized we don't
  //       rewrite since that saves us an extra check in
  //       the fast version which then would call the
  //       slow version anyway (and do a call back into
  //       Java).
  //       If we have a breakpoint, then we don't rewrite
  //       because the _breakpoint bytecode would be lost.
  oop obj = klass->allocate_instance(CHECK);
  thread->set_vm_result(obj);  
IRT_END


IRT_ENTRY(void, InterpreterRuntime::newarray(JavaThread* thread, BasicType type, jint size))
  oop obj = oopFactory::new_typeArray(type, size, CHECK);
  thread->set_vm_result(obj);
IRT_END


IRT_ENTRY(void, InterpreterRuntime::anewarray(JavaThread* thread, constantPoolOop pool, int index, jint size))
  // Note: no oopHandle for pool & klass needed since they are not used
  //       anymore after new_objArray() and no GC can happen before.
  //       (This may have to change if this code changes!)       
  klassOop  klass = pool->klass_at(index, CHECK);    
  objArrayOop obj = oopFactory::new_objArray(klass, size, CHECK);
  thread->set_vm_result(obj);
IRT_END


IRT_ENTRY(void, InterpreterRuntime::multianewarray(JavaThread* thread, jint* first_size_address))
  // We may want to pass in more arguments - could make this slightly faster
  constantPoolOop constants = method(thread)->constants();
  int          i = two_byte_index(thread);
  klassOop klass = constants->klass_at(i, CHECK);
  int   nof_dims = number_of_dimensions(thread);
  assert(oop(klass)->is_klass(), "not a class");
  assert(nof_dims >= 1, "multianewarray rank must be nonzero");
#ifdef _LP64
// In 64 bit mode, the sizes are stored in the top 32 bits of each 64 bit stack entry.  
// first_size_address is actually an intptr_t *
// We must create an array of jints to pass to multi_allocate.
  if ( nof_dims > 1 ) {
    int index;
    for ( index = 1; index < nof_dims; index++ ) {  // First size is ok
	first_size_address[-index] = first_size_address[-index*2];
    }
  }
#endif
  oop obj = arrayKlass::cast(klass)->
              multi_allocate(nof_dims, 
                             first_size_address, 
                             frame::interpreter_frame_expression_stack_direction(),
                             CHECK);
  thread->set_vm_result(obj);
IRT_END


// Quicken instance-of and check-cast bytecodes
IRT_ENTRY(void, InterpreterRuntime::quicken_io_cc(JavaThread* thread))
  // Force resolving; quicken the bytecode
  int which = two_byte_index(thread);
  constantPoolOop cpool = method(thread)->constants();
  // We'd expect to assert that we're only here to quicken bytecodes, but in a multithreaded
  // program we might have seen an unquick'd bytecode in the interpreter but have another
  // thread quicken the bytecode before we get here.
  // assert( cpool->tag_at(which).is_unresolved_klass(), "should only come here to quicken bytecodes" );
  klassOop klass = cpool->klass_at(which, CHECK);
  thread->set_vm_result(klass);
IRT_END


//------------------------------------------------------------------------------------------------------------------------
// Exceptions

#ifndef CORE
// Assume the compiler is (or will be) interested in this event.
// If necessary, create an MDO to hold the information, and record it.
void InterpreterRuntime::note_trap(JavaThread* thread, int reason, TRAPS) {
  assert(ProfileTraps, "call me only if profiling");
  methodHandle trap_method(thread, method(thread));
  if (trap_method.not_null()) {
    methodDataHandle trap_mdo(thread, trap_method->method_data());
    if (trap_mdo.is_null()) {
      methodOopDesc::build_interpreter_method_data(trap_method, THREAD);
      if (HAS_PENDING_EXCEPTION) {
        assert((PENDING_EXCEPTION->is_a(SystemDictionary::OutOfMemoryError_klass())), "we expect only an OOM error here");
        CLEAR_PENDING_EXCEPTION;
      }
      trap_mdo = trap_method->method_data();
      // and fall through...
    }
    if (trap_mdo.not_null()) {
      // Update per-method count of trap events.  The interpreter
      // is updating the MDO to simulate the effect of compiler traps.
      int trap_bci = trap_method->bci_from(bcp(thread));
      Deoptimization::update_method_data_from_interpreter(trap_mdo, trap_bci, reason);
    }
  }
}
#endif //CORE

// Special handling for stack overflow: since we don't have any (java) stack
// space left we use the pre-allocated & pre-initialized StackOverflowError
// klass to create an stack overflow error instance.  We do not call its
// constructor for the same reason (it is empty, anyway).

IRT_ENTRY(void, InterpreterRuntime::throw_StackOverflowError(JavaThread* thread))
  // get klass
  instanceKlass* klass = instanceKlass::cast(SystemDictionary::StackOverflowError_klass());
  assert(klass->is_initialized(), "StackOverflowError klass should have been initialized during VM initialization");
  // create instance - do not call constructor since we have no (java) stack space left
  oop exception_oop = klass->allocate_instance(CHECK);
  Handle exception (thread, exception_oop);
  if (StackTraceInThrowable) {
    java_lang_Throwable::fill_in_stack_trace(exception);
  }
  THROW_HANDLE(exception);
IRT_END


IRT_ENTRY(void, InterpreterRuntime::create_exception(JavaThread* thread, char* name, char* message))
  // lookup exception klass
  symbolHandle s = oopFactory::new_symbol_handle(name, CHECK);
  #ifndef CORE
  if (ProfileTraps) {
    if (s == vmSymbols::java_lang_ArithmeticException()) {
      note_trap(thread, Deoptimization::Reason_div0_check, CHECK);
    } else if (s == vmSymbols::java_lang_NullPointerException()) {
      note_trap(thread, Deoptimization::Reason_null_check, CHECK);
    }
  }
  #endif //CORE
  // create exception 
  Handle exception = Exceptions::new_exception(thread, s(), message);  
  thread->set_vm_result(exception());
IRT_END


IRT_ENTRY(void, InterpreterRuntime::create_klass_exception(JavaThread* thread, char* name, oop obj))
  ResourceMark rm(thread);
  const char* klass_name = Klass::cast(obj->klass())->external_name();
  // lookup exception klass
  symbolHandle s = oopFactory::new_symbol_handle(name, CHECK);
  #ifndef CORE
  if (ProfileTraps) {
    note_trap(thread, Deoptimization::Reason_class_check, CHECK);
  }
  #endif //CORE
  // create exception, with klass name as detail message
  Handle exception = Exceptions::new_exception(thread, s(), klass_name);
  thread->set_vm_result(exception());
IRT_END


IRT_ENTRY(void, InterpreterRuntime::throw_ArrayIndexOutOfBoundsException(JavaThread* thread, char* name, jint index))
  char message[jintAsStringSize];
  // lookup exception klass
  symbolHandle s = oopFactory::new_symbol_handle(name, CHECK);
  #ifndef CORE
  if (ProfileTraps) {
    note_trap(thread, Deoptimization::Reason_range_check, CHECK);
  }
  #endif //CORE
  // create exception 
  sprintf(message, "%d", index);
  THROW_MSG(s(), message);
IRT_END


// exception_handler_for_exception(...) returns the continuation address,
// the exception oop (via TLS) and sets the bci/bcp for the continuation.
// The exception oop is returned to make sure it is preserved over GC (it
// is only on the stack if the exception was thrown explicitly via athrow).
// During this operation, the expression stack contains the values for the
// bci where the exception happened. If the exception was propagated back
// from a call, the expression stack contains the values for the bci at the
// invoke w/o arguments (i.e., as if one were inside the call).
IRT_ENTRY(address, InterpreterRuntime::exception_handler_for_exception(JavaThread* thread, oop exception))

  Handle             h_exception(thread, exception);
  methodHandle       h_method   (thread, method(thread));
  constantPoolHandle h_constants(thread, h_method->constants());
  typeArrayHandle    h_extable  (thread, h_method->exception_table());
  bool               should_repeat;
  int                handler_bci;
  int                current_bci = bcp(thread) - h_method->code_base();
  
  // Need to do this check first since when _do_not_unlock_if_synchronized
  // is set, we don't want to trigger any classloading which may make calls
  // into java, or surprisingly find a matching exception handler for bci 0
  // since at this moment the method hasn't been "officially" entered yet.
  if (thread->do_not_unlock_if_synchronized()) {
    ResourceMark rm;
    assert(current_bci == 0,  "bci isn't zero for do_not_unlock_if_synchronized");
    thread->set_vm_result(exception);
#ifdef CC_INTERP
    return (address) -1;
#else
    return Interpreter::remove_activation_entry();
#endif
  }

  do {
    should_repeat = false;

    // assertions
#ifdef ASSERT
    assert(h_exception.not_null(), "NULL exceptions should be handled by athrow");
    assert(h_exception->is_oop(), "just checking");
    // Check that exception is a subclass of Throwable, otherwise we have a VerifyError
    if (!(h_exception->is_a(SystemDictionary::throwable_klass()))) {
      if (ExitVMOnVerifyError) vm_exit(-1);
      ShouldNotReachHere();
    }
#endif

    // tracing
    if (TraceExceptions) {
      ttyLocker ttyl;
      ResourceMark rm(thread);
      tty->print_cr("Exception <%s> (" INTPTR_FORMAT ")", h_exception->print_value_string(), h_exception());
      tty->print_cr(" thrown in interpreter method <%s>", h_method->print_value_string());
      tty->print_cr(" at bci %d for thread " INTPTR_FORMAT, current_bci, thread);
    }
// Don't go paging in something which won't be used.
//     else if (h_extable->length() == 0) {
//       // disabled for now - interpreter is not using shortcut yet
//       // (shortcut is not to call runtime if we have no exception handlers)
//       // warning("performance bug: should not call runtime if method has no exception handlers");
//     }
    // for AbortVMOnException flag
    NOT_PRODUCT(Exceptions::debug_check_abort(h_exception));

    // exception handler lookup
    KlassHandle h_klass(THREAD, h_exception->klass());
    handler_bci = h_method->fast_exception_handler_bci_for(h_klass, current_bci, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      // We threw an exception while trying to find the exception handler.
      // Transfer the new exception to the exception handle which will
      // be set into thread local storage, and do another lookup for an
      // exception handler for this exception, this time starting at the
      // BCI of the exception handler which caused the exception to be
      // thrown (bug 4307310).
      h_exception = Handle(THREAD, PENDING_EXCEPTION);
      CLEAR_PENDING_EXCEPTION;
      if (handler_bci >= 0) {
	current_bci = handler_bci;
	should_repeat = true;
      }
    }
  } while (should_repeat == true);

  // notify JVMTI of an exception throw; JVMTI will detect if this is a first 
  // time throw or a stack unwinding throw and accordingly notify the debugger
  if (JvmtiExport::can_post_exceptions()) {
    JvmtiExport::post_exception_throw(thread, h_method(), bcp(thread), h_exception());
  }

#ifdef CC_INTERP
  address continuation = (address) handler_bci;
#else
  address continuation = NULL;
#endif
  address handler_pc = NULL;
  if (handler_bci < 0 || !thread->reguard_stack((address) &continuation)) {
    // Forward exception to callee (leaving bci/bcp untouched) because (a) no
    // handler in this method, or (b) after a stack overflow there is not yet
    // enough stack space available to reprotect the stack.
#ifndef CC_INTERP
    continuation = Interpreter::remove_activation_entry();
#endif 
    // Count this for compilation purposes
    COMPILER2_ONLY(h_method->interpreter_throwout_increment());
  } else {
    // handler in this method => change bci/bcp to handler bci/bcp and continue there
    handler_pc = h_method->code_base() + handler_bci;
#ifndef CC_INTERP
    set_bcp_and_mdp(handler_pc, thread);
    continuation = Interpreter::dispatch_table(vtos)[*handler_pc];
#endif
  }
  // notify debugger of an exception catch 
  // (this is good for exceptions caught in native methods as well)
  if (JvmtiExport::can_post_exceptions()) {
    JvmtiExport::notice_unwind_due_to_exception(thread, h_method(), handler_pc, h_exception(), (handler_pc != NULL));
  }

  thread->set_vm_result(h_exception());
  return continuation;
IRT_END


IRT_ENTRY(void, InterpreterRuntime::throw_pending_exception(JavaThread* thread))  
  assert(thread->has_pending_exception(), "must only ne called if there's an exception pending");
  // nothing to do - eventually we should remove this code entirely (see comments @ call sites)
IRT_END


IRT_ENTRY(void, InterpreterRuntime::throw_AbstractMethodError(JavaThread* thread))          
  THROW(vmSymbols::java_lang_AbstractMethodError());
IRT_END


IRT_ENTRY(void, InterpreterRuntime::throw_IncompatibleClassChangeError(JavaThread* thread))          
  THROW(vmSymbols::java_lang_IncompatibleClassChangeError());
IRT_END


//------------------------------------------------------------------------------------------------------------------------
// Fields
//

IRT_ENTRY(void, InterpreterRuntime::resolve_get_put(JavaThread* thread, Bytecodes::Code bytecode))
  // resolve field
  FieldAccessInfo info;
  constantPoolHandle pool(thread, method(thread)->constants());
  bool is_static = (bytecode == Bytecodes::_getstatic || bytecode == Bytecodes::_putstatic);
  bool single_step_hidden = false;
  if (JvmtiExport::should_post_single_step()) {
    single_step_hidden = JvmtiExport::hide_single_stepping(thread);
  }
  LinkResolver::resolve_field(info, pool, two_byte_index(thread), bytecode, false, CHECK);
  if (single_step_hidden) {
    JvmtiExport::expose_single_stepping(thread);
  }  

  // check if link resolution caused cpCache to be updated
  if (already_resolved(thread)) return;

  // compute auxiliary field attributes
  TosState state  = as_TosState(info.field_type());

  // We need to delay resolving put instructions on final fields
  // until we actually invoke one. This is required so we throw
  // exceptions at the correct place. If we do not resolve completely
  // in the current pass, leaving the put_code set to zero will
  // cause the next put instruction to reresolve.
  bool is_put = (bytecode == Bytecodes::_putfield ||
                 bytecode == Bytecodes::_putstatic);
  Bytecodes::Code put_code = (Bytecodes::Code)0;

  // We also need to delay resolving getstatic instructions until the
  // class is intitialized.  This is required so that access to the static
  // field will call the initialization function every time until the class
  // is completely initialized ala. in 2.17.5 in JVM Specification.
  instanceKlass *klass = instanceKlass::cast(info.klass()->as_klassOop());
  bool uninitialized_static = ((bytecode == Bytecodes::_getstatic || bytecode == Bytecodes::_putstatic) &&
                               !klass->is_initialized());
  Bytecodes::Code get_code = (Bytecodes::Code)0;


  if (!uninitialized_static) {
    get_code = ((is_static) ? Bytecodes::_getstatic : Bytecodes::_getfield);
    if (is_put || !info.access_flags().is_final()) {
      put_code = ((is_static) ? Bytecodes::_putstatic : Bytecodes::_putfield);
    }
  }

  cache_entry(thread)->set_field(
    get_code,
    put_code,
    info.klass(),
    info.field_index(),
    info.field_offset(),
    state,
    info.access_flags().is_final(),
    info.access_flags().is_volatile()
  );
IRT_END


//------------------------------------------------------------------------------------------------------------------------
// Synchronization
//
// The interpreter's synchronization code is factored out so that it can
// be shared by method invocation and synchronized blocks.
//%note synchronization_3

static void trace_locking(Handle& h_locking_obj, bool is_locking) {
  NOT_CORE(ObjectSynchronizer::trace_locking(h_locking_obj, false, true, is_locking);)
}


//%note monitor_1
IRT_ENTRY_NO_ASYNC(void, InterpreterRuntime::monitorenter(JavaThread* thread, BasicObjectLock* elem))
#ifdef ASSERT
  thread->last_frame().interpreter_frame_verify_monitor(elem);
#endif
  Handle h_obj(thread, elem->obj());  
  assert(Universe::heap()->is_in_or_null(h_obj()), "must be NULL or an object");
  ObjectSynchronizer::slow_enter(h_obj, elem->lock(), CHECK);
  assert(Universe::heap()->is_in_or_null(elem->obj()), "must be NULL or an object");
#ifdef ASSERT
  thread->last_frame().interpreter_frame_verify_monitor(elem);
#endif
IRT_END


//%note monitor_1
IRT_ENTRY_NO_ASYNC(void, InterpreterRuntime::monitorexit(JavaThread* thread, BasicObjectLock* elem))
#ifdef ASSERT
  thread->last_frame().interpreter_frame_verify_monitor(elem);
#endif
  Handle h_obj(thread, elem->obj());  
  assert(Universe::heap()->is_in_or_null(h_obj()), "must be NULL or an object");
  if (elem == NULL || h_obj()->is_unlocked()) {
    THROW(vmSymbols::java_lang_IllegalMonitorStateException());
  }
  ObjectSynchronizer::slow_exit(h_obj(), elem->lock(), thread);
  // Free entry. This must be done here, since a pending exception might be installed on
  // exit. If it is not cleared, the exception handling code will try to unlock the monitor again.
  elem->set_obj(NULL); 
#ifdef ASSERT
  thread->last_frame().interpreter_frame_verify_monitor(elem);
#endif
IRT_END


IRT_ENTRY(void, InterpreterRuntime::throw_illegal_monitor_state_exception(JavaThread* thread))  
  THROW(vmSymbols::java_lang_IllegalMonitorStateException());
IRT_END


IRT_ENTRY(void, InterpreterRuntime::new_illegal_monitor_state_exception(JavaThread* thread))
  // Returns an illegal exception to install into the current thread. The pending_exception
  // flag is cleared so normal exception handling does not trigger. Any current installed
  // exception will be overwritten. This method will be called during an exception unwind.  
  assert(!HAS_PENDING_EXCEPTION, "no pending exception");
  Handle exception(thread, thread->vm_result());
  assert(exception() != NULL, "vm result should be set");
  thread->set_vm_result(NULL); // clear vm result before continuing (may cause memory leaks and assert failures)
  if (!exception->is_a(SystemDictionary::threaddeath_klass())) {        
    exception = Exceptions::new_exception(thread, 
                     vmSymbols::java_lang_IllegalMonitorStateException(),
                     NULL);
  }
  thread->set_vm_result(exception());
IRT_END


//------------------------------------------------------------------------------------------------------------------------
// Invokes

IRT_ENTRY(Bytecodes::Code, InterpreterRuntime::get_original_bytecode_at(JavaThread* thread, methodOop method, address bcp))
  return method->orig_bytecode_at(method->bci_from(bcp));
IRT_END

IRT_ENTRY(void, InterpreterRuntime::set_original_bytecode_at(JavaThread* thread, methodOop method, address bcp, Bytecodes::Code new_code))
  method->set_orig_bytecode_at(method->bci_from(bcp), new_code);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::_breakpoint(JavaThread* thread, methodOop method, address bcp))
  JvmtiExport::post_raw_breakpoint(thread, method, bcp);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::resolve_invoke(JavaThread* thread, Bytecodes::Code bytecode))  
  // extract receiver from the outgoing argument list if necessary
  Handle receiver(thread, NULL);  
  if (bytecode == Bytecodes::_invokevirtual || bytecode == Bytecodes::_invokeinterface) {
    ResourceMark rm(thread);
    methodHandle m (thread, method(thread));
    int bci = m->bci_from(bcp(thread));    
    Bytecode_invoke* call = Bytecode_invoke_at(m, bci);    
    symbolHandle signature (thread, call->signature());
    receiver = Handle(thread,
                  thread->last_frame().interpreter_callee_receiver(signature));
    assert(Universe::heap()->is_in_or_null(receiver()), "sanity check");    
    assert(receiver.is_null() || Universe::heap()->is_in(receiver->klass()), "sanity check");
  }  

  // resolve method
  CallInfo info;
  constantPoolHandle pool(thread, method(thread)->constants());

  bool single_step_hidden = false;
  if (JvmtiExport::should_post_single_step()) {
    single_step_hidden = JvmtiExport::hide_single_stepping(thread);
  }
  LinkResolver::resolve_invoke(info, receiver, pool, 
			       two_byte_index(thread), bytecode, CHECK);
  if (JvmtiExport::can_hotswap_or_post_breakpoint()) {
    int retry_count = 0;
    while (info.resolved_method()->is_old_version()) {
      // It is very unlikely that method is redefined more than 100 times
      // in the middle of resolve. If it is looping here more than 100 times 
      // means then there could be a bug here.
      guarantee((retry_count++ < 100),
                "Could not resolve to latest version of redefined method");
      // method is redefined in the middle of resolve so re-try.
      LinkResolver::resolve_invoke(info, receiver, pool, 
			           two_byte_index(thread), bytecode, CHECK);
    }
  }

  if (single_step_hidden) {
    JvmtiExport::expose_single_stepping(thread);
  }
  // check if link resolution caused cpCache to be updated
  if (already_resolved(thread)) return;

  if (bytecode == Bytecodes::_invokeinterface) {    

    if (TraceItables && Verbose) {
      ResourceMark rm(thread);
      tty->print_cr("Resolving: klass: %s to method: %s", info.resolved_klass()->name()->as_C_string(), info.resolved_method()->name()->as_C_string());
    }
    if (info.resolved_method()->method_holder() ==
                                            SystemDictionary::object_klass()) {
      // NOTE: THIS IS A FIX FOR A CORNER CASE in the JVM spec
      // (see also cpCacheOop.cpp for details)
      methodHandle rm = info.resolved_method();
      assert(rm->is_final() || info.has_vtable_index(),
             "should have been set already");
      cache_entry(thread)->set_method(bytecode, rm, info.vtable_index()); 
    } else {          
      // Setup itable entry      
      int index = klassItable::compute_itable_index(info.resolved_method()());
      cache_entry(thread)->set_interface_call(info.resolved_method(), index);
    }
  } else {    
    cache_entry(thread)->set_method(
      bytecode,
      info.resolved_method(),
      info.vtable_index());     
  }
IRT_END


#ifndef CORE
IRT_ENTRY_FOR_NMETHOD(address, 
  InterpreterRuntime::nmethod_entry_point(JavaThread* thread, 
                                          methodOop method, nmethod* nm))  
  methodHandle m(thread, method);
  { debug_only(nmethod* nm2 = m->code());
    // Note: nm2 could be null, if an uncommon trap for the method happened
    // right before we entered here (in another thread)
    // This assertion is only valid before the safepoint because the nmethod
    // could be made not entrant and replaced for the method.  The lock above
    // only prevents it from being flushed.
    assert(nm != NULL && (nm2 == NULL || nm == nm2), "nmethods must match");
  }

  // This is a particularly tricky entry point because the interpreter has
  // not set up a new frame and must do some tricks to get the environment
  // setup correct to make this call. Make it even more stressful by
  // triggering a safepoint with stress options.
  if (SafepointALot || ScavengeALot) {
      VM_ForceSafepoint vfs;
      VMThread::execute(&vfs);
  }

  // May block and sets the interpreter_entry_point as a side effect.
  address i2c_entry = nm->interpreter_entry_point();
  thread->set_vm_result(m());
  return i2c_entry;
IRT_END
#endif


//------------------------------------------------------------------------------------------------------------------------
// Miscellaneous


#ifndef CORE
#ifndef PRODUCT
static void trace_frequency_counter_overflow(methodHandle m, int branch_bci, int bci, address branch_bcp) {
  if (TraceInvocationCounterOverflow) {
    InvocationCounter* ic = m->invocation_counter();
    InvocationCounter* bc = m->backedge_counter();
    ResourceMark rm;
    const char* msg =
      branch_bcp == NULL
      ? "comp-policy cntr ovfl @ %d in entry of "
      : "comp-policy cntr ovfl @ %d in loop of ";
    tty->print(msg, bci);
    m->print_value();
    tty->cr();
    ic->print();
    bc->print();
    if (ProfileInterpreter) {
      if (branch_bcp != NULL) {
	methodDataOop mdo = m->method_data();
	if (mdo != NULL) {
	  int count = mdo->bci_to_data(branch_bci)->as_JumpData()->taken();
	  tty->print_cr("back branch count = %d", count);
	}
      }
    }
  }
}

static void trace_osr_request(methodHandle method, nmethod* osr, int bci) {
  if (TraceOnStackReplacement) {
    ResourceMark rm;
    tty->print(osr != NULL ? "Reused OSR entry for " : "Requesting OSR entry for ");
    method->print_short_name(tty);
    tty->print_cr(" at bci %d", bci);
  }    
}
#endif // !PRODUCT

IRT_ENTRY(InterpreterRuntime::IcoResult,
          InterpreterRuntime::frequency_counter_overflow(JavaThread* thread, address branch_bcp))
  // use UnlockFlagSaver to clear and restore the _do_not_unlock_if_synchronized
  // flag, in case this method triggers classloading which will call into Java.
  UnlockFlagSaver fs(thread);

  frame fr = thread->last_frame();
  assert(fr.is_interpreted_frame(), "must come from interpreter");
  methodHandle method(thread, fr.interpreter_frame_method());  
  const int branch_bci = branch_bcp != NULL ? method->bci_from(branch_bcp) : 0;
  const int bci = method->bci_from(fr.interpreter_frame_bcp());
  NOT_PRODUCT(trace_frequency_counter_overflow(method, branch_bci, bci, branch_bcp);)

  if (JvmtiExport::can_post_interpreter_events()) {
    if (thread->is_interp_only_mode()) {
      // If certain JVMTI events (e.g. frame pop event) are requested then the
      // thread is forced to remain in interpreted code. This is
      // implemented partly by a check in the run_compiled_code
      // section of the interpreter whether we should skip running
      // compiled code, and partly by skipping OSR compiles for
      // interpreted-only threads.
      if (branch_bcp != NULL) {
        CompilationPolicy::policy()->reset_counter_for_back_branch_event(method);
        return makeIcoResult(NULL);
      }
    }
  }

  if (branch_bcp == NULL) {
    // when code cache is full, compilation gets switched off, UseCompiler
    // is set to false
    if (!method->has_compiled_code() && UseCompiler) {
      CompilationPolicy::policy()->method_invocation_event(method, CHECK_0);
    } else {
      // Force counter overflow on method entry, even if no compilation
      // happened.  (The method_invocation_event call does this also.)
      CompilationPolicy::policy()->reset_counter_for_invocation_event(method);
    }
    nmethod* nm = method->code();
    return makeIcoResult(nm != NULL ? nm->verified_entry_point() : NULL);

  } else {
    // counter overflow in a loop => try to do on-stack-replacement
    nmethod* osr_nm = method->lookup_osr_nmethod_for(bci);
    NOT_PRODUCT(trace_osr_request(method, osr_nm, bci);)
    // when code cache is full, we should not compile any more...
    if (osr_nm == NULL && UseCompiler) {
      const int branch_bci = method->bci_from(branch_bcp);
      CompilationPolicy::policy()->method_back_branch_event(method, branch_bci, bci, CHECK_0);
      osr_nm = method->lookup_osr_nmethod_for(bci);
    }
    if (osr_nm == NULL) {
      CompilationPolicy::policy()->reset_counter_for_back_branch_event(method);
      return makeIcoResult((address)NULL);
    } else {
      // continue w/ on-stack-replacement code
      OSRAdapter* osr_return_adapter = OnStackReplacement::get_osr_adapter(fr, method);
      // return 2 values in registers
      return makeIcoResult(osr_return_adapter->entry_point(method->is_returning_fp()), osr_nm);
    }
  }
IRT_END

IRT_LEAF(jint, InterpreterRuntime::bcp_to_di(methodOop method, address cur_bcp))
  assert(ProfileInterpreter, "must be profiling interpreter");
  int bci = method->bci_from(cur_bcp);
  methodDataOop mdo = method->method_data();
  if (mdo == NULL)  return 0;
  return mdo->bci_to_di(bci);
IRT_END

IRT_ENTRY(jint, InterpreterRuntime::profile_method(JavaThread* thread, address cur_bcp))
  // use UnlockFlagSaver to clear and restore the _do_not_unlock_if_synchronized
  // flag, in case this method triggers classloading which will call into Java.
  UnlockFlagSaver fs(thread);

  assert(ProfileInterpreter, "must be profiling interpreter");
  frame fr = thread->last_frame();
  assert(fr.is_interpreted_frame(), "must come from interpreter");
  methodHandle method(thread, fr.interpreter_frame_method());
  int bci = method->bci_from(cur_bcp);
  methodOopDesc::build_interpreter_method_data(method, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    assert((PENDING_EXCEPTION->is_a(SystemDictionary::OutOfMemoryError_klass())), "we expect only an OOM error here");
    CLEAR_PENDING_EXCEPTION;
    // and fall through...
  }
  methodDataOop mdo = method->method_data();
  if (mdo == NULL)  return 0;
  return mdo->bci_to_di(bci);
IRT_END


#ifdef ASSERT
IRT_LEAF(void, InterpreterRuntime::verify_mdp(methodOop method, address bcp, address mdp))
  assert(ProfileInterpreter, "must be profiling interpreter");

  methodDataOop mdo = method->method_data();
  assert(mdo != NULL, "must not be null");

  int bci = method->bci_from(bcp);

  address mdp2 = mdo->bci_to_dp(bci);
  if (mdp != mdp2) {
    ResourceMark rm;
    ResetNoHandleMark rnm; // In a LEAF entry.
    HandleMark hm;
    tty->print_cr("FAILED verify : actual mdp %p   expected mdp %p @ bci %d", mdp, mdp2, bci);
    int current_di = mdo->dp_to_di(mdp);
    int expected_di  = mdo->dp_to_di(mdp2);
    tty->print_cr("  actual di %d   expected di %d", current_di, expected_di);
    int expected_approx_bci = mdo->data_at(expected_di)->bci();
    int approx_bci = -1;
    if (current_di >= 0) {
      approx_bci = mdo->data_at(current_di)->bci();
    }
    tty->print_cr("  actual bci is %d  expected bci %d", approx_bci, expected_approx_bci);
    mdo->print_on(tty);
    method->print_codes();
  }
  assert(mdp == mdp2, "wrong mdp");
IRT_END
#endif // ASSERT

IRT_ENTRY(void, InterpreterRuntime::update_mdp_for_ret(JavaThread* thread, int return_bci))
  assert(ProfileInterpreter, "must be profiling interpreter");
  ResourceMark rm(thread);
  HandleMark hm(thread);
  frame fr = thread->last_frame();
  assert(fr.is_interpreted_frame(), "must come from interpreter");
  methodDataHandle h_mdo(thread, fr.interpreter_frame_method()->method_data());

  // Grab a lock to ensure atomic access to setting the return bci and
  // the displacement.  This can block and GC, invalidating all naked oops.
  MutexLocker ml(RetData_lock);

  // ProfileData is essentially a wrapper around a derived oop, so we
  // need to take the lock before making any ProfileData structures.
  ProfileData* data = h_mdo->data_at(h_mdo->dp_to_di(fr.interpreter_frame_mdp()));
  RetData* rdata = data->as_RetData();
  address new_mdp = rdata->fixup_ret(return_bci, h_mdo);
  fr.interpreter_frame_set_mdp(new_mdp);
IRT_END

#endif // CORE


IRT_ENTRY(void, InterpreterRuntime::at_safepoint(JavaThread* thread))
  // We used to need an explict preserve_arguments here for invoke bytecodes. However,
  // stack traversal automatically takes care of preserving arguments for invoke, so
  // this is no longer needed.

  // IRT_END does an implicit safepoint check, hence we are guaranteed to block
  // if this is called during a safepoint

  if (JvmtiExport::should_post_single_step()) {
    // We are called during regular safepoints and when the VM is
    // single stepping. If any thread is marked for single stepping,
    // then we may have JVMTI work to do.
    JvmtiExport::at_single_stepping_point(thread, method(thread), bcp(thread));
  }
IRT_END

IRT_ENTRY(void, InterpreterRuntime::post_field_access(JavaThread *thread, oop obj,
ConstantPoolCacheEntry *cp_entry))

  // check the access_flags for the field in the klass
  instanceKlass* ik = instanceKlass::cast((klassOop)cp_entry->f1());
  typeArrayOop fields = ik->fields();
  int index = cp_entry->holder_index();
  assert(index < fields->length(), "holder index is out of range");
  // bail out if field accesses are not watched
  if ((fields->ushort_at(index) & JVM_ACC_FIELD_ACCESS_WATCHED) == 0) return;

  switch(cp_entry->flag_state()) {
    case btos:    // fall through
    case ctos:    // fall through
    case stos:    // fall through
    case itos:    // fall through
    case ftos:    // fall through
    case ltos:    // fall through
    case dtos:    // fall through
    case atos: break;
    default: ShouldNotReachHere(); return;
  }
  bool is_static = (obj == NULL);
  HandleMark hm(thread);

  Handle h_obj;
  if (!is_static) {
    // non-static field accessors have an object, but we need a handle
    h_obj = Handle(thread, obj);
  }
  instanceKlassHandle h_cp_entry_f1(thread, (klassOop)cp_entry->f1());
  jfieldID fid = jfieldIDWorkaround::to_jfieldID(h_cp_entry_f1, cp_entry->f2(), is_static);
  JvmtiExport::post_field_access(thread, method(thread), bcp(thread), h_cp_entry_f1, h_obj, fid);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::post_field_modification(JavaThread *thread,
  oop obj, ConstantPoolCacheEntry *cp_entry, jvalue *value))

  klassOop k = (klassOop)cp_entry->f1();

  // check the access_flags for the field in the klass
  instanceKlass* ik = instanceKlass::cast(k);
  typeArrayOop fields = ik->fields();
  int index = cp_entry->holder_index();
  assert(index < fields->length(), "holder index is out of range");
  // bail out if field modifications are not watched
  if ((fields->ushort_at(index) & JVM_ACC_FIELD_MODIFICATION_WATCHED) == 0) return;

  char   sig_type = NULL;

  switch(cp_entry->flag_state()) {
    case btos: sig_type = 'Z'; break;
    case ctos: sig_type = 'C'; break;
    case stos: sig_type = 'S'; break;
    case itos: sig_type = 'I'; break;
    case ftos: sig_type = 'F'; break;
    case ltos: sig_type = 'J'; break;
    case dtos: sig_type = 'D'; break;
    case atos: sig_type = 'L'; break;
    default:  ShouldNotReachHere(); return;
  }
  bool is_static = (obj == NULL);

  HandleMark hm(thread);
  instanceKlassHandle h_klass(thread, k);
  jfieldID fid = jfieldIDWorkaround::to_jfieldID(h_klass, cp_entry->f2(), is_static);
  jvalue fvalue = *value;

  Handle h_obj;
  if (!is_static) {
    // non-static field accessors have an object, but we need a handle
    h_obj = Handle(thread, obj);
 } 

  JvmtiExport::post_raw_field_modification(thread, method(thread), bcp(thread), h_klass, h_obj,
                                       fid, sig_type, &fvalue);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::post_method_entry(JavaThread *thread))
  JvmtiExport::post_method_entry(thread, InterpreterRuntime::method(thread), InterpreterRuntime::last_frame(thread));
IRT_END


IRT_ENTRY(void, InterpreterRuntime::post_method_exit(JavaThread *thread))
  JvmtiExport::post_method_exit(thread, InterpreterRuntime::method(thread), InterpreterRuntime::last_frame(thread));
IRT_END

IRT_LEAF(int, InterpreterRuntime::interpreter_contains(address pc))
{
  return (Interpreter::contains(pc) ? 1 : 0);
}
IRT_END


// Implementation of SignatureHandlerLibrary

address SignatureHandlerLibrary::set_handler_blob() {
  BufferBlob* handler_blob = BufferBlob::create("native signature handlers", blob_size);
  if (handler_blob == NULL) {
    return NULL;
  }
  address handler = handler_blob->instructions_begin();
  _handler_blob = handler_blob;
  _handler = handler;
  return handler;
}

void SignatureHandlerLibrary::initialize() {
  if (_fingerprints != NULL) {
    return;
  }
  if (set_handler_blob() == NULL) {
    vm_exit_out_of_memory(blob_size, "native signature handlers");
  }
  _fingerprints = new(ResourceObj::C_HEAP)GrowableArray<uint64_t>(32, true);
  _handlers     = new(ResourceObj::C_HEAP)GrowableArray<address>(32, true);
}

address SignatureHandlerLibrary::set_handler(CodeBuffer* buffer) {
  address handler   = _handler;
  int     code_size = buffer->code_size();
  if (handler + code_size > _handler_blob->instructions_end()) {
    // get a new handler blob
    handler = set_handler_blob();
  }
  if (handler != NULL) {
    memcpy(handler, buffer->code_begin(), code_size);
    pd_set_handler(handler);
    ICache::invalidate_range(handler, code_size);
    _handler = handler + code_size;
  }
  return handler;
}

void SignatureHandlerLibrary::add(methodHandle method) {
  if (method->signature_handler() == NULL) {
    // use slow signature handler if we can't do better
    int handler_index = -1;
    // check if we can use customized (fast) signature handler
    if (UseFastSignatureHandlers && method->size_of_parameters() <= Fingerprinter::max_size_of_parameters) {
      // use customized signature handler
      MutexLocker mu(SignatureHandlerLibrary_lock);
      // make sure data structure is initialized
      initialize();
      // lookup method signature's fingerprint
      uint64_t fingerprint = Fingerprinter(method).fingerprint();
      handler_index = _fingerprints->find(fingerprint);
      // create handler if necessary
      if (handler_index < 0) {
	ResourceMark rm;
	ptrdiff_t align_offset = (address)
	  round_to((intptr_t)_buffer, CodeEntryAlignment) - (address)_buffer;
	CodeBuffer* buffer = new CodeBuffer((address)(_buffer + align_offset),
					    SignatureHandlerLibrary::buffer_size - align_offset);
	InterpreterRuntime::SignatureHandlerGenerator(method, buffer).generate(fingerprint);
	// copy into code heap
	address handler = set_handler(buffer);
        if (handler == NULL) {
          // use slow signature handler
	} else {
          // debugging suppport
          if (PrintSignatureHandlers) {
	    tty->cr();
	    tty->print_cr("argument handler #%d for: %s %s (fingerprint = " UINT64_FORMAT ", %d bytes generated)",
			  _handlers->length(),
			  (method->is_static() ? "static" : "receiver"),
			  method->signature()->as_C_string(),
			  fingerprint,
			  buffer->code_size());
	    Disassembler::decode(handler, handler + buffer->code_size());
#ifndef PRODUCT
	    tty->print_cr(" --- associated result handler ---");
	    address rh_begin = AbstractInterpreter::result_handler(method()->result_type());
	    address rh_end = rh_begin;
	    while (*(int*)rh_end != 0) {
	      rh_end += sizeof(int);
	    }
	    Disassembler::decode(rh_begin, rh_end);
#endif
	  }
	  // add handler to library
	  _fingerprints->append(fingerprint);
	  _handlers->append(handler);
	  // set handler index
	  assert(_fingerprints->length() == _handlers->length(), "sanity check");
	  handler_index = _fingerprints->length() - 1;
	}
      }
    }
    if (handler_index < 0) {
      // use generic signature handler
      method->set_signature_handler(AbstractInterpreter::slow_signature_handler());
    } else {
      // set handler
      method->set_signature_handler(_handlers->at(handler_index));
    }
  }
  assert(method->signature_handler() == AbstractInterpreter::slow_signature_handler() ||
	 _handlers->find(method->signature_handler()) == _fingerprints->find(Fingerprinter(method).fingerprint()),
	 "sanity check");
}


BufferBlob*              SignatureHandlerLibrary::_handler_blob = NULL;
address                  SignatureHandlerLibrary::_handler      = NULL;
GrowableArray<uint64_t>* SignatureHandlerLibrary::_fingerprints = NULL;
GrowableArray<address>*  SignatureHandlerLibrary::_handlers     = NULL;
u_char                   SignatureHandlerLibrary::_buffer[SignatureHandlerLibrary::buffer_size];


IRT_ENTRY(void, InterpreterRuntime::prepare_native_call(JavaThread* thread, methodOop method))
  methodHandle m(thread, method);
  assert(m->is_native(), "sanity check");
  // lookup native function entry point if it doesn't exist
  bool in_base_library;
  if (!m->has_native_function()) {
    NativeLookup::lookup(m, in_base_library, CHECK);
  }
  // make sure signature handler is installed
  SignatureHandlerLibrary::add(m);
  // The interpreter entry point checks the signature handler first,
  // before trying to fetch the native entry point and klass mirror.
  // We must set the signature handler last, so that multiple processors
  // preparing the same method will be sure to see non-null entry & mirror.
IRT_END
