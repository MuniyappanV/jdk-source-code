#ifndef CINTERPRETERBODY_ONCE
#define CINTERPERTERBODY_ONCE 
#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)cInterpretMethod.hpp	1.47 04/03/17 13:37:44 JVM"

#endif
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 *
 * This code was converted from CVM sources to C++ and the Hotspot VM
 *
 */

#ifdef CC_INTERP

/*
 * USELABELS - If using GCC, then use labels for the opcode dispatching
 * rather -then a switch statement. This improves performance because it
 * gives us the oportunity to have the instructions that calculate the
 * next opcode to jump to be intermixed with the rest of the instructions
 * that implement the opcode (see UPDATE_PC_AND_TOS_AND_CONTINUE macro).
 */
#undef USELABELS
#ifdef __GNUC__
/* 
   ASSERT signifies debugging. It is much easier to step thru bytecodes if we
   don't use the computed goto approach.
*/
#ifndef ASSERT
#define USELABELS
#endif
#endif

#undef CASE
#ifdef USELABELS
#define CASE(opcode) opc ## opcode
#define DEFAULT opc_default
#else
#define CASE(opcode) case Bytecodes:: ## opcode
#define DEFAULT default
#endif

/*
 * PREFETCH_OPCCODE - Some compilers do better if you prefetch the next 
 * opcode before going back to the top of the while loop, rather then having
 * the top of the while loop handle it. This provides a better opportunity
 * for instruction scheduling. Some compilers just do this prefetch
 * automatically. Some actually end up with worse performance if you
 * force the prefetch. Solaris gcc seems to do better, but cc does worse.
 */
#undef PREFETCH_OPCCODE
#define PREFETCH_OPCCODE

/*
  Interpreter safepoint: it is expected that the interpreter will have no live
  handles of its own creation live at an interpreter safepoint. Therefore we
  run a HandleMarkCleaner and trash all handles allocated in the call chain
  since the JavaCalls::call_helper invocation that initiated the chain.
  There really shouldn't be any handles remaining to trash but this is cheap
  in relation to a safepoint.
*/
#define SAFEPOINT                                                                 \
    if ( SafepointSynchronize::is_synchronizing()) {                              \
	{                                                                         \
	  /* zap freed handles rather than GC'ing them */                         \
	  HandleMarkCleaner __hmc(THREAD);                                        \
	}                                                                         \
	CALL_VM(SafepointSynchronize::block(THREAD), handle_exception);           \
    }

/*
 * VM_JAVA_ERROR - Macro for throwing a java exception from
 * the interpreter loop. Should really be a CALL_VM but there
 * is no entry point to do the transition to vm so we just
 * do it by hand here.
 */
#define VM_JAVA_ERROR_NO_JUMP(name, msg)                                                  \
    DECACHE_STATE();                                                              \
    SET_LAST_JAVA_FRAME();                                                        \
    {                                                                             \
       ThreadInVMfromJava trans(THREAD);                                          \
       Exceptions::_throw_msg(THREAD, __FILE__, __LINE__, name, msg, NULL, NULL); \
    }                                                                             \
    RESET_LAST_JAVA_FRAME();                                                      \
    CACHE_STATE();                                                                

// Normal throw of a java error
#define VM_JAVA_ERROR(name, msg)                                                  \
    VM_JAVA_ERROR_NO_JUMP(name, msg)                                              \
    goto handle_exception;

#ifdef PRODUCT
#define DO_UPDATE_INSTRUCTION_COUNT(opcode)
#else
#define DO_UPDATE_INSTRUCTION_COUNT(opcode)                                                          \
{                                                                                                    \
    BytecodeCounter::_counter_value++;                                                               \
    BytecodeHistogram::_counters[(Bytecodes::Code)opcode]++;                                         \
    if (StopInterpreterAt && StopInterpreterAt == BytecodeCounter::_counter_value) os::breakpoint(); \
    if (TraceBytecodes) {                                                                            \
      CALL_VM((void)SharedRuntime::trace_bytecode(THREAD, 0, 0, 0), handle_exception);               \
    }                                                                                                \
}
#endif

#undef DEBUGGER_SINGLE_STEP_NOTIFY
#ifdef VM_JVMTI
/* NOTE: (kbr) This macro must be called AFTER the PC has been
   incremented. JvmtiExport::at_single_stepping_point() may cause a
   breakpoint opcode to get inserted at the current PC to allow the
   debugger to coalesce single-step events.
   
   As a result if we call at_single_stepping_point() we refetch opcode
   to get the current opcode. This will override any other prefetching
   that might have occurred.
*/
#define DEBUGGER_SINGLE_STEP_NOTIFY()                                            \
{                                                                                \
      if (_jvmti_interp_events) {                                                \
        if (JvmtiExport::should_post_single_step()) {                            \
	  DECACHE_STATE();                                                       \
	  SET_LAST_JAVA_FRAME();                                                 \
	  ThreadInVMfromJava trans(THREAD);                                      \
	  JvmtiExport::at_single_stepping_point(THREAD,                          \
					  istate->method(),                      \
					  pc);                                   \
	  RESET_LAST_JAVA_FRAME();                                               \
	  CACHE_STATE();                                                         \
	  if (THREAD->pop_frame_pending() &&                                     \
	      !THREAD->pop_frame_in_process()) {                                 \
	    goto handle_Pop_Frame;                                               \
	  }                                                                      \
          opcode = *pc;                                                          \
	}                                                                        \
      }                                                                          \
}
#else
#define DEBUGGER_SINGLE_STEP_NOTIFY() 
#endif

/*
 * CONTINUE - Macro for executing the next opcode.
 */
#undef CONTINUE
#ifdef USELABELS
// Have to do this dispatch this way in C++ because otherwise gcc complains about crossing an
// initialization (which is is the initialization of the table pointer...)
#define DISPATCH(opcode) goto *dispatch_table[opcode]
#define CONTINUE {                              \
        opcode = *pc;                           \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);    \
        DEBUGGER_SINGLE_STEP_NOTIFY();          \
        DISPATCH(opcode);                       \
    }
#else
#ifdef PREFETCH_OPCCODE
#define CONTINUE {                              \
        opcode = *pc;                           \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);    \
        DEBUGGER_SINGLE_STEP_NOTIFY();          \
        continue;                               \
    }
#else
#define CONTINUE {                              \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);    \
        DEBUGGER_SINGLE_STEP_NOTIFY();          \
        continue;                               \
    }
#endif
#endif

// JavaStack Implementation
#define MORE_STACK(count) (topOfStack -= (count))


#define UPDATE_PC(opsize) {pc += opsize; }
/*
 * UPDATE_PC_AND_TOS - Macro for updating the pc and topOfStack.
 */
#undef UPDATE_PC_AND_TOS
#define UPDATE_PC_AND_TOS(opsize, stack) \
    {pc += opsize; MORE_STACK(stack); }

/*
 * UPDATE_PC_AND_TOS_AND_CONTINUE - Macro for updating the pc and topOfStack,
 * and executing the next opcode. It's somewhat similar to the combination
 * of UPDATE_PC_AND_TOS and CONTINUE, but with some minor optimizations.
 */
#undef UPDATE_PC_AND_TOS_AND_CONTINUE
#ifdef USELABELS
#define UPDATE_PC_AND_TOS_AND_CONTINUE(opsize, stack) {         \
        pc += opsize; opcode = *pc; MORE_STACK(stack);          \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);                    \
        DEBUGGER_SINGLE_STEP_NOTIFY();                          \
        DISPATCH(opcode);                                       \
    }

#define UPDATE_PC_AND_CONTINUE(opsize) {                        \
        pc += opsize; opcode = *pc;                             \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);                    \
        DEBUGGER_SINGLE_STEP_NOTIFY();                          \
        DISPATCH(opcode);                                       \
    }
#else
#ifdef PREFETCH_OPCCODE
#define UPDATE_PC_AND_TOS_AND_CONTINUE(opsize, stack) {         \
        pc += opsize; opcode = *pc; MORE_STACK(stack);          \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);                    \
        DEBUGGER_SINGLE_STEP_NOTIFY();                          \
        goto do_continue;                                       \
    }

#define UPDATE_PC_AND_CONTINUE(opsize) {                        \
        pc += opsize; opcode = *pc;                             \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);                    \
        DEBUGGER_SINGLE_STEP_NOTIFY();                          \
        goto do_continue;                                       \
    }
#else
#define UPDATE_PC_AND_TOS_AND_CONTINUE(opsize, stack) { \
        pc += opsize; MORE_STACK(stack);                \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);            \
        DEBUGGER_SINGLE_STEP_NOTIFY();                  \
        goto do_continue;                               \
    }

#define UPDATE_PC_AND_CONTINUE(opsize) {                \
        pc += opsize;                                   \
        DO_UPDATE_INSTRUCTION_COUNT(opcode);            \
        DEBUGGER_SINGLE_STEP_NOTIFY();                  \
        goto do_continue;                               \
    }
#endif /* PREFETCH_OPCCODE */
#endif /* USELABELS */

// About to call a new method, update the save the adjusted pc and return to frame manager
#define UPDATE_PC_AND_RETURN(opsize)  \
   DECACHE_TOS();                     \
   istate->set_bcp(pc+opsize);        \
   return;

#ifdef CORE
#define INCR_INVOCATION_COUNT 
#else
#define INCR_INVOCATION_COUNT istate->method()->invocation_counter()->increment()
#endif
/*
 * For those opcodes that need to have a GC point on a backwards branch
 */
#undef UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK
#define UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, stack) { \
    if ((skip) <= 0) {                                                     \
	/* QQQ This should be much better */                               \
	INCR_INVOCATION_COUNT;                                             \
	SAFEPOINT;                                                         \
    }                                                                      \
    UPDATE_PC_AND_TOS_AND_CONTINUE((skip), stack);                         \
}

#undef UPDATE_PC_AND_CONTINUE_WITH_BACKWARDS_CHECK
#define UPDATE_PC_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip) {                \
    if ((skip) <= 0) {                                                     \
	/* QQQ This should be much better */                               \
	INCR_INVOCATION_COUNT;                                             \
	SAFEPOINT;                                                         \
    }                                                                      \
    UPDATE_PC_AND_CONTINUE((skip));                                        \
}

/*
 * Macros for accessing the stack.
 */
#undef STACK_INFO
#undef STACK_INT
#undef STACK_FLOAT
#undef STACK_ICELL
#undef STACK_ADDR
#undef STACK_OBJECT
#undef STACK_DOUBLE
#undef STACK_LONG
// JavaStack Implementation


#define GET_STACK_SLOT(offset)    (*((intptr_t*) &topOfStack[-(offset)]))
#define STACK_SLOT(offset)    ((address) &topOfStack[-(offset)])
#define STACK_ADDR(offset)    (*((address *) &topOfStack[-(offset)]))
#define STACK_INT(offset)     (*((jint*) &topOfStack[-(offset)]))
#define STACK_FLOAT(offset)   (*((jfloat *) &topOfStack[-(offset)]))
#define STACK_OBJECT(offset)  (*((oop *) &topOfStack [-(offset)]))
#define STACK_DOUBLE(offset)  (((VMJavaVal64*) &topOfStack[-(offset)])->d)
#define STACK_LONG(offset)    (((VMJavaVal64 *) &topOfStack[-(offset)])->l)

#define SET_STACK_SLOT(value, offset)   (*(intptr_t*)&topOfStack[-(offset)] = *(intptr_t*)(value))
#define SET_STACK_ADDR(value, offset)   (*((address *)&topOfStack[-(offset)]) = (value))
#define SET_STACK_INT(value, offset)    (*((jint *)&topOfStack[-(offset)]) = (value))
#define SET_STACK_FLOAT(value, offset)  (*((jfloat *)&topOfStack[-(offset)]) = (value))
#define SET_STACK_OBJECT(value, offset) (*((oop *)&topOfStack[-(offset)]) = (value))
#define SET_STACK_DOUBLE(value, offset) (((VMJavaVal64*)&topOfStack[-(offset)])->d = (value))
#define SET_STACK_DOUBLE_FROM_ADDR(addr, offset) (((VMJavaVal64*)&topOfStack[-(offset)])->d =  \
                                                 ((VMJavaVal64*)(addr))->d)
#define SET_STACK_LONG(value, offset)   (((VMJavaVal64*)&topOfStack[-(offset)])->l = (value))
#define SET_STACK_LONG_FROM_ADDR(addr, offset)   (((VMJavaVal64*)&topOfStack[-(offset)])->l =  \
                                                 ((VMJavaVal64*)(addr))->l)

