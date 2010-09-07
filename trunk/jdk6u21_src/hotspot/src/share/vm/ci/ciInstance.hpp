/*
 * Copyright (c) 1999, 2005, Oracle and/or its affiliates. All rights reserved.
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

// ciInstance
//
// This class represents an instanceOop in the HotSpot virtual
// machine.  This is an oop which corresponds to a non-array
// instance of java.lang.Object.
class ciInstance : public ciObject {
  CI_PACKAGE_ACCESS

protected:
  ciInstance(instanceHandle h_i) : ciObject(h_i) {
    assert(h_i()->is_instance(), "wrong type");
  }

  ciInstance(ciKlass* klass) : ciObject(klass) {}

  instanceOop get_instanceOop() { return (instanceOop)get_oop(); }

  const char* type_string() { return "ciInstance"; }

  void print_impl(outputStream* st);

public:
  // If this object is a java mirror, return the corresponding type.
  // Otherwise, return NULL.
  // (Remember that a java mirror is an instance of java.lang.Class.)
  ciType* java_mirror_type();

  // What kind of ciObject is this?
  bool is_instance()     { return true; }
  bool is_java_object()  { return true; }

  // Constant value of a field.
  ciConstant field_value(ciField* field);

  // Constant value of a field at the specified offset.
  ciConstant field_value_by_offset(int field_offset);
};
