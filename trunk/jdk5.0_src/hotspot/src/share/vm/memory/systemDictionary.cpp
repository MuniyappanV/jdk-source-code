#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)systemDictionary.cpp	1.298 04/03/02 10:20:27 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 * 
 */

# include "incls/_precompiled.incl"
# include "incls/_systemDictionary.cpp.incl"


Dictionary*       SystemDictionary::_dictionary = NULL;
PlaceholderTable* SystemDictionary::_placeholders = NULL;
Dictionary*       SystemDictionary::_shared_dictionary = NULL;
LoaderConstraintTable* SystemDictionary::_loader_constraints = NULL;


int         SystemDictionary::_number_of_modifications = 0;

oop         SystemDictionary::_system_loader_lock_obj     =  NULL;

klassOop    SystemDictionary::_object_klass               =  NULL;
klassOop    SystemDictionary::_string_klass               =  NULL;
klassOop    SystemDictionary::_class_klass                =  NULL;
klassOop    SystemDictionary::_cloneable_klass            =  NULL;
klassOop    SystemDictionary::_classloader_klass          =  NULL;
klassOop    SystemDictionary::_serializable_klass         =  NULL;
klassOop    SystemDictionary::_system_klass               =  NULL;

klassOop    SystemDictionary::_throwable_klass            =  NULL;
klassOop    SystemDictionary::_error_klass                =  NULL;
klassOop    SystemDictionary::_threaddeath_klass          =  NULL;
klassOop    SystemDictionary::_exception_klass            =  NULL;
klassOop    SystemDictionary::_runtime_exception_klass    =  NULL;
klassOop    SystemDictionary::_classNotFoundException_klass = NULL;
klassOop    SystemDictionary::_noClassDefFoundError_klass = NULL;
klassOop    SystemDictionary::_classCastException_klass   =  NULL;
klassOop    SystemDictionary::_arrayStoreException_klass  =  NULL;
klassOop    SystemDictionary::_outOfMemoryError_klass     =  NULL;
klassOop    SystemDictionary::_StackOverflowError_klass   =  NULL;
klassOop    SystemDictionary::_protectionDomain_klass     =  NULL;
klassOop    SystemDictionary::_AccessControlContext_klass = NULL;

klassOop    SystemDictionary::_reference_klass            =  NULL;
klassOop    SystemDictionary::_soft_reference_klass       =  NULL;
klassOop    SystemDictionary::_weak_reference_klass       =  NULL;
klassOop    SystemDictionary::_final_reference_klass      =  NULL;
klassOop    SystemDictionary::_phantom_reference_klass    =  NULL;
klassOop    SystemDictionary::_finalizer_klass            =  NULL;

klassOop    SystemDictionary::_thread_klass               =  NULL;
klassOop    SystemDictionary::_threadGroup_klass          =  NULL;
klassOop    SystemDictionary::_properties_klass           =  NULL;
klassOop    SystemDictionary::_reflect_accessible_object_klass =  NULL;
klassOop    SystemDictionary::_reflect_field_klass        =  NULL;
klassOop    SystemDictionary::_reflect_method_klass       =  NULL;
klassOop    SystemDictionary::_reflect_constructor_klass  =  NULL;
klassOop    SystemDictionary::_reflect_magic_klass        =  NULL;
klassOop    SystemDictionary::_reflect_method_accessor_klass = NULL;
klassOop    SystemDictionary::_reflect_constructor_accessor_klass = NULL;
klassOop    SystemDictionary::_reflect_delegating_classloader_klass = NULL;
klassOop    SystemDictionary::_reflect_constant_pool_klass=  NULL;

klassOop    SystemDictionary::_vector_klass               =  NULL;
klassOop    SystemDictionary::_hashtable_klass            =  NULL;
klassOop    SystemDictionary::_stringBuffer_klass         =  NULL;

klassOop    SystemDictionary::_stackTraceElement_klass    =  NULL;

klassOop    SystemDictionary::_java_nio_Buffer_klass      =  NULL;

klassOop    SystemDictionary::_sun_misc_AtomicLongCSImpl_klass = NULL;

klassOop    SystemDictionary::_boolean_klass              =  NULL;
klassOop    SystemDictionary::_char_klass                 =  NULL;
klassOop    SystemDictionary::_float_klass                =  NULL;
klassOop    SystemDictionary::_double_klass               =  NULL;
klassOop    SystemDictionary::_byte_klass                 =  NULL;
klassOop    SystemDictionary::_short_klass                =  NULL;
klassOop    SystemDictionary::_int_klass                  =  NULL;
klassOop    SystemDictionary::_long_klass                 =  NULL;
klassOop    SystemDictionary::_box_klasses[T_VOID+1]      =  { NULL /*, NULL...*/ };

oop         SystemDictionary::_int_mirror                 =  NULL;
oop         SystemDictionary::_float_mirror               =  NULL;
oop         SystemDictionary::_double_mirror              =  NULL;
oop         SystemDictionary::_byte_mirror                =  NULL;
oop         SystemDictionary::_bool_mirror                =  NULL;
oop         SystemDictionary::_char_mirror                =  NULL;
oop         SystemDictionary::_long_mirror                =  NULL;
oop         SystemDictionary::_short_mirror               =  NULL;
oop         SystemDictionary::_void_mirror                =  NULL;
oop         SystemDictionary::_mirrors[T_VOID+1]          =  { NULL /*, NULL...*/ };

oop         SystemDictionary::_java_system_loader         =  NULL;

bool        SystemDictionary::_has_loadClassInternal      =  false;
bool        SystemDictionary::_has_checkPackageAccess     =  false;

// ----------------------------------------------------------------------------
// Java-level SystemLoader

oop SystemDictionary::java_system_loader() {
  return _java_system_loader;
}

void SystemDictionary::compute_java_system_loader(TRAPS) {
  KlassHandle system_klass(THREAD, _classloader_klass);    
  JavaValue result(T_OBJECT);
  JavaCalls::call_static(&result, 
                         KlassHandle(THREAD, _classloader_klass),
                         vmSymbolHandles::getSystemClassLoader_name(),
                         vmSymbolHandles::void_classloader_signature(),
                         CHECK);
    
  _java_system_loader = (oop)result.get_jobject();    
}


// ----------------------------------------------------------------------------
// debugging

#ifdef ASSERT

// return true if class_name contains no '.' (internal format is '/')
bool SystemDictionary::is_internal_format(symbolHandle class_name) {
  if (class_name.not_null()) {
    ResourceMark rm;
    char* name = class_name->as_C_string();
    return strchr(name, '.') == NULL;
  } else {
    return true;
  }
}

#endif

// ----------------------------------------------------------------------------
// Resolving of classes

// Forwards to resolve_or_null

klassOop SystemDictionary::resolve_or_fail(symbolHandle class_name, Handle class_loader, Handle protection_domain, bool throw_error, TRAPS) {  
  klassOop klass = resolve_or_null(class_name, class_loader, protection_domain, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    // If we have a pending exception we forward it to the caller, unless throw_error is true,
    // in which case we have to check whether the pending exception is a ClassNotFoundException,
    // and if so convert it to a NoClassDefFoundError
    if (throw_error && PENDING_EXCEPTION->is_a(SystemDictionary::classNotFoundException_klass())) {
      // Convert ClassNotFoundException to NoClassDefFoundError (by falling through)
      assert(klass == NULL, "Should not have result with exception pending");
      CLEAR_PENDING_EXCEPTION;
    } else {
      return NULL;  // Forward exception to caller
    }
  }
  // Class not found, throw appropriate error or exception depending on value of throw_error
  if (klass == NULL) {
    ResourceMark rm(THREAD);
    if (throw_error) {
      THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), class_name->as_C_string());
    } else {      
      THROW_MSG_0(vmSymbols::java_lang_ClassNotFoundException(), class_name->as_C_string());      
    }
  }
  return klass;
}


klassOop SystemDictionary::resolve_or_fail(symbolHandle class_name,
                                           bool throw_error, TRAPS)
{
  return resolve_or_fail(class_name, NULL, NULL, throw_error, THREAD);
}


// Forwards to resolve_instance_class_or_null

klassOop SystemDictionary::resolve_or_null(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS) {  
  if (FieldType::is_array(class_name())) {
    return resolve_array_class_or_null(class_name, class_loader, protection_domain, CHECK_0);
  } else {
    return resolve_instance_class_or_null(class_name, class_loader, protection_domain, CHECK_0);
  }
}

klassOop SystemDictionary::resolve_or_null(symbolHandle class_name, TRAPS) {  
  return resolve_or_null(class_name, NULL, NULL, THREAD);
}

// Forwards to resolve_instance_class_or_null

