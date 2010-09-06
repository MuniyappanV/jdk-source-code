#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)ciType.cpp	1.10 03/12/23 16:39:41 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_ciType.cpp.incl"

ciType* ciType::_basic_types[T_CONFLICT+1];

// ciType
//
// This class represents either a class (T_OBJECT), array (T_ARRAY),
// or one of the primitive types such as T_INT.

// ------------------------------------------------------------------
// ciType::ciType
//
ciType::ciType(BasicType basic_type) : ciObject() {
  assert(basic_type >= T_BOOLEAN && basic_type <= T_CONFLICT, "range check");
  assert(basic_type != T_OBJECT && basic_type != T_ARRAY, "not a reference type");
  _basic_type = basic_type;
}

ciType::ciType(KlassHandle k) : ciObject(k) {
  _basic_type = Klass::cast(k())->oop_is_array() ? T_ARRAY : T_OBJECT;
}

ciType::ciType(ciKlass* klass) : ciObject(klass) {
  _basic_type = klass->is_array_klass_klass() ? T_ARRAY : T_OBJECT;
}


// ------------------------------------------------------------------
// ciType::is_subtype_of
//
bool ciType::is_subtype_of(ciType* type) {
  if (this == type)  return true;
  if (is_klass() && type->is_klass())
    return this->as_klass()->is_subtype_of(type->as_klass());
  return false;
}

// ------------------------------------------------------------------
// ciType::print_impl
//
// Implementation of the print method.
void ciType::print_impl() {
  tty->print(" type=");
  print_name();
}

// ------------------------------------------------------------------
// ciType::print_name
//
// Print the name of this type
void ciType::print_name() {
  tty->print(type2name(basic_type()));
}



// ------------------------------------------------------------------
// ciType::java_mirror
//
ciInstance* ciType::java_mirror() {
  VM_ENTRY_MARK;
  return CURRENT_THREAD_ENV->get_object(SystemDictionary::java_mirror(basic_type()))->as_instance();
}

// ------------------------------------------------------------------
// ciType::box_klass
//
ciKlass* ciType::box_klass() {
  if (!is_primitive_type())  return this->as_klass();  // reference types are "self boxing"

  // Void is "boxed" with a null.
  if (basic_type() == T_VOID)  return NULL;

  VM_ENTRY_MARK;
  return CURRENT_THREAD_ENV->get_object(SystemDictionary::box_klass(basic_type()))->as_instance_klass();
}


// ------------------------------------------------------------------
// ciType::make
//
// Produce the ciType for a given primitive BasicType.
// As a bonus, produce the right reference type for T_OBJECT.
// Does not work on T_ARRAY.
ciType* ciType::make(BasicType t) {
  // short, etc.
  // Note: Bare T_ADDRESS means a raw pointer type, not a return_address.
  assert((uint)t < T_CONFLICT+1, "range check");
  if (t == T_OBJECT)  return ciEnv::_Object;  // java/lang/Object
  assert(_basic_types[t] != NULL, "domain check");
  return _basic_types[t];
}

// ciReturnAddress
//
// This class represents the type of a specific return address in the
// bytecodes.

// ------------------------------------------------------------------
// ciReturnAddress::ciReturnAddress
//
ciReturnAddress::ciReturnAddress(int bci) : ciType(T_ADDRESS) {
  assert(0 <= bci, "bci cannot be negative");
  _bci = bci;
}

// ------------------------------------------------------------------
// ciReturnAddress::print_impl
//
// Implementation of the print method.
void ciReturnAddress::print_impl() {
  tty->print(" bci=%d", _bci);
}

// ------------------------------------------------------------------
// ciReturnAddress::make
ciReturnAddress* ciReturnAddress::make(int bci) {
  GUARDED_VM_ENTRY(return CURRENT_ENV->get_return_address(bci);)
}
