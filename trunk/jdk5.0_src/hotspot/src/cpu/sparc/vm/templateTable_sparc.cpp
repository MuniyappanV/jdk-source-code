#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)templateTable_sparc.cpp	1.229 04/04/23 10:43:24 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_templateTable_sparc.cpp.incl"

#define __ _masm->


//----------------------------------------------------------------------------------------------------
// Platform-dependent initialization

void TemplateTable::pd_initialize() {
  // (none)
}


//----------------------------------------------------------------------------------------------------
// Condition conversion
Assembler::Condition ccNot(TemplateTable::Condition cc) {
  switch (cc) {
    case TemplateTable::equal        : return Assembler::notEqual;
    case TemplateTable::not_equal    : return Assembler::equal;
    case TemplateTable::less         : return Assembler::greaterEqual;
    case TemplateTable::less_equal   : return Assembler::greater;
    case TemplateTable::greater      : return Assembler::lessEqual;
    case TemplateTable::greater_equal: return Assembler::less;
  }
  ShouldNotReachHere();
  return Assembler::zero;
}

//----------------------------------------------------------------------------------------------------
// Miscelaneous helper routines 


Address TemplateTable::at_bcp(int offset) {
  assert(_desc->uses_bcp(), "inconsistent uses_bcp information");
  return Address( Lbcp, 0, offset);
}


void TemplateTable::patch_bytecode(Bytecodes::Code bc, Register Rbyte_code,
                                   Register Rscratch,
                                   bool load_bc_into_scratch /*=true*/) {
  // With sharing on, may need to test methodOop flag.
  if (!RewriteBytecodes) return;
  if (load_bc_into_scratch) __ set(bc, Rbyte_code);
  Label patch_done;
  if (JvmtiExport::can_post_breakpoint()) {
    Label fast_patch;
    __ ldub(at_bcp(0), Rscratch);
    __ cmp(Rscratch, Bytecodes::_breakpoint);
    __ br(Assembler::notEqual, false, Assembler::pt, fast_patch);
    __ delayed()->nop();  // don't bother to hoist the stb here
    // perform the quickening, slowly, in the bowels of the breakpoint table
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::set_original_bytecode_at), Lmethod, Lbcp, Rbyte_code);
    __ ba(false, patch_done);
    __ delayed()->nop();
    __ bind(fast_patch);
  }
#ifdef ASSERT
  Bytecodes::Code orig_bytecode =  Bytecodes::java_code(bc);
  Label okay;
  __ ldub(at_bcp(0), Rscratch);
  __ cmp(Rscratch, orig_bytecode);
  __ br(Assembler::equal, false, Assembler::pt, okay);
  __ delayed() ->cmp(Rscratch, Rbyte_code);
  __ br(Assembler::equal, false, Assembler::pt, okay);
  __ delayed()->nop();
  __ stop("Rewriting wrong bytecode location");
  __ bind(okay);
#endif
  __ stb(Rbyte_code, at_bcp(0));
  __ bind(patch_done);
}

//----------------------------------------------------------------------------------------------------
// Individual instructions

void TemplateTable::nop() {
  transition(vtos, vtos);
  // nothing to do
}

void TemplateTable::shouldnotreachhere() {
  transition(vtos, vtos);
  __ stop("shouldnotreachhere bytecode");
}

void TemplateTable::aconst_null() {
  transition(vtos, atos);
  __ clr(Otos_i);
}


void TemplateTable::iconst(int value) {
  transition(vtos, itos);
  __ set(value, Otos_i);
}


void TemplateTable::lconst(int value) {
  transition(vtos, ltos);
  assert(value >= 0, "check this code");
#ifdef _LP64
  __ set(value, Otos_l);
#else
  __ set(value, Otos_l2);
  __ clr( Otos_l1);
#endif
}


void TemplateTable::fconst(int value) {
  transition(vtos, ftos);
  static float zero = 0.0, one = 1.0, two = 2.0;
  float* p;
  switch( value ) {
   default: ShouldNotReachHere();
   case 0:  p = &zero;  break;
   case 1:  p = &one;   break;
   case 2:  p = &two;   break;
  }
  Address a(G3_scratch, (address)p);
  __ sethi(a);
  __ ldf(FloatRegisterImpl::S, a, Ftos_f);
}


void TemplateTable::dconst(int value) {
  transition(vtos, dtos);
  static double zero = 0.0, one = 1.0;
  double* p;
  switch( value ) {
   default: ShouldNotReachHere();
   case 0:  p = &zero;  break;
   case 1:  p = &one;   break;
  }
  Address a(G3_scratch, (address)p);
  __ sethi(a);
  __ ldf(FloatRegisterImpl::D, a, Ftos_d);
}


// %%%%% Should factore most snippet templates across platforms 

void TemplateTable::bipush() {
  transition(vtos, itos);
  __ ldsb( at_bcp(1), Otos_i );
}

void TemplateTable::sipush() {
  transition(vtos, itos);
  __ get_2_byte_integer_at_bcp(1, G3_scratch, Otos_i, InterpreterMacroAssembler::Signed);
}

void TemplateTable::ldc(bool wide) {
  transition(vtos, vtos);
  Label call_ldc, notInt, notString, notClass, exit;

  if (wide) {
    __ get_2_byte_integer_at_bcp(1, G3_scratch, O1, InterpreterMacroAssembler::Unsigned);
  } else {
    __ ldub(Lbcp, 1, O1);
  }
  __ get_cpool_and_tags(O0, O2);

  const int base_offset = constantPoolOopDesc::header_size() * wordSize;
  const int tags_offset = typeArrayOopDesc::header_size(T_BYTE) * wordSize;

  // get type from tags
  __ add(O2, tags_offset, O2);
  __ ldub(O2, O1, O2);
  __ cmp(O2, JVM_CONSTANT_UnresolvedString);    // unresolved string? If so, must resolve
  __ brx(Assembler::equal, true, Assembler::pt, call_ldc);
  __ delayed()->nop();

  __ cmp(O2, JVM_CONSTANT_UnresolvedClass);     // unresolved class? If so, must resolve
  __ brx(Assembler::equal, true, Assembler::pt, call_ldc);
  __ delayed()->nop();

  __ cmp(O2, JVM_CONSTANT_Class);      // need to call vm to get java mirror of the class
  __ brx(Assembler::notEqual, true, Assembler::pt, notClass);
  __ delayed()->add(O0, base_offset, O0);

  __ bind(call_ldc);
  __ set(wide, O1);
  call_VM(Otos_i, CAST_FROM_FN_PTR(address, InterpreterRuntime::ldc), O1);
  __ push(atos);
  __ ba(false, exit);
  __ delayed()->nop();

  __ bind(notClass);
 // __ add(O0, base_offset, O0);
  __ sll(O1, LogBytesPerWord, O1);
  __ cmp(O2, JVM_CONSTANT_Integer);
  __ brx(Assembler::notEqual, true, Assembler::pt, notInt);
  __ delayed()->cmp(O2, JVM_CONSTANT_String);
  __ ld(O0, O1, Otos_i);
  __ push(itos);
  __ ba(false, exit);
  __ delayed()->nop();

  __ bind(notInt);
 // __ cmp(O2, JVM_CONSTANT_String);
  __ brx(Assembler::notEqual, true, Assembler::pt, notString);
  __ delayed()->ldf(FloatRegisterImpl::S, O0, O1, Ftos_f);
  __ ld_ptr(O0, O1, Otos_i);
  __ verify_oop(Otos_i);
  __ push(atos);
  __ ba(false, exit);
  __ delayed()->nop();

  __ bind(notString);
 // __ ldf(FloatRegisterImpl::S, O0, O1, Ftos_f);
  __ push(ftos);

  __ bind(exit);
}

void TemplateTable::ldc2_w() {
  transition(vtos, vtos);
  Label retry, resolved, Long, exit;

  __ bind(retry);
  __ get_2_byte_integer_at_bcp(1, G3_scratch, O1, InterpreterMacroAssembler::Unsigned);
  __ get_cpool_and_tags(O0, O2);

  const int base_offset = constantPoolOopDesc::header_size() * wordSize;
  const int tags_offset = typeArrayOopDesc::header_size(T_BYTE) * wordSize;
  // get type from tags
  __ add(O2, tags_offset, O2);
  __ ldub(O2, O1, O2);

  __ sll(O1, LogBytesPerWord, O1);
  __ add(O0, O1, G3_scratch);

  __ cmp(O2, JVM_CONSTANT_Double);
  __ brx(Assembler::notEqual, false, Assembler::pt, Long);
  __ delayed()->nop();
  // A double can be placed at word-aligned locations in the constant pool.
  // Check out Conversions.java for an example.
  // Also constantPoolOopDesc::header_size() is 20, which makes it very difficult
  // to double-align double on the constant pool.  SG, 11/7/97
  __ load_unaligned_double( G3_scratch, base_offset, Ftos_d);
  __ push(dtos);
  __ ba(false, exit);
  __ delayed()->nop();

  __ bind(Long);
  __ load_unaligned_long(G3_scratch, base_offset, Otos_l);
  __ push(ltos);

  __ bind(exit);
}


void TemplateTable::locals_index(Register reg, int offset) {
  __ ldub( at_bcp(offset), reg );
}


void TemplateTable::locals_index_wide(Register reg) {
  // offset is 2, not 1, because Lbcp points to wide prefix code
  __ get_2_byte_integer_at_bcp(2, G4_scratch, reg, InterpreterMacroAssembler::Unsigned);
}

void TemplateTable::iload() {
  transition(vtos, itos);
  // Rewrite iload,iload  pair into fast_iload2
  //         iload,caload pair into fast_icaload
  if (RewriteFrequentPairs) {
    Label rewrite, done;

    // get next byte
    __ ldub(at_bcp(Bytecodes::length_for(Bytecodes::_iload)), G3_scratch);

    // if _iload, wait to rewrite to iload2.  We only want to rewrite the
    // last two iloads in a pair.  Comparing against fast_iload means that
    // the next bytecode is neither an iload or a caload, and therefore
    // an iload pair.
    __ cmp(G3_scratch, (int)Bytecodes::_iload);
    __ br(Assembler::equal, false, Assembler::pn, done);
    __ delayed()->nop();

    __ cmp(G3_scratch, (int)Bytecodes::_fast_iload);
    __ br(Assembler::equal, false, Assembler::pn, rewrite);
    __ delayed()->set(Bytecodes::_fast_iload2, G4_scratch);

    __ cmp(G3_scratch, (int)Bytecodes::_caload);
    __ br(Assembler::equal, false, Assembler::pn, rewrite);
    __ delayed()->set(Bytecodes::_fast_icaload, G4_scratch);

    __ set(Bytecodes::_fast_iload, G4_scratch);  // don't check again
    // rewrite
    // G4_scratch: fast bytecode
    __ bind(rewrite);
    patch_bytecode(Bytecodes::_iload, G4_scratch, G3_scratch, false);
    __ bind(done);
  }

  // Get the local value into tos
  locals_index(G3_scratch);
  __ access_local_int( G3_scratch, Otos_i );
}

void TemplateTable::fast_iload2() {
  transition(vtos, itos);
  locals_index(G3_scratch);
  __ access_local_int( G3_scratch, Otos_i );
  __ push_i();
  locals_index(G4_scratch, 3);  // get next bytecode's local index.
  __ access_local_int( G4_scratch, Otos_i );
}

void TemplateTable::fast_iload() {
  transition(vtos, itos);
  locals_index(G3_scratch);
  __ access_local_int( G3_scratch, Otos_i );
}

void TemplateTable::lload() {
  transition(vtos, ltos);
  locals_index(G3_scratch);
  __ access_local_long( G3_scratch, Otos_l );
}


void TemplateTable::fload() {
  transition(vtos, ftos);
  locals_index(G3_scratch);
  __ access_local_float( G3_scratch, Ftos_f );
}


void TemplateTable::dload() {
  transition(vtos, dtos);
  locals_index(G3_scratch);
  __ access_local_double( G3_scratch, Ftos_d );
}


void TemplateTable::aload() {
  transition(vtos, atos);
  locals_index(G3_scratch);
  __ access_local_ptr( G3_scratch, Otos_i);
}


void TemplateTable::wide_iload() {
  transition(vtos, itos);
  locals_index_wide(G3_scratch);
  __ access_local_int( G3_scratch, Otos_i );
}


void TemplateTable::wide_lload() {
  transition(vtos, ltos);
  locals_index_wide(G3_scratch);
  __ access_local_long( G3_scratch, Otos_l );
}


void TemplateTable::wide_fload() {
  transition(vtos, ftos);
  locals_index_wide(G3_scratch);
  __ access_local_float( G3_scratch, Ftos_f );
}


void TemplateTable::wide_dload() {
  transition(vtos, dtos);
  locals_index_wide(G3_scratch);
  __ access_local_double( G3_scratch, Ftos_d );
}


void TemplateTable::wide_aload() {
  transition(vtos, atos);
  locals_index_wide(G3_scratch);
  __ access_local_ptr( G3_scratch, Otos_i );
  __ verify_oop(Otos_i);
}


void TemplateTable::index_check(Register array, Register index) {
  Unimplemented(); // moved to InterpreterMacroAssembler
}


void TemplateTable::iaload() {
  transition(itos, itos);
  // Otos_i: index
  // tos: array
  __ index_check(O2, Otos_i, LogBytesPerInt, G3_scratch, O3);
  __ ld(O3, arrayOopDesc::base_offset_in_bytes(T_INT), Otos_i);
}


void TemplateTable::laload() {
  transition(itos, ltos);
  // Otos_i: index
  // O2: array
  __ index_check(O2, Otos_i, LogBytesPerLong, G3_scratch, O3);
  __ ld_long(O3, arrayOopDesc::base_offset_in_bytes(T_LONG), Otos_l);
}


void TemplateTable::faload() {
  transition(itos, ftos);
  // Otos_i: index
  // O2: array
  __ index_check(O2, Otos_i, LogBytesPerInt, G3_scratch, O3);
  __ ldf(FloatRegisterImpl::S, O3, arrayOopDesc::base_offset_in_bytes(T_FLOAT), Ftos_f);
}


void TemplateTable::daload() {
  transition(itos, dtos);
  // Otos_i: index
  // O2: array
  __ index_check(O2, Otos_i, LogBytesPerLong, G3_scratch, O3);
  __ ldf(FloatRegisterImpl::D, O3, arrayOopDesc::base_offset_in_bytes(T_DOUBLE), Ftos_d);
}


void TemplateTable::aaload() {
  transition(itos, atos);
  // Otos_i: index
  // tos: array
  __ index_check(O2, Otos_i, LogBytesPerWord, G3_scratch, O3);
  __ ld_ptr(O3, arrayOopDesc::base_offset_in_bytes(T_OBJECT), Otos_i);
  __ verify_oop(Otos_i);
}


void TemplateTable::baload() {
  transition(itos, itos);
  // Otos_i: index
  // tos: array
  __ index_check(O2, Otos_i, 0, G3_scratch, O3);
  __ ldsb(O3, arrayOopDesc::base_offset_in_bytes(T_BYTE), Otos_i);
}


void TemplateTable::caload() {
  transition(itos, itos);
  // Otos_i: index
  // tos: array
  __ index_check(O2, Otos_i, LogBytesPerShort, G3_scratch, O3);
  __ lduh(O3, arrayOopDesc::base_offset_in_bytes(T_CHAR), Otos_i);
}

void TemplateTable::fast_icaload() {
  transition(vtos, itos);
  // Otos_i: index
  // tos: array
  locals_index(G3_scratch);
  __ access_local_int( G3_scratch, Otos_i );
  __ index_check(O2, Otos_i, LogBytesPerShort, G3_scratch, O3);
  __ lduh(O3, arrayOopDesc::base_offset_in_bytes(T_CHAR), Otos_i);
}


void TemplateTable::saload() {
  transition(itos, itos);
  // Otos_i: index
  // tos: array
  __ index_check(O2, Otos_i, LogBytesPerShort, G3_scratch, O3);
  __ ldsh(O3, arrayOopDesc::base_offset_in_bytes(T_SHORT), Otos_i);
}


void TemplateTable::iload(int n) {
  transition(vtos, itos);
  __ ld( Llocals, -n * BytesPerWord, Otos_i );
}


void TemplateTable::lload(int n) {
  transition(vtos, ltos);
  assert(n+1 < Argument::n_register_parameters, "would need more code");
  __ load_unaligned_long(Llocals, -(n+1)*BytesPerWord, Otos_l);
}