#define LOCALS_SLOT(offset)    ((intptr_t*)&locals[-(offset)])
#define LOCALS_ADDR(offset)    ((address)locals[-(offset)])
#define LOCALS_INT(offset)     ((jint)(locals[-(offset)]))
#define LOCALS_FLOAT(offset)   (*((jfloat*)&locals[-(offset)]))
#define LOCALS_OBJECT(offset)  ((oop)locals[-(offset)])
#define LOCALS_DOUBLE(offset)  (((VMJavaVal64*)&locals[-((offset) + 1)])->d)
#define LOCALS_LONG(offset)    (((VMJavaVal64*)&locals[-((offset) + 1)])->l)
#define LOCALS_LONG_AT(offset) (((address)&locals[-((offset) + 1)]))
#define LOCALS_DOUBLE_AT(offset) (((address)&locals[-((offset) + 1)]))

#define SET_LOCALS_SLOT(value, offset)    (*(intptr_t*)&locals[-(offset)] = *(intptr_t *)(value))
#define SET_LOCALS_ADDR(value, offset)    (*((address *)&locals[-(offset)]) = (value))
#define SET_LOCALS_INT(value, offset)     (*((jint *)&locals[-(offset)]) = (value))
#define SET_LOCALS_FLOAT(value, offset)   (*((jfloat *)&locals[-(offset)]) = (value))
#define SET_LOCALS_OBJECT(value, offset)  (*((oop *)&locals[-(offset)]) = (value))
#define SET_LOCALS_DOUBLE(value, offset)  (((VMJavaVal64*)&locals[-((offset)+1)])->d = (value))
#define SET_LOCALS_LONG(value, offset)	  (((VMJavaVal64*)&locals[-((offset)+1)])->l = (value))
#define SET_LOCALS_DOUBLE_FROM_ADDR(addr, offset) (((VMJavaVal64*)&locals[-((offset)+1)])->d = \
                                                  ((VMJavaVal64*)(addr))->d)
#define SET_LOCALS_LONG_FROM_ADDR(addr, offset) (((VMJavaVal64*)&locals[-((offset)+1)])->l = \
                                                ((VMJavaVal64*)(addr))->l)


/*
 * Macros for caching and flushing the interpreter state. Some local
 * variables need to be flushed out to the frame before we do certain
 * things (like pushing frames or becomming gc safe) and some need to 
 * be recached later (like after popping a frame). We could use one
 * macro to cache or decache everything, but this would be less then
 * optimal because we don't always need to cache or decache everything
 * because some things we know are already cached or decached.
 */
#undef DECACHE_TOS
#undef CACHE_TOS
#undef CACHE_PREV_TOS
#define DECACHE_TOS()    istate->set_stack(topOfStack);

#define CACHE_TOS()      topOfStack = (intptr_t *)istate->stack();

#undef DECACHE_PC
#undef CACHE_PC
#define DECACHE_PC()    istate->set_bcp(pc);
#define CACHE_PC()      pc = istate->bcp();
#define CACHE_CP()      cp = istate->constants();
#define CACHE_LOCALS()  locals = istate->locals();
#undef CACHE_FRAME
#define CACHE_FRAME()   
 
/*
 * CHECK_NULL - Macro for throwing a NullPointerException if the object
 * passed is a null ref.
 * On some architectures/platforms it should be possible to do this implicitly
 */
#undef CHECK_NULL
#define CHECK_NULL(obj_)                                                 \
    if ((obj_) == 0) {                                                   \
        VM_JAVA_ERROR(vmSymbols::java_lang_NullPointerException(), "");  \
    }

// QQQ
#define VMdoubleConstZero() 0.0
#define VMdoubleConstOne() 1.0
#define VMlongConstZero() (max_jlong-max_jlong)
#define VMlongConstOne() ((max_jlong-max_jlong)+1)

/*
 * Alignment
 */
/* #define VMalignWordUp(val)          (((juint)(val) + 3) & ~3) */
#define VMalignWordUp(val)          (((uintptr_t)(val) + 3) & ~3)

// Decache the interpreter state that interpreter modifies directly (i.e. GC is indirect mod)
#define DECACHE_STATE() DECACHE_PC(); DECACHE_TOS();

// Reload interpreter state after calling the VM or a possible GC
#define CACHE_STATE()   \
        CACHE_TOS();    \
	CACHE_PC();     \
	CACHE_CP();     \
	CACHE_LOCALS();

// Call the VM don't check for pending exceptions
#define CALL_VM_NOCHECK(func)                                     \
          DECACHE_STATE();                                        \
	  SET_LAST_JAVA_FRAME();                                  \
          func;                                                   \
	  RESET_LAST_JAVA_FRAME();                                \
	  CACHE_STATE();                                          \
	  if (THREAD->pop_frame_pending() &&                      \
	      !THREAD->pop_frame_in_process()) {                  \
	    goto handle_Pop_Frame;                                \
	  }

// Call the VM and check for pending exceptions
#define CALL_VM(func, label) {                                    \
          CALL_VM_NOCHECK(func);                                  \
	  if (THREAD->pending_exception()) goto label;            \
        }

/*
 * cInterpreter::InterpretMethod(interpreterState istate)
 * cInterpreter::InterpretMethodWithChecks(interpreterState istate)
 *
 * The real deal. This is where byte codes actually get interpreted.
 * Basically it's a big while loop that iterates until we return from
 * the method passed in.
 *
 * The InterpretMethodWithChecks is used if JVMTI or JVMPI are enabled.
 *
 */
