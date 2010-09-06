#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)c1_MacroAssembler_i486.cpp	1.39 03/12/23 16:36:09 JVM"
#endif
// 
// Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
// SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
// 

#include "incls/_precompiled.incl"
#include "incls/_c1_MacroAssembler_i486.cpp.incl"

void C1_MacroAssembler::lock_object(Register hdr, Register obj, Register disp_hdr, Label& slow_case) {
  const int aligned_mask = 3;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(hdr == eax, "hdr must be eax for the cmpxchg instruction");
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  assert(BytesPerWord == 4, "adjust aligned_mask and code");
  Label done;
  // The following move must be the first instruction of emitted since debug
  // information may be generated for it.
  // Load object header
  movl(hdr, Address(obj, hdr_offset));

  verify_oop(obj);

#ifdef ASSERT
  {
    Label ok;

    movl(hdr, Address(obj));
    cmpl(disp_hdr, hdr);
    jcc(Assembler::notEqual, ok);
    stop("bad recursive lock");
    bind(ok);
  }
#endif

  // save object being locked into the BasicObjectLock
  movl(Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()), obj);
  // and mark it as unlocked
  orl(hdr, markOopDesc::unlocked_value);
  // save unlocked object header into the displaced header location on the stack
  movl(Address(disp_hdr), hdr);
  // test if object header is still the same (i.e. unlocked), and if so, store the
  // displaced header address in the object header - if it is not the same, get the
  // object header instead
  if (os::is_MP()) MacroAssembler::lock(); // must be immediately before cmpxchg!
  cmpxchg(disp_hdr, Address(obj, hdr_offset));
  // if the object header was the same, we're done
  jcc(Assembler::equal, done);
  // if the object header was not the same, it is now in the hdr register
  // => test if it is a stack pointer into the same stack (recursive locking), i.e.:
  //
  // 1) (hdr & aligned_mask) == 0
  // 2) esp <= hdr
  // 3) hdr <= esp + page_size
  //
  // these 3 tests can be done by evaluating the following expression:
  //
  // (hdr - esp) & (aligned_mask - page_size)
  //
  // assuming both the stack pointer and page_size have their least
  // significant 2 bits cleared and page_size is a power of 2
  subl(hdr, esp);
  andl(hdr, aligned_mask - os::vm_page_size());
  // for recursive locking, the result is zero => save it in the displaced header
  // location (NULL in the displaced hdr location indicates recursive locking)
  movl(Address(disp_hdr), hdr);
  // otherwise we don't care about the result and handle locking via runtime call
  jcc(Assembler::notZero, slow_case);
  // done
  bind(done);
}


void C1_MacroAssembler::unlock_object(Register hdr, Register obj, Register disp_hdr, Label& slow_case) {
  const int aligned_mask = 3;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(disp_hdr == eax, "disp_hdr must be eax for the cmpxchg instruction");
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  assert(BytesPerWord == 4, "adjust aligned_mask and code");
  Label done;
  // load displaced header
  movl(hdr, Address(disp_hdr));
  // if the loaded hdr is NULL we had recursive locking
  testl(hdr, hdr);
  // if we had recursive locking, we are done
  jcc(Assembler::zero, done);
  // load object
  movl(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));
  verify_oop(obj);
  // test if object header is pointing to the displaced header, and if so, restore
  // the displaced header in the object - if the object header is not pointing to
  // the displaced header, get the object header instead
  if (os::is_MP()) MacroAssembler::lock(); // must be immediately before cmpxchg!
  cmpxchg(hdr, Address(obj, hdr_offset));
  // if the object header was not pointing to the displaced header,
  // we do unlocking via runtime call
  jcc(Assembler::notEqual, slow_case);
  // done
  bind(done);
}


// Defines obj, preserves var_size_in_bytes
void C1_MacroAssembler::try_allocate(Register obj, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2, Label& slow_case) {
  if (UseTLAB) {
    tlab_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, t2, slow_case);
  } else {
    eden_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, slow_case);
  }
}


void C1_MacroAssembler::initialize_header(Register obj, Register klass, Register len) {
  assert_different_registers(obj, klass, len);
  movl(Address(obj, oopDesc::mark_offset_in_bytes ()), (int)markOopDesc::prototype());
  movl(Address(obj, oopDesc::klass_offset_in_bytes()), klass);
  if (len->is_valid()) {
    movl(Address(obj, arrayOopDesc::length_offset_in_bytes()), len);
  }
}