void TemplateTable::fload(int n) {
  transition(vtos, ftos);
  assert(n < Argument::n_register_parameters, "would need more code");
  __ ldf( FloatRegisterImpl::S, Llocals, -n * BytesPerWord,     Ftos_f );
}


void TemplateTable::dload(int n) {
  transition(vtos, dtos);
  FloatRegister dst = Ftos_d;
  __ load_unaligned_double(Llocals, -(n+1)*BytesPerWord, dst);
}


void TemplateTable::aload(int n) {
  transition(vtos, atos);
  __ ld_ptr( Llocals, -n * BytesPerWord, Otos_i );
}


void TemplateTable::aload_0() {
  transition(vtos, atos);

  // According to bytecode histograms, the pairs:
  //
  // _aload_0, _fast_igetfield (itos)
  // _aload_0, _fast_agetfield (atos)
  // _aload_0, _fast_fgetfield (ftos)
  //
  // occur frequently. If RewriteFrequentPairs is set, the (slow) _aload_0
  // bytecode checks the next bytecode and then rewrites the current
  // bytecode into a pair bytecode; otherwise it rewrites the current
  // bytecode into _fast_aload_0 that doesn't do the pair check anymore.
  //
  if (RewriteFrequentPairs) {
    Label rewrite, done;

    // get next byte
    __ ldub(at_bcp(Bytecodes::length_for(Bytecodes::_aload_0)), G3_scratch);

    // do actual aload_0
    aload(0);

    // if _getfield then wait with rewrite
    __ cmp(G3_scratch, (int)Bytecodes::_getfield);
    __ br(Assembler::equal, false, Assembler::pn, done);
    __ delayed()->nop();

    // if _igetfield then rewrite to _fast_iaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_iaccess_0) == Bytecodes::_aload_0, "adjust fast bytecode def");
    __ cmp(G3_scratch, (int)Bytecodes::_fast_igetfield);
    __ br(Assembler::equal, false, Assembler::pn, rewrite);
    __ delayed()->set(Bytecodes::_fast_iaccess_0, G4_scratch);

    // if _agetfield then rewrite to _fast_aaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_aaccess_0) == Bytecodes::_aload_0, "adjust fast bytecode def");
    __ cmp(G3_scratch, (int)Bytecodes::_fast_agetfield);
    __ br(Assembler::equal, false, Assembler::pn, rewrite);
    __ delayed()->set(Bytecodes::_fast_aaccess_0, G4_scratch);

    // if _fgetfield then rewrite to _fast_faccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_faccess_0) == Bytecodes::_aload_0, "adjust fast bytecode def");
    __ cmp(G3_scratch, (int)Bytecodes::_fast_fgetfield);
    __ br(Assembler::equal, false, Assembler::pn, rewrite);
    __ delayed()->set(Bytecodes::_fast_faccess_0, G4_scratch);

    // else rewrite to _fast_aload0
    assert(Bytecodes::java_code(Bytecodes::_fast_aload_0) == Bytecodes::_aload_0, "adjust fast bytecode def");
    __ set(Bytecodes::_fast_aload_0, G4_scratch);

    // rewrite
    // G4_scratch: fast bytecode
    __ bind(rewrite);
    patch_bytecode(Bytecodes::_aload_0, G4_scratch, G3_scratch, false);
    __ bind(done);
  } else {
    aload(0);
  }
}


void TemplateTable::istore() {
  transition(itos, vtos);
  locals_index(G3_scratch);
  __ store_local_int( G3_scratch, Otos_i );
}


void TemplateTable::lstore() {
  transition(ltos, vtos);
  locals_index(G3_scratch);
  __ store_local_long( G3_scratch, Otos_l );
}


void TemplateTable::fstore() {
  transition(ftos, vtos);
  locals_index(G3_scratch);
  __ store_local_float( G3_scratch, Ftos_f );
}


void TemplateTable::dstore() {
  transition(dtos, vtos);
  locals_index(G3_scratch);
  __ store_local_double( G3_scratch, Ftos_d );
}


void TemplateTable::astore() {
  transition(vtos, vtos);
  __ pop_ptr();
  __ verify_oop_or_return_address(Otos_i, G3_scratch);
  locals_index(G3_scratch);
  __ store_local_ptr( G3_scratch, Otos_i );
}


void TemplateTable::wide_istore() {
  transition(vtos, vtos);
  __ pop_i();
  locals_index_wide(G3_scratch);
  __ store_local_int( G3_scratch, Otos_i );
}


void TemplateTable::wide_lstore() {
  transition(vtos, vtos);
  __ pop_l();
  locals_index_wide(G3_scratch);
  __ store_local_long( G3_scratch, Otos_l );
}


void TemplateTable::wide_fstore() {
  transition(vtos, vtos);
  __ pop_f();
  locals_index_wide(G3_scratch);
  __ store_local_float( G3_scratch, Ftos_f );
}


void TemplateTable::wide_dstore() {
  transition(vtos, vtos);
  __ pop_d();
  locals_index_wide(G3_scratch);
  __ store_local_double( G3_scratch, Ftos_d );
}


void TemplateTable::wide_astore() {
  transition(vtos, vtos);
  __ pop_ptr();
  locals_index_wide(G3_scratch);
  __ store_local_ptr( G3_scratch, Otos_i );
}


void TemplateTable::iastore() {
  transition(itos, vtos);
  __ pop_i(O2); // index
  // Otos_i: val
  // O3: array
  __ index_check(O3, O2, LogBytesPerInt, G3_scratch, O2);
  __ st(Otos_i, O2, arrayOopDesc::base_offset_in_bytes(T_INT));
}


void TemplateTable::lastore() {
  transition(ltos, vtos);
  __ pop_i(O2); // index
  // Otos_l: val
  // O3: array
  __ index_check(O3, O2, LogBytesPerLong, G3_scratch, O2);
  __ st_long(Otos_l, O2, arrayOopDesc::base_offset_in_bytes(T_LONG));
}


void TemplateTable::fastore() {
  transition(ftos, vtos);
  __ pop_i(O2); // index
  // Ftos_f: val
  // O3: array
  __ index_check(O3, O2, LogBytesPerInt, G3_scratch, O2);
  __ stf(FloatRegisterImpl::S, Ftos_f, O2, arrayOopDesc::base_offset_in_bytes(T_FLOAT));
}


void TemplateTable::dastore() {
  transition(dtos, vtos);
  __ pop_i(O2); // index
  // Fos_d: val
  // O3: array
  __ index_check(O3, O2, LogBytesPerLong, G3_scratch, O2);
  __ stf(FloatRegisterImpl::D, Ftos_d, O2, arrayOopDesc::base_offset_in_bytes(T_DOUBLE));
}


void TemplateTable::aastore() {
  Label store_ok, is_null, done;
  transition(vtos, vtos);
  __ ld_ptr(Lesp, 1 * wordSize, Otos_i); // get value from stack
  __ ld(Lesp, 2 * wordSize, O2);     	 // get index
  __ ld_ptr(Lesp, 3 * wordSize, O3);     // get array
  // Otos_i: val
  // O2: index
  // O3: array
  __ verify_oop(Otos_i);
  __ index_check_without_pop(O3, O2, LogBytesPerWord, G3_scratch, O1);
  
  // do array store check - check for NULL value first
  __ br_null( Otos_i, false, Assembler::pn, is_null );
  __ delayed()->
     ld_ptr(O3,     oopDesc::klass_offset_in_bytes(), O4); // get array klass

  // Profile the not-null value
  __ profile_checkcast(false, G3_scratch, G4_scratch);

  // do fast instanceof cache test
  __ ld_ptr(Otos_i, oopDesc::klass_offset_in_bytes(), O5); // get value klass

  __ ld_ptr(O4,     sizeof(oopDesc) + objArrayKlass::element_klass_offset_in_bytes(),  O4);
  
  assert(Otos_i == O0, "just checking");
  
  // Otos_i:    value
  // O1:        addr - offset
  // O2:        index
  // O3:        array
  // O4:        array element klass
  // O5:        value klass

  // Generate a fast subtype check.  Branch to store_ok if no
  // failure.  Throw if failure.
  __ gen_subtype_check( O5, O4, G3_scratch, G4_scratch, G1_scratch, store_ok );

  // Not a subtype; so must throw exception
  __ throw_if_not_x( Assembler::never, Interpreter::_throw_ArrayStoreException_entry, G3_scratch );

  // Store is OK.
  __ bind(store_ok);
  __ st_ptr(Otos_i, O1, arrayOopDesc::base_offset_in_bytes(T_OBJECT));
  // Quote from rememberedSet.hpp: For objArrays, the precise card
  // corresponding to the pointer store is dirtied so we don't need to
  // scavenge the entire array.
  Address element(O1, 0, arrayOopDesc::base_offset_in_bytes(T_OBJECT));
  __ add(element, O1);              // address the element precisely
  __ store_check(G3_scratch, O1);
  __ ba(false,done);
  __ delayed()->inc(Lesp, 3 * wordSize);     // adj sp (pops array, index and value)

  __ bind(is_null);
  __ st_ptr(Otos_i, element);
  __ profile_checkcast(true, G3_scratch, G4_scratch);
  __ inc(Lesp, 3 * wordSize);     // adj sp (pops array, index and value)
  __ bind(done);
}


void TemplateTable::bastore() {
  transition(itos, vtos);
  __ pop_i(O2); // index
  // Otos_i: val
  // O3: array
  __ index_check(O3, O2, 0, G3_scratch, O2);
  __ stb(Otos_i, O2, arrayOopDesc::base_offset_in_bytes(T_BYTE));
}


void TemplateTable::castore() {
  transition(itos, vtos);
  __ pop_i(O2); // index
  // Otos_i: val
  // O3: array
  __ index_check(O3, O2, LogBytesPerShort, G3_scratch, O2);
  __ sth(Otos_i, O2, arrayOopDesc::base_offset_in_bytes(T_CHAR));
}


void TemplateTable::sastore() {
  // %%%%% Factor across platform
  castore();
}


void TemplateTable::istore(int n) {
  transition(itos, vtos);
  __ st(Otos_i, Llocals, -n * BytesPerWord);
}


void TemplateTable::lstore(int n) {
  transition(ltos, vtos);
  assert(n+1 < Argument::n_register_parameters, "only handle register cases");
  __ store_unaligned_long(Otos_l, Llocals, -(n+1)*BytesPerWord);

}


void TemplateTable::fstore(int n) {
  transition(ftos, vtos);
  assert(n < Argument::n_register_parameters, "only handle register cases");
  __ stf(FloatRegisterImpl::S, Ftos_f, Llocals, -n * BytesPerWord);
}


void TemplateTable::dstore(int n) {
  transition(dtos, vtos);
  FloatRegister src = Ftos_d;
  __ store_unaligned_double(src, Llocals, -(n+1)*BytesPerWord);
}


void TemplateTable::astore(int n) {
  transition(vtos, vtos);
  __ pop_ptr();
  __ verify_oop_or_return_address(Otos_i, G3_scratch);
  __ st_ptr(Otos_i, Llocals, -n * BytesPerWord);
}


void TemplateTable::pop() {
  transition(vtos, vtos);
  __ inc(Lesp, wordSize);
}


void TemplateTable::pop2() {
  transition(vtos, vtos);
  __ inc(Lesp, 2 * wordSize);
}


void TemplateTable::dup() {
  transition(vtos, vtos);
  // stack: ..., a
  __ ld_ptr(Lesp, +wordSize, Otos_i );
  __ push_ptr();
  // stack: ..., a, a
}


void TemplateTable::dup_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l1); // get a
  __ ld_ptr(Lesp, 1 * wordSize, Otos_l2); // get b
  __ st_ptr(Otos_l2, Lesp, 2 * wordSize); // put b
  __ st_ptr(Otos_l1, Lesp, 1 * wordSize); // put a
  __ push_ptr(Otos_l2);                  // push b
  // stack: ..., b, a, b
}


void TemplateTable::dup_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ ld_ptr(Lesp, 3 * wordSize, Otos_l1 );       // get a
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l2 );       // get b
  __ ld_ptr(Lesp, 1 * wordSize, G3_scratch);       // get c

  __ st_ptr(G3_scratch, Lesp, 3 * wordSize);       // put c
  __ st_ptr(Otos_l1,  Lesp, 2 * wordSize);       // put a
  __ st_ptr(Otos_l2,  Lesp, 1 * wordSize);       // put b
  __ push_ptr(G3_scratch);                        // push c
  // stack: ..., c, a, b, c
}


void TemplateTable::dup2() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l1); // get a
  __ ld_ptr(Lesp, 1 * wordSize, Otos_l2); // get b
  __ push_ptr(Otos_l1);
  __ push_ptr(Otos_l2);
  // stack: ..., a, b, a, b
}


void TemplateTable::dup2_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ ld_ptr(Lesp, 3 * wordSize, G3_scratch); // get a
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l1 ); // get b
  __ ld_ptr(Lesp, 1 * wordSize, Otos_l2 ); // get c

  __ st_ptr( Otos_l1, Lesp, 3 * wordSize); // put b
  __ st_ptr( Otos_l2, Lesp, 2 * wordSize); // put c
  __ st_ptr(G3_scratch, Lesp, 1 * wordSize); // put a
  __ push_ptr(Otos_l1);
  __ push_ptr(Otos_l2);
  // stack: ..., b, c, a, b, c
}


void TemplateTable::dup2_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c, d
  __ ld_ptr(Lesp, 4 * wordSize, G3_scratch ); // get a
  __ ld_ptr(Lesp, 3 * wordSize, G4_scratch); // get b
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l1  ); // get c
  __ ld_ptr(Lesp, 1 * wordSize, Otos_l2  ); // get d

  __ st_ptr(  Otos_l1, Lesp, 4 * wordSize); // put c
  __ st_ptr(  Otos_l2, Lesp, 3 * wordSize); // put d
  __ st_ptr(G3_scratch,  Lesp, 2 * wordSize); // put a
  __ st_ptr(G4_scratch, Lesp, 1 * wordSize); // put b
  __ push_ptr(Otos_l1);
  __ push_ptr(Otos_l2);
  // stack: ..., c, d, a, b, c, d
}


void TemplateTable::swap() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ld_ptr(Lesp, 2 * wordSize, Otos_l2); // get a
  __ ld_ptr(Lesp, 1 * wordSize, Otos_l1); // get b

  __ st_ptr(Otos_l1, Lesp, 2 * wordSize); // put b
  __ st_ptr(Otos_l2, Lesp, 1 * wordSize); // put a
  // stack: ..., b, a
}


void TemplateTable::iop2(Operation op) {
  transition(itos, itos);
  __ pop_i(O1);
  switch (op) {
   case  add:  __  add(O1, Otos_i, Otos_i);  break;
   case  sub:  __  sub(O1, Otos_i, Otos_i);  break;
     // %%%%% Mul may not exist: better to call .mul? 
   case  mul:  __ smul(O1, Otos_i, Otos_i);  break;
   case _and:  __  and3(O1, Otos_i, Otos_i);  break;
   case  _or:  __   or3(O1, Otos_i, Otos_i);  break;
   case _xor:  __  xor3(O1, Otos_i, Otos_i);  break;
   case  shl:  __  sll(O1, Otos_i, Otos_i);  break;
   case  shr:  __  sra(O1, Otos_i, Otos_i);  break;
   case ushr:  __  srl(O1, Otos_i, Otos_i);  break;
   default: ShouldNotReachHere();
  }
}


void TemplateTable::lop2(Operation op) {
  transition(ltos, ltos);
  __ pop_l(O2);
  switch (op) {
#ifdef _LP64
   case  add:  __ add(O2, Otos_l, Otos_l);  break;
   case  sub:  __ sub(O2, Otos_l, Otos_l);  break;
   case _and:  __ and3( O2, Otos_l, Otos_l);  break;
   case  _or:  __  or3( O2, Otos_l, Otos_l);  break;
   case _xor:  __ xor3( O2, Otos_l, Otos_l);  break;
#else
   case  add:  __ addcc(O3, Otos_l2, Otos_l2);  __ addc(O2, Otos_l1, Otos_l1);  break;
   case  sub:  __ subcc(O3, Otos_l2, Otos_l2);  __ subc(O2, Otos_l1, Otos_l1);  break;
   case _and:  __ and3(  O3, Otos_l2, Otos_l2);  __ and3( O2, Otos_l1, Otos_l1);  break;
   case  _or:  __  or3(  O3, Otos_l2, Otos_l2);  __  or3( O2, Otos_l1, Otos_l1);  break;
   case _xor:  __ xor3(  O3, Otos_l2, Otos_l2);  __ xor3( O2, Otos_l1, Otos_l1);  break;
#endif
   default: ShouldNotReachHere();
  }
}


