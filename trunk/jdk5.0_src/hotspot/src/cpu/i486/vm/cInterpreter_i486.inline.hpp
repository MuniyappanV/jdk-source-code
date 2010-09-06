#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)cInterpreter_i486.inline.hpp	1.3 03/12/23 16:36:13 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Inline interpreter functions for IA32

inline jfloat cInterpreter::VMfloatAdd(jfloat op1, jfloat op2) { return op1 + op2; }
inline jfloat cInterpreter::VMfloatSub(jfloat op1, jfloat op2) { return op1 - op2; }
inline jfloat cInterpreter::VMfloatMul(jfloat op1, jfloat op2) { return op1 * op2; }
inline jfloat cInterpreter::VMfloatDiv(jfloat op1, jfloat op2) { return op1 / op2; }
inline jfloat cInterpreter::VMfloatRem(jfloat op1, jfloat op2) { return fmod(op1, op2); }

inline jfloat cInterpreter::VMfloatNeg(jfloat op) { return -op; }

inline int32_t cInterpreter::VMfloatCompare(jfloat op1, jfloat op2, int32_t direction) {
  return ( op1 < op2 ? -1 : 
	       op1 > op2 ? 1 : 
		   op1 == op2 ? 0 : 
		       (direction == -1 || direction == 1) ? direction : 0);

}

inline void cInterpreter::VMmemCopy64(uint32_t to[2], const uint32_t from[2]) {
  // x86 can do unaligned copies but not 64bits at a time
  to[0] = from[0]; to[1] = from[1];
}

// The long operations depend on compiler support for "long long" on x86

inline jlong cInterpreter::VMlongAdd(jlong op1, jlong op2) {
  return op1 + op2;
}

inline jlong cInterpreter::VMlongAnd(jlong op1, jlong op2) {
  return op1 & op2;
}

inline jlong cInterpreter::VMlongDiv(jlong op1, jlong op2) {
  // QQQ what about check and throw...
  return op1 / op2;
}

inline jlong cInterpreter::VMlongMul(jlong op1, jlong op2) {
  return op1 * op2;
}

inline jlong cInterpreter::VMlongOr(jlong op1, jlong op2) {
  return op1 | op2;
}

inline jlong cInterpreter::VMlongSub(jlong op1, jlong op2) {
  return op1 - op2;
}

inline jlong cInterpreter::VMlongXor(jlong op1, jlong op2) {
  return op1 ^ op2;
}

inline jlong cInterpreter::VMlongRem(jlong op1, jlong op2) {
  return op1 % op2;
}

inline jlong cInterpreter::VMlongUshr(jlong op1, jint op2) {
  // CVM did this 0x3f mask, is the really needed??? QQQ
  return ((unsigned long long) op1) >> (op2 & 0x3F);
}

inline jlong cInterpreter::VMlongShr(jlong op1, jint op2) {
  return op1 >> (op2 & 0x3F);
}

inline jlong cInterpreter::VMlongShl(jlong op1, jint op2) {
  return op1 << (op2 & 0x3F);
}

inline jlong cInterpreter::VMlongNeg(jlong op) {
  return -op;
}

inline jlong cInterpreter::VMlongNot(jlong op) {
  return ~op;
}

inline int32_t cInterpreter::VMlongLtz(jlong op) {
  return (op <= 0);
}

inline int32_t cInterpreter::VMlongGez(jlong op) {
  return (op >= 0);
}

inline int32_t cInterpreter::VMlongEqz(jlong op) {
  return (op == 0);
}

inline int32_t cInterpreter::VMlongEq(jlong op1, jlong op2) {
  return (op1 == op2);
}

inline int32_t cInterpreter::VMlongNe(jlong op1, jlong op2) {
  return (op1 != op2);
}

inline int32_t cInterpreter::VMlongGe(jlong op1, jlong op2) {
  return (op1 >= op2);
}

inline int32_t cInterpreter::VMlongLe(jlong op1, jlong op2) {
  return (op1 <= op2);
}