#if defined(VM_JVMTI) || defined(VM_JVMPI)
void
cInterpreter::InterpretMethodWithChecks(interpreterState istate) {
#else
void
cInterpreter::InterpretMethod(interpreterState istate) {
#endif

  // In order to simplify some tests based on switches set at runtime
  // we invoke the interpreter a single time after switches are enabled 
  // and set simpler to to test variables rather than method calls or complex
  // boolean expressions.

  static int initialized = 0;
#ifdef VM_JVMTI
  static bool _jvmti_interp_events = 0;
#endif

#ifndef CORE
  static int _compiling;  // (UseCompiler || CountCompiledCalls)
#endif

#ifdef ASSERT
  // Verify linkages.
  interpreterState l = istate;
  do {
    assert(l == l->_self_link, "bad link");
    l = l->_prev_link;
  } while (l != NULL);
  // Screwups with stack management usually cause us to overwrite istate
  // save a copy so we can verify it.
  interpreterState orig = istate;
#endif

  static volatile jbyte* _byte_map_base; // adjusted card table base for oop store barrier

  register intptr_t*        topOfStack = (intptr_t *)istate->stack(); /* access with STACK macros */
  register address          pc = istate->bcp();
  register jubyte opcode;
  register intptr_t*        locals = istate->locals();
  register constantPoolCacheOop  cp = istate->constants(); // method()->constants()->cache()
#ifdef LOTS_OF_REGS
  register JavaThread*      THREAD = istate->thread();
  register volatile jbyte*  BYTE_MAP_BASE = _byte_map_base;
#else
#undef THREAD
#define THREAD istate->thread()
#undef BYTE_MAP_BASE
#define BYTE_MAP_BASE _byte_map_base
#endif

#ifdef USELABELS
  const static void* const opclabels_data[256] = { 
/* 0x00 */ &&opc_nop,     &&opc_aconst_null,&&opc_iconst_m1,&&opc_iconst_0,
/* 0x04 */ &&opc_iconst_1,&&opc_iconst_2,   &&opc_iconst_3, &&opc_iconst_4,
/* 0x08 */ &&opc_iconst_5,&&opc_lconst_0,   &&opc_lconst_1, &&opc_fconst_0,
/* 0x0C */ &&opc_fconst_1,&&opc_fconst_2,   &&opc_dconst_0, &&opc_dconst_1,

/* 0x10 */ &&opc_bipush, &&opc_sipush, &&opc_ldc,    &&opc_ldc_w,
/* 0x14 */ &&opc_ldc2_w, &&opc_iload,  &&opc_lload,  &&opc_fload,
/* 0x18 */ &&opc_dload,  &&opc_aload,  &&opc_iload_0,&&opc_iload_1,
/* 0x1C */ &&opc_iload_2,&&opc_iload_3,&&opc_lload_0,&&opc_lload_1,

/* 0x20 */ &&opc_lload_2,&&opc_lload_3,&&opc_fload_0,&&opc_fload_1,
/* 0x24 */ &&opc_fload_2,&&opc_fload_3,&&opc_dload_0,&&opc_dload_1,
/* 0x28 */ &&opc_dload_2,&&opc_dload_3,&&opc_aload_0,&&opc_aload_1,
/* 0x2C */ &&opc_aload_2,&&opc_aload_3,&&opc_iaload, &&opc_laload,

/* 0x30 */ &&opc_faload,  &&opc_daload,  &&opc_aaload,  &&opc_baload,
/* 0x34 */ &&opc_caload,  &&opc_saload,  &&opc_istore,  &&opc_lstore,
/* 0x38 */ &&opc_fstore,  &&opc_dstore,  &&opc_astore,  &&opc_istore_0,
/* 0x3C */ &&opc_istore_1,&&opc_istore_2,&&opc_istore_3,&&opc_lstore_0,

/* 0x40 */ &&opc_lstore_1,&&opc_lstore_2,&&opc_lstore_3,&&opc_fstore_0,
/* 0x44 */ &&opc_fstore_1,&&opc_fstore_2,&&opc_fstore_3,&&opc_dstore_0,
/* 0x48 */ &&opc_dstore_1,&&opc_dstore_2,&&opc_dstore_3,&&opc_astore_0,
/* 0x4C */ &&opc_astore_1,&&opc_astore_2,&&opc_astore_3,&&opc_iastore,

/* 0x50 */ &&opc_lastore,&&opc_fastore,&&opc_dastore,&&opc_aastore,
/* 0x54 */ &&opc_bastore,&&opc_castore,&&opc_sastore,&&opc_pop,
/* 0x58 */ &&opc_pop2,   &&opc_dup,    &&opc_dup_x1, &&opc_dup_x2,
/* 0x5C */ &&opc_dup2,   &&opc_dup2_x1,&&opc_dup2_x2,&&opc_swap,

/* 0x60 */ &&opc_iadd,&&opc_ladd,&&opc_fadd,&&opc_dadd,
/* 0x64 */ &&opc_isub,&&opc_lsub,&&opc_fsub,&&opc_dsub,
/* 0x68 */ &&opc_imul,&&opc_lmul,&&opc_fmul,&&opc_dmul,
/* 0x6C */ &&opc_idiv,&&opc_ldiv,&&opc_fdiv,&&opc_ddiv,

/* 0x70 */ &&opc_irem, &&opc_lrem, &&opc_frem,&&opc_drem,
/* 0x74 */ &&opc_ineg, &&opc_lneg, &&opc_fneg,&&opc_dneg,
/* 0x78 */ &&opc_ishl, &&opc_lshl, &&opc_ishr,&&opc_lshr,
/* 0x7C */ &&opc_iushr,&&opc_lushr,&&opc_iand,&&opc_land,

/* 0x80 */ &&opc_ior, &&opc_lor,&&opc_ixor,&&opc_lxor,
/* 0x84 */ &&opc_iinc,&&opc_i2l,&&opc_i2f, &&opc_i2d,
/* 0x88 */ &&opc_l2i, &&opc_l2f,&&opc_l2d, &&opc_f2i,
/* 0x8C */ &&opc_f2l, &&opc_f2d,&&opc_d2i, &&opc_d2l,

/* 0x90 */ &&opc_d2f,  &&opc_i2b,  &&opc_i2c,  &&opc_i2s,
/* 0x94 */ &&opc_lcmp, &&opc_fcmpl,&&opc_fcmpg,&&opc_dcmpl,
/* 0x98 */ &&opc_dcmpg,&&opc_ifeq, &&opc_ifne, &&opc_iflt,
/* 0x9C */ &&opc_ifge, &&opc_ifgt, &&opc_ifle, &&opc_if_icmpeq,

/* 0xA0 */ &&opc_if_icmpne,&&opc_if_icmplt,&&opc_if_icmpge,  &&opc_if_icmpgt,
/* 0xA4 */ &&opc_if_icmple,&&opc_if_acmpeq,&&opc_if_acmpne,  &&opc_goto,
/* 0xA8 */ &&opc_jsr,      &&opc_ret,      &&opc_tableswitch,&&opc_lookupswitch,
/* 0xAC */ &&opc_ireturn,  &&opc_lreturn,  &&opc_freturn,    &&opc_dreturn,

/* 0xB0 */ &&opc_areturn,     &&opc_return,         &&opc_getstatic,    &&opc_putstatic,
/* 0xB4 */ &&opc_getfield,    &&opc_putfield,       &&opc_invokevirtual,&&opc_invokespecial,
/* 0xB8 */ &&opc_invokestatic,&&opc_invokeinterface,NULL,               &&opc_new,
/* 0xBC */ &&opc_newarray,    &&opc_anewarray,      &&opc_arraylength,  &&opc_athrow,

/* 0xC0 */ &&opc_checkcast,   &&opc_instanceof,     &&opc_monitorenter, &&opc_monitorexit,
/* 0xC4 */ &&opc_wide,        &&opc_multianewarray, &&opc_ifnull,       &&opc_ifnonnull,
/* 0xC8 */ &&opc_goto_w,      &&opc_jsr_w,          &&opc_breakpoint,   &&opc_default,
/* 0xCC */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,

/* 0xD0 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xD4 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xD8 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xDC */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,

/* 0xE0 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xE4 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xE8 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xEC */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,

/* 0xF0 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xF4 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xF8 */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default,
/* 0xFC */ &&opc_default,     &&opc_default,        &&opc_default,      &&opc_default
  };
  register uintptr_t *dispatch_table = (uintptr_t*)&opclabels_data[0];
#endif /* USELABELS */

#ifdef ASSERT
  // this will trigger a VERIFY_OOP on entry
  if (istate->msg() != initialize && ! istate->method()->is_static()) {
    oop rcvr = LOCALS_OBJECT(0);
  }
#endif

  /* QQQ this should be a stack method so we don't know actual direction */
  assert(istate->msg() == initialize ||
	 topOfStack >= istate->stack_limit() &&
	 topOfStack < istate->stack_base(), 
	 "Stack top out of range");

  switch (istate->msg()) {
    case initialize: {
      if (initialized++) ShouldNotReachHere(); // Only one initialize call
#ifndef CORE
      _compiling = (UseCompiler || CountCompiledCalls);
#endif
#ifdef VM_JVMTI
      _jvmti_interp_events = JvmtiExport::can_post_interpreter_events();
#endif
      BarrierSet* bs = Universe::heap()->barrier_set();
      assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
      _byte_map_base = (volatile jbyte*)(((CardTableModRefBS*)bs)->byte_map_base);
      return;
    }
    break;
    case method_entry: {
      THREAD->set_do_not_unlock();
      // count invocations
      assert(initialized, "Interpreter not initialized");
#ifndef CORE
      if (_compiling) {
#ifdef COMPILER2
	if (ProfileInterpreter) {
	  istate->method()->increment_interpreter_invocation_count();
	}
#endif // COMPILER2
	INCR_INVOCATION_COUNT;
	if (istate->method()->invocation_counter()->has_overflowed()) {
	    CALL_VM((void)InterpreterRuntime::frequency_counter_overflow(THREAD, NULL), handle_exception);
	    istate->set_msg(retry_method);
	    THREAD->clr_do_not_unlock();
	    return;
	}
	SAFEPOINT;
      }
#endif // !CORE

#ifdef HACK
      {
	ResourceMark rm;
	char *method_name = istate->method()->name_and_sig_as_C_string();
	if (strstr(method_name, "registerFontPropertiesFonts") != NULL) os::breakpoint();
      }
#endif // HACK


      // lock method if synchronized
      if (istate->method()->is_synchronized()) {
          // oop rcvr = locals[0].j.r;
          oop rcvr;
          if (istate->method()->is_static()) {
            rcvr = istate->method()->constants()->pool_holder()->klass_part()->java_mirror();
          } else {
	    rcvr = LOCALS_OBJECT(0);
	  }
          // The initial monitor is ours for the taking
	  BasicObjectLock* mon = &istate->monitor_base()[-1];
          assert(mon->obj() == rcvr, "method monitor mis-initialized");

	  markOop displaced = rcvr->mark()->set_unlocked();
	  mon->lock()->set_displaced_header(displaced);
	  if (Atomic::cmpxchg_ptr(mon, rcvr->mark_addr(), displaced) != displaced) {
	    // Is it simple recursive case?
	    if (THREAD->is_lock_owned((address) displaced->clear_lock_bits())) {
	      mon->lock()->set_displaced_header(NULL);
	    } else {
	      CALL_VM(InterpreterRuntime::monitorenter(THREAD, mon), handle_exception);
	    }
	  }
      }
      THREAD->clr_do_not_unlock();

      // Notify jvmti/jvmpi
#ifdef VM_JVMTI
      if (_jvmti_interp_events) {
        // Whenever JVMTI puts a thread in interp_only_mode, method
        // entry/exit events are sent for that thread to track stack depth.  
        if (THREAD->is_interp_only_mode()) {
          CALL_VM(InterpreterRuntime::post_method_entry(THREAD), 
                  handle_exception);
        }
      }
#endif /* VM_JVMTI */
      if (*jvmpi::event_flags_array_at_addr(JVMPI_EVENT_METHOD_ENTRY ) == JVMPI_EVENT_ENABLED ||
          *jvmpi::event_flags_array_at_addr(JVMPI_EVENT_METHOD_ENTRY2) == JVMPI_EVENT_ENABLED) {
        oop rcvr;
        if (istate->method()->is_static()) {
          rcvr = NULL;
        } else {
	  rcvr = LOCALS_OBJECT(0);
	}
        CALL_VM(SharedRuntime::jvmpi_method_entry(THREAD, istate->method(), 
                rcvr), handle_exception);
      }

      goto run;
    }

    case popping_frame: {
      // returned from a java call to pop the frame, restart the call
      // clear the message so we don't confuse ourselves later
      assert(THREAD->pop_frame_in_process(), "wrong frame pop state");
      istate->set_msg(no_request);
      THREAD->clr_pop_frame_in_process();
      goto run;
    }

    case method_resume: {
      // returned from a java call, continue executing.
      if (THREAD->pop_frame_pending() && !THREAD->pop_frame_in_process()) {
        goto handle_Pop_Frame;
      }
 
      if (THREAD->has_pending_exception()) goto handle_exception;
      // Update the pc by the saved amount of the invoke bytecode size
      UPDATE_PC(istate->bcp_advance());
      goto run;
    }

    case deopt_resume2: {
      // Returned from an opcode that will reexecute. Deopt was
      // a result of a PopFrame request.
      //
      goto run;
    }

    case deopt_resume: {
      // Returned from an opcode that has completed. The stack has
      // the result all we need to do is skip across the bytecode
      // and continue (assuming there is no exception pending)
      // 
      // compute continuation length
      //
      UPDATE_PC(Bytecodes::length_at(pc));
      if (THREAD->has_pending_exception()) goto handle_exception;
      goto run;
    }
    case got_monitors: {
      // continue locking now that we have a monitor to use
      // we expect to find newly allocated monitor at the "top" of the monitor stack.
      oop lockee = STACK_OBJECT(-1);
      // derefing's lockee ought to provoke implicit null check
      // find a free monitor
      BasicObjectLock* entry = (BasicObjectLock*) istate->stack_base();
      assert(entry->obj() == NULL, "Frame manager didn't allocate the monitor");
      entry->set_obj(lockee);

      markOop displaced = lockee->mark()->set_unlocked();
      entry->lock()->set_displaced_header(displaced);
      if (Atomic::cmpxchg_ptr(entry, lockee->mark_addr(), displaced) != displaced) {
	// Is it simple recursive case?
	if (THREAD->is_lock_owned((address) displaced->clear_lock_bits())) {
	  entry->lock()->set_displaced_header(NULL);
#if 0
	  os::breakpoint();
	  ObjectSynchronizer::inflate(lockee);
#endif
	} else {
	  CALL_VM(InterpreterRuntime::monitorenter(THREAD, entry), handle_exception);
	}
      }
      UPDATE_PC_AND_TOS(1, -1);
      goto run;
    }
    default: {
      fatal("Unexpected message from frame manager");
    }
  }

run:

  DO_UPDATE_INSTRUCTION_COUNT(*pc)
  DEBUGGER_SINGLE_STEP_NOTIFY();
#ifdef PREFETCH_OPCCODE
  opcode = *pc;  /* prefetch first opcode */
#endif

#ifndef USELABELS
  while (1)
#endif
  {
#ifndef PREFETCH_OPCCODE
      opcode = *pc;
#endif
      // Seems like this happens twice per opcode. At worst this is only
      // need at entry to the loop.
      // DEBUGGER_SINGLE_STEP_NOTIFY();
      /* Using this labels avoids double breakpoints when quickening and
       * when returing from transition frames.
       */
  opcode_switch:
      assert(istate == orig, "Corrupted istate");
      /* QQQ Hmm this has knowledge of direction, ought to be a stack method */
      assert(topOfStack >= istate->stack_limit(), "Stack overrun");
      assert(topOfStack < istate->stack_base(), "Stack underrun");

#ifdef USELABELS
      DISPATCH(opcode);
#else
      switch (opcode)
#endif
      {
      CASE(_nop):
          UPDATE_PC_AND_CONTINUE(1);

          /* Push miscellaneous constants onto the stack. */

      CASE(_aconst_null):
          SET_STACK_OBJECT(NULL, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);

#undef  OPC_CONST_n
#define OPC_CONST_n(opcode, const_type, value)                          \
      CASE(opcode):                                                     \
          SET_STACK_ ## const_type(value, 0);                           \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);

          OPC_CONST_n(_iconst_m1,   INT,       -1);
          OPC_CONST_n(_iconst_0,    INT,        0);
          OPC_CONST_n(_iconst_1,    INT,        1);
          OPC_CONST_n(_iconst_2,    INT,        2);
          OPC_CONST_n(_iconst_3,    INT,        3);
          OPC_CONST_n(_iconst_4,    INT,        4);
          OPC_CONST_n(_iconst_5,    INT,        5);
          OPC_CONST_n(_fconst_0,    FLOAT,      0.0);
          OPC_CONST_n(_fconst_1,    FLOAT,      1.0);
          OPC_CONST_n(_fconst_2,    FLOAT,      2.0);

#undef  OPC_CONST2_n
#define OPC_CONST2_n(opcname, value, key)                               \
      CASE(_##opcname):                                                 \
      {                                                                 \
         VM##key##2Jvm(&STACK_INFO(DTOS(0)).raw,                        \
             VM##key##Const##value());                                  \
         UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);                          \
      }
#undef  OPC_CONST2_n
#define OPC_CONST2_n(opcname, value, key, kind)                         \
      CASE(_##opcname):                                                 \
      {                                                                 \
          SET_STACK_ ## kind(VM##key##Const##value(), 1);               \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);                         \
      }
         OPC_CONST2_n(dconst_0, Zero, double, DOUBLE);
         OPC_CONST2_n(dconst_1, One,  double, DOUBLE);
         OPC_CONST2_n(lconst_0, Zero, long, LONG);
         OPC_CONST2_n(lconst_1, One,  long, LONG);

         /* Load constant from constant pool: */

          /* Push a 1-byte signed integer value onto the stack. */
      CASE(_bipush):
          SET_STACK_INT((jbyte)(pc[1]), 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, 1);

          /* Push a 2-byte signed integer constant onto the stack. */
      CASE(_sipush):
          SET_STACK_INT((int16_t)Bytes::get_Java_u2(pc + 1), 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(3, 1);

          /* load from local variable */

      CASE(_aload):
      CASE(_iload):
      CASE(_fload):
          SET_STACK_SLOT(LOCALS_SLOT(pc[1]), 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, 1);

      CASE(_lload):
          SET_STACK_LONG_FROM_ADDR(LOCALS_LONG_AT(pc[1]), 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, 2);

      CASE(_dload):
          SET_STACK_DOUBLE_FROM_ADDR(LOCALS_DOUBLE_AT(pc[1]), 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, 2);

#undef  OPC_LOAD_n
#define OPC_LOAD_n(num)                                                 \
      CASE(_aload_##num):                                               \
      CASE(_iload_##num):                                               \
      CASE(_fload_##num):                                               \
          SET_STACK_SLOT(LOCALS_SLOT(num), 0);                          \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);                         \
                                                                        \
      CASE(_lload_##num):                                               \
          SET_STACK_LONG_FROM_ADDR(LOCALS_LONG_AT(num), 1);             \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);                         \
      CASE(_dload_##num):                                               \
          SET_STACK_DOUBLE_FROM_ADDR(LOCALS_DOUBLE_AT(num), 1);         \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);

          OPC_LOAD_n(0);
          OPC_LOAD_n(1);
          OPC_LOAD_n(2);
          OPC_LOAD_n(3);

          /* store to a local variable */

      CASE(_astore):
      CASE(_istore):
      CASE(_fstore):
          SET_LOCALS_SLOT(STACK_SLOT(-1), pc[1]);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, -1);

      CASE(_lstore):
          SET_LOCALS_LONG(STACK_LONG(-1), pc[1]);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, -2);

      CASE(_dstore):
          SET_LOCALS_DOUBLE(STACK_DOUBLE(-1), pc[1]);
          UPDATE_PC_AND_TOS_AND_CONTINUE(2, -2);

      CASE(_wide): {
          uint16_t reg = Bytes::get_Java_u2(pc + 2);

          opcode = pc[1];
          switch(opcode) {
              case Bytecodes::_aload:
              case Bytecodes::_iload:
              case Bytecodes::_fload:
		  SET_STACK_SLOT(LOCALS_SLOT(reg), 0); 
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, 1);

              case Bytecodes::_lload:
                  SET_STACK_LONG_FROM_ADDR(LOCALS_LONG_AT(reg), 1);
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, 2);

              case Bytecodes::_dload:
		  SET_STACK_DOUBLE_FROM_ADDR(LOCALS_LONG_AT(reg), 1);
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, 2);

              case Bytecodes::_istore:
              case Bytecodes::_astore:
              case Bytecodes::_fstore:
                  SET_LOCALS_SLOT(STACK_SLOT(-1), reg);
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, -1);

              case Bytecodes::_lstore:
                  SET_LOCALS_LONG(STACK_LONG(-1), reg);
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, -2);

              case Bytecodes::_dstore:
                  SET_LOCALS_DOUBLE(STACK_DOUBLE(-1), reg);
                  UPDATE_PC_AND_TOS_AND_CONTINUE(4, -2);

              case Bytecodes::_iinc: {
                  int16_t offset = (int16_t)Bytes::get_Java_u2(pc+4); 
		  // Be nice to see what this generates.... QQQ
                  SET_LOCALS_INT(LOCALS_INT(reg) + offset, reg);
                  UPDATE_PC_AND_CONTINUE(6);
              }
              case Bytecodes::_ret:
                  pc = istate->method()->code_base() + (intptr_t)(LOCALS_ADDR(reg));
                  UPDATE_PC_AND_CONTINUE(0);
              default:
                  VM_JAVA_ERROR(vmSymbols::java_lang_InternalError(), "undefined opcode");
          }
      }


#undef  OPC_STORE_n
#define OPC_STORE_n(num)                                                \
      CASE(_astore_##num):                                              \
      CASE(_istore_##num):                                              \
      CASE(_fstore_##num):                                              \
          SET_LOCALS_SLOT(STACK_SLOT(-1), num);                          \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);

          OPC_STORE_n(0);
          OPC_STORE_n(1);
          OPC_STORE_n(2);
          OPC_STORE_n(3);

#undef  OPC_DSTORE_n
#define OPC_DSTORE_n(num)                                               \
      CASE(_dstore_##num):                                              \
	  SET_LOCALS_DOUBLE(STACK_DOUBLE(-1), num);                     \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -2);                        \
      CASE(_lstore_##num):                                              \
	  SET_LOCALS_LONG(STACK_LONG(-1), num);                         \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -2);

          OPC_DSTORE_n(0);
          OPC_DSTORE_n(1);
          OPC_DSTORE_n(2);
          OPC_DSTORE_n(3);

          /* stack pop, dup, and insert opcodes */

         
      CASE(_pop):                /* Discard the top item on the stack */
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);

         
      CASE(_pop2):               /* Discard the top 2 items on the stack */
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -2);

          
      CASE(_dup):               /* Duplicate the top item on the stack */
          SET_STACK_SLOT(STACK_SLOT(-1), 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);

      CASE(_dup2):              /* Duplicate the top 2 items on the stack */
          SET_STACK_SLOT(STACK_SLOT(-2), 0);
          SET_STACK_SLOT(STACK_SLOT(-1), 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);

      CASE(_dup_x1):    /* insert top word two down */
          SET_STACK_SLOT(STACK_SLOT(-1), 0);
          SET_STACK_SLOT(STACK_SLOT(-2), -1);
          SET_STACK_SLOT(STACK_SLOT(0), -2);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);

      CASE(_dup_x2):    /* insert top word three down  */
          SET_STACK_SLOT(STACK_SLOT(-1), 0);
          SET_STACK_SLOT(STACK_SLOT(-2), -1);
          SET_STACK_SLOT(STACK_SLOT(-3), -2);
          SET_STACK_SLOT(STACK_SLOT(0), -3);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);

      CASE(_dup2_x1):   /* insert top 2 slots three down */
          SET_STACK_SLOT(STACK_SLOT(-1), 1);
          SET_STACK_SLOT(STACK_SLOT(-2), 0);
          SET_STACK_SLOT(STACK_SLOT(-3), -1);
          SET_STACK_SLOT(STACK_SLOT(1), -2);
          SET_STACK_SLOT(STACK_SLOT(0), -3);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);

      CASE(_dup2_x2):   /* insert top 2 slots four down */
          SET_STACK_SLOT(STACK_SLOT(-1), 1);
          SET_STACK_SLOT(STACK_SLOT(-2), 0);
          SET_STACK_SLOT(STACK_SLOT(-3), -1);
          SET_STACK_SLOT(STACK_SLOT(-4), -2);
          SET_STACK_SLOT(STACK_SLOT(1), -3);
          SET_STACK_SLOT(STACK_SLOT(0), -4);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);

      CASE(_swap): {        /* swap top two elements on the stack */
	  intptr_t val = GET_STACK_SLOT(-1);
	  SET_STACK_SLOT(STACK_SLOT(-2), -1);
	  SET_STACK_SLOT((address)&val, -2);
          UPDATE_PC_AND_CONTINUE(1);
      }

          /* Perform various binary integer operations */