void TemplateTable::idiv() {
  // %%%%% Later: ForSPARC/V7 call .sdiv library routine, 
  // %%%%% Use ldsw...sdivx on pure V9 ABI. 64 bit safe.
  
  transition(itos, itos);
  __ pop_i(O1); // get 1st op

  // Y contains upper 32 bits of result, set it to 0 or all ones
  __ wry(G0);
  __ mov(~0, G3_scratch);

  __ tst(O1);
     Label neg;
  __ br(Assembler::negative, true, Assembler::pn, neg);
  __ delayed()->wry(G3_scratch);
  __ bind(neg);

     Label ok;
  __ tst(Otos_i); 
  __ throw_if_not_icc( Assembler::notZero, Interpreter::_throw_ArithmeticException_entry, G3_scratch );

  const int min_int = 0x80000000;
  Label regular;
  __ cmp(Otos_i, -1);
  __ br(Assembler::notEqual, false, Assembler::pt, regular);
#ifdef _LP64
  // Don't put set in delay slot 
  // Set will turn into multiple instructions in 64 bit mode
  __ delayed()->nop();
  __ set(min_int, G4_scratch);
#else
  __ delayed()->set(min_int, G4_scratch);
#endif
  Label done;
  __ cmp(O1, G4_scratch);
  __ br(Assembler::equal, true, Assembler::pt, done);
  __ delayed()->mov(O1, Otos_i);   // (mov only executed if branch taken)

  __ bind(regular);
  __ sdiv(O1, Otos_i, Otos_i); // note: irem uses O1 after this instruction!
  __ bind(done);
}


void TemplateTable::irem() {
  transition(itos, itos);
  __ mov(Otos_i, O2); // save divisor
  idiv(); 				// %%%% Hack: exploits fact that idiv leaves dividend in O1 
  __ smul(Otos_i, O2, Otos_i);
  __ sub(O1, Otos_i, Otos_i);
}


void TemplateTable::lmul() {
  transition(ltos, ltos);
  __ pop_l(O2);
#ifdef _LP64
  __ mulx(Otos_l, O2, Otos_l);
#else
  __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::lmul));
#endif

}


void TemplateTable::ldiv() {
  transition(ltos, ltos);

  // check for zero
  __ pop_l(O2);
#ifdef _LP64
  __ tst(Otos_l);
  __ throw_if_not_xcc( Assembler::notZero, Interpreter::_throw_ArithmeticException_entry, G3_scratch);
  __ sdivx(O2, Otos_l, Otos_l);
#else
  __ orcc(Otos_l1, Otos_l2, G0);
  __ throw_if_not_icc( Assembler::notZero, Interpreter::_throw_ArithmeticException_entry, G3_scratch);
  __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::ldiv));
#endif
}


void TemplateTable::lrem() {
  transition(ltos, ltos);

  // check for zero
  __ pop_l(O2);
#ifdef _LP64
  __ tst(Otos_l);
  __ throw_if_not_xcc( Assembler::notZero, Interpreter::_throw_ArithmeticException_entry, G3_scratch);
  __ sdivx(O2, Otos_l, Otos_l2);
  __ mulx (Otos_l2, Otos_l, Otos_l2);
  __ sub  (O2, Otos_l2, Otos_l);
#else
  __ orcc(Otos_l1, Otos_l2, G0);
  __ throw_if_not_icc(Assembler::notZero, Interpreter::_throw_ArithmeticException_entry, G3_scratch);
  __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::lrem));
#endif
}


void TemplateTable::lshl() {
  transition(itos, ltos); // %%%% could optimize, fill delay slot or opt for ultra 

  __ pop_l(O2);                          // shift value in O2, O3
#ifdef _LP64
  __ sllx(O2, Otos_i, Otos_l);
#else
  __ lshl(O2, O3, Otos_i, Otos_l1, Otos_l2, O4);
#endif
}


void TemplateTable::lshr() {
  transition(itos, ltos); // %%%% see lshl comment 

  __ pop_l(O2);                          // shift value in O2, O3
#ifdef _LP64
  __ srax(O2, Otos_i, Otos_l);
#else
  __ lshr(O2, O3, Otos_i, Otos_l1, Otos_l2, O4);
#endif
}



void TemplateTable::lushr() {
  transition(itos, ltos); // %%%% see lshl comment 

  __ pop_l(O2);                          // shift value in O2, O3
#ifdef _LP64
  __ srlx(O2, Otos_i, Otos_l);
#else
  __ lushr(O2, O3, Otos_i, Otos_l1, Otos_l2, O4);
#endif
}


void TemplateTable::fop2(Operation op) {
  transition(ftos, ftos);
  switch (op) {
   case  add:  __  pop_f(F4); __ fadd(FloatRegisterImpl::S, F4, Ftos_f, Ftos_f);  break;
   case  sub:  __  pop_f(F4); __ fsub(FloatRegisterImpl::S, F4, Ftos_f, Ftos_f);  break;
   case  mul:  __  pop_f(F4); __ fmul(FloatRegisterImpl::S, F4, Ftos_f, Ftos_f);  break;
   case  div:  __  pop_f(F4); __ fdiv(FloatRegisterImpl::S, F4, Ftos_f, Ftos_f);  break;
   case  rem:  
     assert(Ftos_f == F0, "just checking");
#ifdef _LP64
     // LP64 calling conventions use F1, F3 for passing 2 floats
     __ pop_f(F1);
     __ fmov(FloatRegisterImpl::S, Ftos_f, F3);
#else
     __ pop_i(O0);
     __ stf(FloatRegisterImpl::S, Ftos_f, __ d_tmp);
     __ ld( __ d_tmp, O1 );
#endif
     __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::frem));
     assert( Ftos_f == F0, "fix this code" );
     break;

   default: ShouldNotReachHere();
  }
}


void TemplateTable::dop2(Operation op) {
  transition(dtos, dtos);
  switch (op) {
   case  add:  __  pop_d(F4); __ fadd(FloatRegisterImpl::D, F4, Ftos_d, Ftos_d);  break;
   case  sub:  __  pop_d(F4); __ fsub(FloatRegisterImpl::D, F4, Ftos_d, Ftos_d);  break;
   case  mul:  __  pop_d(F4); __ fmul(FloatRegisterImpl::D, F4, Ftos_d, Ftos_d);  break;
   case  div:  __  pop_d(F4); __ fdiv(FloatRegisterImpl::D, F4, Ftos_d, Ftos_d);  break;
   case  rem:  
#ifdef _LP64
     // Pass arguments in D0, D2
     __ fmov(FloatRegisterImpl::D, Ftos_f, F2 );
     __ pop_d( F0 );
#else
     // Pass arguments in O0O1, O2O3
     __ stf(FloatRegisterImpl::D, Ftos_f, __ d_tmp);
     __ ldd( __ d_tmp, O2 );
     __ pop_d(Ftos_f);
     __ stf(FloatRegisterImpl::D, Ftos_f, __ d_tmp);
     __ ldd( __ d_tmp, O0 );
#endif
     __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::drem));
     assert( Ftos_d == F0, "fix this code" );
     break;

   default: ShouldNotReachHere();
  }
}


void TemplateTable::ineg() {
  transition(itos, itos);
  __ neg(Otos_i);
}


void TemplateTable::lneg() {
  transition(ltos, ltos);
#ifdef _LP64
  __ sub(G0, Otos_l, Otos_l);
#else
  __ lneg(Otos_l1, Otos_l2);
#endif
}


void TemplateTable::fneg() {
  transition(ftos, ftos);
  __ fneg(FloatRegisterImpl::S, Ftos_f);
}


void TemplateTable::dneg() {
  transition(dtos, dtos);
  // v8 has fnegd if source and dest are the same
  __ fneg(FloatRegisterImpl::D, Ftos_f);
}


void TemplateTable::iinc() {
  transition(vtos, vtos);
  locals_index(G3_scratch);
  __ ldsb(Lbcp, 2, O2);  // load constant
  __ access_local_int(G3_scratch, Otos_i);
  __ add(Otos_i, O2, Otos_i);
  __ st(Otos_i, G3_scratch, 0);    // access_local_int puts E.A. in G3_scratch
}


void TemplateTable::wide_iinc() {
  transition(vtos, vtos);
  locals_index_wide(G3_scratch);
  __ get_2_byte_integer_at_bcp( 4,  O2, O3, InterpreterMacroAssembler::Signed);
  __ access_local_int(G3_scratch, Otos_i);
  __ add(Otos_i, O3, Otos_i);
  __ st(Otos_i, G3_scratch, 0);    // access_local_int puts E.A. in G3_scratch
}


void TemplateTable::convert() {
// %%%%% Factor this first part accross platforms 
  #ifdef ASSERT
    TosState tos_in  = ilgl;
    TosState tos_out = ilgl;
    switch (bytecode()) {
      case Bytecodes::_i2l: // fall through
      case Bytecodes::_i2f: // fall through
      case Bytecodes::_i2d: // fall through
      case Bytecodes::_i2b: // fall through
      case Bytecodes::_i2c: // fall through
      case Bytecodes::_i2s: tos_in = itos; break;
      case Bytecodes::_l2i: // fall through
      case Bytecodes::_l2f: // fall through
      case Bytecodes::_l2d: tos_in = ltos; break;
      case Bytecodes::_f2i: // fall through
      case Bytecodes::_f2l: // fall through
      case Bytecodes::_f2d: tos_in = ftos; break;
      case Bytecodes::_d2i: // fall through
      case Bytecodes::_d2l: // fall through
      case Bytecodes::_d2f: tos_in = dtos; break;
      default             : ShouldNotReachHere();
    }
    switch (bytecode()) {
      case Bytecodes::_l2i: // fall through
      case Bytecodes::_f2i: // fall through
      case Bytecodes::_d2i: // fall through
      case Bytecodes::_i2b: // fall through
      case Bytecodes::_i2c: // fall through
      case Bytecodes::_i2s: tos_out = itos; break;
      case Bytecodes::_i2l: // fall through
      case Bytecodes::_f2l: // fall through
      case Bytecodes::_d2l: tos_out = ltos; break;
      case Bytecodes::_i2f: // fall through
      case Bytecodes::_l2f: // fall through
      case Bytecodes::_d2f: tos_out = ftos; break;
      case Bytecodes::_i2d: // fall through
      case Bytecodes::_l2d: // fall through
      case Bytecodes::_f2d: tos_out = dtos; break;
      default             : ShouldNotReachHere();
    }
    transition(tos_in, tos_out);
  #endif

  
  // Conversion
  Label done;
  switch (bytecode()) {
   case Bytecodes::_i2l: 
#ifdef _LP64
    // Sign extend the 32 bits
    __ sra ( Otos_i, 0, Otos_l );
#else
    __ addcc(Otos_i, 0, Otos_l2);  
    __ br(Assembler::greaterEqual, true, Assembler::pt, done);  
    __ delayed()->clr(Otos_l1); 
    __ set(~0, Otos_l1);  
#endif
    break;
    
   case Bytecodes::_i2f:
    __ st(Otos_i, __ d_tmp );
    __ ldf(FloatRegisterImpl::S,  __ d_tmp, F0);
    __ fitof(FloatRegisterImpl::S, F0, Ftos_f);
    break;
    
   case Bytecodes::_i2d:
    __ st(Otos_i, __ d_tmp);
    __ ldf(FloatRegisterImpl::S,  __ d_tmp, F0);
    __ fitof(FloatRegisterImpl::D, F0, Ftos_f);
    break;
    
   case Bytecodes::_i2b: 
    __ sll(Otos_i, 24, Otos_i); 
    __ sra(Otos_i, 24, Otos_i); 
    break;
    
   case Bytecodes::_i2c:
    __ sll(Otos_i, 16, Otos_i); 
    __ srl(Otos_i, 16, Otos_i); 
    break;
   
   case Bytecodes::_i2s: 
    __ sll(Otos_i, 16, Otos_i);
    __ sra(Otos_i, 16, Otos_i);
    break;

   case Bytecodes::_l2i:
#ifndef _LP64
    __ mov(Otos_l2, Otos_i);
#else
    // Sign-extend into the high 32 bits
    __ sra(Otos_l, 0, Otos_i);
#endif
    break;
    
   case Bytecodes::_l2f:
   case Bytecodes::_l2d:
    __ st_long(Otos_l, __ d_tmp);
    __ ldf(FloatRegisterImpl::D, __ d_tmp, Ftos_d);

    if (VM_Version::v9_instructions_work()) {
      if (bytecode() == Bytecodes::_l2f) {
	__ fxtof(FloatRegisterImpl::S, Ftos_d, Ftos_f);
      } else {
	__ fxtof(FloatRegisterImpl::D, Ftos_d, Ftos_d);
      }
    } else {
      __ call_VM_leaf(
        Lscratch,
        bytecode() == Bytecodes::_l2f
          ? CAST_FROM_FN_PTR(address, SharedRuntime::l2f)
          : CAST_FROM_FN_PTR(address, SharedRuntime::l2d)
      );
    }
    break;
    
  case Bytecodes::_f2i:  {
      Label isNaN;
      // result must be 0 if value is NaN; test by comparing value to itself
      __ fcmp(FloatRegisterImpl::S, Assembler::fcc0, Ftos_f, Ftos_f);
      // According to the v8 manual, you have to have a non-fp instruction
      // between fcmp and fb.
      if (!VM_Version::v9_instructions_work()) {
        __ nop();
      }
      __ fb(Assembler::f_unordered, true, Assembler::pn, isNaN);
      __ delayed()->clr(Otos_i);                                     // NaN
      __ ftoi(FloatRegisterImpl::S, Ftos_f, F30);
      __ stf(FloatRegisterImpl::S, F30, __ d_tmp);
      __ ld(__ d_tmp, Otos_i);
      __ bind(isNaN);
    }
    break;
     
   case Bytecodes::_f2l: 
    // must uncache tos
    __ push_f();
#ifdef _LP64
    __ pop_f(F1);
#else
    __ pop_i(O0);
#endif
    __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::f2l));
    break;

   case Bytecodes::_f2d: 
    __ ftof( FloatRegisterImpl::S, FloatRegisterImpl::D, Ftos_f, Ftos_f);
    break;

   case Bytecodes::_d2i:
   case Bytecodes::_d2l:
    // must uncache tos
    __ push_d();
#ifdef _LP64
    // LP64 calling conventions pass first double arg in D0
    __ pop_d( Ftos_d );
#else
    __ pop_i( O0 );
    __ pop_i( O1 );
#endif
    __ call_VM_leaf(Lscratch,
        bytecode() == Bytecodes::_d2i
          ? CAST_FROM_FN_PTR(address, SharedRuntime::d2i)
          : CAST_FROM_FN_PTR(address, SharedRuntime::d2l));
    break;
    
    case Bytecodes::_d2f:
    if (VM_Version::v9_instructions_work()) {
      __ ftof( FloatRegisterImpl::D, FloatRegisterImpl::S, Ftos_d, Ftos_f);
    }
    else {
      // must uncache tos
      __ push_d();
      __ pop_i(O0);
      __ pop_i(O1);
      __ call_VM_leaf(Lscratch, CAST_FROM_FN_PTR(address, SharedRuntime::d2f));
    }
    break;
    
    default: ShouldNotReachHere();
  }
  __ bind(done);
}


void TemplateTable::lcmp() {
  transition(ltos, itos);

#ifdef _LP64
  __ pop_l(O1); // pop off value 1, value 2 is in O0
  __ lcmp( O1, Otos_l, Otos_i );
#else
  __ pop_l(O2); // cmp O2,3 to O0,1
  __ lcmp( O2, O3, Otos_l1, Otos_l2, Otos_i );
#endif
}


void TemplateTable::float_cmp(bool is_float, int unordered_result) {

  if (is_float) __ pop_f(F2);
  else          __ pop_d(F2);

  assert(Ftos_f == F0  &&  Ftos_d == F0,  "alias checking:");

  __ float_cmp( is_float, unordered_result, F2, F0, Otos_i );
}

