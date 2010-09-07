/*
 * Copyright (c) 1997, 2008, Oracle and/or its affiliates. All rights reserved.
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

// constant tags in Java .class files


enum {
  // See jvm.h for shared JVM_CONSTANT_XXX tags
  // NOTE: replicated in SA in vm/agent/sun/jvm/hotspot/utilities/ConstantTag.java
  // Hotspot specific tags
  JVM_CONSTANT_Invalid                  = 0,    // For bad value initialization
  JVM_CONSTANT_InternalMin              = 100,  // First implementation tag (aside from bad value of course)
  JVM_CONSTANT_UnresolvedClass          = 100,  // Temporary tag until actual use
  JVM_CONSTANT_ClassIndex               = 101,  // Temporary tag while constructing constant pool
  JVM_CONSTANT_UnresolvedString         = 102,  // Temporary tag until actual use
  JVM_CONSTANT_StringIndex              = 103,  // Temporary tag while constructing constant pool
  JVM_CONSTANT_UnresolvedClassInError   = 104,  // Error tag due to resolution error
  JVM_CONSTANT_Object                   = 105,  // Required for BoundMethodHandle arguments.
  JVM_CONSTANT_InternalMax              = 105   // Last implementation tag
};


class constantTag VALUE_OBJ_CLASS_SPEC {
 private:
  jbyte _tag;
 public:
  bool is_klass() const             { return _tag == JVM_CONSTANT_Class; }
  bool is_field () const            { return _tag == JVM_CONSTANT_Fieldref; }
  bool is_method() const            { return _tag == JVM_CONSTANT_Methodref; }
  bool is_interface_method() const  { return _tag == JVM_CONSTANT_InterfaceMethodref; }
  bool is_string() const            { return _tag == JVM_CONSTANT_String; }
  bool is_int() const               { return _tag == JVM_CONSTANT_Integer; }
  bool is_float() const             { return _tag == JVM_CONSTANT_Float; }
  bool is_long() const              { return _tag == JVM_CONSTANT_Long; }
  bool is_double() const            { return _tag == JVM_CONSTANT_Double; }
  bool is_name_and_type() const     { return _tag == JVM_CONSTANT_NameAndType; }
  bool is_utf8() const              { return _tag == JVM_CONSTANT_Utf8; }

  bool is_invalid() const           { return _tag == JVM_CONSTANT_Invalid; }

  bool is_unresolved_klass() const {
    return _tag == JVM_CONSTANT_UnresolvedClass || _tag == JVM_CONSTANT_UnresolvedClassInError;
  }

  bool is_unresolved_klass_in_error() const {
    return _tag == JVM_CONSTANT_UnresolvedClassInError;
  }

  bool is_klass_index() const       { return _tag == JVM_CONSTANT_ClassIndex; }
  bool is_unresolved_string() const { return _tag == JVM_CONSTANT_UnresolvedString; }
  bool is_string_index() const      { return _tag == JVM_CONSTANT_StringIndex; }

  bool is_object() const            { return _tag == JVM_CONSTANT_Object; }

  bool is_klass_reference() const   { return is_klass_index() || is_unresolved_klass(); }
  bool is_klass_or_reference() const{ return is_klass() || is_klass_reference(); }
  bool is_field_or_method() const   { return is_field() || is_method() || is_interface_method(); }
  bool is_symbol() const            { return is_utf8(); }

  constantTag(jbyte tag) {
    assert((tag >= 0 && tag <= JVM_CONSTANT_NameAndType) ||
           (tag >= JVM_CONSTANT_InternalMin && tag <= JVM_CONSTANT_InternalMax), "Invalid constant tag");
    _tag = tag;
  }

  jbyte value()                      { return _tag; }

  void print_on(outputStream* st) const PRODUCT_RETURN;
};