#undef  OPC_INT_BINARY 
#define OPC_INT_BINARY(opcname, opname, test)                           \
      CASE(_i##opcname):                                                \
          if (test && (STACK_INT(-1) == 0)) {                           \
              VM_JAVA_ERROR(vmSymbols::java_lang_ArithmeticException(), \
	                    "/ by int zero");                           \
          }                                                             \
          SET_STACK_INT(VMint##opname(STACK_INT(-2),                    \
				      STACK_INT(-1)),                   \
				      -2);                              \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);                        \
      CASE(_l##opcname):                                                \
      {                                                                 \
          if (test) {                                                   \
            jlong l1 = STACK_LONG(-1);                                  \
            if (VMlongEqz(l1)) {                                        \
              VM_JAVA_ERROR(vmSymbols::java_lang_ArithmeticException(), \
			    "/ by long zero");                          \
            }                                                           \
          }                                                             \
	  /* First long at (-1,-2) next long at (-3,-4) */              \
          SET_STACK_LONG(VMlong##opname(STACK_LONG(-3),                 \
					STACK_LONG(-1)),                \
					-3);                            \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -2);                        \
      }

      OPC_INT_BINARY(add, Add, 0);
      OPC_INT_BINARY(sub, Sub, 0);
      OPC_INT_BINARY(mul, Mul, 0);
      OPC_INT_BINARY(and, And, 0);
      OPC_INT_BINARY(or,  Or,  0);
      OPC_INT_BINARY(xor, Xor, 0);
      OPC_INT_BINARY(div, Div, 1);
      OPC_INT_BINARY(rem, Rem, 1);


      /* Perform various binary floating number operations */
      /* On some machine/platforms/compilers div zero check can be implicit */

#undef  OPC_FLOAT_BINARY 
#define OPC_FLOAT_BINARY(opcname, opname)                                  \
      CASE(_d##opcname): {                                                 \
          SET_STACK_DOUBLE(VMdouble##opname(STACK_DOUBLE(-3),              \
					    STACK_DOUBLE(-1)),             \
					    -3);                           \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -2);                           \
      }                                                                    \
      CASE(_f##opcname):                                                   \
          SET_STACK_FLOAT(VMfloat##opname(STACK_FLOAT(-2),                 \
					  STACK_FLOAT(-1)),                \
					  -2);                             \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);


     OPC_FLOAT_BINARY(add, Add);
     OPC_FLOAT_BINARY(sub, Sub);
     OPC_FLOAT_BINARY(mul, Mul);
     OPC_FLOAT_BINARY(div, Div);
     OPC_FLOAT_BINARY(rem, Rem);

      /* Shift operations                                  
       * Shift left int and long: ishl, lshl           
       * Logical shift right int and long w/zero extension: iushr, lushr
       * Arithmetic shift right int and long w/sign extension: ishr, lshr
       */