void TemplateTable::branch(bool is_jsr, bool is_wide) {
  // Note: on SPARC, we use InterpreterMacroAssembler::if_cmp also.
  __ verify_oop(Lmethod);
  __ verify_thread();

  const Register O2_bumped_count = O2;
  __ profile_taken_branch(G3_scratch, O2_bumped_count);

  // get (wide) offset to O1_disp
  const Register O1_disp = O1;
  if (is_wide)  __ get_4_byte_integer_at_bcp( 1,  G4_scratch, O1_disp,                                    InterpreterMacroAssembler::set_CC);
  else          __ get_2_byte_integer_at_bcp( 1,  G4_scratch, O1_disp, InterpreterMacroAssembler::Signed, InterpreterMacroAssembler::set_CC);
  
  // Handle all the JSR stuff here, then exit.
  // It's much shorter and cleaner than intermingling with the
  // non-JSR normal-branch stuff occuring below.
  if( is_jsr ) {
    // compute return address as bci in Otos_i
    __ ld_ptr(Address(Lmethod, 0, in_bytes(methodOopDesc::const_offset())), G3_scratch);
    __ sub(Lbcp, G3_scratch, G3_scratch);
    __ sub(G3_scratch, in_bytes(constMethodOopDesc::codes_offset()) - (is_wide ? 5 : 3), Otos_i);
  
    // Bump Lbcp to target of JSR
    __ add(Lbcp, O1_disp, Lbcp);
    // And away we go!
    __ dispatch_next_noverify_oop(atos);
    return;
  }

  // Normal (non-jsr) branch handling

  // Save the current Lbcp
  const Register O0_cur_bcp = O0;
  __ mov( Lbcp, O0_cur_bcp );

#ifndef CORE
  bool increment_invocation_counter_for_backward_branches = UseCompiler && UseLoopCounter;
  if ( increment_invocation_counter_for_backward_branches ) {
    Label Lforward;
    // check branch direction
    __ br( Assembler::positive, false,  Assembler::pn, Lforward );
    // Bump bytecode pointer by displacement (take the branch)
    __ delayed()->add( O1_disp, Lbcp, Lbcp );     // add to bc addr
    
    // Update Backedge branch separately from invocations
    const Register G4_invoke_ctr = G4;
    __ increment_backedge_counter(G4_invoke_ctr, G1_scratch);
    if (ProfileInterpreter) {
      __ test_invocation_counter_for_mdp(G4_invoke_ctr, Lbcp, G3_scratch, Lforward);
      if (UseOnStackReplacement) {
        __ test_backedge_count_for_osr(O2_bumped_count, O0_cur_bcp, G3_scratch);
      }
    } else {
      if (UseOnStackReplacement) {
        __ test_backedge_count_for_osr(G4_invoke_ctr, O0_cur_bcp, G3_scratch);
      }
    }
    
    __ bind(Lforward);
  } else
    // Bump bytecode pointer by displacement (take the branch)
    __ add( O1_disp, Lbcp, Lbcp );// add to bc addr
#else
  // Bump bytecode pointer by displacement (take the branch)
  __ add( O1_disp, Lbcp, Lbcp );     // add to bc addr
#endif

  // continue with bytecode @ target
  // %%%%% Like Intel, could speed things up by moving bytecode fetch to code above,
  // %%%%% and changing dispatch_next to dispatch_only 
  __ dispatch_next(vtos);
}


// Note Condition in argument is TemplateTable::Condition
// arg scope is within class scope

void TemplateTable::if_0cmp(Condition cc) {
  // no pointers, integer only!
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  __ cmp( Otos_i, 0);
  __ if_cmp(ccNot(cc), false);
}


void TemplateTable::if_icmp(Condition cc) {
  transition(itos, vtos);
  __ pop_i(O1);
  __ cmp(O1, Otos_i);
  __ if_cmp(ccNot(cc), false);
}


void TemplateTable::if_nullcmp(Condition cc) {
  transition(atos, vtos);
  __ tst(Otos_i);
  __ if_cmp(ccNot(cc), true);
}


void TemplateTable::if_acmp(Condition cc) {
  transition(atos, vtos);
  __ pop_ptr(O1);
  __ verify_oop(O1);
  __ verify_oop(Otos_i);
  __ cmp(O1, Otos_i);
  __ if_cmp(ccNot(cc), true);
}



void TemplateTable::ret() {
  transition(vtos, vtos);
  locals_index(G3_scratch);
  __ access_local_ptr(G3_scratch, Otos_i);
  // Otos_i contains the bci, compute the bcp from that

#ifdef _LP64
#ifdef ASSERT
  // jsr result was labeled as an 'itos' not an 'atos' because we cannot GC
  // the result.  The return address (really a BCI) was stored with an
  // 'astore' because JVM specs claim it's a pointer-sized thing.  Hence in
  // the 64-bit build the 32-bit BCI is actually in the low bits of a 64-bit
  // loaded value.
  { Label zzz ;
     __ set (65536, G3_scratch) ;
     __ cmp (Otos_i, G3_scratch) ;
     __ bp( Assembler::lessEqualUnsigned, false, Assembler::xcc, Assembler::pn, zzz);
     __ delayed()->nop();
     __ stop("BCI is in the wrong register half?");
     __ bind (zzz) ;
  }
#endif
#endif

  __ profile_ret(vtos, Otos_i, G4_scratch);

  __ ld_ptr(Address(Lmethod, 0, in_bytes(methodOopDesc::const_offset())), G3_scratch);
  __ add(G3_scratch, Otos_i, G3_scratch);
  __ add(G3_scratch, in_bytes(constMethodOopDesc::codes_offset()), Lbcp);
  __ dispatch_next(vtos);
}


void TemplateTable::wide_ret() {
  transition(vtos, vtos);
  locals_index_wide(G3_scratch);
  __ access_local_ptr(G3_scratch, Otos_i);
  // Otos_i contains the bci, compute the bcp from that

  __ profile_ret(vtos, Otos_i, G4_scratch);

  __ ld_ptr(Address(Lmethod, 0, in_bytes(methodOopDesc::const_offset())), G3_scratch);
  __ add(G3_scratch, Otos_i, G3_scratch);
  __ add(G3_scratch, in_bytes(constMethodOopDesc::codes_offset()), Lbcp);
  __ dispatch_next(vtos);
}


void TemplateTable::tableswitch() {
  transition(itos, vtos);
  Label default_case, continue_execution;

  // align bcp
  __ add(Lbcp, BytesPerInt, O1);
  __ and3(O1, -BytesPerInt, O1);
  // load lo, hi
  __ ld(O1, 1 * BytesPerInt, O2);	// Low Byte
  __ ld(O1, 2 * BytesPerInt, O3);	// High Byte
#ifdef _LP64
  // Sign extend the 32 bits
  __ sra ( Otos_i, 0, Otos_i );
#endif /* _LP64 */

  // check against lo & hi
  __ cmp( Otos_i, O2);
  __ br( Assembler::less, false, Assembler::pn, default_case);
  __ delayed()->cmp( Otos_i, O3 );
  __ br( Assembler::greater, false, Assembler::pn, default_case);
  // lookup dispatch offset
  __ delayed()->sub(Otos_i, O2, O2);
  __ profile_switch_case(O2, O3, G3_scratch, G4_scratch);
  __ sll(O2, LogBytesPerInt, O2);
  __ add(O2, 3 * BytesPerInt, O2);
  __ ba(false, continue_execution);
  __ delayed()->ld(O1, O2, O2);
  // handle default
  __ bind(default_case);
  __ profile_switch_default(O3);
  __ ld(O1, 0, O2); // get default offset
  // continue execution
  __ bind(continue_execution);
  __ add(Lbcp, O2, Lbcp);
  __ dispatch_next(vtos);
}


void TemplateTable::lookupswitch() {
  transition(itos, itos);
  __ stop("lookupswitch bytecode should have been rewritten");
}  

void TemplateTable::fast_linearswitch() {
  transition(itos, vtos);
    Label loop_entry, loop, found, continue_execution;
  // align bcp
  __ add(Lbcp, BytesPerInt, O1);
  __ and3(O1, -BytesPerInt, O1);
 // set counter
  __ ld(O1, BytesPerInt, O2);
  __ sll(O2, LogBytesPerInt + 1, O2); // in word-pairs
  __ add(O1, 2 * BytesPerInt, O3); // set first pair addr
  __ ba(false, loop_entry);
  __ delayed()->add(O3, O2, O2); // counter now points past last pair
  
  // table search
  __ bind(loop);
  __ cmp(O4, Otos_i);
  __ br(Assembler::equal, true, Assembler::pn, found);
  __ delayed()->ld(O3, BytesPerInt, O4); // offset -> O4
  __ inc(O3, 2 * BytesPerInt);

  __ bind(loop_entry);
  __ cmp(O2, O3);
  __ brx(Assembler::greaterUnsigned, true, Assembler::pt, loop);
  __ delayed()->ld(O3, 0, O4);
  
  // default case
  __ ld(O1, 0, O4); // get default offset
#ifndef CORE
  if (ProfileInterpreter) {
    __ profile_switch_default(O3);
    __ ba(false, continue_execution);
    __ delayed()->nop();
  }
#endif // !CORE

  // entry found -> get offset
  __ bind(found);
#ifndef CORE
  if (ProfileInterpreter) {
    __ sub(O3, O1, O3);
    __ sub(O3, 2*BytesPerInt, O3);
    __ srl(O3, LogBytesPerInt + 1, O3); // in word-pairs
    __ profile_switch_case(O3, O1, O2, G3_scratch);
    
    __ bind(continue_execution);
  }
#endif // !CORE
  __ add(Lbcp, O4, Lbcp);
  __ dispatch_next(vtos);
}


void TemplateTable::fast_binaryswitch() {
  transition(itos, vtos);
  // Implementation using the following core algorithm: (copied from Intel)
  //
  // int binary_search(int key, LookupswitchPair* array, int n) {
  //   // Binary search according to "Methodik des Programmierens" by
  //   // Edsger W. Dijkstra and W.H.J. Feijen, Addison Wesley Germany 1985.
  //   int i = 0;
  //   int j = n;
  //   while (i+1 < j) {
  //     // invariant P: 0 <= i < j <= n and (a[i] <= key < a[j] or Q)
  //     // with      Q: for all i: 0 <= i < n: key < a[i]
  //     // where a stands for the array and assuming that the (inexisting)
  //     // element a[n] is infinitely big.
  //     int h = (i + j) >> 1;
  //     // i < h < j
  //     if (key < array[h].fast_match()) {
  //       j = h;
  //     } else {
  //       i = h;
  //     }
  //   }
  //   // R: a[i] <= key < a[i+1] or Q
  //   // (i.e., if key is within array, i is the correct index)
  //   return i;
  // }
  
  // register allocation
  assert(Otos_i == O0, "alias checking");
  const Register Rkey     = Otos_i;                    // already set (tosca)
  const Register Rarray   = O1;
  const Register Ri       = O2;
  const Register Rj       = O3;
  const Register Rh       = O4; 
  const Register Rscratch = O5;
  
  const int log_entry_size = 3;
  const int entry_size = 1 << log_entry_size;
  
  Label found;
  // Find Array start
  __ add(Lbcp, 3 * BytesPerInt, Rarray);
  __ and3(Rarray, -BytesPerInt, Rarray);
  // initialize i & j (in delay slot)
  __ clr( Ri );

  // and start
  Label entry;
  __ ba(false, entry);
  __ delayed()->ld( Rarray, -BytesPerInt, Rj);
  // (Rj is already in the native byte-ordering.)
  
  // binary search loop
  { Label loop;
    __ bind( loop );
    // int h = (i + j) >> 1;
    __ sra( Rh, 1, Rh );
    // if (key < array[h].fast_match()) {
    //   j = h;
    // } else {
    //   i = h;
    // }
    __ sll( Rh, log_entry_size, Rscratch );
    __ ld( Rarray, Rscratch, Rscratch );
    // (Rscratch is already in the native byte-ordering.)
    __ cmp( Rkey, Rscratch );
    if ( VM_Version::v9_instructions_work() ) {
      __ movcc( Assembler::less,         false, Assembler::icc, Rh, Rj );  // j = h if (key <  array[h].fast_match())
      __ movcc( Assembler::greaterEqual, false, Assembler::icc, Rh, Ri );  // i = h if (key >= array[h].fast_match())
    }
    else {
      Label end_of_if;
      __ br( Assembler::less, true, Assembler::pt, end_of_if );
      __ delayed()->mov( Rh, Rj ); // if (<) Rj = Rh
      __ mov( Rh, Ri ); 	   // else i = h 
      __ bind(end_of_if);          // }
    }
    
    // while (i+1 < j)
    __ bind( entry );
    __ add( Ri, 1, Rscratch );
    __ cmp(Rscratch, Rj);
    __ br( Assembler::less, true, Assembler::pt, loop );
    __ delayed()->add( Ri, Rj, Rh ); // start h = i + j  >> 1;
  }
  
  // end of binary search, result index is i (must check again!)
  Label default_case;
  Label continue_execution;
#ifndef CORE
  if (ProfileInterpreter) {
    __ mov( Ri, Rh ); 	  	   // Save index in i for profiling
  }
#endif // !CORE
  __ sll( Ri, log_entry_size, Ri );
  __ ld( Rarray, Ri, Rscratch );
  // (Rscratch is already in the native byte-ordering.)
  __ cmp( Rkey, Rscratch );
  __ br( Assembler::notEqual, true, Assembler::pn, default_case );
  __ delayed()->ld( Rarray, -2 * BytesPerInt, Rj ); // load default offset -> j
  
  // entry found -> j = offset
  __ inc( Ri, BytesPerInt );
  __ profile_switch_case(Rh, Rj, Rscratch, Rkey);
  __ ld( Rarray, Ri, Rj );
  // (Rj is already in the native byte-ordering.)

#ifndef CORE
  if (ProfileInterpreter) {
    __ ba(false, continue_execution);
    __ delayed()->nop();
  }
#endif // !CORE
  
  __ bind(default_case); // fall through (if not profiling)
  __ profile_switch_default(Ri);

  __ bind(continue_execution);
  __ add( Lbcp, Rj, Lbcp );
  __ dispatch_next( vtos );
}


void TemplateTable::_return(TosState state) {
  transition(state, state);
  assert(_desc->calls_vm(), "inconsistent calls_vm information");
  __ unlock_if_synchronized_method(state, /* throw_monitor_exception */ true);

  // save result (push state before jvmdi/jvmpi call and pop it afterwards) and notify jvmdi/jvmpi
  __ notify_method_exit(false, state); 

  __ verify_oop(Otos_i, state);
  __ verify_oop(Lmethod);
  __ verify_thread();

  // return tos
  assert(Otos_l1 == Otos_i, "adjust code below");
  switch (state) {
#ifdef _LP64
  case ltos: __ mov(Otos_l, Otos_l->after_save()); break;
#else
  case ltos: __ mov(Otos_l2, Otos_l2->after_save()); // fall through
#endif
  case btos:                                         // fall through
  case ctos:
  case stos:                                         // fall through
  case atos:                                         // fall through
  case itos: __ mov(Otos_l1, Otos_l1->after_save()); break;
  case ftos:                                         // fall through
  case dtos:                                         // fall through
  case vtos: /* nothing to do */                     break;
  default  : ShouldNotReachHere();
  }
  
  // The caller's SP was adjusted upon method entry to accomodate
  // the callee's non-argument locals. Undo that adjustment.
  const Register Gtmp1 = G3_scratch;
  __ ret();                             // return to caller
  __ delayed()->restore(IsavedSP, G0, SP);
}


// ----------------------------------------------------------------------------
// Volatile variables demand their effects be made known to all CPU's in
// order.  Store buffers on most chips allow reads & writes to reorder; the
// JMM's ReadAfterWrite.java test fails in -Xint mode without some kind of
// memory barrier (i.e., it's not sufficient that the interpreter does not
// reorder volatile references, the hardware also must not reorder them).
// 
// According to the new Java Memory Model (JMM):
// (1) All volatiles are serialized wrt to each other.  
// ALSO reads & writes act as aquire & release, so:
// (2) A read cannot let unrelated NON-volatile memory refs that happen after
// the read float up to before the read.  It's OK for non-volatile memory refs
// that happen before the volatile read to float down below it.
// (3) Similar a volatile write cannot let unrelated NON-volatile memory refs
// that happen BEFORE the write float down to after the write.  It's OK for
// non-volatile memory refs that happen after the volatile write to float up
// before it.
//
// We only put in barriers around volatile refs (they are expensive), not
// _between_ memory refs (that would require us to track the flavor of the
// previous memory refs).  Requirements (2) and (3) require some barriers
// before volatile stores and after volatile loads.  These nearly cover
// requirement (1) but miss the volatile-store-volatile-load case.  This final
// case is placed after volatile-stores although it could just as well go
// before volatile-loads.
void TemplateTable::volatile_barrier(Assembler::Membar_mask_bits order_constraint) {
  // Helper function to insert a is-volatile test and memory barrier
  // All current sparc implementations run in TSO, needing only StoreLoad
  if ((order_constraint & Assembler::StoreLoad) == 0) return;
  __ membar( order_constraint );
}

