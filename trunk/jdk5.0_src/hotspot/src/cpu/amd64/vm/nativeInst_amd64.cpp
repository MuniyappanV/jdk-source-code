#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)nativeInst_amd64.cpp	1.6 04/03/18 19:02:04 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_nativeInst_amd64.cpp.incl"

void NativeInstruction::wrote(int offset)
{
  ICache::invalidate_word(addr_at(offset));
}

void NativeCall::verify()
{
  // Make sure code pattern is actually a call disp32 instruction.
  uint inst = ubyte_at(0);
  if (inst != instruction_code) {
    tty->print_cr("Addr: " PTR_FORMAT " Code: 0x%x",
                  instruction_address(), inst);
    fatal("not a call disp32");
  }
}

void NativeCall::print()
{
  tty->print_cr(PTR_FORMAT ": call " PTR_FORMAT,
                instruction_address(), destination());
}

// Inserts a native call instruction at a given pc
void NativeCall::insert(address code_pos, address entry)
{
  intptr_t disp = (intptr_t)entry - ((intptr_t)code_pos + 1 + 4);
  guarantee(disp == (intptr_t)(jint)disp, "must be 32-bit offset");

  *code_pos = instruction_code;
  *((jint*)(code_pos + 1)) = (jint)disp;

  ICache::invalidate_range(code_pos, instruction_size);
}

// MT-safe patching of a call instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeCall::replace_mt_safe(address instr_addr, address code_buffer)
{
  assert(Patching_lock->is_locked() || SafepointSynchronize::is_at_safepoint(),
         "concurrent code patching");
  assert(instr_addr != NULL, "illegal address for code patching 1");

  NativeCall* n_call = nativeCall_at(instr_addr);
  if (os::is_MP()) {
    guarantee((intptr_t)instr_addr % BytesPerWord == 0, "must be aligned");
  }

  // First patch dummy jmp in place
  unsigned char patch[4];
  assert(sizeof(patch) == sizeof(jint), "sanity check");
  patch[0] = 0xEB; // jmp rel8off
  patch[1] = 0xFE; // jmp -2 # to patch[0]
  patch[2] = 0xEB; // jmp rel8off
  patch[3] = 0xFE; // jmp -2 # to patch[2]

  *(jint*)instr_addr = *(jint*)patch;

  // Invalidate.  Opteron requires a flush after every write.
  n_call->wrote(0);

  // Patch 4th byte
  instr_addr[4] = code_buffer[4];

  n_call->wrote(4);

  // Patch bytes 0-3
  *(jint*)instr_addr = *(jint*)code_buffer;

  n_call->wrote(0);

#ifdef ASSERT
  // verify patching
  // bytewise comparing
  for (int i = 0; i < instruction_size; i++) {
    address ptr = (address)((intptr_t)code_buffer + i);
    int a_byte = (*ptr) & 0xFF;
    assert(*((address)((intptr_t)instr_addr + i)) == a_byte,
	   "mt safe patching failed");
  }
#endif
}