#undef  OPC_SHIFT_BINARY
#define OPC_SHIFT_BINARY(opcname, opname)                               \
      CASE(_i##opcname):                                                \
         SET_STACK_INT(VMint##opname(STACK_INT(-2),                     \
				     STACK_INT(-1)),                    \
				     -2);                               \
         UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);                         \
      CASE(_l##opcname):                                                \
      {                                                                 \
         SET_STACK_LONG(VMlong##opname(STACK_LONG(-2),                  \
				       STACK_INT(-1)),                  \
				       -2);                             \
         UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);                         \
      }
    
      OPC_SHIFT_BINARY(shl, Shl);
      OPC_SHIFT_BINARY(shr, Shr);
      OPC_SHIFT_BINARY(ushr, Ushr);

     /* Increment local variable by constant */ 
      CASE(_iinc): 
      {
          // locals[pc[1]].j.i += (jbyte)(pc[2]);
	  SET_LOCALS_INT(LOCALS_INT(pc[1]) + (jbyte)(pc[2]), pc[1]);
          UPDATE_PC_AND_CONTINUE(3);
      }

     /* negate the value on the top of the stack */

      CASE(_ineg):
         SET_STACK_INT(VMintNeg(STACK_INT(-1)), -1); 
         UPDATE_PC_AND_CONTINUE(1);

      CASE(_fneg):
         SET_STACK_FLOAT(VMfloatNeg(STACK_FLOAT(-1)), -1); 
         UPDATE_PC_AND_CONTINUE(1);

      CASE(_lneg):
      {
         SET_STACK_LONG(VMlongNeg(STACK_LONG(-1)), -1); 
         UPDATE_PC_AND_CONTINUE(1);
      }

      CASE(_dneg):
      {
         SET_STACK_DOUBLE(VMdoubleNeg(STACK_DOUBLE(-1)), -1); 
         UPDATE_PC_AND_CONTINUE(1);
      }

      /* Conversion operations */

      CASE(_i2f):       /* convert top of stack int to float */
         SET_STACK_FLOAT(VMint2Float(STACK_INT(-1)), -1);
         UPDATE_PC_AND_CONTINUE(1);

      CASE(_i2l):       /* convert top of stack int to long */
      {
	  // this is ugly QQQ
          jlong r = VMint2Long(STACK_INT(-1));
	  MORE_STACK(-1); // Pop
          SET_STACK_LONG(r, 1);

          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_i2d):       /* convert top of stack int to double */
      {
	  // this is ugly QQQ (why cast to jlong?? )
          jdouble r = (jlong)STACK_INT(-1);
	  MORE_STACK(-1); // Pop
          SET_STACK_DOUBLE(r, 1);

          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_l2i):       /* convert top of stack long to int */
      {
          jint r = VMlong2Int(STACK_LONG(-1));
	  MORE_STACK(-2); // Pop
	  SET_STACK_INT(r, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }
      
      CASE(_l2f):   /* convert top of stack long to float */
      {
          jlong r = STACK_LONG(-1);
	  MORE_STACK(-2); // Pop
	  SET_STACK_FLOAT(VMlong2Float(r), 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }

      CASE(_l2d):       /* convert top of stack long to double */
      {
          jlong r = STACK_LONG(-1);
	  MORE_STACK(-2); // Pop
	  SET_STACK_DOUBLE(VMlong2Double(r), 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_f2i):  /* Convert top of stack float to int */
          SET_STACK_INT(SharedRuntime::f2i(STACK_FLOAT(-1)), -1); 
          UPDATE_PC_AND_CONTINUE(1);

      CASE(_f2l):  /* convert top of stack float to long */
      {
          jlong r = SharedRuntime::f2l(STACK_FLOAT(-1));
	  MORE_STACK(-1); // POP
	  SET_STACK_LONG(r, 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_f2d):  /* convert top of stack float to double */
      {
          jfloat f;
          jdouble r;
	  f = STACK_FLOAT(-1);
#ifdef IA64
	  // IA64 gcc bug
          r = ( f == 0.0f ) ? (jdouble) f : (jdouble) f + ia64_double_zero;
#else
          r = (jdouble) f;
#endif
	  MORE_STACK(-1); // POP
	  SET_STACK_DOUBLE(r, 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_d2i): /* convert top of stack double to int */
      {
          jint r1 = SharedRuntime::d2i(STACK_DOUBLE(-1));
	  MORE_STACK(-2);
          SET_STACK_INT(r1, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }

      CASE(_d2f): /* convert top of stack double to float */
      {
          jfloat r1 = VMdouble2Float(STACK_DOUBLE(-1));
	  MORE_STACK(-2);
          SET_STACK_FLOAT(r1, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }

      CASE(_d2l): /* convert top of stack double to long */
      {
          jlong r1 = SharedRuntime::d2l(STACK_DOUBLE(-1));
	  MORE_STACK(-2);
          SET_STACK_LONG(r1, 1);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 2);
      }

      CASE(_i2b):
          SET_STACK_INT(VMint2Byte(STACK_INT(-1)), -1);
          UPDATE_PC_AND_CONTINUE(1);

      CASE(_i2c):
          SET_STACK_INT(VMint2Char(STACK_INT(-1)), -1);
          UPDATE_PC_AND_CONTINUE(1);

      CASE(_i2s):
          SET_STACK_INT(VMint2Short(STACK_INT(-1)), -1);
          UPDATE_PC_AND_CONTINUE(1);

      /* comparison operators */

#define COMPARISON_OP(name, comparison)                                      \
      CASE(_if_icmp##name): {                                                \
          int skip = (STACK_INT(-2) comparison STACK_INT(-1))                \
                      ? (int16_t)Bytes::get_Java_u2(pc + 1) : 3;             \
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -2);     \
      }                                                                      \
      CASE(_if##name): {                                                     \
          int skip = (STACK_INT(-1) comparison 0)                            \
                      ? (int16_t)Bytes::get_Java_u2(pc + 1) : 3;             \
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -1);     \
      }

#define COMPARISON_OP2(name, comparison)                                     \
      COMPARISON_OP(name, comparison)                                        \
      CASE(_if_acmp##name): {                                                \
          int skip = (STACK_OBJECT(-2) comparison STACK_OBJECT(-1))          \
                       ? (int16_t)Bytes::get_Java_u2(pc + 1) : 3;            \
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -2);     \
      }

#define NULL_COMPARISON_NOT_OP(name)                           \
      CASE(_if##name): {                                        \
          int skip = (!(STACK_OBJECT(-1) == 0))           \
                      ? (int16_t)Bytes::get_Java_u2(pc + 1) : 3;         \
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -1);\
      }

#define NULL_COMPARISON_OP(name)                           \
      CASE(_if##name): {                                        \
          int skip = ((STACK_OBJECT(-1) == 0))           \
                      ? (int16_t)Bytes::get_Java_u2(pc + 1) : 3;         \
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -1);\
      }

      COMPARISON_OP(lt, <);
      COMPARISON_OP(gt, >);
      COMPARISON_OP(le, <=);
      COMPARISON_OP(ge, >=);
      COMPARISON_OP2(eq, ==);  /* include ref comparison */
      COMPARISON_OP2(ne, !=);  /* include ref comparison */
      NULL_COMPARISON_OP(null);
      NULL_COMPARISON_NOT_OP(nonnull);

      /* Goto pc at specified offset in switch table. */

      CASE(_tableswitch): {
          jint* lpc  = (jint*)VMalignWordUp(pc+1);
          int32_t  key  = STACK_INT(-1);
          int32_t  low  = Bytes::get_Java_u4((address)&lpc[1]);
          int32_t  high = Bytes::get_Java_u4((address)&lpc[2]);
          int32_t  skip;
          key -= low;
          skip = ((uint32_t) key > (uint32_t)(high - low))
                      ? Bytes::get_Java_u4((address)&lpc[0])
                      : Bytes::get_Java_u4((address)&lpc[key + 3]);
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -1);
      }

      /* Goto pc whose table entry matches specified key */

      CASE(_lookupswitch): {
          jint* lpc  = (jint*)VMalignWordUp(pc+1);
          int32_t  key  = STACK_INT(-1);
          int32_t  skip = Bytes::get_Java_u4((address) lpc); /* default amount */
          int32_t  npairs = Bytes::get_Java_u4((address) &lpc[1]);
          while (--npairs >= 0) {
              lpc += 2;
              if (key == Bytes::get_Java_u4((address)lpc)) {
                  skip = Bytes::get_Java_u4((address)&lpc[1]);
                  break;
              }
          }
          UPDATE_PC_AND_TOS_AND_CONTINUE_WITH_BACKWARDS_CHECK(skip, -1);
      }

      CASE(_fcmpl):
      CASE(_fcmpg):
      {
          SET_STACK_INT(VMfloatCompare(STACK_FLOAT(-2), 
				        STACK_FLOAT(-1), 
				        (opcode == Bytecodes::_fcmpl ? -1 : 1)),
			-2);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);
      }

      CASE(_dcmpl):
      CASE(_dcmpg):
      {
          int r = VMdoubleCompare(STACK_DOUBLE(-3),
				  STACK_DOUBLE(-1),
				  (opcode == Bytecodes::_dcmpl ? -1 : 1));
	  MORE_STACK(-4); // Pop 
	  SET_STACK_INT(r, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }

      CASE(_lcmp):
      {
	  int r = VMlongCompare(STACK_LONG(-3), STACK_LONG(-1));
	  MORE_STACK(-4);
          SET_STACK_INT(r, 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, 1);
      }


      /* Return from a method */

      CASE(_areturn):
      CASE(_ireturn):
      CASE(_freturn):
      {
          // Allow a safepoint before returning to frame manager.
	  SAFEPOINT;

          goto handle_return;
      }

      CASE(_lreturn):
      CASE(_dreturn):
      {
          // Allow a safepoint before returning to frame manager.
	  SAFEPOINT;
          goto handle_return;
      }

      CASE(_return): {

          // Allow a safepoint before returning to frame manager.
	  SAFEPOINT;
          goto handle_return;
      }

      /* Array access byte-codes */

      /* Every array access byte-code starts out like this */
#define ARRAY_INTRO(arrayOff)                                                  \
      arrayOopDesc* arrObj = (arrayOopDesc*)STACK_OBJECT(arrayOff);            \
      jint     index  = STACK_INT(arrayOff + 1);                               \
      char message[jintAsStringSize];                                          \
      CHECK_NULL(arrObj);                                                      \
      if ((uint32_t)index >= (uint32_t)arrObj->length()) {                     \
          sprintf(message, "%d", index);                                       \
          VM_JAVA_ERROR(vmSymbols::java_lang_ArrayIndexOutOfBoundsException(), \
			message);                                              \
      }                                                 

      /* 32-bit loads. These handle conversion from < 32-bit types */
#define ARRAY_LOADTO32(T, T2, format, stackRes, extra)                                \
      {                                                                               \
          ARRAY_INTRO(-2);                                                            \
          extra;                                                                      \
          SET_ ## stackRes(*(T2 *)(((address) arrObj->base(T)) + index * sizeof(T2)), \
			   -2);                                                       \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);                                      \
      }

      /* 64-bit loads */
#define ARRAY_LOADTO64(T,T2, stackRes, extra)                                              \
      {                                                                                    \
          ARRAY_INTRO(-2);                                                                 \
          SET_ ## stackRes(*(T2 *)(((address) arrObj->base(T)) + index * sizeof(T2)), -1); \
          extra;                                                                           \
          UPDATE_PC_AND_CONTINUE(1);                                            \
      }

      CASE(_iaload):
          ARRAY_LOADTO32(T_INT, jint,   "%d",   STACK_INT, 0);
      CASE(_faload):
          ARRAY_LOADTO32(T_FLOAT, jfloat, "%f",   STACK_FLOAT, 0);
      CASE(_aaload):
          ARRAY_LOADTO32(T_OBJECT, oop,   INTPTR_FORMAT, STACK_OBJECT, 0);
      CASE(_baload):
          ARRAY_LOADTO32(T_BYTE, jbyte,  "%d",   STACK_INT, 0);
      CASE(_caload):
          ARRAY_LOADTO32(T_CHAR,  jchar, "%d",   STACK_INT, 0);
      CASE(_saload):
          ARRAY_LOADTO32(T_SHORT, jshort, "%d",   STACK_INT, 0);
      CASE(_laload):
          ARRAY_LOADTO64(T_LONG, jlong, STACK_LONG, 0);
      CASE(_daload):
          ARRAY_LOADTO64(T_DOUBLE, jdouble, STACK_DOUBLE, 0);

      /* 32-bit stores. These handle conversion to < 32-bit types */
#define ARRAY_STOREFROM32(T, T2, format, stackSrc, extra)                            \
      {                                                                              \
          ARRAY_INTRO(-3);                                                           \
          extra;                                                                     \
          *(T2 *)(((address) arrObj->base(T)) + index * sizeof(T2)) = stackSrc( -1); \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -3);                                     \
      }

      /* 64-bit stores */