klassOop SystemDictionary::resolve_array_class_or_null(symbolHandle class_name,
                                                       Handle class_loader, 
                                                       Handle protection_domain,
                                                       TRAPS) {  
  assert(FieldType::is_array(class_name()), "must be array");
  jint dimension;
  symbolOop object_key;
  klassOop k = NULL;  
  // dimension and object_key are assigned as a side-effect of this call
  BasicType t = FieldType::get_array_info(class_name(), 
                                          &dimension, 
                                          &object_key, 
                                          CHECK_0);

  if (t == T_OBJECT) {
    symbolHandle h_key(THREAD, object_key);
    // naked oop "k" is OK here -- we assign back into it
    k = SystemDictionary::resolve_instance_class_or_null(h_key, 
                                                         class_loader, 
                                                         protection_domain, 
                                                         CHECK_0);
    if (k != NULL) {
      k = Klass::cast(k)->array_klass(dimension, CHECK_0);
    }
  } else {
    // This block of code is _before_ the assignment to the naked oop
    // "k", as we can come to a safepoint in find_and_add.  This is used
    // for JVMTI to get loaded class information.  As this is the only
    // barrier to late binding agents getting consistent class
    // information, and primitive array class loading is very limited in
    // frequency, always recording this information seems the correct
    // approach.
    // Update: if a Java program cycles through a lot of class loaders
    // and the classes loaded by those class loaders have a lot of
    // primitive arrays, then the perm gen explodes in size. For now,
    // we revert back to Merlin (1.4.0) behavior and only save this
    // information if an agent is currently attached.
    if (JvmtiEnvBase::env_count() > 0) {
      Handle loader = Handle(THREAD, 
           java_lang_ClassLoader::non_reflection_class_loader(class_loader()));
      unsigned int p_hash = placeholders()->compute_hash(class_name, loader);
      int p_index = placeholders()->hash_to_index(p_hash);
      {
        MutexLocker mu(SystemDictionary_lock, THREAD);
        placeholders()->find_and_add(p_index, p_hash, class_name, loader);
      }
    }
    k = Universe::typeArrayKlassObj(t);
    k = typeArrayKlass::cast(k)->array_klass(dimension, CHECK_0);
  }
  return k;
}


// Called from the ClassFileParser (only)
klassOop SystemDictionary::resolve_super_or_fail(symbolHandle child_name,
                                                 symbolHandle class_name,
                                                 Handle class_loader,
                                                 Handle protection_domain,
                                                 TRAPS) {

  // Make sure there's a placeholder for the *child* before resolving.
  // We'll use it for ClassCircularity checks (they occur in
  // resolve_instance_class_or_null), and also for heap verification
  // (every instanceKlass in the heap needs to be in the system dictionary
  // or have a placeholder).
  //
  // We might not already have a placeholder if this child_name was
  // first seen via resolve_from_stream (jni_DefineClass or JVM_DefineClass);
  // the name of the class might not be known until the stream is actually
  // parsed.
  // Bugs 4643874, 4715493

  unsigned int p_hash = placeholders()->compute_hash(child_name, class_loader);
  int p_index = placeholders()->hash_to_index(p_hash);
  {
    MutexLocker mu(SystemDictionary_lock, THREAD);
    placeholders()->find_and_add(p_index, p_hash, child_name, class_loader);
  }

  // Resolve the super class or interface
  klassOop k = SystemDictionary::resolve_or_fail(class_name,
                                                 class_loader,
                                                 protection_domain,
                                                 true,
                                                 CHECK_0);
  return k;
}


void SystemDictionary::validate_protection_domain(instanceKlassHandle klass,
                                                  Handle class_loader,
                                                  Handle protection_domain,
                                                  TRAPS) {
  if(!has_checkPackageAccess()) return;

  // Now we have to call back to java to check if the initating class has access
  JavaValue result(T_VOID);
  if (TraceProtectionDomainVerification) {
    // Print out trace information
    tty->print_cr("Checking package access");
    tty->print(" - class loader:      "); class_loader()->print_value_on(tty);      tty->cr();
    tty->print(" - protection domain: "); protection_domain()->print_value_on(tty); tty->cr();
    tty->print(" - loading:           "); klass()->print_value_on(tty);             tty->cr();
  }
  
  assert(class_loader() != NULL, "should not have non-null protection domain for null classloader");

  KlassHandle system_loader(THREAD, SystemDictionary::classloader_klass());
  JavaCalls::call_special(&result,
                         class_loader,
                         system_loader,
                         vmSymbolHandles::checkPackageAccess_name(),
                         vmSymbolHandles::class_protectiondomain_signature(), 
                         Handle(THREAD, klass->java_mirror()),
                         protection_domain,
                         THREAD);

  if (TraceProtectionDomainVerification) {
    if (HAS_PENDING_EXCEPTION) {
      tty->print_cr(" -> DENIED !!!!!!!!!!!!!!!!!!!!!");
    } else {
     tty->print_cr(" -> granted");
    }
    tty->cr();
  }

  if (HAS_PENDING_EXCEPTION) return; 
    
  // If no exception has been thrown, we have validated the protection domain
  // Insert the protection domain of the initiating class into the set.
  {
    // We recalculate the entry here -- we've called out to java since
    // the last time it was calculated.
    unsigned int d_hash = dictionary()->compute_hash(klass->name(),
                                                     class_loader);
    int d_index = dictionary()->hash_to_index(d_hash);

    MutexLocker mu(SystemDictionary_lock, THREAD);
    { 
      // Note that we have an entry, and entries can be deleted only during GC,
      // so we cannot allow GC to occur while we're holding this entry.

      // We're using a No_Safepoint_Verifier to catch any place where we
      // might potentially do a GC at all.
      // SystemDictionary::do_unloading() asserts that classes are only
      // unloaded at a safepoint.
      No_Safepoint_Verifier nosafepoint;
      dictionary()->add_protection_domain(d_index, d_hash, klass, class_loader,
                                          protection_domain, THREAD);
    }
  }
}

klassOop SystemDictionary::resolve_instance_class_or_null(symbolHandle class_name, Handle class_loader, Handle protection_domain, TRAPS) {
  assert(class_name.not_null() && !FieldType::is_array(class_name()), "invalid class name");
  // First check to see if we should remove wrapping L and ;
  symbolHandle name;    
  if (FieldType::is_obj(class_name())) {
    ResourceMark rm(THREAD);
    // Ignore wrapping L and ;.
    name = oopFactory::new_symbol_handle(class_name()->as_C_string() + 1, class_name()->utf8_length() - 2, CHECK_0);    
  } else {
    name = class_name;
  }

  // UseNewReflection
  // Fix for 4474172; see evaluation for more details
  class_loader = Handle(THREAD, java_lang_ClassLoader::non_reflection_class_loader(class_loader()));

  // Do lookup to see if class already exist and the protection domain
  // has the right access
  unsigned int d_hash = dictionary()->compute_hash(name, class_loader);
  int d_index = dictionary()->hash_to_index(d_hash);
  klassOop probe = dictionary()->find(d_index, d_hash, name, class_loader,
                                      protection_domain, THREAD);
  if (probe != NULL) return probe;

  unsigned int p_hash = placeholders()->compute_hash(name, class_loader);
  int p_index = placeholders()->hash_to_index(p_hash);

  // Class is not is SystemDictionary so we have to do loading.
  // Make sure we are synchronized on the class loader before we proceed
  ObjectLocker ol(compute_loader_lock_object(class_loader, THREAD), THREAD);

  // Check again (after locking) if class already exist in SystemDictionary
  bool class_has_been_loaded   = false;
  bool throw_circularity_error = false;
  instanceKlassHandle k;

  {           
    MutexLocker mu(SystemDictionary_lock, THREAD);  
    klassOop check = find_class(d_index, d_hash, name, class_loader);
    if (check != NULL) {
      // Klass is already loaded, so just return it
      class_has_been_loaded = true;
      k = instanceKlassHandle(THREAD, check);
    } else {
      symbolOop ph_check = placeholders()->find_and_add(p_index, p_hash,
                                                        name, class_loader);
      if (ph_check != NULL) {
        throw_circularity_error = true;
      }
    }
  }

  // Throw error now if needed (cannot throw while holding
  // SystemDictionary_lock because of rank ordering)
  if (throw_circularity_error) {
   ResourceMark rm(THREAD);
   THROW_MSG_0(vmSymbols::java_lang_ClassCircularityError(), name->as_C_string());
  }

  if (!class_has_been_loaded) {
    // Do actual loading
    k = load_instance_class(name, class_loader, THREAD);

    // If everything was OK (no exceptions, no null return value), and
    // class_loader is NOT the defining loader, do a little more bookkeeping.
    if (!HAS_PENDING_EXCEPTION && !k.is_null() && 
        k->class_loader() != class_loader()) {

      check_constraints(d_index, d_hash, k, class_loader, THREAD);

      // Need to check for a PENDING_EXCEPTION again; check_constraints
      // can throw and doesn't use the CHECK macro.
      if (!HAS_PENDING_EXCEPTION) {
        { // Grabbing the Compile_lock prevents systemDictionary updates
          // during compilations. 
          MutexLocker mu(Compile_lock, THREAD);      
          update_dictionary(d_index, d_hash, p_index, p_hash,
                            k(), class_loader, THREAD);
        }
        if (JvmtiExport::should_post_class_load()) {
          Thread *thread = THREAD;
          assert(thread->is_Java_thread(), "thread->is_Java_thread()");
          JvmtiExport::post_class_load((JavaThread *) thread, k());
        }
      }
    }
    if (HAS_PENDING_EXCEPTION || k.is_null()) {
      // No success, remove place holder and return
      {
        MutexLocker mu(SystemDictionary_lock, THREAD);
        placeholders()->find_and_remove(p_index, p_hash, name, class_loader);
      }
      return NULL;
    }
  }

#ifdef ASSERT
  {
    Handle loader (THREAD, k->class_loader());
    MutexLocker mu(SystemDictionary_lock, THREAD);  
    oop kk = find_class_or_placeholder(name, loader);
    assert(kk == k(), "should be present in dictionary");
  }
#endif

  // return if the protection domain in NULL
  if (protection_domain() == NULL) return k();

  // Check the protection domain has the right access 
  {
    MutexLocker mu(SystemDictionary_lock, THREAD);  
    // Note that we have an entry, and entries can be deleted only during GC,
    // so we cannot allow GC to occur while we're holding this entry.
    // We're using a No_Safepoint_Verifier to catch any place where we
    // might potentially do a GC at all.
    // SystemDictionary::do_unloading() asserts that classes are only
    // unloaded at a safepoint.
    No_Safepoint_Verifier nosafepoint;
    if (dictionary()->is_valid_protection_domain(d_index, d_hash, name,
                                                 class_loader,
                                                 protection_domain)) {
      return k();
    }
  }

  // Verify protection domain. If it fails an exception is thrown
  validate_protection_domain(k, class_loader, protection_domain, CHECK_(klassOop(NULL)));

  return k();
}