// Similar to replace_mt_safe, but just changes the destination.  The
// important thing is that free-running threads are able to execute
// this call instruction at all times.  If the displacement field is
// aligned we can simply rely on atomicity of 32-bit writes to make
// sure other threads will see no intermediate states.  Otherwise, the
// first two bytes of the call are guaranteed to be aligned, and can
// be atomically patched to a self-loop to guard the instruction while
// we change the other bytes.
//
// We cannot rely on locks here, since the free-running threads must
// run at full speed.
//
// Used in the runtime linkage of calls; see class CompiledIC.
// (Cf. 4506997 and 4479829, where threads witnessed garbage displacements.)
void NativeCall::set_destination_mt_safe(address dest)
{
  debug_only(verify());
  // Make sure patching code is locked.  No two threads can patch at
  // the same time but one may be executing this code.
  assert(Patching_lock->is_locked() || SafepointSynchronize::is_at_safepoint(),
         "concurrent code patching");
  // Both C1 and C2 should now be generating code which aligns the
  // patched address to be within a single cache line except that C1
  // does not do the alignment on uniprocessor systems.
  bool is_aligned = ((uintptr_t)displacement_address() + 0) / cache_line_size ==
                    ((uintptr_t)displacement_address() + 3) / cache_line_size;
  guarantee(!os::is_MP() || is_aligned, "destination must be aligned");
  if (is_aligned) {
    // Simple case:  The destination lies within a single cache line.
    set_destination(dest);
    wrote(displacement_offset);
  } else if (((uintptr_t)instruction_address() + 0) / cache_line_size ==
             ((uintptr_t)instruction_address() + 1) / cache_line_size) {
    // Tricky case:  The instruction prefix lies within a single cache line.
    intptr_t disp = dest - return_address();
    guarantee(disp == (intptr_t)(jint)disp, "must be 32-bit offset");

    int call_opcode = instruction_address()[0];

    // First patch dummy jump in place:
    u_char patch_jump[2];
    patch_jump[0] = 0xEB; // jmp rel8
    patch_jump[1] = 0xFE; // jmp -2 # to patch[0]

    assert(sizeof(patch_jump) == sizeof(short), "sanity check");
    *(short*)instruction_address() = *(short*)patch_jump;

    // Invalidate.  Opteron requires a flush after every write.
    wrote(0);

    // (Note: We assume any reader which has already started to read
    // the unpatched call will completely read the whole unpatched call
    // without seeing the next writes we are about to make.)

    // Next, patch the last three bytes:
    u_char patch_disp[5];
    patch_disp[0] = call_opcode;
    *(jint*)(patch_disp + 1) = (jint)disp;
    assert(sizeof(patch_disp) == instruction_size, "sanity check");
    for (int i = sizeof(short); i < instruction_size; i++) {
      instruction_address()[i] = patch_disp[i];
    }

    wrote(sizeof(short));

    // (Note: We assume that any reader which reads the opcode we are
    // about to repatch will also read the writes we just made.)

    // Finally, overwrite the jump:
    *(short*)instruction_address() = *(short*)patch_disp;

    wrote(0);

    debug_only(verify());
    guarantee(destination() == dest, "patch succeeded");
  } else {
    // Impossible:  One or the other must be atomically writable.
    ShouldNotReachHere();
  }
}

void NativeMovConstReg::verify() 
{
  // make sure code pattern is actually a mov reg64, imm64 instruction
  if ((ubyte_at(0) != Assembler::REX_W && ubyte_at(0) != Assembler::REX_WB) ||
      (ubyte_at(1) & (0xff ^ register_mask)) != 0xB8) {
    print();
    fatal("not a REX.W[B] mov reg64, imm64");
  }
}

void NativeMovConstReg::print()
{
  tty->print_cr(PTR_FORMAT ": movq reg, " INTPTR_FORMAT,
                instruction_address(), data());
}

void NativeJump::verify()
{
  if (*(u_char*) instruction_address() != instruction_code) {
    fatal("not a jump instruction");
  }
}

void NativeJump::insert(address code_pos, address entry)
{
  intptr_t disp = (intptr_t)entry - ((intptr_t)code_pos + 1 + 4);
  guarantee(disp == (intptr_t)(jint)disp, "must be 32-bit offset");

  *code_pos = instruction_code;
  *((jint*)(code_pos + 1)) = (jint)disp;

  ICache::invalidate_range(code_pos, instruction_size);
}

void NativeJump::check_verified_entry_alignment(address entry,
                                                address verified_entry)
{
  // Patching to not_entrant can happen while activations of the
  // method are in use. The patching in that instance must happen only
  // when certain alignment restrictions are true. These guarantees
  // check those conditions.
  // Must be 8 byte aligned
  guarantee(((uintptr_t) verified_entry & 7) == 0,
            "illegal address for code patching 2");
  // First 5 bytes must be within the same cache line - 4827828
  guarantee((uintptr_t) verified_entry / 64 ==
            ((uintptr_t) verified_entry + 4) / 64,
            "illegal address for code patching 3");
}

