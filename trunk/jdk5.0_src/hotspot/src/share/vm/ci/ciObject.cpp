#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)ciObject.cpp	1.18 03/12/23 16:39:37 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_ciObject.cpp.incl"

// ciObject
//
// This class represents an oop in the HotSpot virtual machine.
// Its subclasses are structured in a hierarchy which mirrors
// an aggregate of the VM's oop and klass hierarchies (see
// oopHierarchy.hpp).  Each instance of ciObject holds a handle
// to a corresponding oop on the VM side and provides routines
// for accessing the information in its oop.  By using the ciObject
// hierarchy for accessing oops in the VM, the compiler ensures
// that it is safe with respect to garbage collection; that is,
// GC and compilation can proceed independently without
// interference.
//
// Within the VM, the oop and klass hierarchies are separate.
// The compiler interface does not preserve this separation --
// the distinction between `klassOop' and `Klass' are not
// reflected in the interface and instead the Klass hierarchy
// is directly modeled as the subclasses of ciKlass.

// ------------------------------------------------------------------
// ciObject::ciObject
ciObject::ciObject(oop o) {
  ASSERT_IN_VM;
  if (ciObjectFactory::shared_is_initialized()) {
    _handle = JNIHandles::make_local(o);
  } else {
    _handle = JNIHandles::make_global(o, false);
  }
  _klass = NULL;
  _ident = 0;
}

// ------------------------------------------------------------------
// ciObject::ciObject
//
ciObject::ciObject(Handle h) {
  ASSERT_IN_VM;
  if (ciObjectFactory::shared_is_initialized()) {
    _handle = JNIHandles::make_local(h());
  } else {
    _handle = JNIHandles::make_global(h, false);
  }
  _klass = NULL;
  _ident = 0;
}

// ------------------------------------------------------------------
// ciObject::ciObject
//
// Unloaded klass/method variant.  `klass' is the klass of the unloaded
// klass/method, if that makes sense.
ciObject::ciObject(ciKlass* klass) {
  ASSERT_IN_VM;
  assert(klass != NULL, "must supply klass");
  _handle = NULL;
  _klass = klass;
  _ident = 0;
}

// ------------------------------------------------------------------
// ciObject::ciObject
//
// NULL variant.  Used only by ciNullObject.
ciObject::ciObject() {
  ASSERT_IN_VM;
  _handle = NULL;
  _klass = NULL;
  _ident = 0;
}

// ------------------------------------------------------------------
// ciObject::klass
//
// Get the ciKlass of this ciObject.
ciKlass* ciObject::klass() {
  if (_klass == NULL) {
    if (_handle == NULL) {
      // When both _klass and _handle are NULL, we are dealing
      // with the distinguished instance of ciNullObject.
      // No one should ask it for its klass.
      assert(is_null_object(), "must be null object");
      ShouldNotReachHere();
      return NULL;
    }

    GUARDED_VM_ENTRY(
      oop o = get_oop();
      _klass = CURRENT_ENV->get_object(o->klass())->as_klass();
    );
  }
  return _klass;
}

// ------------------------------------------------------------------
// ciObject::set_ident
//
// Set the unique identity number of a ciObject.
void ciObject::set_ident(uint id) {
  assert((_ident >> FLAG_BITS) == 0, "must only initialize once");
  assert( id < ((uint)1 << (BitsPerInt-FLAG_BITS)), "id too big");
  _ident = _ident + (id << FLAG_BITS);
}

// ------------------------------------------------------------------
// ciObject::ident
//
// Report the unique identity number of a ciObject.
uint ciObject::ident() {
  uint id = _ident >> FLAG_BITS;
  assert(id != 0, "must be initialized");
  return id;
}

// ------------------------------------------------------------------
// ciObject::equals
//
// Are two ciObjects equal?
bool ciObject::equals(ciObject* obj) {
  return (this == obj);
}

// ------------------------------------------------------------------
// ciObject::hash
//
// A hash value for the convenience of compilers.
//
// Implementation note: we use the address of the ciObject as the
// basis for the hash.  If the clumping effects of this hashing
// scheme are poor, then we can add an explicit _hash field to
// all ciObjects and compute a well-behaved hash.
int ciObject::hash() {
  return ((intptr_t)this) >> LogBytesPerWord;
}

// ------------------------------------------------------------------
// ciObject::encoding
//
// The address which the compiler should embed into the
// generated code to represent this oop.  This address
// is not the true address of the oop -- it will get patched
// during nmethod creation.
//
//
//
// Implementation note: we use the handle as the encoding.  The
// nmethod constructor resolves the handle and patches in the oop.
//
// This method should be changed to return an generified address
// to discourage use of the JNI handle.
jobject ciObject::encoding() {
  assert(is_null_object() || handle() != NULL, "cannot embed null pointer");
  assert(has_encoding(), "oop must be NULL or perm");
  return handle();
}
  
// ------------------------------------------------------------------
// ciObject::has_encoding
bool ciObject::has_encoding() {
  return handle() == NULL || is_perm();
}


// ------------------------------------------------------------------
// ciObject::print
//
// Print debugging output about this ciObject.
//
// Implementation note: dispatch to the virtual print_impl behavior
// for this ciObject.
void ciObject::print() {
  tty->print("<%s", type_string());
  GUARDED_VM_ENTRY(print_impl();)
  tty->print(" ident=%d %s address=0x%x>", ident(),
        is_perm() ? "PERM" : "",
        (address)this);
}

// ------------------------------------------------------------------
// ciObject::print_oop
//
// Print debugging output about the oop this ciObject represents.
void ciObject::print_oop() {
  if (is_null_object()) {
    tty->print_cr("NULL");
  } else if (!is_loaded()) {
    tty->print_cr("UNLOADED");
  } else {
    GUARDED_VM_ENTRY(get_oop()->print();)
  }
}