inline int32_t cInterpreter::VMlongLt(jlong op1, jlong op2) {
  return (op1 < op2);
}

inline int32_t cInterpreter::VMlongGt(jlong op1, jlong op2) {
  return (op1 > op2);
}

inline int32_t cInterpreter::VMlongCompare(jlong op1, jlong op2) {
  return (VMlongLt(op1, op2) ? -1 : VMlongGt(op1, op2) ? 1 : 0);
}

// Long conversions

inline jdouble cInterpreter::VMlong2Double(jlong val) {
  return (jdouble) val;
}

inline jfloat cInterpreter::VMlong2Float(jlong val) {
  return (jfloat) val;
}

inline jint cInterpreter::VMlong2Int(jlong val) {
  return (jint) val;
}

// Double Arithmetic

inline jdouble cInterpreter::VMdoubleAdd(jdouble op1, jdouble op2) {
  return op1 + op2;
}

inline jdouble cInterpreter::VMdoubleDiv(jdouble op1, jdouble op2) {
  // Divide by zero... QQQ
  return op1 / op2;
}

inline jdouble cInterpreter::VMdoubleMul(jdouble op1, jdouble op2) {
  return op1 * op2;
}

inline jdouble cInterpreter::VMdoubleNeg(jdouble op) {
  return -op;
}

inline jdouble cInterpreter::VMdoubleRem(jdouble op1, jdouble op2) {
  return fmod(op1, op2);
}

inline jdouble cInterpreter::VMdoubleSub(jdouble op1, jdouble op2) {
  return op1 - op2;
}

inline int32_t cInterpreter::VMdoubleCompare(jdouble op1, jdouble op2, int32_t direction) {
  return ( op1 < op2 ? -1 : 
	       op1 > op2 ? 1 : 
		   op1 == op2 ? 0 : 
		       (direction == -1 || direction == 1) ? direction : 0);
}

// Double Conversions

inline jfloat cInterpreter::VMdouble2Float(jdouble val) {
  return (jfloat) val;
}

// Float Conversions

inline jdouble cInterpreter::VMfloat2Double(jfloat op) {
  return (jdouble) op;
}

// Integer Arithmetic

inline jint cInterpreter::VMintAdd(jint op1, jint op2) {
  return op1 + op2;
}

inline jint cInterpreter::VMintAnd(jint op1, jint op2) {
  return op1 & op2;
}

inline jint cInterpreter::VMintDiv(jint op1, jint op2) {
  /* it's possible we could catch this special case implicitly */
  if (op1 == 0x80000000 && op2 == -1) return op1;
  else return op1 / op2;
}

inline jint cInterpreter::VMintMul(jint op1, jint op2) {
  return op1 * op2;
}

inline jint cInterpreter::VMintNeg(jint op) {
  return -op;
}

inline jint cInterpreter::VMintOr(jint op1, jint op2) {
  return op1 | op2;
}

inline jint cInterpreter::VMintRem(jint op1, jint op2) {
  /* it's possible we could catch this special case implicitly */
  if (op1 == 0x80000000 && op2 == -1) return 0;
  else return op1 % op2;
}

inline jint cInterpreter::VMintShl(jint op1, jint op2) {
  return op1 <<  op2;
}

inline jint cInterpreter::VMintShr(jint op1, jint op2) {
  return op1 >>  op2; // QQ op2 & 0x1f??
}

inline jint cInterpreter::VMintSub(jint op1, jint op2) {
  return op1 - op2;
}

inline jint cInterpreter::VMintUshr(jint op1, jint op2) {
  return ((juint) op1) >> op2; // QQ op2 & 0x1f??
}

inline jint cInterpreter::VMintXor(jint op1, jint op2) {
  return op1 ^ op2;
}

inline jdouble cInterpreter::VMint2Double(jint val) {
  return (jdouble) val;
}

inline jfloat cInterpreter::VMint2Float(jint val) {
  return (jfloat) val;
}

inline jlong cInterpreter::VMint2Long(jint val) {
  return (jlong) val;
}