// This routine does not lock the system dictionary.
//
// Since readers don't hold a lock, we must make sure that system
// dictionary entries are only removed at a safepoint (when only one
// thread is running), and are added to in a safe way (all links must
// be updated in an MT-safe manner).
//
// Callers should be aware that an entry could be added just after
// _dictionary->bucket(index) is read here, so the caller will not see
// the new entry.

klassOop SystemDictionary::find(symbolHandle class_name,
                                Handle class_loader, 
                                Handle protection_domain,
                                TRAPS) {

  // Note that we have an entry, and entries can be deleted only during GC,
  // so we cannot allow GC to occur while we're holding this entry.
  // We're using a No_Safepoint_Verifier to catch any place where we
  // might potentially do a GC at all.
  // SystemDictionary::do_unloading() asserts that classes are only
  // unloaded at a safepoint.
  No_Safepoint_Verifier nosafepoint;

  unsigned int d_hash = dictionary()->compute_hash(class_name, class_loader);
  int d_index = dictionary()->hash_to_index(d_hash);
  return dictionary()->find(d_index, d_hash, class_name, class_loader,
                            protection_domain, THREAD);
}


// Look for a loaded instance or array klass by name.  Do not do any loading.
// return NULL in case of error.
klassOop SystemDictionary::find_instance_or_array_klass(symbolHandle class_name,
                                                        Handle class_loader,
							Handle protection_domain,
                                                        TRAPS) {
  klassOop k = NULL;
  assert(class_name() != NULL, "class name must be non NULL");
  if (FieldType::is_array(class_name())) {
    // The name refers to an array.  Parse the name.
    jint dimension;
    symbolOop object_key;

    // dimension and object_key are assigned as a side-effect of this call
    BasicType t = FieldType::get_array_info(class_name(), &dimension,
					    &object_key, CHECK_(NULL));
    if (t != T_OBJECT) {
      k = Universe::typeArrayKlassObj(t);
    } else {
      symbolHandle h_key(THREAD, object_key);
      k = SystemDictionary::find(h_key, class_loader, protection_domain, THREAD);
    }
    if (k != NULL) {
      k = Klass::cast(k)->array_klass_or_null(dimension);
    }
  } else {
    k = find(class_name, class_loader, protection_domain, THREAD);
  }
  return k;
}

// Note: this method is much like resolve_from_stream, but
// updates no supplemental data structures.
// TODO consolidate the two methods with a helper routine?
klassOop SystemDictionary::parse_stream(symbolHandle class_name,
                                        Handle class_loader,
                                        Handle protection_domain,
                                        ClassFileStream* st,
                                        TRAPS) {
  symbolHandle parsed_name;

  // Parse the stream. Note that we do this even though this klass might
  // already be present in the SystemDictionary, otherwise we would not
  // throw potential ClassFormatErrors.
  //
  // Note: "name" is updated.
  // Further note:  a placeholder will be added for this class when
  //   super classes are loaded (resolve_super_or_fail). We expect this
  //   to be called for all classes but java.lang.Object; and we preload
  //   java.lang.Object through resolve_or_fail, not this path.

  instanceKlassHandle k = ClassFileParser(st).parseClassFile(class_name,
                                                             class_loader,
                                                             protection_domain,
                                                             parsed_name,
                                                             THREAD);

  // We don't redefine the class, so we just need to clean up whether there
  // was an error or not (don't want to modify any system dictionary
  // data structures).
  // Parsed name could be null if we threw an error before we got far
  // enough along to parse it -- in that case, there is nothing to clean up.
  if (!parsed_name.is_null()) {
    unsigned int p_hash = placeholders()->compute_hash(parsed_name, 
                                                       class_loader);
    int p_index = placeholders()->hash_to_index(p_hash);
    MutexLocker mu(SystemDictionary_lock, THREAD);
    placeholders()->find_and_remove(p_index, p_hash, parsed_name, class_loader);
  }

  return k();
}

// Add a klass to the system from a stream (called by jni_DefineClass and
// JVM_DefineClass).
// Note: class_name can be NULL. In that case we do not know the name of 
// the class until we have parsed the stream.

klassOop SystemDictionary::resolve_from_stream(symbolHandle class_name, 
                                               Handle class_loader, 
                                               Handle protection_domain, 
                                               ClassFileStream* st, 
                                               TRAPS) {
  // Make sure we are synchronized on the class loader before we initiate 
  // loading.
  ObjectLocker ol(compute_loader_lock_object(class_loader, THREAD), THREAD);

  symbolHandle parsed_name;

  // Parse the stream. Note that we do this even though this klass might 
  // already be present in the SystemDictionary, otherwise we would not 
  // throw potential ClassFormatErrors.
  //
  // Note: "name" is updated.
  // Further note:  a placeholder will be added for this class when
  //   super classes are loaded (resolve_super_or_fail). We expect this
  //   to be called for all classes but java.lang.Object; and we preload
  //   java.lang.Object through resolve_or_fail, not this path.

  instanceKlassHandle k = ClassFileParser(st).parseClassFile(class_name, 
                                                             class_loader, 
                                                             protection_domain,
                                                             parsed_name,
                                                             THREAD);


  if (!HAS_PENDING_EXCEPTION) {
    assert(!parsed_name.is_null(), "Sanity");
    assert(class_name.is_null() || class_name() == parsed_name(), 
           "name mismatch");
    // Verification prevents us from creating names with dots in them, this
    // asserts that that's the case.
    assert(is_internal_format(parsed_name),
           "external class name format used internally");

    // Add class just loaded
    define_instance_class(k, THREAD);
  }

  // If parsing the class file or define_instance_class failed, we
  // need to remove the placeholder added on our behalf. But we
  // must make sure parsed_name is valid first (it won't be if we had
  // a verification error before the class was parsed far enough to
  // find the name).
  if (HAS_PENDING_EXCEPTION && !parsed_name.is_null()) {
    unsigned int p_hash = placeholders()->compute_hash(parsed_name, 
                                                       class_loader);
    int p_index = placeholders()->hash_to_index(p_hash);
    MutexLocker mu(SystemDictionary_lock, THREAD);
    placeholders()->find_and_remove(p_index, p_hash, parsed_name, class_loader);
    return NULL;
  }

  // Make sure that we didn't leave a place holder in the
  // SystemDictionary; this is only done on success
  debug_only( {
    if (!HAS_PENDING_EXCEPTION) {
      assert(!parsed_name.is_null(), "parsed_name is still null?");
      symbolHandle h_name   (THREAD, k->name());
      Handle h_loader (THREAD, k->class_loader());

      MutexLocker mu(SystemDictionary_lock, THREAD);

      oop check = find_class_or_placeholder(parsed_name, class_loader);
      assert(check == k(), "should be present in the dictionary");

      oop check2 = find_class_or_placeholder(h_name, h_loader);
      assert(check == check2, "name inconsistancy in SystemDictionary");
    }
  } );

  return k();
}


void SystemDictionary::set_shared_dictionary(HashtableBucket* t, int length,
                                             int number_of_entries) {
  assert(length == _nof_buckets * sizeof(HashtableBucket),
         "bad shared dictionary size.");
  _shared_dictionary = new Dictionary(_nof_buckets, t, number_of_entries);
}


// If there is a shared dictionary, then find the entry for the
// given shared system class, if any.

