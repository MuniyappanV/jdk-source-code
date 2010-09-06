#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)systemDictionary.hpp	1.127 04/03/02 11:17:44 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// The system dictionary stores all loaded classes and maps:
//
//   [class name,class loader] -> class   i.e.  [symbolOop,oop] -> klassOop
//
// Classes are loaded lazily. The default VM class loader is
// represented as NULL.

// The underlying data structure is an open hash table with a fixed number
// of buckets. During loading the loader object is locked, (for the VM loader 
// a private lock object is used). Class loading can thus be done concurrently,
// but only by different loaders.
//
// During loading a placeholder (name, loader) is temporarily placed in
// a side data structure, and is used to detect ClassCircularityErrors
// and to perform verification during GC.  A GC can occur in the midst
// of class loading, as we call out to Java, have to take locks, etc.
//
// When class loading is finished, a new entry is added to the system
// dictionary and the place holder is removed. Note that the protection
// domain field of the system dictionary has not yet been filled in when
// the "real" system dictionary entry is created.
//
// Clients of this class who are interested in finding if a class has
// been completely loaded -- not classes in the process of being loaded --
// can read the SystemDictionary unlocked. This is safe because
//    - entries are only deleted at safepoints  
//    - readers cannot come to a safepoint while actively examining
//         an entry  (an entry cannot be deleted from under a reader) 
//    - entries must be fully formed before they are available to concurrent
//         readers (we must ensure write ordering)
//
// Note that placeholders are deleted at any time, as they are removed
// when a class is completely loaded. Therefore, readers as well as writers
// of placeholders must hold the SystemDictionary_lock.
// 

class Dictionary;
class PlaceholderTable;
class LoaderConstraintTable;
class HashtableBucket;


class SystemDictionary : AllStatic {
  friend class VMStructs;
  friend class CompactingPermGenGen;
  NOT_PRODUCT(friend class instanceKlassKlass;)

 public:
  // Returns a class with a given class name and class loader.  Loads the
  // class if needed. If not found a NoClassDefFoundError or a
  // ClassNotFoundException is thrown, depending on the value on the
  // throw_error flag.  For most uses the throw_error argument should be set
  // to true.

  static klassOop resolve_or_fail(symbolHandle class_name, Handle class_loader, Handle protection_domain, bool throw_error, TRAPS);
  // Convenient call for null loader and protection domain.
  static klassOop resolve_or_fail(symbolHandle class_name, bool throw_error, TRAPS);

  // Returns a class with a given class name and class loader.
  // Loads the class if needed. If not found NULL is returned.
  static klassOop resolve_or_null(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS);
  // Version with null loader and protection domain
  static klassOop resolve_or_null(symbolHandle class_name, TRAPS);

  // Resolve a superclass. Called from ClassFileParser only.
  // "child_name" is the class whose super interface or class is being resolved.
  static klassOop resolve_super_or_fail(symbolHandle child_name,
                                        symbolHandle class_name,
                                        Handle class_loader,
                                        Handle protection_domain,
                                        TRAPS);

  // Parse new stream. This won't update the system dictionary or
  // class hierarchy, simply parse the stream. Use by jvmdi hotswap.
  static klassOop parse_stream(symbolHandle class_name,
                               Handle class_loader,
                               Handle protection_domain,
                               ClassFileStream* st,
                               TRAPS);
                               
  // Resolve from stream (called by jni_DefineClass and JVM_DefineClass)
  static klassOop resolve_from_stream(symbolHandle class_name, Handle class_loader, Handle protection_domain, ClassFileStream* st, TRAPS);
  
  // Lookup an already loaded class. If not found NULL is returned.
  static klassOop find(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS);

  // Lookup an already loaded instance or array class.
  // Do not make any queries to class loaders; consult only the cache.
  // If not found NULL is returned.
  static klassOop find_instance_or_array_klass(symbolHandle class_name,
					       Handle class_loader,
					       Handle protection_domain,
					       TRAPS);