inline jchar cInterpreter::VMint2Char(jint val) {
  return (jchar) val;
}

inline jshort cInterpreter::VMint2Short(jint val) {
  return (jshort) val;
}

inline jbyte cInterpreter::VMint2Byte(jint val) {
  return (jbyte) val;
}

// The implementations are platform dependent. We have to worry about alignment
// issues on some machines which can change on the same platform depending on
// whether it is an LP64 machine also.
// IA32 is real easy since there are no alignment issues.

// SLOTS
inline jdouble JavaSlot::Double(address p) { return ((VMJavaVal64*)p)->d; }
inline jint JavaSlot::Int(address p) { return *(jint*)p ; }
inline jfloat JavaSlot::Float(address p) { return *(float*) p; }
inline jlong JavaSlot::Long(address p) { return ((VMJavaVal64*)p)->l; }

// STACK_CELL
inline oop JavaSlot::Object(address p) { VERIFY_OOP(*(oop*)p ) ; return *(oop*) p; }
inline address JavaSlot::Address(address p) { return *(address *) p; }
inline intptr_t JavaSlot::Raw(address p) { return *(intptr_t*) p; }

// For copying an internal vm representation to a slot
void JavaSlot::set_Address(address value, address p) { *(address*)p = value; }
void JavaSlot::set_Int(jint value, address p) { *(jint*)p = value; }
void JavaSlot::set_Float(jfloat value, address p) { *(jfloat *)p = value; }
void JavaSlot::set_Object(oop value, address p) { VERIFY_OOP(value); *(oop *)p = value; }

  // For copying a slot representation to another slot
void JavaSlot::set_Raw(address value, address p) { *(intptr_t*)p = *(intptr_t*)value; }
void JavaSlot::set_Double(address value, address p) { ((VMJavaVal64*)p)->d = ((VMJavaVal64*)value)->d; } // Wrong for LP64
void JavaSlot::set_Long(address value, address p) { ((VMJavaVal64*)p)->l = ((VMJavaVal64*)value)->l; }

// LOCALS

// ia32 implementation - locals is array on the stack with indices going from 0..-(locals-1)
// because the locals are actually overlayed on the parameters to the call on the
// expression stack which also grows down. Strange but true...
//
inline jdouble JavaLocals::Double(int slot) { return ((VMJavaVal64*) &_base[-(slot + 1)])->d; }
inline jint JavaLocals::Int(int slot) { return _base[-slot]; }
inline jfloat JavaLocals::Float(int slot) { return *((jfloat*) &_base[-slot]); }
inline jlong JavaLocals::Long(int slot) { return ((VMJavaVal64*) &_base[-(slot + 1)])->l; }

inline oop JavaLocals::Object(int slot) { VERIFY_OOP((oop)_base[-slot]); return (oop) _base[-slot]; }
inline address JavaLocals::Address(int slot) { return (address) _base[-slot]; }
inline intptr_t JavaLocals::Raw(int slot) { return (intptr_t) _base[-slot]; }

// For copying an internal vm representation to a slot
inline void JavaLocals::set_Address(address value, int slot) { *((address *) &_base[-slot]) = value; }
inline void JavaLocals::set_Int(jint value, int slot) { *((jint *) &_base[-slot]) = value; }
inline void JavaLocals::set_Float(jfloat value, int slot) { *((jfloat *) &_base[-slot]) = value; }
inline void JavaLocals::set_Object(oop value, int slot) { VERIFY_OOP(value); *((oop *) &_base[-slot]) = value; }
inline void JavaLocals::set_Double(jdouble value, int slot) { ((VMJavaVal64*) &_base[-(slot+1)])->d = value; }
inline void JavaLocals::set_Long(jlong value, int slot) { ((VMJavaVal64*) &_base[-(slot+1)])->l = value; }

// For copying a slot representation to another slot
inline void JavaLocals::set_Raw(address value, int slot) { *(intptr_t*)&_base[-slot] = *(intptr_t *)value; }
inline void JavaLocals::set_Double(address value, int slot) { ((VMJavaVal64 *)&_base[-(slot+1)])->d = ((VMJavaVal64*)value)->d; }
inline void JavaLocals::set_Long(address value, int slot) { ((VMJavaVal64 *)&_base[-(slot+1)])->l = ((VMJavaVal64 *)value)->l; }