klassOop SystemDictionary::find_shared_class(symbolHandle class_name) {
  if (shared_dictionary() != NULL) {
    unsigned int d_hash = dictionary()->compute_hash(class_name, NULL);
    int d_index = dictionary()->hash_to_index(d_hash);
    return shared_dictionary()->find_shared_class(d_index, d_hash, class_name);
  } else {
    return NULL;
  }
}


// Load a class from the shared spaces (found through the shared system
// dictionary).  Force the superclass and all interfaces to be loaded.
// Update the class definition to include sibling classes and no
// subclasses (yet).  [Classes in the shared space are not part of the
// object hierarchy until loaded.]

instanceKlassHandle SystemDictionary::load_shared_class(
                 symbolHandle class_name, Handle class_loader, TRAPS) {
  instanceKlassHandle ik = find_shared_class(class_name);
  return load_shared_class(ik, class_loader, THREAD);
}

instanceKlassHandle SystemDictionary::load_shared_class(
                 instanceKlassHandle ik, Handle class_loader, TRAPS) {
  assert(class_loader.is_null(), "non-null classloader for shared class?");
  if (ik.not_null()) {
    ik->set_subklass(NULL);
    ik->set_next_sibling(NULL);

    // Found the class, now load the superclass and interfaces.  If they
    // are shared, add them to the main system dictionary and reset
    // their hierarchy references (supers, subs, and interfaces).

    if (ik->super() != NULL) {
      resolve_instance_class_or_null(ik->super()->klass_part()->name(),
                                     class_loader, NULL, THREAD);
    }

    objArrayHandle interfaces = ik->local_interfaces();
    int num_interfaces = interfaces->length();
    for (int index = 0; index < num_interfaces; index++) {
      klassOop k = klassOop(interfaces->obj_at(index));

      // Note: can not use instanceKlass::cast here because
      // interfaces' instanceKlass's C++ vtbls haven't been
      // reinitialized yet (they will be once the interface classes
      // are loaded)
      symbolOop name = k->klass_part()->name();

      resolve_instance_class_or_null(name, class_loader, NULL, THREAD);
    }

    // Adjust methods to recover missing data.  They need addresses for
    // interpreter entry points and their default native method address
    // must be reset.

    objArrayHandle methods = ik->methods();
    int num_methods = methods->length();
    for (int index2 = 0; index2 < num_methods; ++index2) {
      methodOop m = methodOop(methods->obj_at(index2));
      AbstractInterpreter::MethodKind kind;
      kind = (AbstractInterpreter::MethodKind)(m->interpreter_kind());
      address ep = AbstractInterpreter::entry_for_kind(kind);
      m->set_interpreter_entry(ep);
      if (m->is_native()) {
        m->clear_native_function();
      }
    }

    if (TraceClassLoading) {
      ResourceMark rm;
      tty->print("[Loaded %s", ik->external_name());
      tty->print(" from shared objects file");
      tty->print_cr("]");
    }
    // notify a class loaded from shared object
    ClassLoadingService::notify_class_loaded(instanceKlass::cast(ik()), 
                                             true /* shared class */);
  }
  return ik;
}



instanceKlassHandle SystemDictionary::load_instance_class(symbolHandle class_name, Handle class_loader, TRAPS) {
  instanceKlassHandle nh = instanceKlassHandle(); // null Handle
  if (class_loader.is_null()) {
    // Search the shared system dictionary for classes preloaded into the
    // shared spaces.
    instanceKlassHandle k;
    k = load_shared_class(class_name, class_loader, THREAD);

    if (k == NULL) {
      // Use VM class loader
      k = ClassLoader::load_classfile(class_name, CHECK_(nh));
    }

    if (!k.is_null()) {
      define_instance_class(k, CHECK_(nh));
    }
    return k;
  } else {
    // Use user specified class loader to load class. Call loadClass operation on class_loader.
    ResourceMark rm(THREAD);
      
    Handle s = java_lang_String::create_from_symbol(class_name, CHECK_(nh));
    // Translate to external class name format, i.e., convert '/' chars to '.'
    Handle string = java_lang_String::externalize_classname(s, CHECK_(nh));

    JavaValue result(T_OBJECT);

    KlassHandle spec_klass (THREAD, SystemDictionary::classloader_klass());
    if (has_loadClassInternal()) {
      JavaCalls::call_special(&result, 
                              class_loader, 
                              spec_klass,
                              vmSymbolHandles::loadClassInternal_name(),
                              vmSymbolHandles::string_class_signature(), 
                              string,
                              CHECK_(nh));
    } else {
      JavaCalls::call_virtual(&result, 
                              class_loader, 
                              spec_klass,
                              vmSymbolHandles::loadClass_name(),
                              vmSymbolHandles::string_class_signature(), 
                              string,
                              CHECK_(nh));
    }

    assert(result.get_type() == T_OBJECT, "just checking");
    oop obj = (oop) result.get_jobject();

    if (obj != NULL) {      
      instanceKlassHandle k = 
                instanceKlassHandle(THREAD, java_lang_Class::as_klassOop(obj));
      // For user defined Java class loaders, check that the name returned is
      // the same as that requested.  This check is done for the bootstrap
      // loader when parsing the class file.
      if (class_name() == k->name()) {
        return k;
      }
    }
    // Class is not found or has the wrong name, return NULL
    return nh;
  }
}

void SystemDictionary::define_instance_class(instanceKlassHandle k, TRAPS) {

  Handle class_loader_h(THREAD, k->class_loader());

  // Check class-loading constraints. Throw exception if violation is detected.
  // Grabs and releases SystemDictionary_lock
  unsigned int d_hash = dictionary()->compute_hash(k->name(),k->class_loader());
  int d_index = dictionary()->hash_to_index(d_hash);
  check_constraints(d_index, d_hash, k, class_loader_h, CHECK);

  // Register class just loaded with class loader (placed in Vector)
  // Note we do this before updating the dictionary, as this can
  // fail with an OutOfMemoryError (if it does, we will *not* put this
  // class in the dictionary and will not update the class hierarchy).
  if (k->class_loader() != NULL) {
    methodHandle m(THREAD, Universe::loader_addClass_method());
    JavaValue result(T_VOID);
    JavaCallArguments args(class_loader_h);
    args.push_oop(Handle(THREAD, k->java_mirror()));
    JavaCalls::call(&result, m, &args, CHECK);
  }

  // Add the new class. We need recompile lock during update of CHA.
  {
    symbolOop name = k->name();
    oop class_loader = k->class_loader();
    unsigned int d_hash = dictionary()->compute_hash(name, class_loader);
    int d_index = dictionary()->hash_to_index(d_hash);
    unsigned int p_hash = placeholders()->compute_hash(name, class_loader);
    int p_index = placeholders()->hash_to_index(p_hash);

    MutexLocker mu_r(Compile_lock, THREAD);                    

    // Add to class hierarchy, initialize vtables, and do possible
    // deoptimizations.
    add_to_hierarchy(k, CHECK); // No exception, but can block

    // Add to systemDictionary - so other classes can see it.
    // Grabs and releases SystemDictionary_lock
    update_dictionary(d_index, d_hash, p_index, p_hash,
                      k, class_loader_h, THREAD);
  }

  k->eager_initialize(THREAD);

  // notify jvmti
  if (JvmtiExport::should_post_class_load()) {
      assert(THREAD->is_Java_thread(), "thread->is_Java_thread()");
      JvmtiExport::post_class_load((JavaThread *) THREAD, k());

  }
  // notify jvmpi
  if (jvmpi::is_event_enabled(JVMPI_EVENT_CLASS_LOAD)) {
    jvmpi::post_class_load_event(Klass::cast(k())->java_mirror());
  }
}

Handle SystemDictionary::compute_loader_lock_object(Handle class_loader, TRAPS) {
  // If class_loader is NULL we synchronize on _system_loader_lock_obj
  if (class_loader.is_null()) {
    return Handle(THREAD, _system_loader_lock_obj);
  } else {
    return class_loader;
  }
}


// ----------------------------------------------------------------------------
// Lookup

klassOop SystemDictionary::find_class(int index, unsigned int hash,
                                      symbolHandle class_name,
                                      Handle class_loader) {
  assert_locked_or_safepoint(SystemDictionary_lock);
  assert (index == dictionary()->index_for(class_name, class_loader),
          "incorrect index?");

  klassOop k = dictionary()->find_class(index, hash, class_name, class_loader);
  return k;
}


// Basic find on classes in the midst of being loaded
symbolOop SystemDictionary::find_placeholder(int index, unsigned int hash,
                                             symbolHandle class_name,
                                             Handle class_loader) {
  assert_locked_or_safepoint(SystemDictionary_lock);

  return placeholders()->find_entry(index, hash, class_name, class_loader);
}