#define ARRAY_STOREFROM64(T, T2, stackSrc, extra)                                    \
      {                                                                              \
          ARRAY_INTRO(-4);                                                           \
          extra;                                                                     \
          *(T2 *)(((address) arrObj->base(T)) + index * sizeof(T2)) = stackSrc( -1); \
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -4);                                     \
      }

      CASE(_iastore):
          ARRAY_STOREFROM32(T_INT, jint,   "%d",   STACK_INT, 0);
      CASE(_fastore):
          ARRAY_STOREFROM32(T_FLOAT, jfloat, "%f",   STACK_FLOAT, 0);
      /*
       * This one looks different because of the assignability check
       */
      CASE(_aastore): {
          oop rhsObject = STACK_OBJECT(-1);
          ARRAY_INTRO( -3);
	  // arrObj, index are set
          if (rhsObject != NULL) {
	    /* Check assignability of rhsObject into arrObj */
	    klassOop rhsKlassOop = rhsObject->klass(); // EBX (subclass)
	    assert(arrObj->klass()->klass()->klass_part()->oop_is_objArrayKlass(), "Ack not an objArrayKlass");
	    klassOop elemKlassOop = ((objArrayKlass*) arrObj->klass()->klass_part())->element_klass(); // superklass EAX
	    //
	    // Check for compatibilty. This check must not GC!!
	    // Seems way more expensive now that we must dispatch
	    //
	    if (rhsKlassOop != elemKlassOop && !rhsKlassOop->klass_part()->is_subtype_of(elemKlassOop)) { // ebx->is...
	      VM_JAVA_ERROR(vmSymbols::java_lang_ArrayStoreException(), "");
	    }
          }
	  oop* elem_loc = (oop*)(((address) arrObj->base(T_OBJECT)) + index * sizeof(oop));
          // *(oop*)(((address) arrObj->base(T_OBJECT)) + index * sizeof(oop)) = rhsObject;
	  *elem_loc = rhsObject;
	  // Mark the card
          OrderAccess::release_store(&BYTE_MAP_BASE[(uintptr_t)elem_loc >> CardTableModRefBS::card_shift], 0);
          UPDATE_PC_AND_TOS_AND_CONTINUE(1, -3);
      }
      CASE(_bastore):
          ARRAY_STOREFROM32(T_BYTE, jbyte,  "%d",   STACK_INT, 0);
      CASE(_castore):
          ARRAY_STOREFROM32(T_CHAR, jchar,  "%d",   STACK_INT, 0);
      CASE(_sastore):
          ARRAY_STOREFROM32(T_SHORT, jshort, "%d",   STACK_INT, 0);
      CASE(_lastore):
          ARRAY_STOREFROM64(T_LONG, jlong, STACK_LONG, 0);
      CASE(_dastore):
          ARRAY_STOREFROM64(T_DOUBLE, jdouble, STACK_DOUBLE, 0);

      CASE(_arraylength):
      {
	  arrayOopDesc *ary = (arrayOopDesc *) STACK_OBJECT(-1);
	  CHECK_NULL(ary);
	  SET_STACK_INT(ary->length(), -1);
          UPDATE_PC_AND_CONTINUE(1);
      }

      /* monitorenter and monitorexit for locking/unlocking an object */

      CASE(_monitorenter): {
	oop lockee = STACK_OBJECT(-1);
	// derefing's lockee ought to provoke implicit null check
	CHECK_NULL(lockee);
	// find a free monitor or one already allocated for this object
	// if we find a matching object then we need a new monitor
	// since this is recursive enter
	BasicObjectLock* limit = istate->monitor_base();
	BasicObjectLock* most_recent = (BasicObjectLock*) istate->stack_base();
	BasicObjectLock* entry = NULL;
	while (most_recent != limit ) {
	  if (most_recent->obj() == NULL) entry = most_recent;
	  else if (most_recent->obj() == lockee) break;
	  most_recent++;
	}
	if (entry != NULL) {
          entry->set_obj(lockee);
	  markOop displaced = lockee->mark()->set_unlocked();
	  entry->lock()->set_displaced_header(displaced);
	  if (Atomic::cmpxchg_ptr(entry, lockee->mark_addr(), displaced) != displaced) {
	    // Is it simple recursive case?
	    if (THREAD->is_lock_owned((address) displaced->clear_lock_bits())) {
	      entry->lock()->set_displaced_header(NULL);
#if 0
	      os::breakpoint();
	      ObjectSynchronizer::inflate(lockee);
#endif
	    } else {
	      CALL_VM(InterpreterRuntime::monitorenter(THREAD, entry), handle_exception);
	    }
	  }
	  UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);
	} else {
	  istate->set_msg(more_monitors);
	  // HACK FIX LATER
	  // Why was this needed? Seems to be useless now
	  // istate->set_callee((methodOop) lockee);
	  UPDATE_PC_AND_RETURN(0); // Re-execute
	}
      }

      CASE(_monitorexit): {
	oop lockee = STACK_OBJECT(-1);
	CHECK_NULL(lockee);
	// derefing's lockee ought to provoke implicit null check
	// find our monitor slot
	BasicObjectLock* limit = istate->monitor_base();
	BasicObjectLock* most_recent = (BasicObjectLock*) istate->stack_base();
	while (most_recent != limit ) {
	  if ((most_recent)->obj() == lockee) {
	    BasicLock* lock = most_recent->lock();
	    markOop header = lock->displaced_header();
	    most_recent->set_obj(NULL);
	    // If it isn't recursive we either must swap old header or call the runtime
	    if (header != NULL) {
	      if (Atomic::cmpxchg_ptr(header, lockee->mark_addr(), lock) != lock) {
		// restore object for the slow case
		most_recent->set_obj(lockee);
		CALL_VM(InterpreterRuntime::monitorexit(THREAD, most_recent), handle_exception);
	      }
	    }
	    UPDATE_PC_AND_TOS_AND_CONTINUE(1, -1);
	  }
	  most_recent++;
	}
	// Need to throw illegal monitor state exception
	CALL_VM(InterpreterRuntime::throw_illegal_monitor_state_exception(THREAD), handle_exception);
	// Should never reach here...
	assert(false, "Should have thrown illegal monitor exception");
      }

      /* All of the non-quick opcodes. */

      /* -Set clobbersCpIndex true if the quickened opcode clobbers the
       *  constant pool index in the instruction.
       */
      CASE(_getfield):
      CASE(_getstatic):
        {
          u2 index;
	  ConstantPoolCacheEntry* cache;
	  index = Bytes::get_native_u2(pc+1);

	  // QQQ Need to make this as inlined as possible. Probably need to 
          // split all the bytecode cases out so c++ compiler has a chance 
          // for constant prop to fold everything possible away.

	  cache = cp->entry_at(index);
	  if (!cache->is_resolved((Bytecodes::Code)opcode)) {
	    CALL_VM(InterpreterRuntime::resolve_get_put(THREAD, (Bytecodes::Code)opcode), 
		    handle_exception);
	    cache = cp->entry_at(index);
	  }

#ifdef VM_JVMTI
	  if (_jvmti_interp_events) {
            int *count_addr;
            oop obj;
	    // Check to see if a field modification watch has been set 
            // before we take the time to call into the VM.
            count_addr = (int *)JvmtiExport::get_field_access_count_addr();
            if ( *count_addr > 0 ) {
	      if ((Bytecodes::Code)opcode == Bytecodes::_getstatic) {
                obj = (oop)NULL;
              } else {
	        obj = (oop) STACK_OBJECT(-1);
              }
	      CALL_VM(InterpreterRuntime::post_field_access(THREAD, 
                                          obj, 
                                          cache),
		                          handle_exception);
            }
	  }
#endif /* VM_JVMTI */

	  oop obj;
	  if ((Bytecodes::Code)opcode == Bytecodes::_getstatic) {
	    obj = (oop) cache->f1();
            MORE_STACK(1);  // Assume single slot push
	  } else {
	    obj = (oop) STACK_OBJECT(-1);
	    CHECK_NULL(obj);
	  }

	  //
	  // Now store the result on the stack
	  //
          TosState tos_type = cache->flag_state();
          int field_offset = cache->f2();
	  if (cache->is_volatile()) {
	    if (tos_type == atos) {
	      SET_STACK_OBJECT(obj->obj_field_acquire(field_offset), -1);
	    } else if (tos_type == itos) {
	      SET_STACK_INT(obj->int_field_acquire(field_offset), -1);
	    } else if (tos_type == ltos) {
	      SET_STACK_LONG(obj->long_field_acquire(field_offset), 0);
	      MORE_STACK(1);
	    } else if (tos_type == btos) {
	      SET_STACK_INT(obj->byte_field_acquire(field_offset), -1);
	    } else if (tos_type == ctos) {
	      SET_STACK_INT(obj->char_field_acquire(field_offset), -1);
	    } else if (tos_type == stos) {
	      SET_STACK_INT(obj->short_field_acquire(field_offset), -1);
	    } else if (tos_type == ftos) {
	      SET_STACK_FLOAT(obj->float_field_acquire(field_offset), -1);
	    } else {
	      SET_STACK_DOUBLE(obj->double_field_acquire(field_offset), 0);
	      MORE_STACK(1);
	    }
	  } else {
	    if (tos_type == atos) {
	      SET_STACK_OBJECT(obj->obj_field(field_offset), -1);
	    } else if (tos_type == itos) {
	      SET_STACK_INT(obj->int_field(field_offset), -1);
	    } else if (tos_type == ltos) {
	      SET_STACK_LONG(obj->long_field(field_offset), 0);
	      MORE_STACK(1);
	    } else if (tos_type == btos) {
	      SET_STACK_INT(obj->byte_field(field_offset), -1);
	    } else if (tos_type == ctos) {
	      SET_STACK_INT(obj->char_field(field_offset), -1);
	    } else if (tos_type == stos) {
	      SET_STACK_INT(obj->short_field(field_offset), -1);
	    } else if (tos_type == ftos) {
	      SET_STACK_FLOAT(obj->float_field(field_offset), -1);
	    } else {
	      SET_STACK_DOUBLE(obj->double_field(field_offset), 0);
	      MORE_STACK(1);
	    }
	  }

	  UPDATE_PC_AND_CONTINUE(3);
	 }

      CASE(_putfield):
      CASE(_putstatic):
        {
	  u2 index = Bytes::get_native_u2(pc+1);
	  ConstantPoolCacheEntry* cache = cp->entry_at(index);
	  if (!cache->is_resolved((Bytecodes::Code)opcode)) {
	    CALL_VM(InterpreterRuntime::resolve_get_put(THREAD, (Bytecodes::Code)opcode), 
		    handle_exception);
	    cache = cp->entry_at(index);
	  }

#ifdef VM_JVMTI
	  if (_jvmti_interp_events) {
            int *count_addr;
            oop obj;
	    // Check to see if a field modification watch has been set 
            // before we take the time to call into the VM.
            count_addr = (int *)JvmtiExport::get_field_modification_count_addr();
            if ( *count_addr > 0 ) {
              if ((Bytecodes::Code)opcode == Bytecodes::_putstatic) {
                obj = (oop)NULL;
              }
              else {
                if (cache->is_long() || cache->is_double()) {
                  obj = (oop) STACK_OBJECT(-3);
                } else {
                  obj = (oop) STACK_OBJECT(-2);
                }
              }

	      CALL_VM(InterpreterRuntime::post_field_modification(THREAD,
                                          obj, 
                                          cache, 
                                          (jvalue *)STACK_SLOT(-1)),  
		                          handle_exception);
            }
	  }
#endif /* VM_JVMTI */

	  // QQQ Need to make this as inlined as possible. Probably need to split all the bytecode cases
	  // out so c++ compiler has a chance for constant prop to fold everything possible away.

	  oop obj;
	  int count;
          TosState tos_type = cache->flag_state();

	  count = -1;
	  if (tos_type == ltos || tos_type == dtos) {
	    --count;
	  }
	  if ((Bytecodes::Code)opcode == Bytecodes::_putstatic) {
	    obj = (oop) cache->f1();
	  } else {
	    --count;
	    obj = (oop) STACK_OBJECT(count);
	    CHECK_NULL(obj);
	  }

	  //
	  // Now store the result
	  //
          int field_offset = cache->f2();
	  if (cache->is_volatile()) {
	    if (tos_type == itos) {
	      obj->release_int_field_put(field_offset, STACK_INT(-1));
            } else if (tos_type == atos) {
              obj->release_obj_field_put(field_offset, STACK_OBJECT(-1));
              OrderAccess::release_store(&BYTE_MAP_BASE[(uintptr_t)obj >> CardTableModRefBS::card_shift], 0);
	    } else if (tos_type == btos) {
	      obj->release_byte_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == ltos) {
	      obj->release_long_field_put(field_offset, STACK_LONG(-1));
	    } else if (tos_type == ctos) {
	      obj->release_char_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == stos) {
	      obj->release_short_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == ftos) {
	      obj->release_float_field_put(field_offset, STACK_FLOAT(-1));
	    } else {
	      obj->release_double_field_put(field_offset, STACK_DOUBLE(-1));
	    }
	    OrderAccess::storeload();
	  } else {
	    if (tos_type == itos) {
	      obj->int_field_put(field_offset, STACK_INT(-1));
            } else if (tos_type == atos) {
              obj->obj_field_put(field_offset, STACK_OBJECT(-1));
              OrderAccess::release_store(&BYTE_MAP_BASE[(uintptr_t)obj >> CardTableModRefBS::card_shift], 0);
	    } else if (tos_type == btos) {
	      obj->byte_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == ltos) {
	      obj->long_field_put(field_offset, STACK_LONG(-1));
	    } else if (tos_type == ctos) {
	      obj->char_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == stos) {
	      obj->short_field_put(field_offset, STACK_INT(-1));
	    } else if (tos_type == ftos) {
	      obj->float_field_put(field_offset, STACK_FLOAT(-1));
	    } else {
	      obj->double_field_put(field_offset, STACK_DOUBLE(-1));
	    }
	  }

	  UPDATE_PC_AND_TOS_AND_CONTINUE(3, count);
        }  

      CASE(_new): {
        u2 index = Bytes::get_Java_u2(pc+1);
	constantPoolOop constants = istate->method()->constants();
	if (constants->tag_at(index).value() != JVM_CONSTANT_UnresolvedClass) {
	  // Make sure klass is initialized and doesn't have a finalizer
	  oop entry = (klassOop) *constants->obj_at_addr(index);
	  assert(entry->is_klass(), "Should be resolved klass");
	  klassOop k_entry = (klassOop) entry;
	  assert(k_entry->klass_part()->oop_is_instance(), "Should be instanceKlass");
	  instanceKlass* ik = (instanceKlass*) k_entry->klass_part();
	  if ( ik->is_initialized() && ik->can_be_fastpath_allocated() ) {
	    size_t obj_size = ik->size_helper();
	    oop result = NULL;
	    bool need_zero = false;
	    if (UseTLAB) {
              result = (oop) THREAD->tlab().allocate(obj_size);
	    }
	    if (result == NULL) {
              need_zero = true;
	      // Try allocate in shared eden
	retry:
	      HeapWord* compare_to = *Universe::heap()->top_addr();
	      HeapWord* new_top = compare_to + obj_size;
	      if (new_top <= *Universe::heap()->end_addr()) {
		if (Atomic::cmpxchg_ptr(new_top, Universe::heap()->top_addr(), compare_to) != compare_to) {
		  goto retry;
		}
		result = (oop) compare_to;
	      }
	    }
	    if (result != NULL) {
	      // Initialize object (if nonzero size and need) and then the header
	      if (need_zero ) {
		HeapWord* to_zero = (HeapWord*) result + sizeof(oopDesc) / oopSize;
		obj_size -= sizeof(oopDesc) / oopSize;
		if (obj_size > 0 ) {
		  memset(to_zero, 0, obj_size * HeapWordSize);
		}
	      }
	      result->set_mark();
	      result->set_klass(k_entry);
	      SET_STACK_OBJECT(result, 0);
	      UPDATE_PC_AND_TOS_AND_CONTINUE(3, 1);
	    }
	  }
	}
	// Slow case allocation
	CALL_VM(InterpreterRuntime::_new(THREAD, istate->method()->constants(), index),
		handle_exception);
	SET_STACK_OBJECT(THREAD->vm_result(), 0);
	THREAD->set_vm_result(NULL);
	UPDATE_PC_AND_TOS_AND_CONTINUE(3, 1);
      }
      CASE(_anewarray): {
        u2 index = Bytes::get_Java_u2(pc+1);
	jint size = STACK_INT(-1);
	CALL_VM(InterpreterRuntime::anewarray(THREAD, istate->method()->constants(), index, size),
		handle_exception);
	SET_STACK_OBJECT(THREAD->vm_result(), -1);
	THREAD->set_vm_result(NULL);
	UPDATE_PC_AND_CONTINUE(3);
      }
      CASE(_multianewarray): {
        jint dims = *(pc+3);
	jint size = STACK_INT(-1);
	// This is a somewhat lame interface and may not be correct for LP64
	CALL_VM(InterpreterRuntime::multianewarray(THREAD, (jint*) STACK_SLOT(-dims)),
		handle_exception);
	SET_STACK_OBJECT(THREAD->vm_result(), -dims);
	THREAD->set_vm_result(NULL);
	UPDATE_PC_AND_TOS_AND_CONTINUE(4, -(dims-1));
      }
      CASE(_checkcast):
          if (STACK_OBJECT(-1) != NULL) {
	    u2 index = Bytes::get_Java_u2(pc+1);
#ifndef CORE
	    if (ProfileInterpreter) {
	      // needs Profile_checkcast QQQ
	      ShouldNotReachHere();
	    }
#endif
	    // Constant pool may have actual klass or unresolved klass. If it is
	    // unresolved we must resolve it
	    if (istate->method()->constants()->tag_at(index).is_unresolved_klass()) {
	      CALL_VM(InterpreterRuntime::quicken_io_cc(THREAD), handle_exception);
	    }
	    klassOop klassOf = (klassOop) *(istate->method()->constants()->obj_at_addr(index));
	    klassOop objKlassOop = STACK_OBJECT(-1)->klass(); //ebx
	    //
	    // Check for compatibilty. This check must not GC!!
	    // Seems way more expensive now that we must dispatch
	    //
	    if (objKlassOop != klassOf && !objKlassOop->klass_part()->is_subtype_of(klassOf)) {
	      VM_JAVA_ERROR(vmSymbols::java_lang_ClassCastException(), "");
	    }
          } else {
#ifdef COMPILER2
            if (UncommonNullCast) {
//	      istate->method()->set_null_cast_seen();
// [RGV] Not sure what to do here!
	      ShouldNotReachHere();

	    }
#endif
	  }
	  UPDATE_PC_AND_CONTINUE(3);

      CASE(_instanceof):
          if (STACK_OBJECT(-1) == NULL) {
	    SET_STACK_INT(0, -1);
	  } else {
	    u2 index = Bytes::get_Java_u2(pc+1);
	    // Constant pool may have actual klass or unresolved klass. If it is
	    // unresolved we must resolve it
	    if (istate->method()->constants()->tag_at(index).is_unresolved_klass()) {
	      CALL_VM(InterpreterRuntime::quicken_io_cc(THREAD), handle_exception);
	    }
	    klassOop klassOf = (klassOop) *(istate->method()->constants()->obj_at_addr(index));
	    klassOop objKlassOop = STACK_OBJECT(-1)->klass();
	    //
	    // Check for compatibilty. This check must not GC!!
	    // Seems way more expensive now that we must dispatch
	    //
	    if ( objKlassOop == klassOf || objKlassOop->klass_part()->is_subtype_of(klassOf)) {
	      SET_STACK_INT(1, -1);
	    } else {
	      SET_STACK_INT(0, -1);
	    }
          }
	  UPDATE_PC_AND_CONTINUE(3);

      CASE(_ldc_w):
      CASE(_ldc):
        {
	  u2 index;
	  bool wide = false;
	  int incr = 2; // frequent case
	  if (opcode == Bytecodes::_ldc) {
	    index = pc[1];
	  } else {
	    index = Bytes::get_Java_u2(pc+1);
	    incr = 3;
	    wide = true;
	  }

	  constantPoolOop constants = istate->method()->constants();
	  switch (constants->tag_at(index).value()) {
	  case JVM_CONSTANT_Integer:
	    SET_STACK_INT(constants->int_at(index), 0);
	    break;

	  case JVM_CONSTANT_Float:
	    SET_STACK_FLOAT(constants->float_at(index), 0);
	    break;

	  case JVM_CONSTANT_String:
	    SET_STACK_OBJECT(constants->resolved_string_at(index), 0);
	    break;

	  case JVM_CONSTANT_Class:
	    SET_STACK_OBJECT(constants->resolved_klass_at(index)->klass_part()->java_mirror(), 0);
	    break;

	  case JVM_CONSTANT_UnresolvedString:
	  case JVM_CONSTANT_UnresolvedClass:
	    CALL_VM(InterpreterRuntime::ldc(THREAD, wide), handle_exception);
	    SET_STACK_OBJECT(THREAD->vm_result(), 0);
	    THREAD->set_vm_result(NULL);
	    break;

	  default:  ShouldNotReachHere();
	  }
	  UPDATE_PC_AND_TOS_AND_CONTINUE(incr, 1);
	}

      CASE(_ldc2_w):
        {
	  u2 index = Bytes::get_Java_u2(pc+1);

	  constantPoolOop constants = istate->method()->constants();
	  switch (constants->tag_at(index).value()) {

	  case JVM_CONSTANT_Long:
	     SET_STACK_LONG(constants->long_at(index), 1);
	    break;

	  case JVM_CONSTANT_Double:
	     SET_STACK_DOUBLE(constants->double_at(index), 1);
	    break;
	  default:  ShouldNotReachHere();
	  }
	  UPDATE_PC_AND_TOS_AND_CONTINUE(3, 2);
	}

      CASE(_invokeinterface): {
        u2 index = Bytes::get_native_u2(pc+1);

	// QQQ Need to make this as inlined as possible. Probably need to split all the bytecode cases
	// out so c++ compiler has a chance for constant prop to fold everything possible away.

	ConstantPoolCacheEntry* cache = cp->entry_at(index);
	if (!cache->is_resolved((Bytecodes::Code)opcode)) {
	  CALL_VM(InterpreterRuntime::resolve_invoke(THREAD, (Bytecodes::Code)opcode), 
		  handle_exception);
	  cache = cp->entry_at(index);
	}

	istate->set_msg(call_method);

	// Special case of invokeinterface called for virtual method of
	// java.lang.Object.  See cpCacheOop.cpp for details.
	// This code isn't produced by javac, but could be produced by
	// another compliant java compiler.
	if (cache->is_methodInterface()) {
	  methodOop callee;
	  CHECK_NULL(STACK_OBJECT(-(cache->parameter_size())));
	  if (cache->is_vfinal()) {
            callee = (methodOop) cache->f2();
	  } else {
	    // get receiver
	    int parms = cache->parameter_size();
            // Same comments as invokevirtual apply here
	    instanceKlass* rcvrKlass = (instanceKlass*)
                                 STACK_OBJECT(-parms)->klass()->klass_part();
	    callee = (methodOop) rcvrKlass->start_of_vtable()[ cache->f2()];
	  }
	  istate->set_callee(callee);
	  istate->set_callee_entry_point(callee->interpreter_entry());
	  istate->set_bcp_advance(5);
	  UPDATE_PC_AND_RETURN(0); // I'll be back...
	}

	// this could definitely be cleaned up QQQ
	methodOop callee;
	klassOop iclass = (klassOop)cache->f1();
	// instanceKlass* interface = (instanceKlass*) iclass->klass_part();
	// get receiver
	int parms = cache->parameter_size();
	oop rcvr = STACK_OBJECT(-parms);
	CHECK_NULL(rcvr); 
	instanceKlass* int2 = (instanceKlass*) rcvr->klass()->klass_part();
	itableOffsetEntry* ki = (itableOffsetEntry*) int2->start_of_itable();
	int i;
	for ( i = 0 ; i < int2->itable_length() ; i++, ki++ ) {
	  if (ki->interface_klass() == iclass) break;
	}
        // If the interface isn't found, this class doesn't implement this
        // interface.  The link resolver checks this but only for the first
        // time this interface is called.
	if (i == int2->itable_length()) {
	  VM_JAVA_ERROR(vmSymbols::java_lang_IncompatibleClassChangeError(), "");
        }
	int mindex = cache->f2();
	itableMethodEntry* im = ki->first_method_entry(rcvr->klass());
	callee = im[mindex].method();
	if (callee == NULL) {
	  VM_JAVA_ERROR(vmSymbols::java_lang_AbstractMethodError(), "");
	}
	
	istate->set_callee(callee);
	istate->set_callee_entry_point(callee->interpreter_entry());
	istate->set_bcp_advance(5);
	UPDATE_PC_AND_RETURN(0); // I'll be back...
      }

      CASE(_invokevirtual):
      CASE(_invokespecial):
      CASE(_invokestatic): {
        u2 index = Bytes::get_native_u2(pc+1);

	ConstantPoolCacheEntry* cache = cp->entry_at(index);
	// QQQ Need to make this as inlined as possible. Probably need to split all the bytecode cases
	// out so c++ compiler has a chance for constant prop to fold everything possible away.

	if (!cache->is_resolved((Bytecodes::Code)opcode)) {
	  CALL_VM(InterpreterRuntime::resolve_invoke(THREAD, (Bytecodes::Code)opcode), 
		  handle_exception);
	  cache = cp->entry_at(index);
	}
     
	istate->set_msg(call_method);
        {
	  methodOop callee;
	  if ((Bytecodes::Code)opcode == Bytecodes::_invokevirtual) {
	    CHECK_NULL(STACK_OBJECT(-(cache->parameter_size())));
	    if (cache->is_vfinal()) callee = (methodOop) cache->f2();
	    else {
	      // get receiver
	      int parms = cache->parameter_size();
	      // this works but needs a resourcemark and seems to create a vtable on every call:
	      // methodOop callee = rcvr->klass()->klass_part()->vtable()->method_at(cache->f2());
	      // 
	      // this fails with an assert
	      // instanceKlass* rcvrKlass = instanceKlass::cast(STACK_OBJECT(-parms)->klass());
	      // but this works
	      instanceKlass* rcvrKlass = (instanceKlass*) STACK_OBJECT(-parms)->klass()->klass_part();
	      /*
		Executing this code in java.lang.String:
		    public String(char value[]) {
			  this.count = value.length;
			  this.value = (char[])value.clone();
		     }

		 a find on rcvr->klass()->klass_part() reports:
		 {type array char}{type array class} 
		  - klass: {other class}

		  but using instanceKlass::cast(STACK_OBJECT(-parms)->klass()) causes in assertion failure
		  because rcvr->klass()->klass_part()->oop_is_instance() == 0
		  However it seems to have a vtable in the right location. Huh?

	      */
	      callee = (methodOop) rcvrKlass->start_of_vtable()[ cache->f2()];
	    }
	  } else {
	    if ((Bytecodes::Code)opcode == Bytecodes::_invokespecial) {
	      CHECK_NULL(STACK_OBJECT(-(cache->parameter_size())));
	    }
	    callee = (methodOop) cache->f1();
	  }

	  istate->set_callee(callee);
	  istate->set_callee_entry_point(callee->interpreter_entry());
	  istate->set_bcp_advance(3);
	  UPDATE_PC_AND_RETURN(0); // I'll be back...
        }
      }

      /* Allocate memory for a new java object. */

      CASE(_newarray): {
        BasicType atype = (BasicType) *(pc+1);
	jint size = STACK_INT(-1);
	CALL_VM(InterpreterRuntime::newarray(THREAD, atype, size),
		handle_exception);
	SET_STACK_OBJECT(THREAD->vm_result(), -1);
	THREAD->set_vm_result(NULL);

	UPDATE_PC_AND_CONTINUE(2);
      }

      /* Throw an exception. */

      CASE(_athrow): {
	  oop except_oop = STACK_OBJECT(-1);
	  CHECK_NULL(except_oop);
	  // set pending_exception so we use common code
	  THREAD->set_pending_exception(except_oop, NULL, 0);
	  goto handle_exception;
      }

      /* goto and jsr. They are exactly the same except jsr pushes
       * the address of the next instruction first.
       */

      CASE(_jsr): {
          /* push bytecode index on stack */
          SET_STACK_ADDR(((address)pc - (intptr_t)(istate->method()->code_base()) + 3), 0);
          MORE_STACK(1);
          /* FALL THROUGH */
      }

      CASE(_goto):
      {
          int16_t offset = (int16_t)Bytes::get_Java_u2(pc + 1);
          UPDATE_PC_AND_CONTINUE_WITH_BACKWARDS_CHECK(offset);
      }

      CASE(_jsr_w): {
          /* push return address on the stack */
          SET_STACK_ADDR(((address)pc - (intptr_t)(istate->method()->code_base()) + 5), 0);
          MORE_STACK(1);
          /* FALL THROUGH */
      }

      CASE(_goto_w):
      {
          int32_t offset = Bytes::get_Java_u4(pc + 1);
          UPDATE_PC_AND_CONTINUE_WITH_BACKWARDS_CHECK(offset);
      }

      /* return from a jsr or jsr_w */

      CASE(_ret): {
          pc = istate->method()->code_base() + (intptr_t)(LOCALS_ADDR(pc[1]));
          UPDATE_PC_AND_CONTINUE(0);
      }

      /* debugger breakpoint */

      CASE(_breakpoint): {
          Bytecodes::Code original_bytecode;
          DECACHE_STATE();                                        
          SET_LAST_JAVA_FRAME();                                  
	  original_bytecode = InterpreterRuntime::get_original_bytecode_at(THREAD, 
                              istate->method(), pc);
          RESET_LAST_JAVA_FRAME();
          CACHE_STATE();
          if (THREAD->pending_exception()) goto handle_exception;
	    CALL_VM(InterpreterRuntime::_breakpoint(THREAD, istate->method(), pc),
						    handle_exception);

	  opcode = (jubyte)original_bytecode;
          goto opcode_switch;
      }

      DEFAULT:
          fatal2("\t*** Unimplemented opcode: %d = %s\n",
                 opcode, Bytecodes::name((Bytecodes::Code)opcode));
          goto finish;

      } /* switch(opc) */

      