// MT safe inserting of a jump over an unknown instruction sequence
// (used by nmethod::makeZombie) The problem: jmp <dest> is a 5-byte
// instruction. Atomical write can be only with 4 bytes.  First
// patches the first word atomically to be a jump to itself.  Then
// patches the last byte and then atomically patches the first word
// (4-bytes), thus inserting the desired jump.  This code is mt-safe
// with the following conditions: entry point is 4 byte aligned, entry
// point is in same cache line as unverified entry point, and the
// instruction being patched is >= 5 byte (size of patch).
// 
// In C2 the 5+ byte sized instruction is enforced by code in
// MachPrologNode::emit.
//
// In C1 the restriction is enforced by CodeEmitter::method_entry.
//
void NativeJump::patch_verified_entry(address entry, address verified_entry,
                                      address dest)
{
  // complete jump instruction (to be inserted) is in code_buffer.
  unsigned char code_buffer[5];
  code_buffer[0] = instruction_code;
  intptr_t disp = (intptr_t)dest - ((intptr_t)verified_entry + 1 + 4);
  guarantee(disp == (intptr_t)(jint)disp, "must be 32-bit offset");
  *(jint*)(code_buffer + 1) = (jint)disp;

#ifndef CORE
  check_verified_entry_alignment(entry, verified_entry);
#endif /* CORE */

  // Can't call nativeJump_at() because it's asserts jump exists
  NativeJump* n_jump = (NativeJump*) verified_entry;

  // First patch dummy jmp in place
  unsigned char patch[4];
  assert(sizeof(patch)==sizeof(jint), "sanity check");
  patch[0] = 0xEB;       // jmp rel8
  patch[1] = 0xFE;       // jmp to self
  patch[2] = 0xEB;
  patch[3] = 0xFE;

  *(jint*)verified_entry = *(jint*)patch;

  n_jump->wrote(0);

  // Patch 5th byte (from jump instruction)
  verified_entry[4] = code_buffer[4];

  n_jump->wrote(4);

  // Patch bytes 0-3 (from jump instruction)
  *(jint*)verified_entry = *(jint*)code_buffer;

  n_jump->wrote(0);
}

void NativePopReg::insert(address code_pos, Register reg) 
{
  assert(reg->encoding() < 8, "no space for REX");
  assert(NativePopReg::instruction_size == sizeof(char),
         "right address unit for update");
  *code_pos = (u_char) (instruction_code | reg->encoding());
  ICache::invalidate_range(code_pos, instruction_size);
}

void NativeIllegalInstruction::insert(address code_pos)
{
  assert(NativeIllegalInstruction::instruction_size == sizeof(short),
         "right address unit for update");
  *(short*) code_pos = instruction_code;
  ICache::invalidate_range(code_pos, instruction_size);
}

void NativeGeneralJump::verify() 
{
  assert(((NativeInstruction*) this)->is_jump() ||
	 ((NativeInstruction*) this)->is_cond_jump(), 
         "not a general jump instruction");
}

void NativeGeneralJump::insert_unconditional(address code_pos, address entry)
{
  intptr_t disp = (intptr_t)entry - ((intptr_t)code_pos + 1 + 4);
  guarantee(disp == (intptr_t)(jint)disp, "must be 32-bit offset");

  *code_pos = unconditional_long_jump;
  *((jint*)(code_pos + 1)) = (jint)disp;

  ICache::invalidate_range(code_pos, instruction_size);
}

// MT-safe patching of a long jump instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeGeneralJump::replace_mt_safe(address instr_addr, address code_buffer) {
  assert(instr_addr != NULL, "illegal address for code patching 4");
  NativeGeneralJump* n_jump = nativeGeneralJump_at(instr_addr);

  unsigned char patch[4];
  assert(sizeof(patch) == sizeof(jint), "sanity check");
  patch[0] = 0xEB; // jmp rel8
  patch[1] = 0xFE; // jmp -2 # to patch[0]
  patch[2] = 0xEB; // jmp rel8
  patch[3] = 0xFE; // jmp -2 # to patch[2]

  // First patch dummy jmp in place
  *(jint*)instr_addr = *(jint*)patch;

  n_jump->wrote(0);

  // Patch 4th byte
  instr_addr[4] = code_buffer[4];

  n_jump->wrote(4);

  // Patch bytes 0-3
  *(jint*)instr_addr = *(jint*)code_buffer;

  n_jump->wrote(0);

#ifdef ASSERT
  // verify patching
  for (int i = 0; i < instruction_size; i++) {
    // bytewise comparing
    address ptr = (address)((intptr_t)code_buffer + i);
    int a_byte = (*ptr) & 0xFF;
    assert(*((address)((intptr_t)instr_addr + i)) == a_byte, 
	   "mt safe patching failed");
  }
#endif
}

address NativeGeneralJump::jump_destination() const
{
  int op_code = ubyte_at(0);
  bool is_rel32off = (op_code == 0xE9 || op_code == 0x0F);
  int offset = (op_code == 0x0F) ? 2 : 1;
  int length = offset + (is_rel32off ? 4 : 1);
  
  if (is_rel32off) {
    return addr_at(0) + length + int_at(offset);
  } else {
    return addr_at(0) + length + sbyte_at(offset);
  }
}