#ifndef PRODUCT
// Used for assertions and verification only
oop SystemDictionary::find_class_or_placeholder(symbolHandle class_name, 
                                                Handle class_loader) {
  assert_locked_or_safepoint(SystemDictionary_lock);
  symbolOop class_name_ = class_name();
  oop class_loader_ = class_loader();

  // First look in the loaded class array
  unsigned int d_hash = dictionary()->compute_hash(class_name, class_loader);
  int d_index = dictionary()->hash_to_index(d_hash);
  oop lookup = find_class(d_index, d_hash, class_name, class_loader);

  if (lookup == NULL) {
    // Next try the placeholders
    unsigned int p_hash = placeholders()->compute_hash(class_name,class_loader);
    int p_index = placeholders()->hash_to_index(p_hash);
    lookup = find_placeholder(p_index, p_hash, class_name, class_loader);
  }

  return lookup;
}
#endif


// Get the next class in the diictionary.
klassOop SystemDictionary::try_get_next_class() {
  return dictionary()->try_get_next_class();
}


// ----------------------------------------------------------------------------
// Update hierachy. This is done before the new klass has been added to the SystemDictionary. The Recompile_lock
// is held, to ensure that the compiler is not using the class hierachy, and that deoptimization will kick in
// before a new class is used.

void SystemDictionary::add_to_hierarchy(instanceKlassHandle k, TRAPS) {
  assert(k.not_null(), "just checking");
  // Link into hierachy. Make sure the vtables are initialized before linking into 
  k->append_to_sibling_list();                    // add to superklass/sibling list
  k->process_interfaces(THREAD);                  // handle all "implements" declarations  
  k->set_init_state(instanceKlass::loaded);
  // Now flush all code that depended on old class hierarchy.
  // Note: must be done *after* linking k into the hierarchy (was bug 12/9/97)
  // Also, first reinitialize vtable because it may have gotten out of synch 
  // while the new class wasn't connected to the class hierarchy.     
  NOT_CORE(Universe::flush_dependents_on(k);)
}


// ----------------------------------------------------------------------------
// GC support

// Following roots during mark-sweep is separated in two phases. 
//
// The first phase follows preloaded classes and all other system 
// classes, since these will never get unloaded anyway.
//
// The second phase removes (unloads) unreachable classes from the
// system dictionary and follows the remaining classes' contents.

void SystemDictionary::always_strong_oops_do(OopClosure* blk) {
  // Follow preloaded classes/mirrors and system loader object
  blk->do_oop(&_java_system_loader);
  preloaded_oops_do(blk);
  always_strong_classes_do(blk);
}


void SystemDictionary::always_strong_classes_do(OopClosure* blk) {
  // Follow all system classes and temporary placeholders in dictionary
  dictionary()->always_strong_classes_do(blk);
  
  // Placeholders. These are *always* strong roots, as they
  // represent classes we're actively loading.
  placeholders_do(blk);  

  // Loader constraints. We must keep the symbolOop used in the name alive.
  constraints()->always_strong_classes_do(blk);
}


void SystemDictionary::placeholders_do(OopClosure* blk) {
  placeholders()->oops_do(blk);
}


bool SystemDictionary::do_unloading(BoolObjectClosure* is_alive,
                                    OopClosure* keep_alive) {
  bool result = dictionary()->do_unloading(is_alive, keep_alive);
  constraints()->purge_loader_constraints(is_alive);
  return result;
}


void SystemDictionary::oops_do(OopClosure* f) {
  // Adjust preloaded classes/mirrors and system loader object
  f->do_oop(&_java_system_loader);
  preloaded_oops_do(f);

  // Adjust dictionary
  dictionary()->oops_do(f);

  // Partially loaded classes
  placeholders()->oops_do(f);

  // Adjust constraint table
  constraints()->oops_do(f);
}


void SystemDictionary::preloaded_oops_do(OopClosure* f) {
  f->do_oop((oop*) &_string_klass);
  f->do_oop((oop*) &_object_klass);
  f->do_oop((oop*) &_class_klass);
  f->do_oop((oop*) &_cloneable_klass);
  f->do_oop((oop*) &_classloader_klass);
  f->do_oop((oop*) &_serializable_klass);
  f->do_oop((oop*) &_system_klass);

  f->do_oop((oop*) &_throwable_klass);
  f->do_oop((oop*) &_error_klass);
  f->do_oop((oop*) &_threaddeath_klass);
  f->do_oop((oop*) &_exception_klass);
  f->do_oop((oop*) &_runtime_exception_klass);
  f->do_oop((oop*) &_classNotFoundException_klass);
  f->do_oop((oop*) &_noClassDefFoundError_klass);
  f->do_oop((oop*) &_classCastException_klass);
  f->do_oop((oop*) &_arrayStoreException_klass);
  f->do_oop((oop*) &_outOfMemoryError_klass);
  f->do_oop((oop*) &_StackOverflowError_klass);
  f->do_oop((oop*) &_protectionDomain_klass);
  f->do_oop((oop*) &_AccessControlContext_klass);

  f->do_oop((oop*) &_reference_klass);
  f->do_oop((oop*) &_soft_reference_klass);
  f->do_oop((oop*) &_weak_reference_klass);
  f->do_oop((oop*) &_final_reference_klass);
  f->do_oop((oop*) &_phantom_reference_klass);
  f->do_oop((oop*) &_finalizer_klass);
  
  f->do_oop((oop*) &_thread_klass);
  f->do_oop((oop*) &_threadGroup_klass);
  f->do_oop((oop*) &_properties_klass);      
  f->do_oop((oop*) &_reflect_accessible_object_klass);      
  f->do_oop((oop*) &_reflect_field_klass);      
  f->do_oop((oop*) &_reflect_method_klass);      
  f->do_oop((oop*) &_reflect_constructor_klass);      
  f->do_oop((oop*) &_reflect_magic_klass);
  f->do_oop((oop*) &_reflect_method_accessor_klass);
  f->do_oop((oop*) &_reflect_constructor_accessor_klass);
  f->do_oop((oop*) &_reflect_delegating_classloader_klass);
  f->do_oop((oop*) &_reflect_constant_pool_klass);

  f->do_oop((oop*) &_stringBuffer_klass);
  f->do_oop((oop*) &_vector_klass);
  f->do_oop((oop*) &_hashtable_klass);

  f->do_oop((oop*) &_stackTraceElement_klass);

  f->do_oop((oop*) &_java_nio_Buffer_klass);

  f->do_oop((oop*) &_sun_misc_AtomicLongCSImpl_klass);

  f->do_oop((oop*) &_boolean_klass);
  f->do_oop((oop*) &_char_klass);
  f->do_oop((oop*) &_float_klass);
  f->do_oop((oop*) &_double_klass);
  f->do_oop((oop*) &_byte_klass);
  f->do_oop((oop*) &_short_klass);
  f->do_oop((oop*) &_int_klass);
  f->do_oop((oop*) &_long_klass);
  {
    for (int i = 0; i < T_VOID+1; i++) {
      if (_box_klasses[i] != NULL) {
	assert(i >= T_BOOLEAN, "checking");
	f->do_oop((oop*) &_box_klasses[i]);
      }
    }
  }

  f->do_oop((oop*) &_int_mirror);
  f->do_oop((oop*) &_float_mirror);
  f->do_oop((oop*) &_double_mirror);
  f->do_oop((oop*) &_byte_mirror);
  f->do_oop((oop*) &_bool_mirror);
  f->do_oop((oop*) &_char_mirror);
  f->do_oop((oop*) &_long_mirror);
  f->do_oop((oop*) &_short_mirror);
  f->do_oop((oop*) &_void_mirror);
  {
    for (int i = 0; i < T_VOID+1; i++) {
      if (_mirrors[i] != NULL) {
	assert(i >= T_BOOLEAN, "checking");
	f->do_oop((oop*) &_mirrors[i]);
      }
    }
  }

  f->do_oop((oop*) &_system_loader_lock_obj);
}


//   Just the classes from declaring class loaders
// Don't iterate over placeholders
void SystemDictionary::classes_do(void f(klassOop)) {
  dictionary()->classes_do(f);
}


//   All classes, and their class loaders
// Don't iterate over placeholders
void SystemDictionary::classes_do(void f(klassOop, oop)) {
  dictionary()->classes_do(f);
}


//   Do all arrays of primitive types
//  These are stored in the system dictionary as placeholders
//  and are used by jvmti
void SystemDictionary::prim_array_classes_do(void f(klassOop, oop)) {
  placeholders()->prim_array_classes_do(f);
}


void SystemDictionary::methods_do(void f(methodOop)) {
  dictionary()->methods_do(f);
}


// ----------------------------------------------------------------------------
// Initialization

void SystemDictionary::initialize(TRAPS) {
  // Allocate arrays
  assert(dictionary() == NULL,
         "SystemDictionary should only be initialized once");
  _dictionary = new Dictionary(_nof_buckets);
  _placeholders = new PlaceholderTable(_nof_buckets);
  _number_of_modifications = 0;
  _loader_constraints = new LoaderConstraintTable(_loader_constraint_size);
  // Allocate private object used as system class loader lock
  _system_loader_lock_obj = oopFactory::new_system_objArray(0, CHECK);
  // Initialize basic classes
  initialize_preloaded_classes(CHECK);
  initialize_basic_type_mirrors(CHECK);
}