  // Lookup an instance or array class that has already been loaded
  // either into the given class loader, or else into another class
  // loader that is constrained (via loader constraints) to produce
  // a consistent class.  Do not take protection domains into account.
  // Do not make any queries to class loaders; consult only the cache.
  // Return NULL if the class is not found.
  //
  // This function is a strict superset of find_instance_or_array_klass.
  // This function (the unchecked version) makes a conservative prediction
  // of the result of the checked version, assuming successful lookup.
  // If both functions return non-null, they must return the same value.
  // Also, the unchecked version may sometimes be non-null where the
  // checked version is null.  This can occur in several ways:
  //   1. No query has yet been made to the class loader.
  //   2. The class loader was queried, but chose not to delegate.
  //   3. ClassLoader.checkPackageAccess rejected a proposed protection domain.
  //   4. Loading was attempted, but there was a linkage error of some sort.
  // In all of these cases, the loader constraints on this type are
  // satisfied, and it is safe for classes in the given class loader
  // to manipulate strongly-typed values of the found class, subject
  // to local linkage and access checks.
  static klassOop find_constrained_instance_or_array_klass(symbolHandle class_name,
                                                           Handle class_loader,
                                                           TRAPS);
  
  // Iterate over all klasses in dictionary
  //   Just the classes from declaring class loaders
  static void classes_do(void f(klassOop));
  //   All classes, and their class loaders
  static void classes_do(void f(klassOop, oop));
  //   All array classes of primitive type, and their class loaders
  static void prim_array_classes_do(void f(klassOop, oop));

  // Iterate over all methods in all klasses in dictionary
  static void methods_do(void f(methodOop));

  // Garbage collection support

  // This method applies "blk->do_oop" to all the pointers to "system"
  // classes and loaders.
  static void always_strong_oops_do(OopClosure* blk);
  static void always_strong_classes_do(OopClosure* blk);
  // This method applies "blk->do_oop" to all the placeholders.
  static void placeholders_do(OopClosure* blk);

  // Unload (that is, break root links to) all unmarked classes and
  // loaders.  Returns "true" iff something was unloaded.
  static bool do_unloading(BoolObjectClosure* is_alive,
                                   OopClosure* keep_alive);

  // Applies "f->do_oop" to all root oops in the system dictionary.
  static void oops_do(OopClosure* f);


private:
  //    Traverses preloaded oops: various system classes.  These are
  //    guaranteed to be in the perm gen.
  static void preloaded_oops_do(OopClosure* f);

public:
  // Sharing support.
  static void reorder_dictionary();
  static void copy_buckets(char** top, char* end);
  static void copy_table(char** top, char* end);
  static void reverse();
  static void set_shared_dictionary(HashtableBucket* t, int length,
                                    int number_of_entries);
  static bool is_shared_dictionary(Dictionary* d) {
    assert(d != NULL, "null dictionary");
    return (d == shared_dictionary());
  }

  // Printing
  static void print()                   PRODUCT_RETURN;
  static void print_class_statistics()  PRODUCT_RETURN;
  static void print_method_statistics() PRODUCT_RETURN;

  // Number of contained klasses
  // This is both fully loaded classes and classes in the process
  // of being loaded
  static int number_of_classes();

  // Monotonically increasing counter which grows as classes are
  // loaded or modifications such as hot-swapping or setting/removing
  // of breakpoints are performed
  static inline int number_of_modifications()     { assert_locked_or_safepoint(Compile_lock); return _number_of_modifications; }
  // Needed by evolution and breakpoint code
  static inline void notice_modification()        { assert_locked_or_safepoint(Compile_lock); ++_number_of_modifications;      }

  // Verification
  static void verify() PRODUCT_RETURN;

#ifdef ASSERT
  static bool is_internal_format(symbolHandle class_name);
#endif

  // Verify class is in dictionary
  static void verify_obj_klass_present(Handle obj,
                                       symbolHandle class_name,
                                       Handle class_loader) PRODUCT_RETURN;

  // Initialization
  static void initialize(TRAPS);

  // Fast access to commonly used classes (preloaded)
  static klassOop check_klass(klassOop k) {
    assert(k != NULL, "preloaded klass not initialized"); 
    return k;
  }

public:
  static klassOop object_klass()            { return check_klass(_object_klass); }
  static klassOop string_klass()            { return check_klass(_string_klass); }
  static klassOop class_klass()             { return check_klass(_class_klass); }
  static klassOop cloneable_klass()         { return check_klass(_cloneable_klass); }
  static klassOop classloader_klass()       { return check_klass(_classloader_klass); }
  static klassOop serializable_klass()      { return check_klass(_serializable_klass); }
  static klassOop system_klass()            { return check_klass(_system_klass); }