// preserves obj, destroys len_in_bytes
void C1_MacroAssembler::initialize_body(Register obj, Register len_in_bytes, int hdr_size_in_bytes, Register t1) {
  Label done;
  assert(obj != len_in_bytes && obj != t1 && t1 != len_in_bytes, "registers must be different");
  assert((hdr_size_in_bytes & (BytesPerWord - 1)) == 0, "header size is not a multiple of BytesPerWord");
  Register index = len_in_bytes;
  subl(index, hdr_size_in_bytes);
  jcc(Assembler::zero, done);
  // initialize topmost word, divide index by 2, check if odd and test if zero
  // note: for the remaining code to work, index must be a multiple of BytesPerWord
#ifdef ASSERT
  { Label L;
    testl(index, BytesPerWord - 1);
    jcc(Assembler::zero, L);
    stop("index is not a multiple of BytesPerWord");
    bind(L);
  }
#endif
  xorl(t1, t1);      // use _zero reg to clear memory (shorter code)
  shrl(index, 3);    // divide by 8 and set carry flag if bit 2 was set
  // index could have been not a multiple of 8 (i.e., bit 2 was set)
  { Label even;
    // note: if index was a multiple of 8, than it cannot
    //       be 0 now otherwise it must have been 0 before
    //       => if it is even, we don't need to check for 0 again
    jcc(Assembler::carryClear, even);
    // clear topmost word (no jump needed if conditional assignment would work here)
    movl(Address(obj, index, Address::times_8, hdr_size_in_bytes - 0*BytesPerWord), t1);
    // index could be 0 now, need to check again
    jcc(Assembler::zero, done);
    bind(even);
  }
  // initialize remaining object fields: edx is a multiple of 2 now
  { Label loop;
    bind(loop);
    movl(Address(obj, index, Address::times_8, hdr_size_in_bytes - 1*BytesPerWord), t1);
    movl(Address(obj, index, Address::times_8, hdr_size_in_bytes - 2*BytesPerWord), t1);
    decl(index);
    jcc(Assembler::notZero, loop);
  }

  // done
  bind(done);
}


void C1_MacroAssembler::allocate_object(Register obj, Register t1, Register t2, int header_size, int object_size, Register klass, Label& slow_case) {
  assert(obj == eax, "obj must be in eax for cmpxchg");
  assert(obj != t1 && obj != t2 && t1 != t2, "registers must be different");
  assert(header_size >= 0 && object_size >= header_size, "illegal sizes");

  try_allocate(obj, noreg, object_size * BytesPerWord, t1, t2, slow_case);

  initialize_object(obj, klass, noreg, object_size * HeapWordSize, t1, t2);
}

void C1_MacroAssembler::initialize_object(Register obj, Register klass, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2) {
  assert((con_size_in_bytes & MinObjAlignmentInBytesMask) == 0,
         "con_size_in_bytes is not multiple of alignment");
  const int hdr_size_in_bytes = oopDesc::header_size_in_bytes();

  initialize_header(obj, klass, noreg);

  // clear rest of allocated space
  const Register t1_zero = t1;
  const Register index = t2;
  const int threshold = 6;   // approximate break even point for code size (see comments below)
  if (var_size_in_bytes != noreg) {
    movl(index, var_size_in_bytes);
    initialize_body(obj, index, hdr_size_in_bytes, t1_zero);
  } else if (con_size_in_bytes <= threshold) {
    // use explicit null stores
    // code size = 2 + 3*n bytes (n = number of fields to clear)
    xorl(t1_zero, t1_zero); // use t1_zero reg to clear memory (shorter code)
    for (int i = hdr_size_in_bytes; i < con_size_in_bytes; i++) 
      movl(Address(obj, i), t1_zero);
  } else if (con_size_in_bytes > hdr_size_in_bytes) {
    // use loop to null out the fields
    // code size = 16 bytes for even n (n = number of fields to clear)
    // initialize last object field first if odd number of fields
    xorl(t1_zero, t1_zero); // use t1_zero reg to clear memory (shorter code)
    movl(index, (con_size_in_bytes - hdr_size_in_bytes) >> 3);
    // initialize last object field if constant size is odd
    if (((con_size_in_bytes - hdr_size_in_bytes) & 4) != 0) 
      movl(Address(obj, con_size_in_bytes - (1*BytesPerWord)), t1_zero);
    // initialize remaining object fields: edx is a multiple of 2
    { Label loop;
      bind(loop);
      movl(Address(obj, index, Address::times_8, 
	hdr_size_in_bytes - (1*BytesPerWord)), t1_zero);
      movl(Address(obj, index, Address::times_8, 
	hdr_size_in_bytes - (2*BytesPerWord)), t1_zero);
      decl(index);
      jcc(Assembler::notZero, loop);
    }
  }
  verify_oop(obj);
}

