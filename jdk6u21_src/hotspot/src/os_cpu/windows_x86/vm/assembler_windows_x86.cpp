/*
 * Copyright (c) 1999, 2008, Oracle and/or its affiliates. All rights reserved.
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

#include "incls/_precompiled.incl"
#include "incls/_assembler_windows_x86.cpp.incl"


void MacroAssembler::int3() {
  emit_byte(0xCC);
}

#ifndef _LP64
//  The current scheme to accelerate access to the thread
//  pointer is to store the current thread in the os_exception_wrapper
//  and reference the current thread from stubs and compiled code
//  via the FS register.  FS[0] contains a pointer to the structured
//  exception block which is actually a stack address.  The first time
//  we call the os exception wrapper, we calculate and store the
//  offset from this exception block and use that offset here.
//
//  The last mechanism we used was problematic in that the
//  the offset we had hard coded in the VM kept changing as Microsoft
//  evolved the OS.
//
// Warning: This mechanism assumes that we only attempt to get the
//          thread when we are nested below a call wrapper.
//
// movl reg, fs:[0]                        Get exeception pointer
// movl reg, [reg + thread_ptr_offset]     Load thread
//
void MacroAssembler::get_thread(Register thread) {
  // can't use ExternalAddress because it can't take NULL
  AddressLiteral null(0, relocInfo::none);

  prefix(FS_segment);
  movptr(thread, null);
  assert(ThreadLocalStorage::get_thread_ptr_offset() != 0,
         "Thread Pointer Offset has not been initialized");
  movl(thread, Address(thread, ThreadLocalStorage::get_thread_ptr_offset()));
}
#else
// call (Thread*)TlsGetValue(thread_index());
void MacroAssembler::get_thread(Register thread) {
   if (thread != rax) {
     push(rax);
   }
   push(rdi);
   push(rsi);
   push(rdx);
   push(rcx);
   push(r8);
   push(r9);
   push(r10);
   // XXX
   mov(r10, rsp);
   andq(rsp, -16);
   push(r10);
   push(r11);

   movl(c_rarg0, ThreadLocalStorage::thread_index());
   call(RuntimeAddress((address)TlsGetValue));

   pop(r11);
   pop(rsp);
   pop(r10);
   pop(r9);
   pop(r8);
   pop(rcx);
   pop(rdx);
   pop(rsi);
   pop(rdi);
   if (thread != rax) {
       mov(thread, rax);
       pop(rax);
   }
}
#endif