  static klassOop throwable_klass()         { return check_klass(_throwable_klass); }
  static klassOop error_klass()             { return check_klass(_error_klass); }
  static klassOop threaddeath_klass()       { return check_klass(_threaddeath_klass); }
  static klassOop exception_klass()         { return check_klass(_exception_klass); }
  static klassOop runtime_exception_klass() { return check_klass(_runtime_exception_klass); }
  static klassOop classNotFoundException_klass() { return check_klass(_classNotFoundException_klass); }
  static klassOop noClassDefFoundError_klass()   { return check_klass(_noClassDefFoundError_klass); }
  static klassOop ClassCastException_klass() { return check_klass(_classCastException_klass); }
  static klassOop ArrayStoreException_klass() { return check_klass(_arrayStoreException_klass); }
  static klassOop OutOfMemoryError_klass()  { return check_klass(_outOfMemoryError_klass); }
  static klassOop StackOverflowError_klass() { return check_klass(_StackOverflowError_klass); }
  static klassOop protectionDomain_klass()  { return check_klass(_protectionDomain_klass); }
  static klassOop AccessControlContext_klass() { return check_klass(_AccessControlContext_klass); }
  static klassOop reference_klass()         { return check_klass(_reference_klass); }
  static klassOop soft_reference_klass()    { return check_klass(_soft_reference_klass); }
  static klassOop weak_reference_klass()    { return check_klass(_weak_reference_klass); }
  static klassOop final_reference_klass()   { return check_klass(_final_reference_klass); }
  static klassOop phantom_reference_klass() { return check_klass(_phantom_reference_klass); }
  static klassOop finalizer_klass()         { return check_klass(_finalizer_klass); }
  
  static klassOop thread_klass()            { return check_klass(_thread_klass); }
  static klassOop threadGroup_klass()       { return check_klass(_threadGroup_klass); }
  static klassOop properties_klass()        { return check_klass(_properties_klass); }  
  static klassOop reflect_accessible_object_klass() { return check_klass(_reflect_accessible_object_klass); }
  static klassOop reflect_field_klass()     { return check_klass(_reflect_field_klass); }
  static klassOop reflect_method_klass()    { return check_klass(_reflect_method_klass); }
  static klassOop reflect_constructor_klass() { return check_klass(_reflect_constructor_klass); }
  static klassOop reflect_method_accessor_klass() { assert(Universe::is_gte_jdk14x_version() && UseNewReflection, "JDK 1.4 only"); return check_klass(_reflect_method_accessor_klass); }
  static klassOop reflect_constructor_accessor_klass() { assert(Universe::is_gte_jdk14x_version() && UseNewReflection, "JDK 1.4 only"); return check_klass(_reflect_constructor_accessor_klass); }
  // NOTE: needed too early in bootstrapping process to have checks based on JDK version
  static klassOop reflect_magic_klass()     { return _reflect_magic_klass; }
  static klassOop reflect_delegating_classloader_klass() { return _reflect_delegating_classloader_klass; }
  static klassOop reflect_constant_pool_klass() { assert(Universe::is_gte_jdk15x_version(), "JDK 1.5 only"); return _reflect_constant_pool_klass; }

  static klassOop vector_klass()            { return check_klass(_vector_klass); }
  static klassOop hashtable_klass()         { return check_klass(_hashtable_klass); }
  static klassOop stringBuffer_klass()      { return check_klass(_stringBuffer_klass); }
  static klassOop stackTraceElement_klass() { return check_klass(_stackTraceElement_klass); }

  static klassOop java_nio_Buffer_klass()   { return check_klass(_java_nio_Buffer_klass); }

  static klassOop sun_misc_AtomicLongCSImpl_klass() { return _sun_misc_AtomicLongCSImpl_klass; }

  static klassOop boolean_klass()           { return check_klass(_boolean_klass); }
  static klassOop char_klass()              { return check_klass(_char_klass); }
  static klassOop float_klass()             { return check_klass(_float_klass); }
  static klassOop double_klass()            { return check_klass(_double_klass); }
  static klassOop byte_klass()              { return check_klass(_byte_klass); }
  static klassOop short_klass()             { return check_klass(_short_klass); }
  static klassOop int_klass()               { return check_klass(_int_klass); }
  static klassOop long_klass()              { return check_klass(_long_klass); } 