#ifdef USELABELS
    check_for_exception: 
#endif
    {
      if (!THREAD->has_pending_exception()) {
	CONTINUE;
      }
      /* We will be gcsafe soon, so flush our state. */
      DECACHE_PC();
      goto handle_exception;
    }
  do_continue: ;

  } /* while (1) interpreter loop */


  // An exception exists in the thread state see whether this activation can handle it
  handle_exception: {

    HandleMarkCleaner __hmc(THREAD);
    Handle except_oop(THREAD, THREAD->pending_exception());
    // Prevent any subsequent HandleMarkCleaner in the VM 
    // from freeing the except_oop handle.
    HandleMark __hm(THREAD);

    THREAD->clear_pending_exception();
    assert(except_oop(), "No exception to process");
    intptr_t continuation_bci;
    // expression stack is emptied
    topOfStack = istate->stack_base() - 1;
    CALL_VM(continuation_bci = (intptr_t)InterpreterRuntime::exception_handler_for_exception(THREAD, except_oop()), 
	    handle_exception);

    except_oop = (oop) THREAD->vm_result();
    THREAD->set_vm_result(NULL);
    if (continuation_bci >= 0) {
      // Place exception on top of stack
      SET_STACK_OBJECT(except_oop(), 0);
      MORE_STACK(1);
      pc = istate->method()->code_base() + continuation_bci;
      if (TraceExceptions) {
        ttyLocker ttyl;
	ResourceMark rm;
	tty->print_cr("Exception <%s> (" INTPTR_FORMAT ")", except_oop->print_value_string(), except_oop());
        tty->print_cr(" thrown in interpreter method <%s>", istate->method()->print_value_string());
        tty->print_cr(" at bci %d, continuing at %d for thread " INTPTR_FORMAT,
		      pc - (intptr_t)istate->method()->code_base(),
		      continuation_bci, THREAD);
      }
      // for AbortVMOnException flag
      NOT_PRODUCT(Exceptions::debug_check_abort(except_oop));
      goto run;
    }
    if (TraceExceptions) {
      ttyLocker ttyl;
      ResourceMark rm;
      tty->print_cr("Exception <%s> (" INTPTR_FORMAT ")", except_oop->print_value_string(), except_oop());
      tty->print_cr(" thrown in interpreter method <%s>", istate->method()->print_value_string());
      tty->print_cr(" at bci %d, unwinding for thread " INTPTR_FORMAT,
		    pc  - (intptr_t) istate->method()->code_base(),
		    THREAD);
    }
    // for AbortVMOnException flag
    NOT_PRODUCT(Exceptions::debug_check_abort(except_oop));
    // No handler in this activation, unwind and try again
    THREAD->set_pending_exception(except_oop(), NULL, 0);
    goto handle_return;
  }  /* handle_exception: */
      


  // Return from an interpreter invocation with the result of the interpretation
  // on the top of the Java Stack (or a pending exception)