void SystemDictionary::initialize_preloaded_classes(TRAPS) {
  assert(_object_klass == NULL, "preloaded classes should only be initialized once");
  // Preload commonly used klasses
  _object_klass            = resolve_or_fail(vmSymbolHandles::java_lang_Object(),                true, CHECK);
  _string_klass            = resolve_or_fail(vmSymbolHandles::java_lang_String(),                true, CHECK);  
  _class_klass             = resolve_or_fail(vmSymbolHandles::java_lang_Class(),                 true, CHECK);
  debug_only(instanceKlass::verify_class_klass_nonstatic_oop_maps(_class_klass));
  // Fixup mirrors for classes loaded before java.lang.Class
  Universe::fixup_mirrors(CHECK);

  _cloneable_klass         = resolve_or_fail(vmSymbolHandles::java_lang_Cloneable(),             true, CHECK);
  _classloader_klass       = resolve_or_fail(vmSymbolHandles::java_lang_ClassLoader(),           true, CHECK);
  _serializable_klass      = resolve_or_fail(vmSymbolHandles::java_io_Serializable(),            true, CHECK);
  _system_klass            = resolve_or_fail(vmSymbolHandles::java_lang_System(),                true, CHECK);  

  _throwable_klass         = resolve_or_fail(vmSymbolHandles::java_lang_Throwable(),             true, CHECK);
  _error_klass             = resolve_or_fail(vmSymbolHandles::java_lang_Error(),                 true, CHECK);
  _threaddeath_klass       = resolve_or_fail(vmSymbolHandles::java_lang_ThreadDeath(),           true, CHECK);
  _exception_klass         = resolve_or_fail(vmSymbolHandles::java_lang_Exception(),             true, CHECK);
  _runtime_exception_klass = resolve_or_fail(vmSymbolHandles::java_lang_RuntimeException(),      true, CHECK);
  _protectionDomain_klass  = resolve_or_fail(vmSymbolHandles::java_security_ProtectionDomain(),  true, CHECK);
  _AccessControlContext_klass = resolve_or_fail(vmSymbolHandles::java_security_AccessControlContext(),  true, CHECK);
  _classNotFoundException_klass = resolve_or_fail(vmSymbolHandles::java_lang_ClassNotFoundException(),  true, CHECK);
  _noClassDefFoundError_klass   = resolve_or_fail(vmSymbolHandles::java_lang_NoClassDefFoundError(),  true, CHECK);  
  _classCastException_klass = resolve_or_fail(vmSymbolHandles::java_lang_ClassCastException(),   true, CHECK);  
  _arrayStoreException_klass = resolve_or_fail(vmSymbolHandles::java_lang_ArrayStoreException(),   true, CHECK);  
  _outOfMemoryError_klass  = resolve_or_fail(vmSymbolHandles::java_lang_OutOfMemoryError(),      true, CHECK);  
  _StackOverflowError_klass = resolve_or_fail(vmSymbolHandles::java_lang_StackOverflowError(),   true, CHECK);  

  // Preload ref klasses and set reference types
  _reference_klass         = resolve_or_fail(vmSymbolHandles::java_lang_ref_Reference(),         true, CHECK);
  instanceKlass::cast(_reference_klass)->set_reference_type(REF_OTHER);
  instanceRefKlass::update_nonstatic_oop_maps(_reference_klass);

  _soft_reference_klass    = resolve_or_fail(vmSymbolHandles::java_lang_ref_SoftReference(),     true, CHECK);
  instanceKlass::cast(_soft_reference_klass)->set_reference_type(REF_SOFT);
  _weak_reference_klass    = resolve_or_fail(vmSymbolHandles::java_lang_ref_WeakReference(),     true, CHECK);
  instanceKlass::cast(_weak_reference_klass)->set_reference_type(REF_WEAK);
  _final_reference_klass   = resolve_or_fail(vmSymbolHandles::java_lang_ref_FinalReference(),    true, CHECK);
  instanceKlass::cast(_final_reference_klass)->set_reference_type(REF_FINAL);
  _phantom_reference_klass = resolve_or_fail(vmSymbolHandles::java_lang_ref_PhantomReference(),  true, CHECK);
  instanceKlass::cast(_phantom_reference_klass)->set_reference_type(REF_PHANTOM);
  _finalizer_klass         = resolve_or_fail(vmSymbolHandles::java_lang_ref_Finalizer(),         true, CHECK);

  _thread_klass           = resolve_or_fail(vmSymbolHandles::java_lang_Thread(),                true, CHECK);
  _threadGroup_klass      = resolve_or_fail(vmSymbolHandles::java_lang_ThreadGroup(),           true, CHECK);
  _properties_klass       = resolve_or_fail(vmSymbolHandles::java_util_Properties(),            true, CHECK);  
  _reflect_accessible_object_klass = resolve_or_fail(vmSymbolHandles::java_lang_reflect_AccessibleObject(),  true, CHECK);  
  _reflect_field_klass    = resolve_or_fail(vmSymbolHandles::java_lang_reflect_Field(),         true, CHECK);  
  _reflect_method_klass   = resolve_or_fail(vmSymbolHandles::java_lang_reflect_Method(),        true, CHECK);  
  _reflect_constructor_klass = resolve_or_fail(vmSymbolHandles::java_lang_reflect_Constructor(),   true, CHECK);  
  // Universe::is_gte_jdk14x_version() is not set up by this point.
  // It's okay if these turn out to be NULL in non-1.4 JDKs.
  _reflect_magic_klass    = resolve_or_null(vmSymbolHandles::sun_reflect_MagicAccessorImpl(),         CHECK);
  _reflect_method_accessor_klass = resolve_or_null(vmSymbolHandles::sun_reflect_MethodAccessorImpl(),     CHECK);
  _reflect_constructor_accessor_klass = resolve_or_null(vmSymbolHandles::sun_reflect_ConstructorAccessorImpl(),     CHECK);
  _reflect_delegating_classloader_klass = resolve_or_null(vmSymbolHandles::sun_reflect_DelegatingClassLoader(),     CHECK);
  _reflect_constant_pool_klass = resolve_or_null(vmSymbolHandles::sun_reflect_ConstantPool(),         CHECK);

  _vector_klass           = resolve_or_fail(vmSymbolHandles::java_util_Vector(),                true, CHECK);  
  _hashtable_klass        = resolve_or_fail(vmSymbolHandles::java_util_Hashtable(),             true, CHECK);  
  _stringBuffer_klass     = resolve_or_fail(vmSymbolHandles::java_lang_StringBuffer(),          true, CHECK);  

  // It's NULL in non-1.4 JDKs.
  _stackTraceElement_klass = resolve_or_fail(vmSymbolHandles::java_lang_StackTraceElement(),    true, CHECK);

  // Universe::is_gte_jdk14x_version() is not set up by this point.
  // It's okay if this turns out to be NULL in non-1.4 JDKs.
  _java_nio_Buffer_klass   = resolve_or_null(vmSymbolHandles::java_nio_Buffer(),                 CHECK);

  // If this class isn't present, it won't be referenced.
  _sun_misc_AtomicLongCSImpl_klass = resolve_or_null(vmSymbolHandles::sun_misc_AtomicLongCSImpl(),     CHECK);

  // Preload boxing klasses
  _boolean_klass           = resolve_or_fail(vmSymbolHandles::java_lang_Boolean(),               true, CHECK);
  _char_klass              = resolve_or_fail(vmSymbolHandles::java_lang_Character(),             true, CHECK);
  _float_klass             = resolve_or_fail(vmSymbolHandles::java_lang_Float(),                 true, CHECK);
  _double_klass            = resolve_or_fail(vmSymbolHandles::java_lang_Double(),                true, CHECK);
  _byte_klass              = resolve_or_fail(vmSymbolHandles::java_lang_Byte(),                  true, CHECK);
  _short_klass             = resolve_or_fail(vmSymbolHandles::java_lang_Short(),                 true, CHECK);
  _int_klass               = resolve_or_fail(vmSymbolHandles::java_lang_Integer(),               true, CHECK);
  _long_klass              = resolve_or_fail(vmSymbolHandles::java_lang_Long(),                  true, CHECK);

  _box_klasses[T_BOOLEAN] = _boolean_klass;
  _box_klasses[T_CHAR]    = _char_klass;
  _box_klasses[T_FLOAT]   = _float_klass;
  _box_klasses[T_DOUBLE]  = _double_klass;
  _box_klasses[T_BYTE]    = _byte_klass;
  _box_klasses[T_SHORT]   = _short_klass;
  _box_klasses[T_INT]     = _int_klass;
  _box_klasses[T_LONG]    = _long_klass;
  //_box_klasses[T_OBJECT]  = _object_klass;
  //_box_klasses[T_ARRAY]   = _object_klass;

  { // Compute whether we should use loadClass or loadClassInternal when loading classes.
    methodOop method = instanceKlass::cast(classloader_klass())->find_method(vmSymbols::loadClassInternal_name(), vmSymbols::string_class_signature());
    _has_loadClassInternal = (method != NULL);
  }

  { // Compute whether we should use checkPackageAccess or NOT
    methodOop method = instanceKlass::cast(classloader_klass())->find_method(vmSymbols::checkPackageAccess_name(), vmSymbols::class_protectiondomain_signature());
    _has_checkPackageAccess = (method != NULL); 
  }
}

