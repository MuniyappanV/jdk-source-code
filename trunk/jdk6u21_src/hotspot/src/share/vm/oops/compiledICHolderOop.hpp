/*
 * Copyright (c) 1998, 2004, Oracle and/or its affiliates. All rights reserved.
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

// A compiledICHolderOop is a helper object for the inline cache implementation.
// It holds an intermediate value (method+klass pair) used when converting from
// compiled to an interpreted call.
//
// compiledICHolderOops are always allocated permanent (to avoid traversing the
// codeCache during scavenge).


class compiledICHolderOopDesc : public oopDesc {
  friend class VMStructs;
 private:
  methodOop _holder_method;
  klassOop  _holder_klass;    // to avoid name conflict with oopDesc::_klass
 public:
  // accessors
  methodOop holder_method() const     { return _holder_method; }
  klassOop  holder_klass()  const     { return _holder_klass; }

  void set_holder_method(methodOop m) { oop_store_without_check((oop*)&_holder_method, (oop)m); }
  void set_holder_klass(klassOop k)   { oop_store_without_check((oop*)&_holder_klass, (oop)k); }

  static int header_size()            { return sizeof(compiledICHolderOopDesc)/HeapWordSize; }
  static int object_size()            { return align_object_size(header_size()); }

  // interpreter support (offsets in bytes)
  static int holder_method_offset()   { return offset_of(compiledICHolderOopDesc, _holder_method); }
  static int holder_klass_offset()    { return offset_of(compiledICHolderOopDesc, _holder_klass); }

  // GC support
  oop* adr_holder_method() const      { return (oop*)&_holder_method; }
  oop* adr_holder_klass() const       { return (oop*)&_holder_klass; }
};