// Return the address of the slot representation
inline address JavaLocals::Double_At(int slot) { return (address) &_base[-(slot+1)]; }
inline address JavaLocals::Long_At(int slot) { return (address) &_base[-(slot+1)]; }
inline address JavaLocals::Raw_At(int slot) { return (address) &_base[-slot]; }

inline void JavaLocals::Locals(intptr_t* _new_base) { _base = _new_base; }
inline intptr_t* JavaLocals::base(void) { return _base; }
inline intptr_t** JavaLocals::base_addr(void) { return &_base; }

// STACK
// On ia32 _tos is growing down towards lower memory as items are pushed
// QQQ seems like int/float might have issues with raw implemenation...

inline jdouble JavaStack::Double(int offset) { return ((VMJavaVal64*) &_tos[-offset])->d; }
inline jint JavaStack::Int(int offset) { return *((jint*) &_tos[-offset]); }
inline jfloat JavaStack::Float(int offset) { return *((jfloat *) &_tos[-offset]); }
inline jlong JavaStack::Long(int offset) { return ((VMJavaVal64 *) &_tos[-offset])->l; }

inline oop JavaStack::Object(int offset) { VERIFY_OOP(*(oop *) &_tos[-offset]); return *((oop *) &_tos[-offset]); }
inline address JavaStack::Address(int offset) { return *((address *) &_tos[-offset]); }
inline intptr_t JavaStack::Raw(int offset) { return *((intptr_t*) &_tos[-offset]); }

// For copying an internal vm representation to a slot
inline void JavaStack::set_Address(address value, int offset) { *((address *)&_tos[-offset]) = value; }
inline void JavaStack::set_Int(jint value, int offset) { *((jint *)&_tos[-offset]) = value; }
inline void JavaStack::set_Float(jfloat value, int offset) { *((jfloat *)&_tos[-offset]) = value; }
inline void JavaStack::set_Object(oop value, int offset) { VERIFY_OOP(value); *((oop *)&_tos[-offset]) = value; }
inline void JavaStack::set_Double(jdouble value, int offset) { ((VMJavaVal64*)&_tos[-offset])->d = value; }
inline void JavaStack::set_Long(jlong value, int offset) { ((VMJavaVal64*)&_tos[-offset])->l = value; }

// For copying a slot representation to a stack location (offset)
inline void JavaStack::set_Raw(address value, int offset) { *(intptr_t*)&_tos[-offset] = *(intptr_t*)value; }
inline void JavaStack::set_Double(address value, int offset) { ((VMJavaVal64*)&_tos[-offset])->d = ((VMJavaVal64*)value)->d; }
inline void JavaStack::set_Long(address value, int offset) { ((VMJavaVal64*)&_tos[-offset])->l = ((VMJavaVal64*)value)->l; }

// Return the address of the slot representation
inline address JavaStack::Double_At(int offset) { return (address) &_tos[-offset]; }
inline address JavaStack::Long_At(int offset) { return (address) &_tos[-offset]; }
inline address JavaStack::Raw_At(int offset) { return (address) &_tos[-offset]; }

// Stack grows down
inline void JavaStack::Pop(int count) { _tos +=count; }
inline void JavaStack::Push(int count) { _tos -= count; }
inline void JavaStack::Adjust(int count) { 
  // conceptual stack says count negative -> pop, count positive -> push
  // since stack grows down we reverse the sense
  _tos -= count; 
}

// inline void JavaStack::Tos(intptr_t* new_tos) { _tos = new_tos; }
inline void JavaStack::Reset(intptr_t* base) { _tos = base - 1; } // prepush. We don't like the knowledge leak here. QQQ
inline intptr_t* JavaStack::get_Tos() { return _tos; }
inline intptr_t* JavaStack::top() { return _tos + 1; }