void SystemDictionary::initialize_basic_type_mirrors(TRAPS) { 
  assert(_int_mirror==NULL, "basic type mirrors already initialized");
  _int_mirror     = java_lang_Class::create_basic_type_mirror("int",    CHECK); 
  _float_mirror   = java_lang_Class::create_basic_type_mirror("float",  CHECK);
  _double_mirror  = java_lang_Class::create_basic_type_mirror("double", CHECK); 
  _byte_mirror    = java_lang_Class::create_basic_type_mirror("byte",   CHECK);
  _bool_mirror    = java_lang_Class::create_basic_type_mirror("boolean",CHECK);
  _char_mirror    = java_lang_Class::create_basic_type_mirror("char",   CHECK);
  _long_mirror    = java_lang_Class::create_basic_type_mirror("long",   CHECK);
  _short_mirror   = java_lang_Class::create_basic_type_mirror("short",  CHECK);
  _void_mirror    = java_lang_Class::create_basic_type_mirror("void",   CHECK);

  _mirrors[T_INT]     = _int_mirror;
  _mirrors[T_FLOAT]   = _float_mirror;
  _mirrors[T_DOUBLE]  = _double_mirror;
  _mirrors[T_BYTE]    = _byte_mirror;
  _mirrors[T_BOOLEAN] = _bool_mirror;
  _mirrors[T_CHAR]    = _char_mirror;
  _mirrors[T_LONG]    = _long_mirror;
  _mirrors[T_SHORT]   = _short_mirror;
  _mirrors[T_VOID]    = _void_mirror;
  //_mirrors[T_OBJECT]  = instanceKlass::cast(_object_klass)->java_mirror();
  //_mirrors[T_ARRAY]   = instanceKlass::cast(_object_klass)->java_mirror();
}


// Tells if a given klass is a box (wrapper class, such as java.lang.Integer).
// If so, returns the basic type it holds.  If not, returns T_OBJECT.
BasicType SystemDictionary::box_klass_type(klassOop k) {
  assert(k != NULL, "");
  for (int i = T_BOOLEAN; i < T_VOID+1; i++) {
    if (_box_klasses[i] == k)
      return (BasicType)i;
  }
  return T_OBJECT;
}

// Tells if a given oop is a mirror for a primitive type (such as Integer.TYPE).
// If so, returns the basic type it mirrors.  If not, returns T_OBJECT.
BasicType SystemDictionary::java_mirror_type(oop m) {
  assert(m != NULL, "");
  for (int i = T_BOOLEAN; i < T_VOID+1; i++) {
    if (_mirrors[i] == m)
      return (BasicType)i;
  }
  return T_OBJECT;
}

// Constraints on class loaders. The details of the algorithm can be
// found in the OOPSLA'98 paper "Dynamic Class Loading in the Java
// Virtual Machine" by Sheng Liang and Gilad Bracha.  The basic idea is
// that the system dictionary needs to maintain a set of contraints that
// must be satisfied by all classes in the dictionary.

void SystemDictionary::check_constraints(int d_index, unsigned int d_hash,
                                         instanceKlassHandle k,
                                         Handle class_loader, TRAPS) {
  const char *linkage_error = NULL;
  {
    symbolHandle name (THREAD, k->name());
    MutexLocker mu(SystemDictionary_lock, THREAD);         

    klassOop check = find_class(d_index, d_hash, name, class_loader);
    if (check != NULL) {
      linkage_error = "duplicate class definition: %s";
    }

#ifdef ASSERT
    unsigned int p_hash = placeholders()->compute_hash(name, class_loader);
    int p_index = placeholders()->hash_to_index(p_hash);
    symbolOop ph_check = find_placeholder(p_index, p_hash, name, class_loader);
    assert(ph_check == NULL || ph_check == name(), "invalid symbol");
#endif

    if (linkage_error == NULL) {
      linkage_error = constraints()->check_or_update(k, class_loader, name);
    }
  }

  // Throw error now if needed (cannot throw while holding 
  // SystemDictionary_lock because of rank ordering)

  if (linkage_error) {
    ResourceMark rm(THREAD);
    char message[128];
    jio_snprintf(message, sizeof(message), linkage_error,
                 k->name()->as_C_string());
    THROW_MSG(vmSymbols::java_lang_LinkageError(), message);
  }
}


// Update system dictionary - done after check_constraint and add_to_hierachy 
// have been called.
void SystemDictionary::update_dictionary(int d_index, unsigned int d_hash,
                                         int p_index, unsigned int p_hash,
                                         instanceKlassHandle k, 
                                         Handle class_loader, 
                                         TRAPS) {
  // Compile_lock prevents systemDictionary updates during compilations
  assert_locked_or_safepoint(Compile_lock);
  symbolHandle name (THREAD, k->name());

  MutexLocker mu1(SystemDictionary_lock, THREAD);           

  // Check for a placeholder. If there, remove it and make a
  // new system dictionary entry.
  symbolOop check = find_placeholder(p_index, p_hash, name, class_loader);
  if (check != NULL) {
    placeholders()->remove_entry(p_index, p_hash, name, class_loader);
  }
  klassOop sd_check = find_class(d_index, d_hash, name, class_loader);
  if (sd_check == NULL) {
    dictionary()->add_klass(name, class_loader, k);
    notice_modification();
  }
#ifdef ASSERT
  sd_check = find_class(d_index, d_hash, name, class_loader);
  assert (sd_check != NULL, "should have entry in system dictionary");
  symbolOop ph_check = find_placeholder(p_index, p_hash, name, class_loader);
  assert (ph_check == NULL, "should not have a placeholder entry");
#endif
}


klassOop SystemDictionary::find_constrained_instance_or_array_klass(
                    symbolHandle class_name, Handle class_loader, TRAPS) {

  // First see if it has been loaded directly.
  // Force the protection domain to be null.  (This removes protection checks.)
  Handle no_protection_domain;
  klassOop klass = find_instance_or_array_klass(class_name, class_loader,
                                                no_protection_domain, CHECK_0);
  if (klass != NULL)
    return klass;

  // Now look to see if it has been loaded elsewhere, and is subject to
  // a loader constraint that would require this loader to return the
  // klass that is already loaded.
  return constraints()->find_constrained_klass(class_name, class_loader);
}


void SystemDictionary::add_loader_constraint(symbolHandle class_name,
                                             Handle class_loader1,
                                             Handle class_loader2, TRAPS) {
  unsigned int d_hash1 = dictionary()->compute_hash(class_name, class_loader1);
  int d_index1 = dictionary()->hash_to_index(d_hash1);

  unsigned int d_hash2 = dictionary()->compute_hash(class_name, class_loader2);
  int d_index2 = dictionary()->hash_to_index(d_hash2);

  bool constraints_ok = true;
  {
    MutexLocker mu_s(SystemDictionary_lock, THREAD);

    // Better never do a GC while we're holding these oops
    No_Safepoint_Verifier nosafepoint;

    klassOop klass1 = find_class(d_index1, d_hash1, class_name, class_loader1);
    klassOop klass2 = find_class(d_index2, d_hash2, class_name, class_loader2);
    constraints_ok = constraints()->add_entry(class_name, klass1, class_loader1,
                                              klass2, class_loader2, THREAD);
  }

  // Throw error now if needed (cannot throw while holding
  // SystemDictionary_lock)
  if (!constraints_ok) {
    ResourceMark rm(THREAD);
    char message[128];
    jio_snprintf(message, sizeof(message),
                 "loader constraints violated when linking %s class",
                 class_name()->as_C_string());
    THROW_MSG(vmSymbols::java_lang_LinkageError(), message);
  }
}


// Make sure all class components (including arrays) in the given
// signature will be resolved to the same class in both loaders.
//
void SystemDictionary::check_signature_loaders(symbolHandle signature,
                                               Handle loader1, Handle loader2,
                                               bool is_method, TRAPS)  {
  // Nothing to do if loaders are the same. 
  if (loader1() == loader2()) {
    return;
  }
  
  SignatureStream sig_strm(signature, is_method);
  while (!sig_strm.is_done()) {
    symbolOop s = sig_strm.as_symbol(CHECK);
    symbolHandle sig (THREAD, s);
    if (sig_strm.is_object()) {
      add_loader_constraint(sig, loader1, loader2, CHECK);
    }
    sig_strm.next();
  }
}


// Since the identity hash code for symbols changes when the symbols are
// moved from the regular perm gen (hash in the mark word) to the shared
// spaces (hash is the address), the classes loaded into the dictionary
// may be in the wrong buckets.

void SystemDictionary::reorder_dictionary() {
  dictionary()->reorder_dictionary();
}


void SystemDictionary::copy_buckets(char** top, char* end) {
  dictionary()->copy_buckets(top, end);
}