// ----------------------------------------------------------------------------
void TemplateTable::resolve_cache_and_index(int byte_no, Register Rcache, Register index) {
  assert(byte_no == 1 || byte_no == 2, "byte_no out of range");
  // Depends on cpCacheOop layout!
  const int shift_count = (1 + byte_no)*BitsPerByte;
  Label resolved;

  __ get_cache_and_index_at_bcp(Rcache, index, 1);
  __ ld_ptr(Address(Rcache, 0, in_bytes(constantPoolCacheOopDesc::base_offset() +
                                        ConstantPoolCacheEntry::indices_offset())), Lbyte_code);

  __ srl(  Lbyte_code, shift_count, Lbyte_code );
  __ and3( Lbyte_code,        0xFF, Lbyte_code );
  __ cmp(  Lbyte_code, (int)bytecode());
  __ br(   Assembler::equal, false, Assembler::pt, resolved);
  __ delayed()->set((int)bytecode(), O1);

  address entry;
  switch (bytecode()) {
    case Bytecodes::_getstatic      : // fall through
    case Bytecodes::_putstatic      : // fall through
    case Bytecodes::_getfield       : // fall through
    case Bytecodes::_putfield       : entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_get_put); break;
    case Bytecodes::_invokevirtual  : // fall through
    case Bytecodes::_invokespecial  : // fall through
    case Bytecodes::_invokestatic   : // fall through
    case Bytecodes::_invokeinterface: entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invoke);  break;
    default                         : ShouldNotReachHere();                                 break;
  }
  // first time invocation - must resolve first
  __ call_VM(noreg, entry, O1); 
  // Update registers with resolved info
  __ get_cache_and_index_at_bcp(Rcache, index, 1);
  __ bind(resolved);
}

void TemplateTable::load_invoke_cp_cache_entry(int byte_no,
                                               Register Rmethod,
                                               Register Ritable_index,
                                               Register Rflags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal) {
  // Uses both G3_scratch and G4_scratch
  Register Rcache = G3_scratch;
  Register Rscratch = G4_scratch;
  assert_different_registers(Rcache, Rmethod, Ritable_index);

  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  // determine constant pool cache field offsets
  const int method_offset = in_bytes(
    cp_base_offset +
      (is_invokevirtual
       ? ConstantPoolCacheEntry::f2_offset()
       : ConstantPoolCacheEntry::f1_offset()
      )
    );
  const int flags_offset = in_bytes(cp_base_offset +
                                    ConstantPoolCacheEntry::flags_offset());
  // access constant pool cache fields
  const int index_offset = in_bytes(cp_base_offset + 
                                    ConstantPoolCacheEntry::f2_offset());

  if (is_invokevfinal) {
    __ get_cache_and_index_at_bcp(Rcache, Rscratch, 1);
  } else {
    resolve_cache_and_index(byte_no, Rcache, Rscratch);
  }

  __ ld_ptr(Address(Rcache, 0, method_offset), Rmethod);
  if (Ritable_index != noreg) {
    __ ld_ptr(Address(Rcache, 0, index_offset), Ritable_index);
  }
  __ ld_ptr(Address(Rcache, 0, flags_offset),  Rflags);
}


void TemplateTable::load_field_cp_cache_entry(int byte_no,
                                              Register Robj,
                                              Register Roffset,
                                              Register Rflags,
                                              bool is_static) {
  // Uses both G3_scratch and G4_scratch
  Register Rcache = G3_scratch;
  Register Rscratch = G4_scratch;
  assert_different_registers(Rcache, Rflags, Roffset);

  // Potential call into the VM to resolve constant pool here: could destroy
  // registers
  resolve_cache_and_index(byte_no, Rcache, Rscratch);
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset +
                             ConstantPoolCacheEntry::flags_offset())), Rflags);
  __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset +
                             ConstantPoolCacheEntry::f2_offset())), Roffset);
  if (is_static) {
    __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset +
                             ConstantPoolCacheEntry::f1_offset())), Robj);
  }
}

void TemplateTable::jvmti_post_field_access(int byte_no, bool is_static, bool has_tos) {
  Register Rcache = G3_scratch;
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  if (JvmtiExport::can_post_field_access()) { 
    // Check to see if a field access watch has been set before we take
    // the time to call into the VM.
    Label Label1;
    Address get_field_access_count_addr(G4_scratch, (address)JvmtiExport::get_field_access_count_addr(), relocInfo::none);
    __ load_contents(get_field_access_count_addr, G4_scratch);
    __ tst(G4_scratch);
    __ br(Assembler::zero, false, Assembler::pt, Label1);
    __ delayed()->nop();

    // Potential call into the VM to resolve constant pool here: could destroy registers

    if (has_tos) {
      // quickened
      __ get_cache_and_index_at_bcp(Rcache, G4_scratch, 1);
    } else {
      resolve_cache_and_index(byte_no, Rcache, G4_scratch);
    }

    __ add(Rcache, in_bytes(cp_base_offset), G4_scratch);

    if (is_static) {
      __ clr(Otos_i);
    } else {
      if (has_tos) {
      // save object pointer before call_VM() clobbers it
        __ mov(Otos_i, Lscratch);
      } else {
        __ ld_ptr(Lesp, wordSize, Otos_i);      // Load top of stack (do not pop the value off the stack);
      }
      __ verify_oop(Otos_i);
    }
    // Otos_i:    object pointer or NULL if static
    // G4_scratch: cache entry pointer
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::post_field_access), Otos_i, G4_scratch);
    if (!is_static && has_tos) {
      __ mov(Lscratch, Otos_i);  // restore object pointer
      __ verify_oop(Otos_i);
    }
    __ bind(Label1);
  }
}

void TemplateTable::getfield_or_static(int byte_no, bool is_static) {
  transition(vtos, vtos);
  Register Rcache = G3_scratch;
  Register Rclass = Rcache;
  Register Roffset= G4_scratch;
  Register Rflags = G1_scratch;
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  jvmti_post_field_access(byte_no, is_static, false);

  // Uses Rcache
  load_field_cp_cache_entry(byte_no, Rclass, Roffset, Rflags, is_static);

  if (!is_static) {
    __ pop_ptr(Rclass);             // Get object 
  }

  __ verify_oop(Rclass);
  if (!is_static) {
    __ null_check(Rclass);
  }

  Label exit;
  
  Assembler::Membar_mask_bits membar_bits =
    Assembler::Membar_mask_bits(Assembler::LoadLoad | Assembler::LoadStore);

  if (__ membar_has_effect(membar_bits)) {
    // Get volatile flag
    __ set((1 << ConstantPoolCacheEntry::volatileField), Lscratch);
    __ and3(Rflags, Lscratch, Lscratch);
  }

  Label checkVolatile;

  // compute field type
  Label notByte, notInt, notShort, notChar, notLong, notFloat, notObj;
  __ srl(Rflags, ConstantPoolCacheEntry::tosBits, Rflags);
  // Make sure we don't need to mask Rflags for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();

  // Check atos before itos for getstatic, more likely (in Queens at least)
  __ cmp(Rflags, atos);
  __ br(Assembler::notEqual, false, Assembler::pt, notObj);
  __ delayed() ->cmp(Rflags, itos);

  // atos
  __ ld_ptr(Rclass, Roffset, Otos_i);          
  __ verify_oop(Otos_i);
  __ push(atos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_agetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notObj);

  // cmp(Rflags, itos);
  __ br(Assembler::notEqual, false, Assembler::pt, notInt);
  __ delayed() ->cmp(Rflags, ltos);

  // itos
  __ ld(Rclass, Roffset, Otos_i);
  __ push(itos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_igetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notInt);

  // cmp(Rflags, ltos);
  __ br(Assembler::notEqual, false, Assembler::pt, notLong);
  __ delayed() ->cmp(Rflags, btos);

  // ltos
  // load must be atomic
  __ ld_long(Rclass, Roffset, Otos_l);
  __ push(ltos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_lgetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notLong);

  // cmp(Rflags, btos);
  __ br(Assembler::notEqual, false, Assembler::pt, notByte);
  __ delayed() ->cmp(Rflags, ctos);

  // btos
  __ ldsb(Rclass, Roffset, Otos_i);
  __ push(itos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bgetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notByte);

  // cmp(Rflags, ctos);
  __ br(Assembler::notEqual, false, Assembler::pt, notChar);
  __ delayed() ->cmp(Rflags, stos);

  // ctos
  __ lduh(Rclass, Roffset, Otos_i);
  __ push(itos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_cgetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notChar);

  // cmp(Rflags, stos);
  __ br(Assembler::notEqual, false, Assembler::pt, notShort);
  __ delayed() ->cmp(Rflags, ftos);

  // stos
  __ ldsh(Rclass, Roffset, Otos_i);
  __ push(itos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_sgetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notShort);


  // cmp(Rflags, ftos);
  __ br(Assembler::notEqual, false, Assembler::pt, notFloat);
  __ delayed() ->tst(Lscratch);

  // ftos
  __ ldf(FloatRegisterImpl::S, Rclass, Roffset, Ftos_f);   
  __ push(ftos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_fgetfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notFloat);


  // dtos
  __ ldf(FloatRegisterImpl::D, Rclass, Roffset, Ftos_d);
  __ push(dtos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_dgetfield, G3_scratch, G4_scratch);
  }

  __ bind(checkVolatile);
  if (__ membar_has_effect(membar_bits)) {
    // __ tst(Lscratch); executed in delay slot
    __ br(Assembler::zero, false, Assembler::pt, exit);
    __ delayed()->nop();
    volatile_barrier(membar_bits);
  }

  __ bind(exit);
}


void TemplateTable::getfield(int byte_no) {
  getfield_or_static(byte_no, false);
}

void TemplateTable::getstatic(int byte_no) {
  getfield_or_static(byte_no, true);
}


void TemplateTable::fast_accessfield(TosState state) {
  transition(atos, state);
  Register Rcache = G3_scratch;
  Register Roffset = G4_scratch;
  Register Rflags = Rcache;
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  jvmti_post_field_access(/*byte_no*/1, /*is_static*/false, /*has_tos*/true);

  __ get_cache_and_index_at_bcp(Rcache, G4_scratch, 1);
  __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset + ConstantPoolCacheEntry::f2_offset())), Roffset);

  __ null_check(Otos_i);
  __ verify_oop(Otos_i);

  Label exit;

  Assembler::Membar_mask_bits membar_bits = 
    Assembler::Membar_mask_bits(Assembler::LoadLoad | Assembler::LoadStore);
  if (__ membar_has_effect(membar_bits)) {
    // Get volatile flag
    __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset + ConstantPoolCacheEntry::f2_offset())), Rflags);
    __ set((1 << ConstantPoolCacheEntry::volatileField), Lscratch);
  }

  switch (bytecode()) {
    case Bytecodes::_fast_bgetfield: 
      __ ldsb(Otos_i, Roffset, Otos_i);
      break;
    case Bytecodes::_fast_cgetfield: 
      __ lduh(Otos_i, Roffset, Otos_i);
      break;
    case Bytecodes::_fast_sgetfield: 
      __ ldsh(Otos_i, Roffset, Otos_i);
      break;
    case Bytecodes::_fast_igetfield: 
      __ ld(Otos_i, Roffset, Otos_i);
      break;
    case Bytecodes::_fast_lgetfield: 
      __ ld_long(Otos_i, Roffset, Otos_l);
      break;
    case Bytecodes::_fast_fgetfield: 
      __ ldf(FloatRegisterImpl::S, Otos_i, Roffset, Ftos_f); 
      break;
    case Bytecodes::_fast_dgetfield: 
      __ ldf(FloatRegisterImpl::D, Otos_i, Roffset, Ftos_d); 
      break;
    case Bytecodes::_fast_agetfield:
      __ ld_ptr(Otos_i, Roffset, Otos_i);          
      break;
    default:
      ShouldNotReachHere();
  }
  
  if (__ membar_has_effect(membar_bits)) {
    __ btst(Lscratch, Rflags);
    __ br(Assembler::zero, false, Assembler::pt, exit);
    __ delayed()->nop();
    volatile_barrier(membar_bits);
    __ bind(exit);
  }

  if (state == atos) {
    __ verify_oop(Otos_i);    // does not blow flags!
  }
}

void TemplateTable::jvmti_post_fast_field_mod() {
  if (JvmtiExport::can_post_field_modification()) { 
    // Check to see if a field modification watch has been set before we take
    // the time to call into the VM.
    Label done;
    Address get_field_modification_count_addr(G4_scratch, (address)JvmtiExport::get_field_modification_count_addr(), relocInfo::none);
    __ load_contents(get_field_modification_count_addr, G4_scratch);
    __ tst(G4_scratch);
    __ br(Assembler::zero, false, Assembler::pt, done);
    __ delayed()->nop();
    __ pop_ptr(G4_scratch);     // copy the object pointer from tos
    __ verify_oop(G4_scratch);
    __ push_ptr(G4_scratch);    // put the object pointer back on tos
    __ get_cache_entry_pointer_at_bcp(G1_scratch, G3_scratch, 1);
    // Save tos values before call_VM() clobbers them. Since we have
    // to do it for every data type, we use the saved values as the
    // jvalue object.
    switch (bytecode()) {  // save tos values before call_VM() clobbers them
    case Bytecodes::_fast_aputfield: __ push_ptr(Otos_i); break;
    case Bytecodes::_fast_bputfield: // fall through
    case Bytecodes::_fast_sputfield: // fall through
    case Bytecodes::_fast_cputfield: // fall through
    case Bytecodes::_fast_iputfield: __ push_i(Otos_i); break;
    case Bytecodes::_fast_dputfield: __ push_d(Ftos_d); break;
    case Bytecodes::_fast_fputfield: __ push_f(Ftos_f); break;
    // get words in right order for use as jvalue object
    case Bytecodes::_fast_lputfield: __ push_l(Otos_l); break;
    }
    // setup pointer to jvalue object
    __ mov(Lesp, G3_scratch); __ inc(G3_scratch, wordSize);
    // G4_scratch:  object pointer
    // G1_scratch: cache entry pointer
    // G3_scratch: jvalue object on the stack
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::post_field_modification), G4_scratch, G1_scratch, G3_scratch);
    switch (bytecode()) {             // restore tos values
    case Bytecodes::_fast_aputfield: __ pop_ptr(Otos_i); break;
    case Bytecodes::_fast_bputfield: // fall through
    case Bytecodes::_fast_sputfield: // fall through
    case Bytecodes::_fast_cputfield: // fall through
    case Bytecodes::_fast_iputfield: __ pop_i(Otos_i); break;
    case Bytecodes::_fast_dputfield: __ pop_d(Ftos_d); break;
    case Bytecodes::_fast_fputfield: __ pop_f(Ftos_f); break;
    case Bytecodes::_fast_lputfield: __ pop_l(Otos_l); break;
    }
    __ bind(done);
  }
}

void TemplateTable::jvmti_post_field_mod(int byte_no, bool is_static) {
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  if (JvmtiExport::can_post_field_modification()) {
    // Check to see if a field modification watch has been set before we take
    // the time to call into the VM.
    Label Label1;
    Address get_field_modification_count_addr(G4_scratch, (address)JvmtiExport::get_field_modification_count_addr(), relocInfo::none);
    __ load_contents(get_field_modification_count_addr, G4_scratch);
    __ tst(G4_scratch);
    __ br(Assembler::zero, false, Assembler::pt, Label1);
    __ delayed()->nop();

    // We depend on the bytecode being resolved and the cpCache being correctly filled in.
    resolve_cache_and_index(byte_no, G1_scratch, G4_scratch);
    __ add(G1_scratch, in_bytes(cp_base_offset), G3_scratch);
    if (is_static) {
      // Life is simple.  Null out the object pointer.
      __ clr(G4_scratch);
    } else {
      Register Rflags = G1_scratch;
      // Life is harder. The stack holds the value on top, followed by the
      // object.  We don't know the size of the value, though; it could be
      // one or two words depending on its type. As a result, we must find
      // the type to determine where the object is.

      Label two_word;
      __ ld_ptr(Address(G1_scratch, 0, in_bytes(cp_base_offset + ConstantPoolCacheEntry::flags_offset())), Rflags);
      __ mov(Lesp, G4_scratch);
      __ inc(G4_scratch, wordSize * 3);
      __ srl(Rflags, ConstantPoolCacheEntry::tosBits, Rflags);
      // Make sure we don't need to mask Rflags for tosBits after the above shift
      ConstantPoolCacheEntry::verify_tosBits();
      __ cmp(Rflags, ltos);
      __ br(Assembler::equal, false, Assembler::pt, two_word);
      __ delayed()->cmp(Rflags, dtos);
      __ br(Assembler::equal, false, Assembler::pt, two_word);
      __ delayed()->nop();
      __ inc(G4_scratch, -wordSize);        // one word jvalue (not ltos, dtos)
      __ bind(two_word);

      // setup object pointer
      __ ld_ptr(G4_scratch, 0, G4_scratch);
      __ verify_oop(G4_scratch);
    }
    // setup pointer to jvalue object
    __ mov(Lesp, G1_scratch); __ inc(G1_scratch, wordSize);
    // G4_scratch:  object pointer or NULL if static
    // G3_scratch: cache entry pointer
    // G1_scratch: jvalue object on the stack
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::post_field_modification), G4_scratch, G3_scratch, G1_scratch);
    __ bind(Label1);
  }
}