  static klassOop box_klass(BasicType t) {
    assert((uint)t < T_VOID+1, "range check");
    return check_klass(_box_klasses[t]);
  }
  static BasicType box_klass_type(klassOop k);  // inverse of box_klass

private:
  // Tells whether ClassLoader.loadClassInternal is present
  static bool has_loadClassInternal()       { return _has_loadClassInternal; }

public:
  // Tells whether ClassLoader.checkPackageAccess is present
  static bool has_checkPackageAccess()      { return _has_checkPackageAccess; }

  static bool class_klass_loaded()          { return _class_klass != NULL; }
  static bool cloneable_klass_loaded()      { return _cloneable_klass != NULL; }
  
  // Returns default system loader
  static oop java_system_loader();

  // Compute the default system loader
  static void compute_java_system_loader(TRAPS);

private:
  // Mirrors for primitive classes (created eagerly)
  static oop check_mirror(oop m) {
    assert(m != NULL, "mirror not initialized"); 
    return m;
  }

public:
  static oop int_mirror()                   { return check_mirror(_int_mirror); }
  static oop float_mirror()                 { return check_mirror(_float_mirror); }
  static oop double_mirror()                { return check_mirror(_double_mirror); }
  static oop byte_mirror()                  { return check_mirror(_byte_mirror); }
  static oop bool_mirror()                  { return check_mirror(_bool_mirror); }
  static oop char_mirror()                  { return check_mirror(_char_mirror); }
  static oop long_mirror()                  { return check_mirror(_long_mirror); }
  static oop short_mirror()                 { return check_mirror(_short_mirror); }
  static oop void_mirror()                  { return check_mirror(_void_mirror); }

  static oop java_mirror(BasicType t) {
    assert((uint)t < T_VOID+1, "range check");
    return check_mirror(_mirrors[t]);
  }
  static BasicType java_mirror_type(oop mirror);  // inverse of mirror

  // Check class loader constraints
  static void add_loader_constraint(symbolHandle name, Handle loader1,
                                    Handle loader2, TRAPS);
  static void check_signature_loaders(symbolHandle signature, Handle loader1,
                                      Handle loader2, bool is_method, TRAPS);

 private:

  enum Constants {
    _loader_constraint_size = 107,                     // number of entries in constraint table
    _nof_buckets            = 1009                     // number of buckets in hash table
  };


  // Static variables

  // Hashtable holding loaded classes.
  static Dictionary*            _dictionary;

  // Hashtable holding placeholders for classes being loaded.
  static PlaceholderTable*       _placeholders;

  // Hashtable holding classes from the shared archive.
  static Dictionary*             _shared_dictionary;

  // Monotonically increasing counter which grows with
  // _number_of_classes as well as hot-swapping and breakpoint setting
  // and removal.
  static int                     _number_of_modifications;

  // Lock object for system class loader
  static oop                     _system_loader_lock_obj;

  // Constraints on class loaders
  static LoaderConstraintTable*  _loader_constraints;

public:
  // for VM_CounterDecay iteration support
  friend class CounterDecay;
  static klassOop try_get_next_class();

private:
  static void validate_protection_domain(instanceKlassHandle klass,
                                         Handle class_loader,
                                         Handle protection_domain, TRAPS);

  friend class VM_PopulateDumpSharedSpace;
  friend class TraversePlaceholdersClosure;
  static Dictionary*         dictionary() { return _dictionary; }
  static Dictionary*         shared_dictionary() { return _shared_dictionary; }
  static PlaceholderTable*   placeholders() { return _placeholders; }
  static LoaderConstraintTable* constraints() { return _loader_constraints; }

  // Basic loading operations
  static klassOop resolve_instance_class_or_null(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS);
  static klassOop resolve_array_class_or_null(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS);
  static void define_instance_class(instanceKlassHandle k, TRAPS);
  static instanceKlassHandle load_shared_class(symbolHandle class_name,
                                               Handle class_loader, TRAPS);
  static instanceKlassHandle load_shared_class(instanceKlassHandle ik,
                                               Handle class_loader, TRAPS);
  static instanceKlassHandle load_instance_class(symbolHandle class_name, Handle class_loader, TRAPS);
  static Handle compute_loader_lock_object(Handle class_loader, TRAPS);

  static klassOop find_shared_class(symbolHandle class_name);

  // Setup link to hierarchy
  static void add_to_hierarchy(instanceKlassHandle k, TRAPS);  
 
private:
  // We pass in the hashtable index so we can calculate it outside of
  // the SystemDictionary_lock.   

  // Basic find on loaded classes 
  static klassOop find_class(int index, unsigned int hash,
                             symbolHandle name, Handle loader);