void SystemDictionary::copy_table(char** top, char* end) {
  dictionary()->copy_table(top, end);
}


void SystemDictionary::reverse() {
  dictionary()->reverse();
}

int SystemDictionary::number_of_classes() {
  return dictionary()->number_of_entries();
}


// ----------------------------------------------------------------------------
#ifndef PRODUCT

void SystemDictionary::print() {
  dictionary()->print();

  // Placeholders
  GCMutexLocker mu(SystemDictionary_lock);
  placeholders()->print();
}


void SystemDictionary::verify() {
  guarantee(dictionary() != NULL, "Verify of system dictionary failed");
  guarantee(constraints() != NULL,
            "Verify of loader constraints failed");
  guarantee(dictionary()->number_of_entries() >= 0 &&
            placeholders()->number_of_entries() >= 0,
            "Verify of system dictionary failed");

  // Verify dictionary
  dictionary()->verify();

  GCMutexLocker mu(SystemDictionary_lock);
  placeholders()->verify();

  // Verify constraint table
  guarantee(constraints() != NULL, "Verify of loader constraints failed");
  constraints()->verify(dictionary());
}


void SystemDictionary::verify_obj_klass_present(Handle obj,
                                                symbolHandle class_name,
                                                Handle class_loader) {
  GCMutexLocker mu(SystemDictionary_lock);
  oop probe = find_class_or_placeholder(class_name, class_loader);
  if (probe == NULL) {
    probe = SystemDictionary::find_shared_class(class_name);
  }
  guarantee(probe != NULL && 
            (!probe->is_klass() || probe == obj()), 
                     "Loaded klasses should be in SystemDictionary");
}


// statistics code
class ClassStatistics: AllStatic {
 private:
  static int nclasses;        // number of classes
  static int nmethods;        // number of methods
  static int nmethoddata;     // number of methodData    
  static int class_size;      // size of class objects in words
  static int method_size;     // size of method objects in words
  static int debug_size;      // size of debug info in methods
  static int methoddata_size; // size of methodData objects in words

  static void do_class(klassOop k) {
    nclasses++;
    class_size += k->size();
    if (k->klass_part()->oop_is_instance()) {
      instanceKlass* ik = (instanceKlass*)k->klass_part();
      class_size += ik->methods()->size();
      class_size += ik->constants()->size();
      class_size += ik->local_interfaces()->size();
      class_size += ik->transitive_interfaces()->size();
      // We do not have to count implementors, since we only store one!      
      class_size += ik->fields()->size();
    }
  }

  static void do_method(methodOop m) {
    nmethods++;
    method_size += m->size();
    // class loader uses same objArray for empty vectors, so don't count these
    if (m->exception_table()->length() != 0)   method_size += m->exception_table()->size();
    if (m->stackmap_u1()->length() != 0)       method_size += m->stackmap_u1()->size();
    if (m->stackmap_u2()->length() != 0)       method_size += m->stackmap_u2()->size();

#ifndef CORE
    methodDataOop mdo = m->method_data();
    if (mdo) {
      nmethoddata++;
      methoddata_size += mdo->size();
    }
#endif
  }

 public:
  static void print() {
    SystemDictionary::classes_do(do_class);
    SystemDictionary::methods_do(do_method);
    tty->print_cr("Class statistics:");
    tty->print_cr("%d classes (%d bytes)", nclasses, class_size * oopSize);
    tty->print_cr("%d methods (%d bytes = %d base + %d debug info)", nmethods, 
                  (method_size + debug_size) * oopSize, method_size * oopSize, debug_size * oopSize);
    tty->print_cr("%d methoddata (%d bytes)", nmethoddata, methoddata_size * oopSize);
  }
};


int ClassStatistics::nclasses        = 0;  
int ClassStatistics::nmethods        = 0;
int ClassStatistics::nmethoddata     = 0;
int ClassStatistics::class_size      = 0;
int ClassStatistics::method_size     = 0; 
int ClassStatistics::debug_size      = 0;
int ClassStatistics::methoddata_size = 0;

void SystemDictionary::print_class_statistics() {
  ResourceMark rm;
  ClassStatistics::print();
}


class MethodStatistics: AllStatic {
 public:
  enum {
    max_parameter_size = 10
  };
 private:

  static int _number_of_methods;
  static int _number_of_final_methods;
  static int _number_of_static_methods;
  static int _number_of_native_methods;
  static int _number_of_synchronized_methods;
  static int _number_of_profiled_methods;
  static int _number_of_bytecodes;
  static int _parameter_size_profile[max_parameter_size];
  static int _bytecodes_profile[Bytecodes::number_of_java_codes];

  static void initialize() {
    _number_of_methods        = 0;
    _number_of_final_methods  = 0;
    _number_of_static_methods = 0;
    _number_of_native_methods = 0;
    _number_of_synchronized_methods = 0;
    _number_of_profiled_methods = 0;
    _number_of_bytecodes      = 0;
    for (int i = 0; i < max_parameter_size             ; i++) _parameter_size_profile[i] = 0;
    for (int j = 0; j < Bytecodes::number_of_java_codes; j++) _bytecodes_profile     [j] = 0;
  };

  static void do_method(methodOop m) {
    _number_of_methods++;
    // collect flag info
    if (m->is_final()       ) _number_of_final_methods++;
    if (m->is_static()      ) _number_of_static_methods++;
    if (m->is_native()      ) _number_of_native_methods++;
    if (m->is_synchronized()) _number_of_synchronized_methods++;
#ifndef CORE
    if (m->method_data() != NULL) _number_of_profiled_methods++;
#endif // !CORE
    // collect parameter size info (add one for receiver, if any)
    _parameter_size_profile[MIN2(m->size_of_parameters() + (m->is_static() ? 0 : 1), max_parameter_size - 1)]++;
    // collect bytecodes info
    { 
      Thread *thread = Thread::current();
      HandleMark hm(thread);
      BytecodeStream s(methodHandle(thread, m));
      Bytecodes::Code c;
      while ((c = s.next()) >= 0) {
        _number_of_bytecodes++;
        _bytecodes_profile[c]++;
      }
    }
  }

 public:
  static void print() {
    initialize();
    SystemDictionary::methods_do(do_method);
    // generate output
    tty->cr();
    tty->print_cr("Method statistics (static):");
    // flag distribution
    tty->cr();
    tty->print_cr("%6d final        methods  %6.1f%%", _number_of_final_methods       , _number_of_final_methods        * 100.0F / _number_of_methods);
    tty->print_cr("%6d static       methods  %6.1f%%", _number_of_static_methods      , _number_of_static_methods       * 100.0F / _number_of_methods);
    tty->print_cr("%6d native       methods  %6.1f%%", _number_of_native_methods      , _number_of_native_methods       * 100.0F / _number_of_methods);
    tty->print_cr("%6d synchronized methods  %6.1f%%", _number_of_synchronized_methods, _number_of_synchronized_methods * 100.0F / _number_of_methods);
    tty->print_cr("%6d profiled     methods  %6.1f%%", _number_of_profiled_methods, _number_of_profiled_methods * 100.0F / _number_of_methods);
    // parameter size profile
    tty->cr();
    { int tot = 0;
      int avg = 0;
      for (int i = 0; i < max_parameter_size; i++) {
        int n = _parameter_size_profile[i];
        tot += n;
        avg += n*i;
        tty->print_cr("parameter size = %1d: %6d methods  %5.1f%%", i, n, n * 100.0F / _number_of_methods);
      }
      assert(tot == _number_of_methods, "should be the same");
      tty->print_cr("                    %6d methods  100.0%%", _number_of_methods);
      tty->print_cr("(average parameter size = %3.1f including receiver, if any)", (float)avg / _number_of_methods);
    }
    // bytecodes profile
    tty->cr();
    { int tot = 0;
      for (int i = 0; i < Bytecodes::number_of_java_codes; i++) {
        if (Bytecodes::is_defined(i)) {
          Bytecodes::Code c = Bytecodes::cast(i);
          int n = _bytecodes_profile[c];
          tot += n;
          tty->print_cr("%9d  %7.3f%%  %s", n, n * 100.0F / _number_of_bytecodes, Bytecodes::name(c));
        }
      }
      assert(tot == _number_of_bytecodes, "should be the same");
      tty->print_cr("%9d  100.000%%", _number_of_bytecodes);
    }
    tty->cr();
  }
};

int MethodStatistics::_number_of_methods;
int MethodStatistics::_number_of_final_methods;
int MethodStatistics::_number_of_static_methods;
int MethodStatistics::_number_of_native_methods;
int MethodStatistics::_number_of_synchronized_methods;
int MethodStatistics::_number_of_profiled_methods;
int MethodStatistics::_number_of_bytecodes;
int MethodStatistics::_parameter_size_profile[MethodStatistics::max_parameter_size];
int MethodStatistics::_bytecodes_profile[Bytecodes::number_of_java_codes];


void SystemDictionary::print_method_statistics() {
  MethodStatistics::print();
}

#endif // PRODUCT