void TemplateTable::putfield_or_static(int byte_no, bool is_static) {
  transition(vtos, vtos);
  Register Rcache = G3_scratch;
  Register Rclass = Rcache;
  Register Roffset= G4_scratch;
  Register Rflags = G1_scratch;
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  jvmti_post_field_mod(byte_no, is_static);

  load_field_cp_cache_entry(byte_no, Rclass, Roffset, Rflags, is_static);

  Assembler::Membar_mask_bits read_bits = 
    Assembler::Membar_mask_bits(Assembler::LoadStore | Assembler::StoreStore);
  Assembler::Membar_mask_bits write_bits = Assembler::StoreLoad;

  Label notVolatile, checkVolatile, exit;
  if (__ membar_has_effect(read_bits) || __ membar_has_effect(write_bits)) {
    __ set((1 << ConstantPoolCacheEntry::volatileField), Lscratch);
    __ and3(Rflags, Lscratch, Lscratch);
    
    if (__ membar_has_effect(read_bits)) {
      __ tst(Lscratch);
      __ br(Assembler::zero, false, Assembler::pt, notVolatile); 
      __ delayed()->nop();
      volatile_barrier(read_bits);
      __ bind(notVolatile);
    }
  }

  __ srl(Rflags, ConstantPoolCacheEntry::tosBits, Rflags);
  // Make sure we don't need to mask Rflags for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();

  // compute field type
  Label notInt, notShort, notChar, notObj, notByte, notLong, notFloat;

  if (is_static) {
    // putstatic with object type most likely, check that first
    __ cmp(Rflags, atos );
    __ br(Assembler::notEqual, false, Assembler::pt, notObj);
    __ delayed() ->cmp(Rflags, itos );

    // atos
    __ pop_ptr();
    __ st_ptr(Otos_i, Rclass, Roffset);          
    __ store_check(G1_scratch, Rclass, Roffset);
    __ ba(false, checkVolatile);
    __ delayed()->tst(Lscratch);
  
    __ bind(notObj);                               

    // cmp(Rflags, itos ); 
    __ br(Assembler::notEqual, false, Assembler::pt, notInt);
    __ delayed() ->cmp(Rflags, btos ); 

    // itos
    __ pop_i();
    __ st(Otos_i, Rclass, Roffset);
    __ ba(false, checkVolatile);
    __ delayed()->tst(Lscratch);
  
    __ bind(notInt);
  
  } else {
    // putfield with int type most likely, check that first
    __ cmp(Rflags, itos );
    __ br(Assembler::notEqual, false, Assembler::pt, notInt);
    __ delayed() ->cmp(Rflags, atos );
  
    // itos
    __ pop_i();
    __ pop_ptr(Rclass);
    __ st(Otos_i, Rclass, Roffset);
    patch_bytecode(Bytecodes::_fast_iputfield, G3_scratch, G4_scratch);
    __ ba(false, checkVolatile);
    __ delayed()->tst(Lscratch);
  
    __ bind(notInt);
    // cmp(Rflags, atos ); 
    __ br(Assembler::notEqual, false, Assembler::pt, notObj);
    __ delayed() ->cmp(Rflags, btos ); 
  
    // atos
    __ pop_ptr();
    __ pop_ptr(Rclass);
    __ st_ptr(Otos_i, Rclass, Roffset);          
    __ store_check(G1_scratch, Rclass, Roffset);
    patch_bytecode(Bytecodes::_fast_aputfield, G3_scratch, G4_scratch);
    __ ba(false, checkVolatile);
    __ delayed()->tst(Lscratch);
  
    __ bind(notObj);                               
  }

  // cmp(Rflags, btos );
  __ br(Assembler::notEqual, false, Assembler::pt, notByte);
  __ delayed() ->cmp(Rflags, ltos );

  // btos
  __ pop_i();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ stb(Otos_i, Rclass, Roffset);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bputfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notByte);

  // cmp(Rflags, ltos );
  __ br(Assembler::notEqual, false, Assembler::pt, notLong);
  __ delayed() ->cmp(Rflags, ctos );

  // ltos 
  __ pop_l();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ st_long(Otos_l, Rclass, Roffset);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_lputfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notLong);

  // cmp(Rflags, ctos ); 
  __ br(Assembler::notEqual, false, Assembler::pt, notChar);
  __ delayed() ->cmp(Rflags, stos ); 

  // ctos (char)
  __ pop_i();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ sth(Otos_i, Rclass, Roffset);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_cputfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notChar);                               
  // cmp(Rflags, stos ); 
  __ br(Assembler::notEqual, false, Assembler::pt, notShort);
  __ delayed() ->cmp(Rflags, ftos ); 

  // stos (char)
  __ pop_i();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ sth(Otos_i, Rclass, Roffset);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_sputfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notShort);                               
  // cmp(Rflags, ftos );
  __ br(Assembler::notZero, false, Assembler::pt, notFloat);
  __ delayed()->nop();

  // ftos
  __ pop_f();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ stf(FloatRegisterImpl::S, Ftos_f, Rclass, Roffset);   
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_fputfield, G3_scratch, G4_scratch);
  }
  __ ba(false, checkVolatile);
  __ delayed()->tst(Lscratch);

  __ bind(notFloat);

  // dtos
  __ pop_d();
  if (!is_static) {
    __ pop_ptr(Rclass);
  }
  __ stf(FloatRegisterImpl::D, Ftos_d, Rclass, Roffset);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_dputfield, G3_scratch, G4_scratch);
  }

  __ bind(checkVolatile);
  __ tst(Lscratch);

  if (__ membar_has_effect(write_bits)) {
    // __ tst(Lscratch); in delay slot
    __ br(Assembler::zero, false, Assembler::pt, exit); 
    __ delayed()->nop();
    volatile_barrier(Assembler::StoreLoad);
    __ bind(exit);
  }
}

void TemplateTable::fast_storefield(TosState state) {
  transition(state, vtos);
  Register Rcache = G3_scratch;
  Register Rclass = Rcache;
  Register Roffset= G4_scratch;
  Register Rflags = G1_scratch;
  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();

  jvmti_post_fast_field_mod();

  __ get_cache_and_index_at_bcp(Rcache, G4_scratch, 1);

  Assembler::Membar_mask_bits read_bits =
    Assembler::Membar_mask_bits(Assembler::LoadStore | Assembler::StoreStore);
  Assembler::Membar_mask_bits write_bits = Assembler::StoreLoad;

  Label notVolatile, checkVolatile, exit;
  if (__ membar_has_effect(read_bits) || __ membar_has_effect(write_bits)) {
    __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset +
                             ConstantPoolCacheEntry::flags_offset())), Rflags);
    __ set((1 << ConstantPoolCacheEntry::volatileField), Lscratch);
    __ and3(Rflags, Lscratch, Lscratch);
    if (__ membar_has_effect(read_bits)) {
      __ tst(Lscratch);
      __ br(Assembler::zero, false, Assembler::pt, notVolatile); 
      __ delayed()->nop();
      volatile_barrier(read_bits);
      __ bind(notVolatile);
    }
  }

  __ ld_ptr(Address(Rcache, 0, in_bytes(cp_base_offset +
                             ConstantPoolCacheEntry::f2_offset())), Roffset);
  __ pop_ptr(Rclass);
  __ verify_oop(Rclass);
  __ null_check(Rclass);

  switch (bytecode()) {
    case Bytecodes::_fast_bputfield: __ stb(Otos_i, Rclass, Roffset); break;
    case Bytecodes::_fast_cputfield: /* fall through */
    case Bytecodes::_fast_sputfield: __ sth(Otos_i, Rclass, Roffset); break;
    case Bytecodes::_fast_iputfield: __ st(Otos_i, Rclass, Roffset);  break;
    case Bytecodes::_fast_lputfield: __ st_long(Otos_l, Rclass, Roffset); break;
    case Bytecodes::_fast_fputfield:
      __ stf(FloatRegisterImpl::S, Ftos_f, Rclass, Roffset); 
      break;  
    case Bytecodes::_fast_dputfield: 
      __ stf(FloatRegisterImpl::D, Ftos_d, Rclass, Roffset); 
      break;
    case Bytecodes::_fast_aputfield:
      __ st_ptr(Otos_i, Rclass, Roffset);          
      __ store_check(G1_scratch, Rclass, Roffset);
      break;
    default:
      ShouldNotReachHere();
  }

  if (__ membar_has_effect(write_bits)) {
    __ tst(Lscratch);
    __ br(Assembler::zero, false, Assembler::pt, exit); 
    __ delayed()->nop();
    volatile_barrier(Assembler::StoreLoad);
    __ bind(exit);
  }
}


void TemplateTable::putfield(int byte_no) {
  putfield_or_static(byte_no, false);
}

void TemplateTable::putstatic(int byte_no) {
  putfield_or_static(byte_no, true);
}


void TemplateTable::fast_xaccess(TosState state) {
  transition(vtos, state);
  Register Rcache = G3_scratch;
  Register Roffset = G4_scratch;
  Register Rflags  = G4_scratch;
  Register Rreceiver = Lscratch;

  __ ld_ptr(Llocals, 0, Rreceiver);

  // access constant pool cache  (is resolved)
  __ get_cache_and_index_at_bcp(Rcache, G4_scratch, 2);
  __ ld_ptr(Address(Rcache, 0, in_bytes(constantPoolCacheOopDesc::base_offset() + ConstantPoolCacheEntry::f2_offset())), Roffset);
  __ add(Lbcp, 1, Lbcp);       // needed to report exception at the correct bcp

  // offset = 0 => assumes MacroAssembler::needs_explicit_null_check(offset) = false
  __ verify_oop(Rreceiver);
  __ null_check(Rreceiver, 0);
  if (state == atos) { 
    __ ld_ptr(Rreceiver, Roffset, Otos_i); 
  } else if (state == itos) {
    __ ld (Rreceiver, Roffset, Otos_i) ; 
  } else if (state == ftos) {
    __ ldf(FloatRegisterImpl::S, Rreceiver, Roffset, Ftos_f); 
  } else {
    ShouldNotReachHere();
  }

  Assembler::Membar_mask_bits membar_bits = 
    Assembler::Membar_mask_bits(Assembler::LoadLoad | Assembler::LoadStore);
  if (__ membar_has_effect(membar_bits)) {

    // Get is_volatile value in Rflags and check if membar is needed
    __ ld_ptr(Address(Rcache, 0, in_bytes(constantPoolCacheOopDesc::base_offset() + ConstantPoolCacheEntry::flags_offset())), Rflags);

    // Test volatile
    Label notVolatile;
    __ set((1 << ConstantPoolCacheEntry::volatileField), Lscratch);
    __ btst(Rflags, Lscratch);
    __ br(Assembler::zero, false, Assembler::pt, notVolatile); 
    __ delayed()->nop();
    volatile_barrier(membar_bits);
    __ bind(notVolatile);
  }

  __ verify_oop(Otos_i, state);
  __ sub(Lbcp, 1, Lbcp);
}

//----------------------------------------------------------------------------------------------------
// Calls

void TemplateTable::count_calls(Register method, Register temp) {
  // implemented elsewhere
  ShouldNotReachHere();
}

void TemplateTable::generate_vtable_call(Register Rrecv, Register Rindex, Register Rret) {
  Register Rtemp = G4_scratch;
  Register Rcall = Rindex;
  assert_different_registers(Rcall, G5_method, Gargs, Rret);

  // get target methodOop & entry point
  const int base = instanceKlass::vtable_start_offset() * wordSize;
  if (vtableEntry::size() % 3 == 0) {
    // scale the vtable index by 12:
    int one_third = vtableEntry::size() / 3;
    __ sll(Rindex, exact_log2(one_third * 1 * wordSize), Rtemp);
    __ sll(Rindex, exact_log2(one_third * 2 * wordSize), Rindex);
    __ add(Rindex, Rtemp, Rindex);
  } else {
    // scale the vtable index by 8:
    __ sll(Rindex, exact_log2(vtableEntry::size() * wordSize), Rindex);
  }

  __ add(Rrecv, Rindex, Rrecv);
  __ ld_ptr(Rrecv, base + vtableEntry::method_offset_in_bytes(), G5_method);
  __ ld_ptr(G5_method, in_bytes(methodOopDesc::interpreter_entry_offset()), Rcall);
  // the i2c_adapters need methodOop in G5_method (right? %%%)
  // do the call
#ifdef ASSERT
  __ tst(Rcall);
  __ breakpoint_trap(Assembler::zero);
#endif // ASSERT
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmp(Rcall, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);
}

void TemplateTable::invokevirtual(int byte_no) {
  transition(vtos, vtos);

  Register Rscratch = G3_scratch;
  Register Rtemp = G4_scratch;
  Register Rret = Lscratch;
  Register Rrecv = G5_method;
  Label notFinal;

  load_invoke_cp_cache_entry(byte_no, G5_method, noreg, Rret, true);

  // Check for vfinal 
  __ set((1 << ConstantPoolCacheEntry::vfinalMethod), G4_scratch);
  __ btst(Rret, G4_scratch);
  __ br(Assembler::zero, false, Assembler::pt, notFinal); 
  __ delayed()->and3(Rret, 0xFF, G4_scratch);      // gets number of parameters

  patch_bytecode(Bytecodes::_fast_invokevfinal, Rscratch, Rtemp);

  invokevfinal_helper(Rscratch, Rret);

  __ bind(notFinal);

  __ mov(G5_method, Rscratch);  // better scratch register
  __ sll(G4_scratch, LogBytesPerWord, G4_scratch);
  __ ld_ptr(Lesp, G4_scratch, O0);          // gets receiver Oop

  // receiver is in O0
  __ verify_oop(O0);

  // get return address
  const intptr_t table = (intptr_t)Interpreter::return_3_addrs_by_index_table();
  __ load_address(Address(Rtemp, (address)table));
  __ srl(Rret, ConstantPoolCacheEntry::tosBits, Rret);		// get return type
  // Make sure we don't need to mask Rret for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  __ sll(Rret,  LogBytesPerWord, Rret);
  __ ld_ptr(Rtemp, Rret, Rret);		// get return address

  // get receiver klass
  __ null_check(O0, oopDesc::klass_offset_in_bytes());
  __ ld_ptr(Address(O0, 0, oopDesc::klass_offset_in_bytes()), Rrecv);
  __ verify_oop(Rrecv);

  __ profile_virtual_call(Rrecv, O4);

  generate_vtable_call(Rrecv, Rscratch, Rret);
}

void TemplateTable::fast_invokevfinal(int byte_no) {
  transition(vtos, vtos);

  load_invoke_cp_cache_entry(byte_no, G5_method, noreg, Lscratch, true,
                             /*is_invokevfinal*/true);
  invokevfinal_helper(G3_scratch, Lscratch);
}

void TemplateTable::invokevfinal_helper(Register Rscratch, Register Rret) {
  Register Rtemp = G4_scratch;

  __ verify_oop(G5_method);

  __ lduh(Address(G5_method, 0, in_bytes(methodOopDesc::size_of_parameters_offset())), G4_scratch);

  __ sll(G4_scratch, LogBytesPerWord, G4_scratch);
  __ ld_ptr(Lesp, G4_scratch, O0);           // gets receiver Oop
  
  // receiver NULL check 
  __ null_check(O0);

  __ profile_final_call(O4);

  // get return address
  const intptr_t table = (intptr_t)Interpreter::return_3_addrs_by_index_table();
  __ load_address(Address(Rtemp, (address)table));
  __ srl(Rret, ConstantPoolCacheEntry::tosBits, Rret);		// get return type
  // Make sure we don't need to mask Rret for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  __ sll(Rret,  LogBytesPerWord, Rret);
  __ ld_ptr(Rtemp, Rret, Rret);		// get return address


  // do the call
  __ ld_ptr(Address(G5_method, 0, in_bytes(methodOopDesc::interpreter_entry_offset())), Rscratch);
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmp(Rscratch, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);
}

