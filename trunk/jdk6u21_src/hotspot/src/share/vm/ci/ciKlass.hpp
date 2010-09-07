/*
 * Copyright (c) 1999, 2009, Oracle and/or its affiliates. All rights reserved.
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

// ciKlass
//
// This class and its subclasses represent klassOops in the
// HotSpot virtual machine.  In the vm, each klassOop contains an
// embedded Klass object.  ciKlass is subclassed to explicitly
// represent the kind of Klass embedded in the klassOop.  For
// example, a klassOop with an embedded objArrayKlass object is
// represented in the ciObject hierarchy by the class
// ciObjArrayKlass.
class ciKlass : public ciType {
  CI_PACKAGE_ACCESS
  friend class ciEnv;
  friend class ciField;
  friend class ciMethod;
  friend class ciObjArrayKlass;

private:
  ciSymbol* _name;
  jint _layout_helper;

protected:
  ciKlass(KlassHandle k_h, ciSymbol* name);
  ciKlass(ciSymbol* name, ciKlass* klass);

  klassOop get_klassOop() const {
    klassOop k = (klassOop)get_oop();
    assert(k != NULL, "illegal use of unloaded klass");
    return k;
  }

  Klass*   get_Klass() const { return get_klassOop()->klass_part(); }

  // Certain subklasses have an associated class loader.
  virtual oop loader()             { return NULL; }
  virtual jobject loader_handle()  { return NULL; }

  virtual oop protection_domain()             { return NULL; }
  virtual jobject protection_domain_handle()  { return NULL; }

  const char* type_string() { return "ciKlass"; }

  void print_impl(outputStream* st);

public:
  ciKlass(KlassHandle k_h);

  // What is the name of this klass?
  ciSymbol* name() const { return _name; }

  // What is its layout helper value?
  jint layout_helper() { return _layout_helper; }

  bool is_subtype_of(ciKlass* klass);
  bool is_subclass_of(ciKlass* klass);
  juint super_depth();
  juint super_check_offset();
  ciKlass* super_of_depth(juint i);
  bool can_be_primary_super();
  static juint primary_super_limit() { return Klass::primary_super_limit(); }

  // Get the shared parent of two klasses.
  ciKlass* least_common_ancestor(ciKlass* k);

  virtual bool is_interface() {
    return false;
  }

  virtual bool is_abstract() {
    return false;
  }

  // Does this type (array, class, interface) have no subtypes?
  virtual bool is_leaf_type() {
    return false;
  }

  // Attempt to get a klass using this ciKlass's loader.
  ciKlass* find_klass(ciSymbol* klass_name);
  // Note:  To find a class from its name string, use ciSymbol::make,
  // but consider adding to vmSymbols.hpp instead.

  // Get the instance of java.lang.Class corresponding to this klass.
  ciInstance*            java_mirror();

  // Fetch Klass::modifier_flags.
  jint                   modifier_flags();

  // Fetch Klass::access_flags.
  jint                   access_flags();

  // What kind of ciObject is this?
  bool is_klass() { return true; }

  void print_name_on(outputStream* st);
};