handle_Pop_Frame:

  // We don't really do anything special here except we must be aware
  // that we can get here without ever locking the method (if sync).
  // Also we skip the notification of the exit.

  istate->set_msg(popping_frame);
  // Clear pending so while the pop is in process
  // we don't start another one if a call_vm is done.
  THREAD->clr_pop_frame_pending();
  // Let interpreter (only) see the we're in the process of popping a frame
  THREAD->set_pop_frame_in_process();

handle_return:
  {
    DECACHE_STATE();

    bool suppress_error = istate->msg() == popping_frame;
    bool suppress_exit_event = THREAD->has_pending_exception() || suppress_error;
    Handle original_exception(THREAD, THREAD->pending_exception());
    Handle illegal_state_oop(THREAD, NULL);

    // We'd like a HandleMark here to prevent any subsequent HandleMarkCleaner
    // in any following VM entries from freeing our live handles, but illegal_state_oop
    // isn't really allocated yet and so doesn't become live until later and
    // in unpredicatable places. Instead we must protect the places where we enter the
    // VM. It would be much simpler (and safer) if we could allocate a real handle with
    // a NULL oop in it and then overwrite the oop later as needed. This isn't
    // unfortunately isn't possible.

    THREAD->clear_pending_exception();

    //
    // As far as we are concerned we have returned. If we have a pending exception
    // that will be returned as this invocation's result. However if we get any
    // exception(s) while checking monitor state one of those IllegalMonitorStateExceptions
    // will be our final result (i.e. monitor exception trumps a pending exception).
    //

    // If we never locked the method (or really passed the point where we would have),
    // there is no need to unlock it (or look for other monitors), since that
    // could not have happened.

    if (!THREAD->do_not_unlock()) {
      // At this point we consider that we have returned. We now check that the
      // locks were properly block structured. If we find that they were not
      // used properly we will return with an illegal monitor exception.
      // The exception is checked by the caller not the callee since this
      // checking is considered to be part of the invocation and therefore
      // in the callers scope (JVM spec 8.13).
      //
      // Another weird thing to watch for is if the method was locked
      // recursively and then not exited properly. This means we must
      // examine all the entries in reverse time(and stack) order and
      // unlock as we find them. If we find the method monitor before
      // we are at the initial entry then we should throw an exception.
      // It is not clear the template based interpreter does this
      // correctly
	
      BasicObjectLock* base = istate->monitor_base();
      BasicObjectLock* end = (BasicObjectLock*) istate->stack_base();
      bool method_unlock_needed = istate->method()->is_synchronized();
      // We know the initial monitor was used for the method don't check that
      // slot in the loop
      if (method_unlock_needed) base--;

      // Check all the monitors to see they are unlocked. Install exception if found to be locked.
      while (end < base) {
	oop lockee = end->obj();
	if (lockee != NULL) {
	  BasicLock* lock = end->lock();
	  markOop header = lock->displaced_header();
	  end->set_obj(NULL);
	  // If it isn't recursive we either must swap old header or call the runtime
	  if (header != NULL) {
	    if (Atomic::cmpxchg_ptr(header, lockee->mark_addr(), lock) != lock) {
	      // restore object for the slow case
	      end->set_obj(lockee);
	      {
		// Prevent any HandleMarkCleaner from freeing our live handles
		HandleMark __hm(THREAD); 
		CALL_VM_NOCHECK(InterpreterRuntime::monitorexit(THREAD, end));
	      }
	    }
	  }
	  // One error is plenty
	  if (illegal_state_oop() == NULL && !suppress_error) {
	    {
	      // Prevent any HandleMarkCleaner from freeing our live handles
	      HandleMark __hm(THREAD); 
	      CALL_VM_NOCHECK(InterpreterRuntime::throw_illegal_monitor_state_exception(THREAD));
	    }
	    assert(THREAD->has_pending_exception(), "Lost our exception!");
	    illegal_state_oop = THREAD->pending_exception();
	    THREAD->clear_pending_exception();
	  }
	}
	end++;
      }
      // Unlock the method if needed
      if (method_unlock_needed) {
	if (base->obj() == NULL) {
	  // The method is already unlocked this is not good.
	  if (illegal_state_oop() == NULL && !suppress_error) {
	    {
	      // Prevent any HandleMarkCleaner from freeing our live handles
	      HandleMark __hm(THREAD); 
	      CALL_VM_NOCHECK(InterpreterRuntime::throw_illegal_monitor_state_exception(THREAD));
	    }
	    assert(THREAD->has_pending_exception(), "Lost our exception!");
	    illegal_state_oop = THREAD->pending_exception();
	    THREAD->clear_pending_exception();
	  }
	} else {
	  //
	  // The initial monitor is always used for the method
	  // However if that slot is no longer the oop for the method it was unlocked
	  // and reused by something that wasn't unlocked!
	  //
	  // deopt can come in with rcvr dead because c2 knows
	  // its value is preserved in the monitor. So we can't use locals[0] at all
	  // and must use first monitor slot.
	  //
	  oop rcvr = base->obj();
	  if (rcvr == NULL) {
	    if (!suppress_error) {
              VM_JAVA_ERROR_NO_JUMP(vmSymbols::java_lang_NullPointerException(), "");
	      illegal_state_oop = THREAD->pending_exception();
	      THREAD->clear_pending_exception();
	    }
	  } else {
	    BasicLock* lock = base->lock();
	    markOop header = lock->displaced_header();
	    base->set_obj(NULL);
	    // If it isn't recursive we either must swap old header or call the runtime
	    if (header != NULL) {
	      if (Atomic::cmpxchg_ptr(header, rcvr->mark_addr(), lock) != lock) {
		// restore object for the slow case
		base->set_obj(rcvr);
		{
		  // Prevent any HandleMarkCleaner from freeing our live handles
		  HandleMark __hm(THREAD); 
		  CALL_VM_NOCHECK(InterpreterRuntime::monitorexit(THREAD, base));
		}
		if (THREAD->has_pending_exception()) {
		  if (!suppress_error) illegal_state_oop = THREAD->pending_exception();
		  THREAD->clear_pending_exception();
		}
	      }
	    }
	  }
	}
      }
    }

    //
    // Notify jvmti/jvmdi/jvmpi
    //
    // NOTE: we do not notify a method_exit if we have a pending exception,
    // including an exception we generate for unlocking checks.  In the former
    // case, JVMDI has already been notified by our call for the exception handler
    // and in both cases as far as JVMDI is concerned we have already returned.
    // If we notify it again JVMDI will be all confused about how many frames
    // are still on the stack (4340444).
    //
    // Further note that jvmpi does not suppress method_exit notifications
    // in the case of exceptions (which makes more sense to me). See bug
    // 4933156
    //
    // NOTE Further! It turns out the the JVMTI spec in fact expects to see
    // method_exit events whenever we leave an activation unless it was done
    // for popframe. This is just like jvmpi and nothing like jvmdi. However
    // we are passing the tests at the moment (apparently becuase they are
    // jvmdi based) so rather than change this code and possibly fail tests
    // we will leave it alone (with this note) in anticipation of changing
    // the vm and the tests simultaneously.


    //
    suppress_exit_event = suppress_exit_event || illegal_state_oop() != NULL;



#ifdef VM_JVMTI
      if (_jvmti_interp_events) {
        // Whenever JVMTI puts a thread in interp_only_mode, method
        // entry/exit events are sent for that thread to track stack depth.  
        if ( !suppress_exit_event && THREAD->is_interp_only_mode() ) {
	  {
	    // Prevent any HandleMarkCleaner from freeing our live handles
	    HandleMark __hm(THREAD); 
	    CALL_VM_NOCHECK(InterpreterRuntime::post_method_exit(THREAD));
	  }
        }
      }
#endif /* VM_JVMTI */

    /* Only suppress method_exit events for jvmpi if we are doing a popFrame */
    if ( istate->msg() != popping_frame && *jvmpi::event_flags_array_at_addr(JVMPI_EVENT_METHOD_EXIT) == JVMPI_EVENT_ENABLED) {
      {
	// Prevent any HandleMarkCleaner from freeing our live handles
	HandleMark __hm(THREAD); 
	CALL_VM_NOCHECK(SharedRuntime::jvmpi_method_exit(THREAD, istate->method()))
      }
    }

    //
    // See if we are returning any exception
    // A pending exception that was pending prior to a possible popping frame
    // overrides the popping frame.
    //
    assert(!suppress_error || suppress_error && illegal_state_oop() == NULL, "Error was not suppressed");
    if (illegal_state_oop() != NULL || original_exception() != NULL) {
      // inform the frame manager we have no result
      istate->set_msg(throwing_exception);
      if (illegal_state_oop() != NULL) 
	THREAD->set_pending_exception(illegal_state_oop(), NULL, 0);
      else
	THREAD->set_pending_exception(original_exception(), NULL, 0);
      istate->set_return_kind((Bytecodes::Code)opcode);
      UPDATE_PC_AND_RETURN(0);
    }

    if (istate->msg() == popping_frame) {
      // Make it simpler on the assembly code and set the message for the frame pop.
      // returns
      if (istate->prev() == NULL) {
	// We must be returning to a deoptimized frame (because popframe only happens between
	// two interpreted frames). We need to save the current arguments in C heap so that
	// the deoptimized frame when it restarts can copy the arguments to its expression
	// stack and re-execute the call. We also have to notify deoptimization that this
	// has occured and to pick the preerved args copy them to the deoptimized frame's
	// java expression stack. Yuck.
	//
#ifndef CORE
	THREAD->popframe_preserve_args(in_ByteSize(istate->method()->size_of_parameters() * wordSize),
	                        LOCALS_SLOT(istate->method()->size_of_parameters() - 1));
	THREAD->set_popframe_condition_bit(JavaThread::popframe_force_deopt_reexecution_bit);
#else
         assert(false, "must return to interpreted frame");
#endif
      }
      UPDATE_PC_AND_RETURN(1);
    } else {
      // Normal return
      // Advance the pc and return to frame manager
      istate->set_msg(return_from_method);
      istate->set_return_kind((Bytecodes::Code)opcode);
      UPDATE_PC_AND_RETURN(1);
    }
  } /* handle_return: */

// This is really a fatal error return

finish:
  DECACHE_TOS();
  DECACHE_PC();

  return;
}

#endif // CC_INTERP