void TemplateTable::invokespecial(int byte_no) {
  transition(vtos, vtos);

  Register Rscratch = G3_scratch;
  Register Rtemp = G4_scratch;
  Register Rret = Lscratch;

  load_invoke_cp_cache_entry(byte_no, G5_method, noreg, Rret, false);

  __ verify_oop(G5_method);

  // get parameters
  //%%% use the cache entry (already in a register) to get the param size
  __ lduh(Address(G5_method, 0, in_bytes(methodOopDesc::size_of_parameters_offset())), G4_scratch);

  __ sll(G4_scratch, LogBytesPerWord, G4_scratch);
  __ ld_ptr(Lesp, G4_scratch, O0);          // gets receiver Oop

  // receiver NULL check
  __ null_check(O0);

  __ profile_call(O4);

  // get return address
  const intptr_t table = (intptr_t)Interpreter::return_3_addrs_by_index_table();
  __ load_address(Address(Rtemp, (address)table));
  __ srl(Rret, ConstantPoolCacheEntry::tosBits, Rret);		// get return type
  // Make sure we don't need to mask Rret for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  __ sll(Rret,  LogBytesPerWord, Rret);
  __ ld_ptr(Rtemp, Rret, Rret);		// get return address

  // do the call
  __ ld_ptr(Address(G5_method, 0, in_bytes(methodOopDesc::interpreter_entry_offset())), Rscratch);
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmp(Rscratch, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);
}

void TemplateTable::invokestatic(int byte_no) {
  transition(vtos, vtos);

  Register Rscratch = G3_scratch;
  Register Rtemp = G4_scratch;
  Register Rret = Lscratch;

  load_invoke_cp_cache_entry(byte_no, G5_method, noreg, Rret, false);

  __ verify_oop(G5_method);

  __ profile_call(O4);

  // get return address
  const intptr_t table = (intptr_t)Interpreter::return_3_addrs_by_index_table();
  __ load_address(Address(Rtemp, (address)table));
  __ srl(Rret, ConstantPoolCacheEntry::tosBits, Rret);		// get return type
  // Make sure we don't need to mask Rret for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  __ sll(Rret,  LogBytesPerWord, Rret);
  __ ld_ptr(Rtemp, Rret, Rret);		// get return address

  // do the call
  __ ld_ptr(Address(G5_method, 0, in_bytes(methodOopDesc::interpreter_entry_offset())), Rscratch);
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmpl(Address(Rscratch, 0, 0), G0, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);
}


void TemplateTable::invokeinterface_object_method(Register RklassOop,
                                                  Register Rcall,
                                                  Register Rret,
                                                  Register Rflags) {
  Register Rscratch = G4_scratch;
  Register Rindex = Lscratch;

  assert_different_registers(Rscratch, Rindex, Rret);

  Label notFinal;

  // Check for vfinal 
  __ set((1 << ConstantPoolCacheEntry::vfinalMethod), Rscratch);
  __ btst(Rflags, Rscratch);
  __ br(Assembler::zero, false, Assembler::pt, notFinal); 
  __ delayed()->nop();

  __ profile_final_call(O4);

  // do the call - the index (f2) contains the methodOop
  assert_different_registers(G5_method, Gargs, Rcall);
  __ mov(Rindex, G5_method);
  __ ld_ptr(Address(G5_method, 0, in_bytes(methodOopDesc::interpreter_entry_offset())), Rcall);
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmp(Rcall, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);

  __ bind(notFinal);

  generate_vtable_call(RklassOop, Rindex, Rret);
}


void TemplateTable::invokeinterface(int byte_no) {
  transition(vtos, vtos);

  Register Rscratch = G4_scratch;
  Register Rret = G3_scratch;
  Register Rindex = Lscratch;
  Register Rinterface = G1_scratch;
  Register RklassOop = G5_method;
  Register Rflags = O1;
  assert_different_registers(Rscratch, G5_method);

  load_invoke_cp_cache_entry(byte_no, Rinterface, Rindex, Rflags, false);

  // get receiver
  __ and3(Rflags, 0xFF, Rscratch);       // gets number of parameters
  __ sll(Rscratch, LogBytesPerWord, Rscratch);
  __ ld_ptr(Lesp, Rscratch, O0);           // gets receiver Oop
  __ verify_oop(O0);

  __ mov(Rflags, Rret);

  // get return address
  const intptr_t table = (intptr_t)Interpreter::return_5_addrs_by_index_table();
  __ load_address(Address(Rscratch, (address)table));
  __ srl(Rret, ConstantPoolCacheEntry::tosBits, Rret);		// get return type
  // Make sure we don't need to mask Rret for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  __ sll(Rret,  LogBytesPerWord, Rret);
  __ ld_ptr(Rscratch, Rret, Rret);	// get return address

  // get receiver klass
  __ null_check(O0, oopDesc::klass_offset_in_bytes());
  __ ld_ptr(O0, oopDesc::klass_offset_in_bytes(), RklassOop);
  __ verify_oop(RklassOop);

  // Special case of invokeinterface called for virtual method of
  // java.lang.Object.  See cpCacheOop.cpp for details.
  // This code isn't produced by javac, but could be produced by
  // another compliant java compiler.
  Label notMethod;
  __ set((1 << ConstantPoolCacheEntry::methodInterface), Rscratch);
  __ btst(Rflags, Rscratch);
  __ br(Assembler::zero, false, Assembler::pt, notMethod); 
  __ delayed()->nop();

  invokeinterface_object_method(RklassOop, Rinterface, Rret, Rflags);

  __ bind(notMethod);

  __ profile_virtual_call(RklassOop, O4);

  //
  // find entry point to call
  //

  // compute start of first itableOffsetEntry (which is at end of vtable)
  const int base = instanceKlass::vtable_start_offset() * wordSize;
  Label search;
  Register Rtemp = Rflags;

  __ ld(Address(RklassOop, 0, instanceKlass::vtable_length_offset() * wordSize), Rtemp);
  if (align_object_offset(1) > 1) {
    __ round_to(Rtemp, align_object_offset(1));
  }
  __ sll(Rtemp, LogBytesPerWord, Rtemp);   // Rscratch *= 4;
  if (Assembler::is_simm13(base)) {
    __ add(Rtemp, base, Rtemp);
  } else {
    __ set(base, Rscratch);
    __ add(Rscratch, Rtemp, Rtemp);
  }
  __ add(RklassOop, Rtemp, Rscratch);

  __ bind(search);

  __ ld_ptr(Rscratch, itableOffsetEntry::interface_offset_in_bytes(), Rtemp); 
  {
    Label ok;

    // Check that entry is non-null.  Null entries are probably a bytecode
    // problem.  If the interface isn't implemented by the reciever class,
    // the VM should throw IncompatibleClassChangeError.  linkResolver checks
    // this too but that's only if the entry isn't already resolved, so we
    // need to check again.
    __ br_notnull( Rtemp, false, Assembler::pt, ok);
    __ delayed()->nop();
    call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_IncompatibleClassChangeError));
    __ should_not_reach_here();
    __ bind(ok);
    __ verify_oop(Rtemp);
  }

  __ verify_oop(Rinterface);

  __ cmp(Rinterface, Rtemp);
  __ brx(Assembler::notEqual, true, Assembler::pn, search);
  __ delayed()->add(Rscratch, itableOffsetEntry::size() * wordSize, Rscratch);

  // entry found and Rscratch points to it
  __ ld(Rscratch, itableOffsetEntry::offset_offset_in_bytes(), Rscratch);

  assert(itableMethodEntry::method_offset_in_bytes() == 0, "adjust instruction below");
  __ sll(Rindex, exact_log2(itableMethodEntry::size() * wordSize), Rindex);       // Rindex *= 8;
  __ add(Rscratch, Rindex, Rscratch);
  __ ld_ptr(RklassOop, Rscratch, G5_method);

  // Check for abstract method error.
  {
    Label ok;
    __ tst(G5_method);
    __ brx(Assembler::notZero, false, Assembler::pt, ok);
    __ delayed()->nop();
    call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_AbstractMethodError));
    __ should_not_reach_here();
    __ bind(ok);
  }

  Register Rcall = Rinterface;
  assert_different_registers(Rcall, G5_method, Gargs, Rret);

  __ verify_oop(G5_method);
  __ ld_ptr(G5_method, in_bytes(methodOopDesc::interpreter_entry_offset()), Rcall);

  // do the call (Rcall has the entry point now)
#ifdef ASSERT
  {
    Label ok;
    __ br_notnull(Rcall, false, Assembler::pt, ok);
    __ delayed()->nop();
    __ stop("null entry point found in itable's vtable");
    __ bind(ok);
  }
#endif // ASSERT
  __ add(Lesp, BytesPerWord, Gargs); // setup parameter pointer
  __ jmp(Rcall, 0);
  __ delayed()->add(Rret, -frame::pc_return_offset, O7);
}


//----------------------------------------------------------------------------------------------------
// Allocation

void TemplateTable::_new() {
  transition(vtos, atos);

  Label slow_case;
  Label done;
  Label initialize_header;
  Label initialize_object;  // including clearing the fields

  Register RallocatedObject = Otos_i;
  Register RinstanceKlass = O1;
  Register RinstanceSize = O2;
  Register Roffset = O3;
  Register Rscratch = O4;

  __ get_2_byte_integer_at_bcp(1, Rscratch, Roffset, InterpreterMacroAssembler::Unsigned);
  __ get_cpool_and_tags(Rscratch, G3_scratch);
  // make sure the class we're about to instantiate has been resolved
  __ add(G3_scratch, typeArrayOopDesc::header_size(T_BYTE) * wordSize, G3_scratch);
  __ ldub(G3_scratch, Roffset, G3_scratch);
  __ cmp(G3_scratch, JVM_CONSTANT_UnresolvedClass);
  __ br(Assembler::equal, false, Assembler::pn, slow_case);
  __ delayed()->sll(Roffset, LogBytesPerWord, Roffset);

  //__ sll(Roffset, LogBytesPerWord, Roffset);        // executed in delay slot
  __ add(Roffset, sizeof(constantPoolOopDesc), Roffset);
  __ ld_ptr(Rscratch, Roffset, RinstanceKlass);

  // make sure klass is fully initialized:
  __ ld(RinstanceKlass, instanceKlass::init_state_offset_in_bytes() + sizeof(oopDesc), G3_scratch);
  __ cmp(G3_scratch, instanceKlass::fully_initialized);
  __ br(Assembler::notEqual, false, Assembler::pn, slow_case);
  __ delayed()->ld(RinstanceKlass, Klass::access_flags_offset_in_bytes() + sizeof(oopDesc), G3_scratch);

  // make sure klass does not have has_finalizer, or is abstract, or interface or java/lang/Class
  //__ ld(RinstanceKlass, Klass::access_flags_offset_in_bytes() + sizeof(oopDesc), G3_scratch);
  __ set(JVM_ACC_CAN_BE_FASTPATH_ALLOCATED, G1_scratch);
  __ btst(G3_scratch, G1_scratch);
  __ brx(Assembler::zero, false, Assembler::pn, slow_case);
  __ delayed()->ld(RinstanceKlass, Klass::size_helper_offset_in_bytes() + sizeof(oopDesc), RinstanceSize);
  
  // get instance_size in instanceKlass (already aligned)
  //__ ld(RinstanceKlass, Klass::size_helper_offset_in_bytes() + sizeof(oopDesc), RinstanceSize);
  __ sll(RinstanceSize, LogBytesPerWord, Roffset);
 
  // allocate the instance
  // 1) Try to allocate in the TLAB
  // 2) if fail, and the TLAB is not full enough to discard, allocate in the shared Eden
  // 3) if the above fails (or is not applicable), go to a slow case (creates a new TLAB, etc.)

  if(UseTLAB) {
    Register RoldTopValue = RallocatedObject;
    Register RtopAddr = G3_scratch, RtlabWasteLimitValue = G3_scratch;
    Register RnewTopValue = G1_scratch;
    Register RendValue = Rscratch;
    Register RfreeValue = RnewTopValue; 
 
    // check if we can allocate in the TLAB
    __ ld_ptr(G2_thread, in_bytes(JavaThread::tlab_top_offset()), RoldTopValue); // sets up RalocatedObject
    __ ld_ptr(G2_thread, in_bytes(JavaThread::tlab_end_offset()), RendValue);
    __ add(RoldTopValue, Roffset, RnewTopValue);   

    // if there is enough space, we do not CAS and do not clear
    __ cmp(RnewTopValue, RendValue);
    if(ZeroTLAB) {
      // the fields have been lready cleared
      __ brx(Assembler::lessEqualUnsigned, true, Assembler::pt, initialize_header);
    } else {
      // initialize both the header and fields
      __ brx(Assembler::lessEqualUnsigned, true, Assembler::pt, initialize_object);
    }
    __ delayed()->st_ptr(RnewTopValue, G2_thread, in_bytes(JavaThread::tlab_top_offset()));

    if (CMSIncrementalMode) {
      // No allocation in the shared eden.
      __ br(Assembler::always, false, Assembler::pt, slow_case);
      __ delayed()->nop();
    } else {
    // Check if tlab should be discarded (refill_waste_limit >= free)
    __ ld_ptr(G2_thread, in_bytes(JavaThread::tlab_refill_waste_limit_offset()), RtlabWasteLimitValue);
    __ sub(RendValue, RoldTopValue, RfreeValue);   
#ifdef _LP64
    __ srlx(RfreeValue, LogHeapWordSize, RfreeValue);
#else
    __ srl(RfreeValue, LogHeapWordSize, RfreeValue);
#endif
    __ cmp(RtlabWasteLimitValue, RfreeValue);
    __ brx(Assembler::greaterEqualUnsigned, false, Assembler::pt, slow_case); // tlab waste is small
    __ delayed()->nop();

    // increment waste limit to prevent getting stuck on this slow path
    __ add(RtlabWasteLimitValue, ThreadLocalAllocBuffer::refill_waste_limit_increment(), RtlabWasteLimitValue);
    __ st_ptr(RtlabWasteLimitValue, G2_thread, in_bytes(JavaThread::tlab_refill_waste_limit_offset()));
    }  
  }

  // Allocation in the shared Eden
  {
    Register RoldTopValue = G1_scratch;
    Register RtopAddr = G3_scratch;
    Register RnewTopValue = RallocatedObject;
    Register RendValue = Rscratch;

    __ set((intptr_t)Universe::heap()->top_addr(), RtopAddr);

    Label retry;
    __ bind(retry);
    __ set((intptr_t)Universe::heap()->end_addr(), RendValue);
    __ ld_ptr(RendValue, 0, RendValue);
    __ ld_ptr(RtopAddr, 0, RoldTopValue);
    __ add(RoldTopValue, Roffset, RnewTopValue);
 
    // RnewTopValue contains the top address after the new object
    // has been allocated.
    __ cmp(RnewTopValue, RendValue);
    __ brx(Assembler::greaterUnsigned, false, Assembler::pn, slow_case);
    __ delayed()->nop();

    __ casx_under_lock(RtopAddr, RoldTopValue, RnewTopValue,
      VM_Version::v9_instructions_work() ? NULL :
      (address)StubRoutines::sparc::atomic_memory_operation_lock_addr());

    // if someone beat us on the allocation, try again, otherwise continue
    __ cmp(RoldTopValue, RnewTopValue);
    __ brx(Assembler::notEqual, false, Assembler::pn, retry);
    __ delayed()->nop();
  }

  // clear object fields
  __ bind(initialize_object);
  __ deccc(RinstanceSize, sizeof(oopDesc) / oopSize);
  __ br(Assembler::zero, false, Assembler::pt, initialize_header);
  __ delayed()->nop();

  __ add(RallocatedObject, sizeof(oopDesc), G3_scratch);
  __ sll(RinstanceSize, LogBytesPerWord, RinstanceSize);
 
  // initialize remaining object fields
  { Label loop;
    __ sub(RinstanceSize, wordSize, Roffset);
    __ bind(loop);
    //__ sub(RinstanceSize, wordSize, Roffset);      // executed above loop or in delay slot
    __ st_ptr(G0, G3_scratch, Roffset);
    __ subcc(RinstanceSize, wordSize, RinstanceSize);
    __ br(Assembler::equal, false, Assembler::pt, initialize_header);
    __ delayed()->nop();
    __ ba(false, loop);
    __ delayed()->sub(RinstanceSize, wordSize, Roffset);
  }

  // slow case
  __ bind(slow_case);
  __ get_2_byte_integer_at_bcp(1, G3_scratch, O2, InterpreterMacroAssembler::Unsigned);
  __ get_constant_pool(O1);

  call_VM(Otos_i, CAST_FROM_FN_PTR(address, InterpreterRuntime::_new), O1, O2);

  __ ba(false, done);
  __ delayed()->nop();

  // Initialize the header: mark, klass
  __ bind(initialize_header);

  __ set((intptr_t)markOopDesc::prototype(), G4_scratch);
  __ st_ptr(G4_scratch, RallocatedObject, oopDesc::mark_offset_in_bytes());       // mark
  __ st_ptr(RinstanceKlass, RallocatedObject, oopDesc::klass_offset_in_bytes()); // klass  


  // continue
  __ bind(done);
}