void C1_MacroAssembler::allocate_array(Register obj, Register len, Register t1, Register t2, int header_size, Address::ScaleFactor f, Register klass, Label& slow_case) {
  assert(obj == eax, "obj must be in eax for cmpxchg");
  assert_different_registers(obj, len, t1, t2, klass);

  // determine alignment mask
  assert(BytesPerWord == 4, "must be a multiple of 2 for masking code to work");

  // check for negative or excessive length
  const int max_length = 0x00FFFFFF;
  cmpl(len, max_length);
  jcc(Assembler::above, slow_case);

  const Register arr_size = t2; // okay to be the same
  // align object end
  movl(arr_size, header_size * BytesPerWord + MinObjAlignmentInBytesMask);
  leal(arr_size, Address(arr_size, len, f));
  andl(arr_size, ~MinObjAlignmentInBytesMask);

  try_allocate(obj, arr_size, 0, t1, t2, slow_case);

  initialize_header(obj, klass, len);

  // clear rest of allocated space
  const Register len_zero = len;
  initialize_body(obj, arr_size, header_size * BytesPerWord, len_zero);
  verify_oop(obj);
}



void C1_MacroAssembler::inline_cache_check(Register receiver, Register iCache) {
  verify_oop(receiver);
  // explicit NULL check not needed since load from [klass_offset] causes a trap  
  // check against inline cache
  assert(!MacroAssembler::needs_explicit_null_check(oopDesc::klass_offset_in_bytes()), "must add explicit null check");
  int start_offset = offset();
  cmpl(iCache, Address(receiver, oopDesc::klass_offset_in_bytes())); 
  // if icache check fails, then jump to runtime routine
  // Note: RECEIVER must still contain the receiver!
  jcc(Assembler::notEqual, Runtime1::entry_for(Runtime1::handle_ic_miss_id), relocInfo::runtime_call_type); 
  assert(offset() - start_offset == 9, "check alignment in emit_method_entry");
}


void C1_MacroAssembler::method_exit(bool restore_frame) {
  if (restore_frame) {
    leave();
  }
  address ret_addr = pc();
  ret(0);
  if (!SafepointPolling) {
    // for compatibility reasons with compiler 2, we must emit ret_start + 1
    // as pc address; safepoint handling is substracting one from that relocInfo (!!!)
    // we do not need oop maps, and pc/bci mapping here
    code()->relocate(ret_addr + 1, relocInfo::return_type);
    // add two nops so that return can be patched for compiler safepoint
    nop ();
    nop ();
  }
}


void C1_MacroAssembler::build_frame(int frame_size_in_bytes) {
  // Make sure there is enough stack space for this method's activation.
  // Note that we do this before doing an enter(). This matches the
  // ordering of C2's stack overflow check / esp decrement and allows
  // the SharedRuntime stack overflow handling to be consistent
  // between the two compilers.
  generate_stack_overflow_check(frame_size_in_bytes);

  enter();
  decrement(esp, frame_size_in_bytes); // does not emit code for frame_size == 0
}


void C1_MacroAssembler::exception_handler(bool has_Java_exception_handler, int frame_size_in_bytes) {
  if (has_Java_exception_handler) {
#ifdef ASSERT
    // make sure the frame size hasn't changed during execution of this method.
    // this code can go away once we're sure we've removed all the pushes and
    // pops in our generated code.
    movl(esi, esp);
    addl(esi, frame_size_in_bytes);
    cmpl(ebp, esi);
    Label ok;
    jcc(Assembler::equal, ok);
    stop("stacks don't match");
    should_not_reach_here();
    bind(ok);
#endif // ASSERT
    // exception handler exist
    // => check if handler for particular exception exists,
    //    continue at exception handler if so, return otherwise
    // eax: exception
    // edx: throwing pc 
    call(Runtime1::entry_for(Runtime1::handle_exception_id), relocInfo::runtime_call_type);
    // exception handler for particular exception doesn't exist
    // => unwind activation and forward exception to caller
    // eax: exception
  } else {
    // There is no handler in this method, we continue in caller
  }
}


// Tries fast Object.hashCode access; 
void C1_MacroAssembler::fast_ObjectHashCode(Register receiver, Register result) {
  Label slowCase;
  assert(result != receiver, "do not destroy receiver");
  Register hdr = result; // will also be the result
  movl(hdr, Address(receiver, oopDesc::mark_offset_in_bytes()));

  // check if locked
  testl (hdr, markOopDesc::unlocked_value);
  jcc (Assembler::zero, slowCase);

  // get hash
  andl (hdr, markOopDesc::hash_mask_in_place);
  // test if hashCode exists
  jcc  (Assembler::zero, slowCase);
  shrl (result, markOopDesc::hash_shift);
  method_exit(false);
  bind (slowCase);
}


void C1_MacroAssembler::unverified_entry(Register receiver, Register ic_klass) {
  if (C1Breakpoint) int3();
  inline_cache_check(receiver, ic_klass);
}


void C1_MacroAssembler::verified_entry() {
  if (C1Breakpoint)int3();
  // build frame
  verify_FPU(0, "method_entry");
}