  // Basic find on classes in the midst of being loaded
  static symbolOop find_placeholder(int index, unsigned int hash,
                                    symbolHandle name, Handle loader);

#ifndef PRODUCT
  // Basic find operation of loaded classes and classes in the midst
  // of loading;  used for assertions only.
  static oop find_class_or_placeholder(symbolHandle class_name,
                                       Handle class_loader);
#endif

  // Updating entry in dictionary
  // Add a completely loaded class 
  static void add_klass(int index, symbolHandle class_name,
                        Handle class_loader, KlassHandle obj);

  // Add a placeholder for a class being loaded
  static void add_placeholder(int index, 
                              symbolHandle class_name, 
                              Handle class_loader);
  static void remove_placeholder(int index,
                                 symbolHandle class_name, 
                                 Handle class_loader);

  // Performs cleanups after resolve_super_or_fail. This typically needs
  // to be called on failure.
  // Won't throw, but can block.
  static void resolution_cleanups(symbolHandle class_name,
                                  Handle class_loader,
                                  TRAPS);
  
  // Initialization
  static void initialize_preloaded_classes(TRAPS);
  static void initialize_basic_type_mirrors(TRAPS);
    
  // Class loader constraints
  static void check_constraints(int index, unsigned int hash,
                                instanceKlassHandle k, Handle loader, TRAPS);
  static void update_dictionary(int d_index, unsigned int d_hash,
                                int p_index, unsigned int p_hash,
                                instanceKlassHandle k, Handle loader, TRAPS);

  // Variables holding commonly used klasses (preloaded)
  static klassOop _object_klass;
  static klassOop _string_klass;
  static klassOop _class_klass;
  static klassOop _cloneable_klass;
  static klassOop _classloader_klass;
  static klassOop _serializable_klass;
  static klassOop _system_klass;
  
  static klassOop _throwable_klass;
  static klassOop _error_klass;
  static klassOop _threaddeath_klass;
  static klassOop _exception_klass;
  static klassOop _runtime_exception_klass;
  static klassOop _classNotFoundException_klass;
  static klassOop _noClassDefFoundError_klass;
  static klassOop _classCastException_klass;
  static klassOop _arrayStoreException_klass;
  static klassOop _outOfMemoryError_klass;
  static klassOop _StackOverflowError_klass;
  static klassOop _protectionDomain_klass;
  static klassOop _AccessControlContext_klass;
  static klassOop _reference_klass;
  static klassOop _soft_reference_klass;
  static klassOop _weak_reference_klass;
  static klassOop _final_reference_klass;
  static klassOop _phantom_reference_klass;
  static klassOop _finalizer_klass;

  static klassOop _thread_klass;
  static klassOop _threadGroup_klass;
  static klassOop _properties_klass;      
  static klassOop _reflect_accessible_object_klass;
  static klassOop _reflect_field_klass;
  static klassOop _reflect_method_klass;
  static klassOop _reflect_constructor_klass;
  // 1.4 reflection implementation
  static klassOop _reflect_magic_klass;
  static klassOop _reflect_method_accessor_klass;
  static klassOop _reflect_constructor_accessor_klass;
  static klassOop _reflect_delegating_classloader_klass;
  // 1.5 annotations implementation
  static klassOop _reflect_constant_pool_klass;

  static klassOop _stringBuffer_klass;
  static klassOop _vector_klass;
  static klassOop _hashtable_klass;

  static klassOop _stackTraceElement_klass;

  static klassOop _java_nio_Buffer_klass;

  static klassOop _sun_misc_AtomicLongCSImpl_klass;

  // Box klasses
  static klassOop _boolean_klass;
  static klassOop _char_klass;
  static klassOop _float_klass;
  static klassOop _double_klass;
  static klassOop _byte_klass;
  static klassOop _short_klass;
  static klassOop _int_klass;
  static klassOop _long_klass;

  // table of same
  static klassOop _box_klasses[T_VOID+1];

  static oop  _java_system_loader;

  static bool _has_loadClassInternal;
  static bool _has_checkPackageAccess;

  // Primitive classes
  static oop _int_mirror;
  static oop _float_mirror;
  static oop _double_mirror;
  static oop _byte_mirror;
  static oop _bool_mirror;
  static oop _char_mirror;
  static oop _long_mirror;
  static oop _short_mirror;
  static oop _void_mirror;

  // table of same
  static oop _mirrors[T_VOID+1];
};