void TemplateTable::newarray() {
  transition(itos, atos);
  __ ldub(Lbcp, 1, O1);
     call_VM(Otos_i, CAST_FROM_FN_PTR(address, InterpreterRuntime::newarray), O1, Otos_i);
}


void TemplateTable::anewarray() {
  transition(itos, atos);
  __ get_constant_pool(O1);
  __ get_2_byte_integer_at_bcp(1, G4_scratch, O2, InterpreterMacroAssembler::Unsigned);
     call_VM(Otos_i, CAST_FROM_FN_PTR(address, InterpreterRuntime::anewarray), O1, O2, Otos_i);
}


void TemplateTable::arraylength() {
  transition(atos, itos);
  Label ok;
  __ verify_oop(Otos_i);
  __ tst(Otos_i);
  __ throw_if_not_1_x( Assembler::notZero, ok );
  __ delayed()->ld(Otos_i, arrayOopDesc::length_offset_in_bytes(), Otos_i);
  __ throw_if_not_2( Interpreter::_throw_NullPointerException_entry, G3_scratch, ok);
}


void TemplateTable::checkcast() {
  transition(atos, atos);
  Label done, is_null, quicked, cast_ok, resolved;
  Register Roffset = G1_scratch;
  Register RobjKlass = O5;
  Register RspecifiedKlass = O4;

  // Check for casting a NULL
  __ br_null(Otos_i, false, Assembler::pn, is_null);
  __ delayed()->nop();

  // Profile the not-null value
  __ profile_checkcast(false, G3_scratch, G4_scratch);

  // Get value klass in RobjKlass
  __ ld_ptr(Otos_i, oopDesc::klass_offset_in_bytes(), RobjKlass); // get value klass
  // Get constant pool tag
  __ get_2_byte_integer_at_bcp(1, Lscratch, Roffset, InterpreterMacroAssembler::Unsigned);

  // See if the checkcast has been quickened
  __ get_cpool_and_tags(Lscratch, G3_scratch);
  __ add(G3_scratch, typeArrayOopDesc::header_size(T_BYTE) * wordSize, G3_scratch);
  __ ldub(G3_scratch, Roffset, G3_scratch);
  __ cmp(G3_scratch, JVM_CONSTANT_Class);
  __ br(Assembler::equal, true, Assembler::pt, quicked);
  __ delayed()->sll(Roffset, LogBytesPerWord, Roffset);

  __ push(atos); // save receiver for result, and for GC
  call_VM(RspecifiedKlass, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc) );
  __ pop(atos); // restore receiver

  __ br(Assembler::always, false, Assembler::pt, resolved);
  __ delayed()->ld_ptr(Otos_i, oopDesc::klass_offset_in_bytes(), RobjKlass); // get value klass

  // Extract target class from constant pool
  __ bind(quicked);
  __ add(Roffset, sizeof(constantPoolOopDesc), Roffset);
  __ ld_ptr(Lscratch, Roffset, RspecifiedKlass);
  __ bind(resolved);

  // Generate a fast subtype check.  Branch to cast_ok if no
  // failure.  Throw exception if failure.
  __ gen_subtype_check( RobjKlass, RspecifiedKlass, G3_scratch, G4_scratch, G1_scratch, cast_ok );

  // Not a subtype; so must throw exception
  __ throw_if_not_x( Assembler::never, Interpreter::_throw_ClassCastException_entry, G3_scratch );

  __ bind(cast_ok);

  if (ProfileInterpreter || UncommonNullCast) {
    __ ba(false, done);
    __ delayed()->nop();
  }
  __ bind(is_null);
  __ profile_checkcast(true, G3_scratch, G4_scratch);
  __ bind(done);
}


void TemplateTable::instanceof() {
  Label done, quicked, resolved;
  transition(atos, itos);
  Register Roffset = G1_scratch;
  Register RobjKlass = O5;
  Register RspecifiedKlass = O4;

  // Check for casting a NULL
  __ br_null(Otos_i, false, Assembler::pt, done);
  __ delayed()->nop();

  // Get value klass in RobjKlass
  __ ld_ptr(Otos_i, oopDesc::klass_offset_in_bytes(), RobjKlass); // get value klass
  // Get constant pool tag
  __ get_2_byte_integer_at_bcp(1, Lscratch, Roffset, InterpreterMacroAssembler::Unsigned);

  // See if the checkcast has been quickened
  __ get_cpool_and_tags(Lscratch, G3_scratch);
  __ add(G3_scratch, typeArrayOopDesc::header_size(T_BYTE) * wordSize, G3_scratch);
  __ ldub(G3_scratch, Roffset, G3_scratch);
  __ cmp(G3_scratch, JVM_CONSTANT_Class);
  __ br(Assembler::equal, true, Assembler::pt, quicked);
  __ delayed()->sll(Roffset, LogBytesPerWord, Roffset);

  __ push(atos); // save receiver for result, and for GC
  call_VM(RspecifiedKlass, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc) );
  __ pop(atos); // restore receiver

  __ br(Assembler::always, false, Assembler::pt, resolved);
  __ delayed()->ld_ptr(Otos_i, oopDesc::klass_offset_in_bytes(), RobjKlass); // get value klass


  // Extract target class from constant pool
  __ bind(quicked);
  __ add(Roffset, sizeof(constantPoolOopDesc), Roffset);
  __ get_constant_pool(Lscratch);
  __ ld_ptr(Lscratch, Roffset, RspecifiedKlass);
  __ bind(resolved);

  // Generate a fast subtype check.  Branch to cast_ok if no
  // failure.  Return 0 if failure.
  __ or3(G0, 1, Otos_i);      // set result assuming quick tests succeed
  __ gen_subtype_check( RobjKlass, RspecifiedKlass, G3_scratch, G4_scratch, G1_scratch, done );
  // Not a subtype; return 0;
  __ clr( Otos_i );
  __ bind(done);
}

void TemplateTable::_breakpoint() {

   // Note: We get here even if we are single stepping..
   // jbug inists on setting breakpoints at every bytecode 
   // even if we are in single step mode.  
 
   transition(vtos, vtos);
   // get the unpatched byte code
   __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::get_original_bytecode_at), Lmethod, Lbcp);
   __ mov(O0, Lbyte_code);

   // post the breakpoint event
   __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::_breakpoint), Lmethod, Lbcp);

   // complete the execution of original bytecode
   __ dispatch_normal(vtos);
}


//----------------------------------------------------------------------------------------------------
// Exceptions

void TemplateTable::athrow() {
  transition(atos, vtos);

  // This works because exception is cached in Otos_i which is same as O0,
  // which is same as what throw_exception_entry_expects 
  assert(Otos_i == Oexception, "see explanation above");

  __ verify_oop(Otos_i);
  __ null_check(Otos_i);
  __ throw_if_not_x(Assembler::never, Interpreter::throw_exception_entry(), G3_scratch);
}


//----------------------------------------------------------------------------------------------------
// Synchronization


// See frame_sparc.hpp for monitor block layout.
// Monitor elements are dynamically allocated by growing stack as needed.

void TemplateTable::monitorenter() {
  transition(atos, vtos);
  __ verify_oop(Otos_i);
  // Try to acquire a lock on the object
  // Repeat until succeeded (i.e., until
  // monitorenter returns true).

  {   Label ok;
    __ tst(Otos_i);
    __ throw_if_not_1_x( Assembler::notZero,  ok);
    __ delayed()->mov(Otos_i, Lscratch); // save obj
    __ throw_if_not_2( Interpreter::_throw_NullPointerException_entry, G3_scratch, ok);
  }

  assert(O0 == Otos_i, "Be sure where the object to lock is");

  // find a free slot in the monitor block


  // initialize entry pointer
  __ clr(O1); // points to free slot or NULL

  {
    Label entry, loop, exit;
    __ add( __ top_most_monitor(), O2 ); // last one to check
    __ ba( false, entry );
    __ delayed()->mov( Lmonitors, O3 ); // first one to check


    __ bind( loop );

    __ verify_oop(O4);		// verify each monitor's oop
    __ tst(O4); // is this entry unused?
    if (VM_Version::v9_instructions_work())
#ifdef _LP64
      __ movcc( Assembler::zero, false, Assembler::xcc, O3, O1);
#else
      __ movcc( Assembler::zero, false, Assembler::icc, O3, O1);
#endif
    else {
      Label L;
      __ br( Assembler::zero, true, Assembler::pn, L );
      __ delayed()->mov(O3, O1); // rememeber this one if match
      __ bind(L);        
    }

    __ cmp(O4, O0); // check if current entry is for same object
    __ brx( Assembler::equal, false, Assembler::pn, exit );
    __ delayed()->inc( O3, frame::interpreter_frame_monitor_size() * wordSize ); // check next one

    __ bind( entry );

    __ cmp( O3, O2 );
    __ brx( Assembler::lessEqualUnsigned, true, Assembler::pt, loop );
    __ delayed()->ld_ptr(O3, BasicObjectLock::obj_offset_in_bytes(), O4);

    __ bind( exit );
  }

  { Label allocated;

    // found free slot?
    __ br_notnull(O1, false, Assembler::pn, allocated);
    __ delayed()->nop();

    __ add_monitor_to_stack( false, O2, O3 );
    __ mov(Lmonitors, O1);

    __ bind(allocated);
  }

  // Increment bcp to point to the next bytecode, so exception handling for async. exceptions work correctly. 
  // The object has already been poped from the stack, so the expression stack looks correct.
  __ inc(Lbcp);
  
  __ st_ptr(O0, O1, BasicObjectLock::obj_offset_in_bytes()); // store object
  __ lock_object(O1, O0);

  // The bcp has already been incremented. Just need to dispatch to next instruction.
  __ dispatch_next(vtos); 
}


void TemplateTable::monitorexit() {
  transition(atos, vtos);
  __ verify_oop(Otos_i);
  __ tst(Otos_i);
  __ throw_if_not_x( Assembler::notZero, Interpreter::_throw_NullPointerException_entry, G3_scratch );

  assert(O0 == Otos_i, "just checking");

  { Label entry, loop, found;
    __ add( __ top_most_monitor(), O2 ); // last one to check
    __ ba(false, entry );
    // use Lscratch to hold monitor elem to check, start with most recent monitor,
    // By using a local it survives the call to the C routine.
    __ delayed()->mov( Lmonitors, Lscratch ); 

    __ bind( loop );

    __ verify_oop(O4);		// verify each monitor's oop
    __ cmp(O4, O0); // check if current entry is for desired object
    __ brx( Assembler::equal, true, Assembler::pt, found );
    __ delayed()->mov(Lscratch, O1); // pass found entry as argument to monitorexit

    __ inc( Lscratch, frame::interpreter_frame_monitor_size() * wordSize ); // advance to next

    __ bind( entry );

    __ cmp( Lscratch, O2 );
    __ brx( Assembler::lessEqualUnsigned, true, Assembler::pt, loop );
    __ delayed()->ld_ptr(Lscratch, BasicObjectLock::obj_offset_in_bytes(), O4);

    call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_illegal_monitor_state_exception));
    __ should_not_reach_here();

    __ bind(found);
  }
  __ unlock_object(O1);
}


//----------------------------------------------------------------------------------------------------
// Wide instructions

void TemplateTable::wide() {
  transition(vtos, vtos);
  __ ldub(Lbcp, 1, G3_scratch);// get next bc
  __ sll(G3_scratch, LogBytesPerWord, G3_scratch); 
  Address ep(G4_scratch, (address)Interpreter::_wentry_point);
  __ load_address(ep);
  __ ld_ptr(ep.base(), G3_scratch, G3_scratch);
  __ jmp(G3_scratch, G0);
  __ delayed()->nop();
  // Note: the Lbcp increment step is part of the individual wide bytecode implementations
}


//----------------------------------------------------------------------------------------------------
// Multi arrays

void TemplateTable::multianewarray() {
  transition(vtos, atos);
     // put ndims * wordSize into Lscratch
  __ ldub( Lbcp,     3,               Lscratch);
  __ sll(  Lscratch, LogBytesPerWord, Lscratch);
     // Lesp points past last_dim, so set to O1 to first_dim address
  __ add(  Lesp,     Lscratch,        O1); 
     call_VM(Otos_i, CAST_FROM_FN_PTR(address, InterpreterRuntime::multianewarray), O1);
  __ add(  Lesp,     Lscratch,        Lesp); // pop all dimensions off the stack
}


//Reconciliation History
// 1.81 97/11/12 09:26:42 templateTable_i486.cpp
// 1.88 97/12/04 10:19:22 templateTable_i486.cpp
// 1.89 97/12/08 13:19:30 templateTable_i486.cpp
// 1.90 97/12/12 15:43:37 templateTable_i486.cpp
// 1.91 98/01/12 11:39:11 templateTable_i486.cpp
// 1.98 98/02/03 19:06:46 templateTable_i486.cpp
// 1.105 98/02/11 12:07:56 templateTable_i486.cpp
// 1.107 98/03/03 14:38:36 templateTable_i486.cpp
// 1.115 98/03/18 13:14:33 templateTable_i486.cpp
// 1.124 98/03/30 17:20:33 templateTable_i486.cpp
// 1.142 98/04/08 13:43:13 templateTable_i486.cpp
// 1.159 98/04/16 13:20:45 templateTable_i486.cpp
// 1.167 98/05/04 11:55:17 templateTable_i486.cpp
// 1.170 98/05/19 18:08:50 templateTable_i486.cpp
// 1.172 98/05/29 17:36:52 templateTable_i486.cpp
// 1.173 98/06/22 14:49:10 templateTable_i486.cpp
// 1.175 98/07/24 13:34:39 templateTable_i486.cpp
// 1.176 98/07/31 11:41:18 templateTable_i486.cpp
// 1.177 98/08/28 11:31:14 templateTable_i486.cpp
// 1.179 98/10/02 15:22:49 templateTable_i486.cpp
// 1.180 98/10/05 02:32:12 templateTable_i486.cpp
// 1.181 98/10/05 16:30:27 templateTable_i486.cpp
// 1.183 98/10/15 14:08:32 templateTable_i486.cpp
// 1.184 98/10/26 11:37:16 templateTable_i486.cpp
// 1.185 98/11/12 11:58:41 templateTable_i486.cpp
// 1.186 98/12/17 15:37:41 templateTable_i486.cpp
// 1.187 99/01/05 17:32:03 templateTable_i486.cpp
// 1.188 99/01/06 11:43:54 templateTable_i486.cpp
// 1.189 99/01/20 15:59:45 templateTable_i486.cpp
// 1.190 99/02/19 12:51:05 templateTable_i486.cpp
// 1.192 99/04/01 13:02:53 templateTable_i486.cpp
// 1.199 99/07/13 11:16:59 templateTable_i486.cpp
// 1.201 99/07/19 10:21:37 templateTable_i486.cpp
// 1.198 99/06/02 15:27:40 templateTable_i486.cpp
// 1.200 99/06/24 14:25:02 templateTable_i486.cpp
// 1.207 99/07/23 18:43:52 templateTable_i486.cpp
// 1.207 99/08/17 02:40:41 templateTable_i486.cpp
// 1.208 99/08/17 11:36:54 templateTable_i486.cpp
// 1.208 99/08/26 16:15:39 templateTable_i486.cpp
// 1.209 99/08/27 12:55:50 templateTable_i486.cpp
// 1.208 99/10/19 15:05:40 templateTable_i486.cpp
//End

//TO DO: 1.204 99/07/20 10:58:53 templateTable_i486.cpp

