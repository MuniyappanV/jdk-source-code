#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)jni.cpp	1.381 04/06/15 12:17:34 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_jni.cpp.incl"

static jint CurrentVersion = JNI_VERSION_1_4;

// out-of-line helpers for class jfieldIDWorkaround:

bool jfieldIDWorkaround::is_valid_jfieldID(klassOop k, jfieldID id) {
  if (jfieldIDWorkaround::is_instance_jfieldID(k, id)) {
    uintptr_t as_uint = (uintptr_t) id;
    intptr_t offset = raw_instance_offset(id);
    if (is_checked_jfieldID(id)) {
      if (!klass_hash_ok(k, id)) {
        return false;
      }  
    }
    return instanceKlass::cast(k)->contains_field_offset(offset);
  } else {
    JNIid* result = (JNIid*) id;
#ifdef ASSERT
    return result != NULL && result->is_static_field_id();
#else
    return result != NULL;
#endif
  }
}


intptr_t jfieldIDWorkaround::encode_klass_hash(klassOop k, intptr_t offset) {
  if (offset <= small_offset_mask) {
    klassOop field_klass = k;
    klassOop super_klass = Klass::cast(field_klass)->super();
    while (instanceKlass::cast(super_klass)->contains_field_offset(offset)) {
      field_klass = super_klass;   // super contains the field also
      super_klass = Klass::cast(field_klass)->super();
    }
    uintptr_t klass_hash = field_klass->identity_hash();
    return ((klass_hash & klass_mask) << klass_shift) | checked_mask_in_place;
  } else {
#if 0
    #ifndef PRODUCT
    {
      ResourceMark rm;
      warning("VerifyJNIFields: long offset %d in %s", offset, Klass::cast(k)->external_name());
    }
    #endif
#endif
    return 0;
  }
}

bool jfieldIDWorkaround::klass_hash_ok(klassOop k, jfieldID id) {
  uintptr_t as_uint = (uintptr_t) id;
  intptr_t klass_hash = (as_uint >> klass_shift) & klass_mask;
  do {
    // Could use a non-blocking query for identity_hash here...
    if ((k->identity_hash() & klass_mask) == klass_hash)
      return true;
    k = Klass::cast(k)->super();
  } while (k != NULL);
  return false;
}

void jfieldIDWorkaround::verify_instance_jfieldID(klassOop k, jfieldID id) {
  guarantee(jfieldIDWorkaround::is_instance_jfieldID(k, id), "must be an instance field" ); 
  uintptr_t as_uint = (uintptr_t) id;
  intptr_t offset = raw_instance_offset(id);
  if (VerifyJNIFields) {
    if (is_checked_jfieldID(id)) {
      guarantee(klass_hash_ok(k, id),
		"Bug in native code: jfieldID class must match object");
    } else {
#if 0
      #ifndef PRODUCT
      if (Verbose) {
	ResourceMark rm;
	warning("VerifyJNIFields: unverified offset %d for %s", offset, Klass::cast(k)->external_name());
      }
      #endif
#endif
    }
  }
  guarantee(instanceKlass::cast(k)->contains_field_offset(offset),
	    "Bug in native code: jfieldID offset must address interior of object");
}

// Pick a reasonable higher bound for local capacity requested
// for EnsureLocalCapacity and PushLocalFrame.  We don't want it too
// high because a test (or very unusual application) may try to allocate
// that many handles and run out of swap space.  An implementation is
// permitted to allocate more handles than the ensured capacity, so this
// value is set high enough to prevent compatibility problems.
const int MAX_REASONABLE_LOCAL_CAPACITY = 4*K;


// Wrapper to trace JNI functions

#ifdef ASSERT
  Histogram* JNIHistogram;
  static volatile jint JNIHistogram_lock = 0;

  class JNITraceWrapper : public StackObj {
   public:
    JNITraceWrapper(const char* format, ...) {      
      if (TraceJNICalls) {
        va_list ap;
        va_start(ap, format);
        tty->print("JNI ");
        tty->vprint_cr(format, ap);
        va_end(ap);
      }
    }
  };

  class JNIHistogramElement : public HistogramElement {
    public:
     JNIHistogramElement(const char* name);
  };

  JNIHistogramElement::JNIHistogramElement(const char* elementName) {
    _name = elementName;
    uintx count = 0;

    while (Atomic::cmpxchg(1, &JNIHistogram_lock, 0) != 0) {
      while (OrderAccess::load_acquire(&JNIHistogram_lock) != 0) {
        count +=1;
        if ( (WarnOnStalledSpinLock > 0)
          && (count % WarnOnStalledSpinLock == 0)) {
          warning("JNIHistogram_lock seems to be stalled");
        }
      }
     }


    if(JNIHistogram == NULL)
      JNIHistogram = new Histogram("JNI Call Counts",100);

    JNIHistogram->add_element(this);
    Atomic::dec(&JNIHistogram_lock);
  }

  #define JNICountWrapper(arg)                                     \
     static JNIHistogramElement* e = new JNIHistogramElement(arg); \
      /* There is a MT-race condition in VC++. So we need to make sure that that e has been initialized */ \
     if (e != NULL) e->increment_count()
  #define JNIWrapper(arg) JNICountWrapper(arg); JNITraceWrapper(arg)
#else
  #define JNIWrapper(arg)
#endif


// Implementation of JNI entries

JNI_ENTRY(jclass, jni_DefineClass(JNIEnv *env, const char *name, jobject loaderRef,
                                  const jbyte *buf, jsize bufLen))
  JNIWrapper("DefineClass");
  
  // Since exceptions can be thrown, class initialization can take place  
  // if name is NULL no check for class name in .class stream has to be made.
  symbolHandle class_name;
  if (name != NULL) {
    const int str_len = (int)strlen(name);
    if (str_len > symbolOopDesc::max_length()) {
      // It's impossible to create this class;  the name cannot fit
      // into the constant pool.
      THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), name);
    }
    class_name = oopFactory::new_symbol_handle(name, str_len, CHECK_0);
  }

  ResourceMark rm(THREAD);
  ClassFileStream st((u1*) buf, bufLen, NULL);
  Handle class_loader (THREAD, JNIHandles::resolve(loaderRef));
  klassOop k = SystemDictionary::resolve_from_stream(class_name, class_loader, 
                                                     Handle(), &st, CHECK_0);

  return (jclass) JNIHandles::make_local(env, Klass::cast(k)->java_mirror());
JNI_END



static bool first_time_FindClass = true;


JNI_ENTRY(jclass, jni_FindClass(JNIEnv *env, const char *name))
  JNIWrapper("FindClass");

  // Remember if we are the first invocation of jni_FindClass
  bool first_time = first_time_FindClass;
  first_time_FindClass = false;

  // Sanity check the name:  it cannot be null or larger than the maximum size 
  // name we can fit in the constant pool.
  if (name == NULL || (int)strlen(name) > symbolOopDesc::max_length()) {
    THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), name);
  }

  //%note jni_3
  Handle loader;
  Handle protection_domain;
  // Find calling class
  instanceKlassHandle k (THREAD, thread->security_get_caller_class(0));
  if (k.not_null()) {    
    loader = Handle(THREAD, k->class_loader());
    // Special handling to make sure JNI_OnLoad and JNI_OnUnload are executed 
    // in the correct class context.
    if (loader.is_null() && 
        k->name() == vmSymbols::java_lang_ClassLoader_NativeLibrary()) {      
      JavaValue result(T_OBJECT);
      JavaCalls::call_static(&result, k, 
                                      vmSymbolHandles::getFromClass_name(), 
                                      vmSymbolHandles::void_class_signature(), 
                                      thread);
      if (HAS_PENDING_EXCEPTION) {
        Handle ex(thread, thread->pending_exception());
        CLEAR_PENDING_EXCEPTION;
        THROW_HANDLE_0(ex);
      }
      oop mirror = (oop) result.get_jobject();
      loader = Handle(THREAD, 
        instanceKlass::cast(java_lang_Class::as_klassOop(mirror))->class_loader());
      protection_domain = Handle(THREAD, 
        instanceKlass::cast(java_lang_Class::as_klassOop(mirror))->protection_domain());
    }
  } else {
    // We call ClassLoader.getSystemClassLoader to obtain the system class loader.
    loader = Handle(THREAD, SystemDictionary::java_system_loader());
  }

  symbolHandle sym = oopFactory::new_symbol_handle(name, CHECK_0);
  jclass result = find_class_from_class_loader(env, sym, true, loader, 
                                               protection_domain, true, thread);

  // If we were the first invocation of jni_FindClass, we enable compilation again
  // rather than just allowing invocation counter to overflow and decay. 
  // Controlled by flag DelayCompilationDuringStartup.
  NOT_CORE(if (first_time && !CompileTheWorld) 
             CompilationPolicy::completed_vm_startup();)

  return result;
JNI_END


JNI_ENTRY(jmethodID, jni_FromReflectedMethod(JNIEnv *env, jobject method))
  JNIWrapper("FromReflectedMethod");
  // method is a handle to a java.lang.reflect.Method object
  oop reflected  = JNIHandles::resolve_non_null(method);
  oop mirror     = NULL;
  int slot       = 0;

  if (reflected->klass() == SystemDictionary::reflect_constructor_klass()) {
    mirror = java_lang_reflect_Constructor::clazz(reflected);  
    slot   = java_lang_reflect_Constructor::slot(reflected);
  } else {
    assert(reflected->klass() == SystemDictionary::reflect_method_klass(), "wrong type");
    mirror = java_lang_reflect_Method::clazz(reflected);  
    slot   = java_lang_reflect_Method::slot(reflected);
  }
  klassOop k     = java_lang_Class::as_klassOop(mirror);

  KlassHandle k1(THREAD, k);
  // Make sure class is initialized before handing id's out to methods
  Klass::cast(k1())->initialize(CHECK_0);

  return jniIdSupport::to_jmethod_id(k1(), slot);
JNI_END

JNI_ENTRY(jfieldID, jni_FromReflectedField(JNIEnv *env, jobject field))
  JNIWrapper("FromReflectedField");

  // field is a handle to a java.lang.reflect.Field object
  oop reflected   = JNIHandles::resolve_non_null(field);
  oop mirror      = java_lang_reflect_Field::clazz(reflected);
  klassOop k      = java_lang_Class::as_klassOop(mirror);
  int slot        = java_lang_reflect_Field::slot(reflected);
  int modifiers   = java_lang_reflect_Field::modifiers(reflected);

  KlassHandle k1(THREAD, k);
  // Make sure class is initialized before handing id's out to fields
  Klass::cast(k1())->initialize(CHECK_0);

  // First check if this is a static field
  if (modifiers & JVM_ACC_STATIC) {
    intptr_t offset = instanceKlass::cast(k1())->offset_from_fields( slot );
    JNIid* id = instanceKlass::cast(k1())->jni_id_for(offset);
    assert(id != NULL, "corrupt Field object");
    debug_only(id->set_is_static_field_id();)
    // A jfieldID for a static field is a JNIid specifying the field holder and the offset within the klassOop
    return jfieldIDWorkaround::to_static_jfieldID(id);
  }

  // The slot is the index of the field description in the field-array 
  // The jfieldID is the offset of the field within the object
  // It may also have hash bits for k, if VerifyJNIFields is turned on.
  intptr_t offset = instanceKlass::cast(k1())->offset_from_fields( slot );
  assert(instanceKlass::cast(k1())->contains_field_offset(offset), "stay within object");
  return jfieldIDWorkaround::to_instance_jfieldID(k1(), offset);
JNI_END


JNI_ENTRY(jobject, jni_ToReflectedMethod(JNIEnv *env, jclass cls, jmethodID method_id, jboolean isStatic))
  JNIWrapper("ToReflectedMethod");
  methodHandle m (THREAD, jniIdSupport::to_method_oop(method_id));
  assert(m->is_static() == isStatic, "jni_ToReflectedMethod access flags doesn't match");
  oop reflection_method;
  if (m->is_initializer()) {
    reflection_method = Reflection::new_constructor(m, CHECK_0);
  } else {
    reflection_method = Reflection::new_method(m, UseNewReflection, false, CHECK_0);
  }
  return JNIHandles::make_local(env, reflection_method);
JNI_END

JNI_ENTRY(jclass, jni_GetSuperclass(JNIEnv *env, jclass sub))
  JNIWrapper("GetSuperclass");
  oop mirror = JNIHandles::resolve_non_null(sub);
  // primitive classes return NULL
  if (java_lang_Class::is_primitive(mirror)) return NULL;

  // Rules of Class.getSuperClass as implemented by KLass::java_super:
  // arrays return Object
  // interfaces return NULL
  // proper classes return Klass::super()
  klassOop k = java_lang_Class::as_klassOop(mirror);
  if (Klass::cast(k)->is_interface()) return NULL;

  // return mirror for superclass
  klassOop super = Klass::cast(k)->java_super();
  return (super == NULL) ? NULL : (jclass) JNIHandles::make_local(Klass::cast(super)->java_mirror());
JNI_END


JNI_QUICK_ENTRY(jboolean, jni_IsAssignableFrom(JNIEnv *env, jclass sub, jclass super))
  JNIWrapper("IsSubclassOf");
  oop sub_mirror   = JNIHandles::resolve_non_null(sub);
  oop super_mirror = JNIHandles::resolve_non_null(super);
  if (java_lang_Class::is_primitive(sub_mirror) || java_lang_Class::is_primitive(super_mirror)) {
    return sub_mirror == super_mirror;
  }
  klassOop sub_klass   = java_lang_Class::as_klassOop(sub_mirror);
  klassOop super_klass = java_lang_Class::as_klassOop(super_mirror);
  assert(sub_klass != NULL && super_klass != NULL, "invalid arguments to jni_IsAssignableFrom");
  return Klass::cast(sub_klass)->is_subtype_of(super_klass) ? JNI_TRUE : JNI_FALSE;
JNI_END


JNI_ENTRY(jint, jni_Throw(JNIEnv *env, jthrowable obj))
  JNIWrapper("Throw");
  THROW_OOP_(JNIHandles::resolve(obj), JNI_OK);
JNI_END


JNI_ENTRY(jint, jni_ThrowNew(JNIEnv *env, jclass clazz, const char *message))  
  JNIWrapper("ThrowNew");  
  instanceKlass* k = instanceKlass::cast(java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));  
  symbolHandle name = symbolHandle(THREAD, k->name());
  Handle class_loader (THREAD,  k->class_loader());
  Handle protection_domain (THREAD, k->protection_domain());
  THROW_MSG_LOADER_(name, (char *)message, class_loader, protection_domain, JNI_OK);
JNI_END


// JNI functions only transform a pending async exception to a synchronous
// exception in ExceptionOccurred and ExceptionCheck calls, since
// delivering an async exception in other places won't change the native
// code's control flow and would be harmful when native code further calls
// JNI functions with a pending exception. Async exception is also checked
// during the call, so ExceptionOccurred/ExceptionCheck won't return
// false but deliver the async exception at the very end during
// state transition.

static void jni_check_async_exceptions(JavaThread *thread) {
  assert(thread == Thread::current(), "must be itself");
  thread->check_and_handle_async_exceptions();
}

JNI_ENTRY(jthrowable, jni_ExceptionOccurred(JNIEnv *env))
  JNIWrapper("ExceptionOccurred");
  jni_check_async_exceptions(thread);
  oop exception = thread->pending_exception();
  return (jthrowable) JNIHandles::make_local(env, exception);
JNI_END


JNI_ENTRY(void, jni_ExceptionDescribe(JNIEnv *env))  
  JNIWrapper("ExceptionDescribe");
  if (thread->has_pending_exception()) {
    Handle ex(thread, thread->pending_exception());
    thread->clear_pending_exception();
    if (ex->is_a(SystemDictionary::threaddeath_klass())) {
      // Don't print anything if we are being killed.
    } else {
      jio_fprintf(defaultStream::error_stream(), "Exception ");
      if (thread != NULL && thread->threadObj() != NULL) {
        ResourceMark rm(THREAD);
        jio_fprintf(defaultStream::error_stream(),
		    "in thread \"%s\" ", thread->get_thread_name());
      }
      if (ex->is_a(SystemDictionary::throwable_klass())) {        
        JavaValue result(T_VOID);
        JavaCalls::call_virtual(&result, 
                                ex, 
                                KlassHandle(THREAD,
                                  SystemDictionary::throwable_klass()),
                                vmSymbolHandles::printStackTrace_name(), 
                                vmSymbolHandles::void_method_signature(), 
                                THREAD);
        // If an exception is thrown in the call it gets thrown away. Not much
        // we can do with it. The native code that calls this, does not check
        // for the exception - hence, it might still be in the thread when DestroyVM gets
        // called, potentially causing a few asserts to trigger - since no pending exception
        // is expected.
        CLEAR_PENDING_EXCEPTION;
      } else {
        ResourceMark rm(THREAD);
        jio_fprintf(defaultStream::error_stream(),
		    ". Uncaught exception of type %s.",
		    Klass::cast(ex->klass())->external_name());        
      }
    }
  }
JNI_END


JNI_QUICK_ENTRY(void, jni_ExceptionClear(JNIEnv *env))
  JNIWrapper("ExceptionClear");
  thread->clear_pending_exception();
JNI_END


JNI_ENTRY(void, jni_FatalError(JNIEnv *env, const char *msg))
  JNIWrapper("FatalError");
  tty->print_cr("FATAL ERROR in native method: %s", msg);
  thread->print_stack();
  os::abort(false);	// Prevent core dump, causes a jck failure.
JNI_END


JNI_ENTRY(jint, jni_PushLocalFrame(JNIEnv *env, jint capacity))
  JNIWrapper("PushLocalFrame");
  //%note jni_11
  if (capacity < 0 && capacity > MAX_REASONABLE_LOCAL_CAPACITY) {
    return JNI_ERR;
  }
  JNIHandleBlock* old_handles = thread->active_handles();
  JNIHandleBlock* new_handles = JNIHandleBlock::allocate_block(thread);
  assert(new_handles != NULL, "should not be NULL");
  new_handles->set_pop_frame_link(old_handles);
  thread->set_active_handles(new_handles);
  return JNI_OK;
JNI_END


JNI_ENTRY(jobject, jni_PopLocalFrame(JNIEnv *env, jobject result))
  JNIWrapper("PopLocalFrame");
  //%note jni_11
  Handle result_handle(thread, JNIHandles::resolve(result));
  JNIHandleBlock* old_handles = thread->active_handles();
  JNIHandleBlock* new_handles = old_handles->pop_frame_link();
  if (new_handles != NULL) {
    // As a sanity check we only release the handle blocks if the pop_frame_link is not NULL.
    // This way code will still work if PopLocalFrame is called without a corresponding
    // PushLocalFrame call. Note that we set the pop_frame_link to NULL explicitly, otherwise
    // the release_block call will release the blocks.
    thread->set_active_handles(new_handles);
    old_handles->set_pop_frame_link(NULL);              // clear link we won't release new_handles below
    JNIHandleBlock::release_block(old_handles, thread); // may block
    result = JNIHandles::make_local(thread, result_handle());
  }
  return result;
JNI_END


JNI_ENTRY(jobject, jni_NewGlobalRef(JNIEnv *env, jobject ref))
  JNIWrapper("NewGlobalRef");
  Handle ref_handle(thread, JNIHandles::resolve(ref));
  return JNIHandles::make_global(ref_handle);
JNI_END

// Must be JNI_ENTRY (with HandleMark) 
// When jvmpi is enabled, the agent might allocate handles on receiving 
// JNI_GLOBALREF_FREE event.
JNI_ENTRY(void, jni_DeleteGlobalRef(JNIEnv *env, jobject ref))
  JNIWrapper("DeleteGlobalRef");
  JNIHandles::destroy_global(ref);
JNI_END


JNI_QUICK_ENTRY(void, jni_DeleteLocalRef(JNIEnv *env, jobject obj))
  JNIWrapper("DeleteLocalRef");
  JNIHandles::destroy_local(obj);
JNI_END


JNI_QUICK_ENTRY(jboolean, jni_IsSameObject(JNIEnv *env, jobject r1, jobject r2))
  JNIWrapper("IsSameObject");
  oop a = JNIHandles::resolve(r1);
  oop b = JNIHandles::resolve(r2);
  return (a == b) ? JNI_TRUE : JNI_FALSE;
JNI_END


JNI_ENTRY(jobject, jni_NewLocalRef(JNIEnv *env, jobject ref))
  JNIWrapper("NewLocalRef");
  return JNIHandles::make_local(env, JNIHandles::resolve(ref));
JNI_END


JNI_LEAF(jint, jni_EnsureLocalCapacity(JNIEnv *env, jint capacity))
  JNIWrapper("EnsureLocalCapacity");
  if (capacity >= 0 && capacity <= MAX_REASONABLE_LOCAL_CAPACITY) {
    return JNI_OK;
  } else {
    return JNI_ERR;
  }
JNI_END


class JNI_ArgumentPusher : public SignatureIterator {
 protected:
  JavaCallArguments*  _arguments; 

  virtual void get_bool   () = 0;
  virtual void get_char   () = 0;
  virtual void get_short  () = 0;
  virtual void get_byte   () = 0;
  virtual void get_int    () = 0;
  virtual void get_long   () = 0;
  virtual void get_float  () = 0;
  virtual void get_double () = 0;
  virtual void get_object () = 0;

  JNI_ArgumentPusher(Thread *thread, symbolOop signature)
       : SignatureIterator(thread, signature) {    
    this->_return_type = T_ILLEGAL;
    _arguments = NULL;
  }

 public:
  virtual void iterate( uint64_t fingerprint ) = 0;

  void set_java_argument_object(JavaCallArguments *arguments) { _arguments = arguments; }

  inline void do_bool()                     { if (!is_return_type()) get_bool();   }
  inline void do_char()                     { if (!is_return_type()) get_char();   }
  inline void do_short()                    { if (!is_return_type()) get_short();  }
  inline void do_byte()                     { if (!is_return_type()) get_byte();   }
  inline void do_int()                      { if (!is_return_type()) get_int();    }
  inline void do_long()                     { if (!is_return_type()) get_long();   }
  inline void do_float()                    { if (!is_return_type()) get_float();  }
  inline void do_double()                   { if (!is_return_type()) get_double(); }
  inline void do_object(int begin, int end) { if (!is_return_type()) get_object(); }
  inline void do_array(int begin, int end)  { if (!is_return_type()) get_object(); } // do_array uses get_object -- there is no get_array
  inline void do_void()                     { }

  JavaCallArguments* arguments()     { return _arguments; }
  void push_receiver(Handle h)       { _arguments->push_oop(h); }
};


class JNI_ArgumentPusherVaArg : public JNI_ArgumentPusher {
 protected:
  va_list _ap;

  inline void get_bool()   { _arguments->push_int(va_arg(_ap, jint)); } // bool is coerced to int when using va_arg
  inline void get_char()   { _arguments->push_int(va_arg(_ap, jint)); } // char is coerced to int when using va_arg
  inline void get_short()  { _arguments->push_int(va_arg(_ap, jint)); } // short is coerced to int when using va_arg
  inline void get_byte()   { _arguments->push_int(va_arg(_ap, jint)); } // byte is coerced to int when using va_arg
  inline void get_int()    { _arguments->push_int(va_arg(_ap, jint)); }

  // each of these paths is exercized by the various jck Call[Static,Nonvirtual,][Void,Int,..]Method[A,V,] tests

  inline void get_long()   { _arguments->push_long(va_arg(_ap, jlong)); }
  inline void get_float()  { _arguments->push_float((jfloat)va_arg(_ap, jdouble)); } // float is coerced to double w/ va_arg
  inline void get_double() { _arguments->push_double(va_arg(_ap, jdouble)); }
  inline void get_object() { jobject l = va_arg(_ap, jobject); 
                             _arguments->push_oop(Handle((oop *)l, false)); }
      
  inline void set_ap(va_list rap) {
#ifdef va_copy
    va_copy(_ap, rap);
#elif defined (__va_copy)
    __va_copy(_ap, rap);
#else
    _ap = rap;
#endif
  }

 public:
  JNI_ArgumentPusherVaArg(Thread *thread, symbolOop signature, va_list rap)
       : JNI_ArgumentPusher(thread, signature) {
    set_ap(rap);
  }
  JNI_ArgumentPusherVaArg(Thread *thread, jmethodID method_id, va_list rap)
      : JNI_ArgumentPusher(thread, jniIdSupport::to_method_oop(method_id)->signature()) {
    set_ap(rap);
  }

  // Optimized path if we have the bitvector form of signature
  void iterate( uint64_t fingerprint ) {
    if ( fingerprint == CONST64(-1) ) SignatureIterator::iterate();// Must be too many arguments
    else {
      _return_type = (BasicType)((fingerprint >> static_feature_size) & 
                                  result_feature_mask);
    
      assert(fingerprint, "Fingerprint should not be 0");
      fingerprint = fingerprint >> (static_feature_size + result_feature_size);
      while ( 1 ) {
        switch ( fingerprint & parameter_feature_mask ) {
          case bool_parm:
          case char_parm:
          case short_parm:
          case byte_parm:
          case int_parm:
            get_int();
            break;
          case obj_parm: 
            get_object();
            break;
          case long_parm:
            get_long();
            break;
          case float_parm:
            get_float();
            break;
          case double_parm:
            get_double();
            break;
          case done_parm:
            return;
            break;
          default:
            ShouldNotReachHere();
            break;
        }
        fingerprint >>= parameter_feature_size;
      }
    }
  }
};


class JNI_ArgumentPusherArray : public JNI_ArgumentPusher {
 protected:
  const jvalue *_ap;

  inline void get_bool()   { _arguments->push_int((jint)(_ap++)->z); }
  inline void get_char()   { _arguments->push_int((jint)(_ap++)->c); }
  inline void get_short()  { _arguments->push_int((jint)(_ap++)->s); }
  inline void get_byte()   { _arguments->push_int((jint)(_ap++)->b); }
  inline void get_int()    { _arguments->push_int((jint)(_ap++)->i); }

  inline void get_long()   { _arguments->push_long((_ap++)->j);  }
  inline void get_float()  { _arguments->push_float((_ap++)->f); }
  inline void get_double() { _arguments->push_double((_ap++)->d);}
  inline void get_object() { _arguments->push_oop(Handle((oop *)(_ap++)->l, false)); }

  inline void set_ap(const jvalue *rap) { _ap = rap; }

 public:
  JNI_ArgumentPusherArray(Thread *thread, symbolOop signature, const jvalue *rap)
       : JNI_ArgumentPusher(thread, signature) {
    set_ap(rap);
  }
  JNI_ArgumentPusherArray(Thread *thread, jmethodID method_id, const jvalue *rap) 
      : JNI_ArgumentPusher(thread, jniIdSupport::to_method_oop(method_id)->signature()) {
    set_ap(rap);
  }

  // Optimized path if we have the bitvector form of signature
  void iterate( uint64_t fingerprint ) {
    if ( fingerprint == CONST64(-1) ) SignatureIterator::iterate(); // Must be too many arguments
    else {
      _return_type = (BasicType)((fingerprint >> static_feature_size) & 
                                  result_feature_mask);
      assert(fingerprint, "Fingerprint should not be 0");
      fingerprint = fingerprint >> (static_feature_size + result_feature_size);
      while ( 1 ) {
        switch ( fingerprint & parameter_feature_mask ) {
          case bool_parm:
            get_bool();
            break;
          case char_parm:
            get_char();
            break;
          case short_parm:
            get_short();
            break;
          case byte_parm:
            get_byte();
            break;
          case int_parm:
            get_int();
            break;
          case obj_parm:
            get_object();
            break;
          case long_parm:
            get_long();
            break;
          case float_parm:
            get_float();
            break;
          case double_parm:
            get_double();
            break;
          case done_parm:
            return;
            break;
          default:
            ShouldNotReachHere();
            break;
        }
        fingerprint >>= parameter_feature_size;
      }
    }
  }
};


enum JNICallType {
  JNI_STATIC,
  JNI_VIRTUAL,
  JNI_NONVIRTUAL
};

static methodHandle jni_resolve_interface_call(Handle recv, methodHandle method, TRAPS) {
  assert(!method.is_null() , "method should not be null");

  KlassHandle recv_klass; // Default to NULL (use of ?: can confuse gcc)
  if (recv.not_null()) recv_klass = KlassHandle(THREAD, recv->klass());
  KlassHandle spec_klass (THREAD, method->method_holder());
  symbolHandle name (THREAD, method->name());
  symbolHandle signature (THREAD, method->signature());
  CallInfo info;
  LinkResolver::resolve_interface_call(info, recv, recv_klass,  spec_klass, name, signature, KlassHandle(), false, true, CHECK_(methodHandle()));
  return info.selected_method();
}

static methodHandle jni_resolve_virtual_call(Handle recv, methodHandle method, TRAPS) {
  assert(!method.is_null() , "method should not be null");

  KlassHandle recv_klass; // Default to NULL (use of ?: can confuse gcc)
  if (recv.not_null()) recv_klass = KlassHandle(THREAD, recv->klass());
  KlassHandle spec_klass (THREAD, method->method_holder());
  symbolHandle name (THREAD, method->name());
  symbolHandle signature (THREAD, method->signature());
  CallInfo info;
  LinkResolver::resolve_virtual_call(info, recv, recv_klass,  spec_klass, name, signature, KlassHandle(), false, true, CHECK_(methodHandle()));
  return info.selected_method();
}



static void jni_invoke_static(JNIEnv *env, JavaValue* result, jobject receiver, JNICallType call_type, jmethodID method_id, JNI_ArgumentPusher *args, TRAPS) {
  methodHandle method(THREAD, jniIdSupport::to_method_oop(method_id));  
  
  // Create object to hold arguments for the JavaCall, and associate it with
  // the jni parser  
  ResourceMark rm(THREAD);  
  int number_of_parameters = method->size_of_parameters();
  JavaCallArguments java_args(number_of_parameters);
  args->set_java_argument_object(&java_args);
  
  assert(method->is_static(), "method should be static");
 
  // Fill out JavaCallArguments object
  args->iterate( Fingerprinter(THREAD, method).fingerprint() );
  // Initialize result type
  result->set_type(args->get_ret_type());

  // Invoke the method. Result is returned as oop.
  JavaCalls::call(result, method, &java_args, CHECK);

  // Convert result
  if (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY) {
    result->set_jobject(JNIHandles::make_local(env, (oop) result->get_jobject()));
  }
}


static void jni_invoke_nonstatic(JNIEnv *env, JavaValue* result, jobject receiver, JNICallType call_type, jmethodID method_id, JNI_ArgumentPusher *args, TRAPS) {
  oop recv = JNIHandles::resolve(receiver);
  if (recv == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  Handle h_recv(THREAD, recv);

  int number_of_parameters;
  methodOop selected_method;
  {
    // We don't handlize m. Make sure the code in this block regets m from
    // the method_id if it allows GC (in jniIdSupport::to_method_details).
    methodOop m = jniIdSupport::to_method_oop(method_id);
    number_of_parameters = m->size_of_parameters();
    klassOop holder = m->method_holder();
    if (!(Klass::cast(holder))->is_interface()) {
      if (call_type == JNI_VIRTUAL) {
        // jni_GetMethodID makes sure class is linked and initialized
        // so m should have a valid vtable index.
        int vtbl_index = m->vtable_index();
        if (vtbl_index != -1) {
          klassOop k = h_recv->klass();
          // k might be an arrayKlassOop but all vtables start at
          // the same place. The cast is to avoid virtual call and assertion.
          instanceKlass *ik = (instanceKlass*)k->klass_part();
          selected_method = ik->method_at_vtable(vtbl_index);
        } else {
          // final method
          selected_method = m;
        }
      } else {
        // JNI_NONVIRTUAL call
        selected_method = m;
      }
    } else {    // interface call
      KlassHandle h_holder(THREAD, holder);
  
      jniIdDetails* details;
      // This may lock. So reassign m.
      m = jniIdSupport::to_method_details(method_id, &details);
  
      int itbl_index = details->itable_index();
      if (itbl_index == -1) {
        itbl_index = klassItable::compute_itable_index(m);
        details->set_itable_index(itbl_index);
      }
      klassOop k = h_recv->klass();
      selected_method = instanceKlass::cast(k)->method_at_itable(h_holder(), itbl_index, CHECK);
    }
  }

  methodHandle method(THREAD, selected_method);

  // Create object to hold arguments for the JavaCall, and associate it with
  // the jni parser  
  ResourceMark rm(THREAD);
  JavaCallArguments java_args(number_of_parameters);
  args->set_java_argument_object(&java_args);

  // handle arguments
  assert(!method->is_static(), "method should not be static");
  args->push_receiver(h_recv); // Push jobject handle

  // Fill out JavaCallArguments object
  args->iterate( Fingerprinter(THREAD, method).fingerprint() );
  // Initialize result type
  result->set_type(args->get_ret_type()); 

  // Invoke the method. Result is returned as oop.
  JavaCalls::call(result, method, &java_args, CHECK);

  // Convert result
  if (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY) {
    result->set_jobject(JNIHandles::make_local(env, (oop) result->get_jobject()));
  }
}


static instanceOop alloc_object(jclass clazz, TRAPS) {
  KlassHandle k(THREAD, java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));
  Klass::cast(k())->check_valid_for_instantiation(false, CHECK_0);
  instanceKlass::cast(k())->initialize(CHECK_0);
  instanceOop ih = instanceKlass::cast(k())->allocate_instance(THREAD);
  return ih;
}


JNI_ENTRY(jobject, jni_AllocObject(JNIEnv *env, jclass clazz))
  JNIWrapper("AllocObject");
  instanceOop i = alloc_object(clazz, CHECK_0);
  return JNIHandles::make_local(env, i);
JNI_END


JNI_ENTRY(jobject, jni_NewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args))  
  JNIWrapper("NewObjectA");
  instanceOop i = alloc_object(clazz, CHECK_0);
  jobject obj = JNIHandles::make_local(env, i);
  JavaValue jvalue(T_VOID);  
  JNI_ArgumentPusherArray ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0);
  return obj;
JNI_END


JNI_ENTRY(jobject, jni_NewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args))  
  JNIWrapper("NewObjectV");
  instanceOop i = alloc_object(clazz, CHECK_0);
  jobject obj = JNIHandles::make_local(env, i);
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0);
  return obj;
JNI_END


JNI_ENTRY(jobject, jni_NewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...))  
  JNIWrapper("NewObject");
  instanceOop i = alloc_object(clazz, CHECK_0);
  jobject obj = JNIHandles::make_local(env, i);
  va_list args;
  va_start(args, methodID);
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0);
  va_end(args);
  return obj;
JNI_END


JNI_ENTRY(jclass, jni_GetObjectClass(JNIEnv *env, jobject obj))
  JNIWrapper("GetObjectClass");
  klassOop k = JNIHandles::resolve_non_null(obj)->klass();
  return (jclass) JNIHandles::make_local(env, Klass::cast(k)->java_mirror());
JNI_END


JNI_QUICK_ENTRY(jboolean, jni_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz))
  JNIWrapper("IsInstanceOf");
  if (obj == NULL) return JNI_TRUE;
  klassOop k = java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz));
  if (k == NULL) return JNI_FALSE;
  return JNIHandles::resolve_non_null(obj)->is_a(k) ? JNI_TRUE : JNI_FALSE;
JNI_END


static jmethodID get_method_id(JNIEnv *env, jclass clazz, const char *name_str, 
                               const char *sig, bool is_static, TRAPS) {
  // %%%% This code should probably just call into a method in the LinkResolver
  //
  // The class should have been loaded (we have an instance of the class
  // passed in) so the method and signature should already be in the symbol
  // table.  If they're not there, the method doesn't exist.
  symbolHandle signature = 
           symbolHandle(THREAD, SymbolTable::probe(sig, (int)strlen(sig)));
  symbolHandle name;
  if (name_str == NULL) {
    name = vmSymbolHandles::object_initializer_name();
  } else {
    name = symbolHandle(THREAD, 
                        SymbolTable::probe(name_str, (int)strlen(name_str)));
  }
  if (name.is_null() || signature.is_null()) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchMethodError(), name_str);
  }

  // Throw a NoSuchMethodError exception if we have an instance of a 
  // primitive java.lang.Class
  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(clazz))) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchMethodError(), name_str);
  }

  KlassHandle klass(THREAD, 
               java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));

  // Make sure class is linked and initialized before handing id's out to 
  // methodOops. 
  Klass::cast(klass())->initialize(CHECK_0);  

  methodOop m;
  if (name() == vmSymbols::object_initializer_name() || 
      name() == vmSymbols::class_initializer_name()) {
    // Never search superclasses for constructors
    if (klass->oop_is_instance()) {
      m = instanceKlass::cast(klass())->find_method(name(), signature());
    } else {
      m = NULL;
    }
  } else {
    m = klass->lookup_method(name(), signature());
    // Look up interfaces
    if (m == NULL && klass->oop_is_instance()) {
      m = instanceKlass::cast(klass())->lookup_method_in_all_interfaces(name(), 
                                                                   signature()); 
    }
  }
  if (m == NULL || (m->is_static() != is_static)) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchMethodError(), name_str);
  }
  return m->jmethod_id();
}


JNI_ENTRY(jmethodID, jni_GetMethodID(JNIEnv *env, jclass clazz, 
                                     const char *name, const char *sig))
  JNIWrapper("GetMethodID");
  return get_method_id(env, clazz, name, sig, false, thread);
JNI_END


JNI_ENTRY(jmethodID, jni_GetStaticMethodID(JNIEnv *env, jclass clazz, 
                                           const char *name, const char *sig))
  JNIWrapper("GetStaticMethodID");
  return get_method_id(env, clazz, name, sig, true, thread);
JNI_END


//
// Calling Methods
//


#define DEFINE_CALLMETHOD(ResultType, Result, Tag) \
JNI_ENTRY(ResultType, \
          jni_Call##Result##Method(JNIEnv *env, jobject obj, jmethodID methodID, ...)) \
  JNIWrapper("Call" XSTR(Result) "Method"); \
  va_list args; \
  va_start(args, methodID); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK_0); \
  va_end(args); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_Call##Result##MethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)) \
  JNIWrapper("Call" XSTR(Result) "MethodV"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_Call##Result##MethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args)) \
  JNIWrapper("Call" XSTR(Result) "MethodA"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherArray ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END

// the runtime type of subword integral basic types is integer
DEFINE_CALLMETHOD(jboolean, Boolean, T_BOOLEAN)
DEFINE_CALLMETHOD(jbyte,    Byte,    T_BYTE)
DEFINE_CALLMETHOD(jchar,    Char,    T_CHAR)
DEFINE_CALLMETHOD(jshort,   Short,   T_SHORT)

DEFINE_CALLMETHOD(jobject,  Object,  T_OBJECT)
DEFINE_CALLMETHOD(jint,     Int,     T_INT)
DEFINE_CALLMETHOD(jlong,    Long,    T_LONG)
DEFINE_CALLMETHOD(jfloat,   Float,   T_FLOAT)
DEFINE_CALLMETHOD(jdouble,  Double,  T_DOUBLE)

JNI_ENTRY(void, jni_CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...))  
  JNIWrapper("CallVoidMethod");
  va_list args;
  va_start(args, methodID);
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK);
  va_end(args);
JNI_END
                                                
                                                   
JNI_ENTRY(void, jni_CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args))  
  JNIWrapper("CallVoidMethodV");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK);
JNI_END
                                  
                                     
JNI_ENTRY(void, jni_CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args))  
  JNIWrapper("CallVoidMethodA");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherArray ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_VIRTUAL, methodID, &ap, CHECK);
JNI_END


#define DEFINE_CALLNONVIRTUALMETHOD(ResultType, Result, Tag) \
\
\
JNI_ENTRY(ResultType, \
          jni_CallNonvirtual##Result##Method(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, ...)) \
  JNIWrapper("CallNonvitual" XSTR(Result) "Method"); \
  va_list args; \
  va_start(args, methodID); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0); \
  va_end(args); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_CallNonvirtual##Result##MethodV(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, va_list args)) \
  JNIWrapper("CallNonvitual" XSTR(Result) "#MethodV"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_CallNonvirtual##Result##MethodA(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, const jvalue *args)) \
  JNIWrapper("CallNonvitual" XSTR(Result) "MethodA"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherArray ap(THREAD, methodID, args); \
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END

// the runtime type of subword integral basic types is integer
DEFINE_CALLNONVIRTUALMETHOD(jboolean, Boolean, T_BOOLEAN)
DEFINE_CALLNONVIRTUALMETHOD(jbyte,    Byte,    T_BYTE)
DEFINE_CALLNONVIRTUALMETHOD(jchar,    Char,    T_CHAR)
DEFINE_CALLNONVIRTUALMETHOD(jshort,   Short,   T_SHORT)

DEFINE_CALLNONVIRTUALMETHOD(jobject,  Object,  T_OBJECT)
DEFINE_CALLNONVIRTUALMETHOD(jint,     Int,     T_INT)
DEFINE_CALLNONVIRTUALMETHOD(jlong,    Long,    T_LONG)
DEFINE_CALLNONVIRTUALMETHOD(jfloat,   Float,   T_FLOAT)
DEFINE_CALLNONVIRTUALMETHOD(jdouble,  Double,  T_DOUBLE)


JNI_ENTRY(void, jni_CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, ...))  
  JNIWrapper("CallNonvirtualVoidMethod");
  va_list args;
  va_start(args, methodID);
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK);
  va_end(args);
JNI_END
                                                 
                                                   
JNI_ENTRY(void, jni_CallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, va_list args))  
  JNIWrapper("CallNonvirtualVoidMethodV");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK);
JNI_END
                             
                                     
JNI_ENTRY(void, jni_CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass cls, jmethodID methodID, const jvalue *args))  
  JNIWrapper("CallNonvirtualVoidMethodA");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherArray ap(THREAD, methodID, args);
  jni_invoke_nonstatic(env, &jvalue, obj, JNI_NONVIRTUAL, methodID, &ap, CHECK);
JNI_END


#define DEFINE_CALLSTATICMETHOD(ResultType, Result, Tag) \
\
\
JNI_ENTRY(ResultType, \
          jni_CallStatic##Result##Method(JNIEnv *env, jclass cls, jmethodID methodID, ...)) \
  JNIWrapper("CallStatic" XSTR(Result) "Method"); \
  va_list args; \
  va_start(args, methodID); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK_0); \
  va_end(args); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_CallStatic##Result##MethodV(JNIEnv *env, jclass cls, jmethodID methodID, va_list args)) \
  JNIWrapper("CallStatic" XSTR(Result) "MethodV"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args); \
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END \
\
\
JNI_ENTRY(ResultType, \
          jni_CallStatic##Result##MethodA(JNIEnv *env, jclass cls, jmethodID methodID, const jvalue *args)) \
  JNIWrapper("CallStatic" XSTR(Result) "MethodA"); \
  JavaValue jvalue(Tag); \
  JNI_ArgumentPusherArray ap(THREAD, methodID, args); \
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK_0); \
  return jvalue.get_##ResultType(); \
JNI_END

// the runtime type of subword integral basic types is integer
DEFINE_CALLSTATICMETHOD(jboolean, Boolean, T_BOOLEAN)
DEFINE_CALLSTATICMETHOD(jbyte,    Byte,    T_BYTE)
DEFINE_CALLSTATICMETHOD(jchar,    Char,    T_CHAR)
DEFINE_CALLSTATICMETHOD(jshort,   Short,   T_SHORT)

DEFINE_CALLSTATICMETHOD(jobject,  Object,  T_OBJECT)
DEFINE_CALLSTATICMETHOD(jint,     Int,     T_INT)
DEFINE_CALLSTATICMETHOD(jlong,    Long,    T_LONG)
DEFINE_CALLSTATICMETHOD(jfloat,   Float,   T_FLOAT)
DEFINE_CALLSTATICMETHOD(jdouble,  Double,  T_DOUBLE)


JNI_ENTRY(void, jni_CallStaticVoidMethod(JNIEnv *env, jclass cls, jmethodID methodID, ...))  
  JNIWrapper("CallStaticVoidMethod");
  va_list args;
  va_start(args, methodID);
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK);
  va_end(args);
JNI_END
                                                 
                                                   
JNI_ENTRY(void, jni_CallStaticVoidMethodV(JNIEnv *env, jclass cls, jmethodID methodID, va_list args))  
  JNIWrapper("CallStaticVoidMethodV");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherVaArg ap(THREAD, methodID, args);
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK);
JNI_END
                                  
                                     
JNI_ENTRY(void, jni_CallStaticVoidMethodA(JNIEnv *env, jclass cls, jmethodID methodID, const jvalue *args))  
  JNIWrapper("CallStaticVoidMethodA");
  JavaValue jvalue(T_VOID);
  JNI_ArgumentPusherArray ap(THREAD, methodID, args);
  jni_invoke_static(env, &jvalue, NULL, JNI_STATIC, methodID, &ap, CHECK);
JNI_END


//
// Accessing Fields
//


JNI_ENTRY(jfieldID, jni_GetFieldID(JNIEnv *env, jclass clazz, 
                                   const char *name, const char *sig))
  JNIWrapper("GetFieldID");
  // The class should have been loaded (we have an instance of the class
  // passed in) so the field and signature should already be in the symbol
  // table.  If they're not there, the field doesn't exist.
  symbolHandle fieldname = 
            symbolHandle(THREAD, SymbolTable::probe(name, (int)strlen(name)));
  symbolHandle signame   = 
            symbolHandle(THREAD, SymbolTable::probe(sig, (int)strlen(sig)));
  if (fieldname.is_null() || signame.is_null()) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchFieldError(), (char*) name);
  }
  KlassHandle k(THREAD, 
                java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));
  // Make sure class is initialized before handing id's out to fields
  Klass::cast(k())->initialize(CHECK_0);

  fieldDescriptor fd;
  if (!Klass::cast(k())->oop_is_instance() || 
      !instanceKlass::cast(k())->find_field(fieldname(), signame(), false, &fd)) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchFieldError(), (char*) name);
  }

  // A jfieldID for a non-static field is simply the offset of the field within the instanceOop
  // It may also have hash bits for k, if VerifyJNIFields is turned on.
  return jfieldIDWorkaround::to_instance_jfieldID(k(), fd.offset());
JNI_END


JNI_ENTRY(jobject, jni_GetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID))
  JNIWrapper("GetObjectField");
  oop o = JNIHandles::resolve_non_null(obj);
  klassOop k = o->klass();
  int offset = jfieldIDWorkaround::from_instance_jfieldID(k, fieldID);
  // Keep JVMTI addition small and only check enabled flag here.
  // jni_GetField_probe() assumes that is okay to create handles.
  if (JvmtiExport::should_post_field_access()) {
    o = JvmtiExport::jni_GetField_probe(thread, obj, o, k, fieldID, false);
  }
  return JNIHandles::make_local(env, o->obj_field(offset));
JNI_END


#define DEFINE_GETFIELD(Return,Fieldname,Result) \
JNI_QUICK_ENTRY(Return, jni_Get##Result##Field(JNIEnv *env, jobject obj, jfieldID fieldID)) \
  JNIWrapper("Get" XSTR(Result) "Field"); \
  oop o = JNIHandles::resolve_non_null(obj); \
  klassOop k = o->klass(); \
  int offset = jfieldIDWorkaround::from_instance_jfieldID(k, fieldID);  \
  /* Keep JVMTI addition small and only check enabled flag here.       */ \
  /* jni_GetField_probe_nh() assumes that is not okay to create handles */ \
  /* and creates a ResetNoHandleMark.                                   */ \
  if (JvmtiExport::should_post_field_access()) { \
    o = JvmtiExport::jni_GetField_probe_nh(thread, obj, o, k, fieldID, false); \
  } \
  return o->Fieldname##_field(offset); \
JNI_END

DEFINE_GETFIELD(jboolean, bool,   Boolean)
DEFINE_GETFIELD(jbyte,    byte,   Byte)
DEFINE_GETFIELD(jchar,    char,   Char)
DEFINE_GETFIELD(jshort,   short,  Short)
DEFINE_GETFIELD(jint,     int,    Int)
DEFINE_GETFIELD(jlong,    long,   Long)
DEFINE_GETFIELD(jfloat,   float,  Float)
DEFINE_GETFIELD(jdouble,  double, Double)

address jni_GetBooleanField_addr() {
  return (address)jni_GetBooleanField;
}
address jni_GetByteField_addr() {
  return (address)jni_GetByteField;
}
address jni_GetCharField_addr() {
  return (address)jni_GetCharField;
}
address jni_GetShortField_addr() {
  return (address)jni_GetShortField;
}
address jni_GetIntField_addr() {
  return (address)jni_GetIntField;
}
address jni_GetLongField_addr() {
  return (address)jni_GetLongField;
}
address jni_GetFloatField_addr() {
  return (address)jni_GetFloatField;
}
address jni_GetDoubleField_addr() {
  return (address)jni_GetDoubleField;
}

JNI_QUICK_ENTRY(void, jni_SetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject value))
  JNIWrapper("SetObjectField"); 
  oop o = JNIHandles::resolve_non_null(obj); 
  klassOop k = o->klass();
  int offset = jfieldIDWorkaround::from_instance_jfieldID(k, fieldID);
  // Keep JVMTI addition small and only check enabled flag here.
  // jni_SetField_probe_nh() assumes that is not okay to create handles
  // and creates a ResetNoHandleMark.
  if (JvmtiExport::should_post_field_modification()) {
    jvalue field_value;
    field_value.l = value;
    o = JvmtiExport::jni_SetField_probe_nh(thread, obj, o, k, fieldID, false, 'L', (jvalue *)&field_value);
  }
  o->obj_field_put(offset, JNIHandles::resolve(value));
JNI_END

#define DEFINE_SETFIELD(Argument,Fieldname,Result,SigType,unionType) \
JNI_QUICK_ENTRY(void, jni_Set##Result##Field(JNIEnv *env, jobject obj, jfieldID fieldID, Argument value)) \
  JNIWrapper("Set" XSTR(Result) "Field"); \
  oop o = JNIHandles::resolve_non_null(obj); \
  klassOop k = o->klass(); \
  int offset = jfieldIDWorkaround::from_instance_jfieldID(k, fieldID);  \
  /* Keep JVMTI addition small and only check enabled flag here.       */ \
  /* jni_SetField_probe_nh() assumes that is not okay to create handles */ \
  /* and creates a ResetNoHandleMark.                                   */ \
  if (JvmtiExport::should_post_field_modification()) { \
    jvalue field_value; \
    field_value.unionType = value; \
    o = JvmtiExport::jni_SetField_probe_nh(thread, obj, o, k, fieldID, false, SigType, (jvalue *)&field_value); \
  } \
  o->Fieldname##_field_put(offset, value); \
JNI_END

DEFINE_SETFIELD(jboolean, bool,   Boolean, 'Z', z)
DEFINE_SETFIELD(jbyte,    byte,   Byte,    'B', b)
DEFINE_SETFIELD(jchar,    char,   Char,    'C', c)
DEFINE_SETFIELD(jshort,   short,  Short,   'S', s)
DEFINE_SETFIELD(jint,     int,    Int,     'I', i)
DEFINE_SETFIELD(jlong,    long,   Long,    'J', j)
DEFINE_SETFIELD(jfloat,   float,  Float,   'F', f)
DEFINE_SETFIELD(jdouble,  double, Double,  'D', d)


JNI_ENTRY(jobject, jni_ToReflectedField(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic))
  JNIWrapper("ToReflectedField");
  fieldDescriptor fd;
  bool found = false;
  klassOop k = java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(cls));

  assert(jfieldIDWorkaround::is_static_jfieldID(fieldID) == isStatic, "invalid fieldID");

  if (isStatic) {
    // Static field. The fieldID a JNIid specifying the field holder and the offset within the klassOop.
    JNIid* id = jfieldIDWorkaround::from_static_jfieldID(fieldID);
    assert(id->is_static_field_id(), "invalid static field id");
    found = instanceKlass::cast(id->holder())->find_local_field_from_offset(id->offset(), true, &fd);
  } else {
    // Non-static field. The fieldID is really the offset of the field within the instanceOop.
    int offset = jfieldIDWorkaround::from_instance_jfieldID(k, fieldID);    
    found = instanceKlass::cast(k)->find_field_from_offset(offset, false, &fd);
  } 
  assert(found, "bad fieldID passed into jni_ToReflectedField");     
  oop reflected = Reflection::new_field(&fd, UseNewReflection, CHECK_0);
  return JNIHandles::make_local(env, reflected);
JNI_END


//
// Accessing Static Fields
//

JNI_ENTRY(jfieldID, jni_GetStaticFieldID(JNIEnv *env, jclass clazz, 
                                        const char *name, const char *sig))
  JNIWrapper("GetStaticFieldID");
  // The class should have been loaded (we have an instance of the class
  // passed in) so the field and signature should already be in the symbol
  // table.  If they're not there, the field doesn't exist.
  symbolHandle fieldname = 
           symbolHandle(THREAD, SymbolTable::probe(name, (int)strlen(name)));
  symbolHandle signame   = 
           symbolHandle(THREAD, SymbolTable::probe(sig, (int)strlen(sig)));
  if (fieldname.is_null() || signame.is_null()) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchFieldError(), (char*) name);
  }
  KlassHandle k(THREAD, 
                java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));
  // Make sure class is initialized before handing id's out to static fields
  Klass::cast(k())->initialize(CHECK_0);

  fieldDescriptor fd;
  if (!Klass::cast(k())->oop_is_instance() || 
      !instanceKlass::cast(k())->find_field(fieldname(), signame(), true, &fd)) {
    THROW_MSG_0(vmSymbols::java_lang_NoSuchFieldError(), (char*) name);
  }

  // A jfieldID for a static field is a JNIid specifying the field holder and the offset within the klassOop
  JNIid* id = instanceKlass::cast(fd.field_holder())->jni_id_for(fd.offset());
  debug_only(id->set_is_static_field_id();)

  debug_only(int first_offset = instanceKlass::cast(fd.field_holder())->offset_of_static_fields();)
  debug_only(int end_offset = first_offset + (instanceKlass::cast(fd.field_holder())->static_field_size() * wordSize);)
  assert(id->offset() >= first_offset && id->offset() < end_offset, "invalid static field offset");

  return jfieldIDWorkaround::to_static_jfieldID(id);
JNI_END


JNI_ENTRY(jobject, jni_GetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID))
  JNIWrapper("GetStaticObjectField");
  DEBUG_ONLY(klassOop param_k = jniCheck::validate_class(thread, clazz);)
  JNIid* id = jfieldIDWorkaround::from_static_jfieldID(fieldID);
  assert(id->is_static_field_id(), "invalid static field id");
  // Keep JVMTI addition small and only check enabled flag here.
  // jni_GetField_probe() assumes that is okay to create handles.
  if (JvmtiExport::should_post_field_access()) {
    JvmtiExport::jni_GetField_probe(thread, NULL, NULL, id->holder(), fieldID, true);
  }
  return JNIHandles::make_local(id->holder()->obj_field(id->offset()));
JNI_END

#define DEFINE_GETSTATICFIELD(Return,Fieldname,Result) \
JNI_ENTRY(Return, jni_GetStatic##Result##Field(JNIEnv *env, jclass clazz, jfieldID fieldID)) \
  JNIWrapper("GetStatic" XSTR(Result) "Field"); \
  JNIid* id = jfieldIDWorkaround::from_static_jfieldID(fieldID); \
  assert(id->is_static_field_id(), "invalid static field id"); \
  /* Keep JVMTI addition small and only check enabled flag here. */ \
  /* jni_GetField_probe() assumes that is okay to create handles. */ \
  if (JvmtiExport::should_post_field_access()) { \
    JvmtiExport::jni_GetField_probe(thread, NULL, NULL, id->holder(), fieldID, true); \
  } \
  return id->holder()-> Fieldname##_field (id->offset()); \
JNI_END

DEFINE_GETSTATICFIELD(jboolean, bool,   Boolean)
DEFINE_GETSTATICFIELD(jbyte,    byte,   Byte)
DEFINE_GETSTATICFIELD(jchar,    char,   Char)
DEFINE_GETSTATICFIELD(jshort,   short,  Short)
DEFINE_GETSTATICFIELD(jint,     int,    Int)
DEFINE_GETSTATICFIELD(jlong,    long,   Long)
DEFINE_GETSTATICFIELD(jfloat,   float,  Float)
DEFINE_GETSTATICFIELD(jdouble,  double, Double)


JNI_ENTRY(void, jni_SetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value))
  JNIWrapper("SetStaticObjectField");
  JNIid* id = jfieldIDWorkaround::from_static_jfieldID(fieldID); \
  assert(id->is_static_field_id(), "invalid static field id"); \
  // Keep JVMTI addition small and only check enabled flag here.
  // jni_SetField_probe() assumes that is okay to create handles.
  if (JvmtiExport::should_post_field_modification()) {
    jvalue field_value;
    field_value.l = value;
    JvmtiExport::jni_SetField_probe(thread, NULL, NULL, id->holder(), fieldID, true, 'L', (jvalue *)&field_value);
  }
  id->holder()->obj_field_put(id->offset(), JNIHandles::resolve(value));
JNI_END


#define DEFINE_SETSTATICFIELD(Argument,Fieldname,Result,SigType,unionType) \
JNI_ENTRY(void, jni_SetStatic##Result##Field(JNIEnv *env, jclass clazz, jfieldID fieldID, Argument value)) \
  JNIWrapper("SetStatic" XSTR(Result) "Field"); \
  JNIid* id = jfieldIDWorkaround::from_static_jfieldID(fieldID); \
  assert(id->is_static_field_id(), "invalid static field id"); \
  /* Keep JVMTI addition small and only check enabled flag here. */ \
  /* jni_SetField_probe() assumes that is okay to create handles. */ \
  if (JvmtiExport::should_post_field_modification()) { \
    jvalue field_value; \
    field_value.unionType = value; \
    JvmtiExport::jni_SetField_probe(thread, NULL, NULL, id->holder(), fieldID, true, SigType, (jvalue *)&field_value); \
  } \
  id->holder()-> Fieldname##_field_put (id->offset(), value); \
JNI_END

DEFINE_SETSTATICFIELD(jboolean, bool,   Boolean, 'Z', z)
DEFINE_SETSTATICFIELD(jbyte,    byte,   Byte,    'B', b)
DEFINE_SETSTATICFIELD(jchar,    char,   Char,    'C', c)
DEFINE_SETSTATICFIELD(jshort,   short,  Short,   'S', s)
DEFINE_SETSTATICFIELD(jint,     int,    Int,     'I', i)
DEFINE_SETSTATICFIELD(jlong,    long,   Long,    'J', j)
DEFINE_SETSTATICFIELD(jfloat,   float,  Float,   'F', f)
DEFINE_SETSTATICFIELD(jdouble,  double, Double,  'D', d)


//
// String Operations
//

// Unicode Interface

JNI_ENTRY(jstring, jni_NewString(JNIEnv *env, const jchar *unicodeChars, jsize len))
  JNIWrapper("NewString");
  oop string=java_lang_String::create_oop_from_unicode((jchar*) unicodeChars, len, CHECK_0);
  return (jstring) JNIHandles::make_local(env, string);
JNI_END


JNI_QUICK_ENTRY(jsize, jni_GetStringLength(JNIEnv *env, jstring string))
  JNIWrapper("GetStringLength");
  return java_lang_String::length(JNIHandles::resolve_non_null(string));
JNI_END


JNI_QUICK_ENTRY(const jchar*, jni_GetStringChars(JNIEnv *env, jstring string, jboolean *isCopy))
  JNIWrapper("GetStringChars");
  //%note jni_5
  if (isCopy != NULL) {
    *isCopy = JNI_TRUE;
  }
  oop s = JNIHandles::resolve_non_null(string);
  int s_len = java_lang_String::length(s);
  typeArrayOop s_value = java_lang_String::value(s);
  int s_offset = java_lang_String::offset(s);
  jchar* buf = NEW_C_HEAP_ARRAY(jchar, s_len + 1);  // add one for zero termination
  if (s_len > 0) {
    memcpy(buf, s_value->char_at_addr(s_offset), sizeof(jchar)*s_len);
  }
  buf[s_len] = 0;
  return buf;
JNI_END


JNI_QUICK_ENTRY(void, jni_ReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars))
  JNIWrapper("ReleaseStringChars");
  //%note jni_6
  if (chars != NULL) {
    // Since String objects are supposed to be immutable, don't copy any
    // new data back.  A bad user will have to go after the char array.
    FreeHeap((void*) chars);
  }
JNI_END


// UTF Interface

JNI_ENTRY(jstring, jni_NewStringUTF(JNIEnv *env, const char *bytes))
  JNIWrapper("NewStringUTF");
  oop result = java_lang_String::create_oop_from_str((char*) bytes, CHECK_0);
  return (jstring) JNIHandles::make_local(env, result);
JNI_END


JNI_ENTRY(jsize, jni_GetStringUTFLength(JNIEnv *env, jstring string))
  JNIWrapper("GetStringUTFLength");
  return java_lang_String::utf8_length(JNIHandles::resolve_non_null(string));
JNI_END


JNI_ENTRY(const char*, jni_GetStringUTFChars(JNIEnv *env, jstring string, jboolean *isCopy))
  JNIWrapper("GetStringUTFChars");
  ResourceMark rm;
  char* str = java_lang_String::as_utf8_string(JNIHandles::resolve_non_null(string));
  int length = (int)strlen(str);
  char* result = AllocateHeap(length+1, "GetStringUTFChars");
  strcpy(result, str);
  if (isCopy != NULL) *isCopy = JNI_TRUE;
  return result;
JNI_END


JNI_LEAF(void, jni_ReleaseStringUTFChars(JNIEnv *env, jstring str, const char *chars))
  JNIWrapper("ReleaseStringUTFChars");
  if (chars != NULL) {
    FreeHeap((char*) chars);
  }
JNI_END


JNI_QUICK_ENTRY(jsize, jni_GetArrayLength(JNIEnv *env, jarray array))
  JNIWrapper("GetArrayLength");
  arrayOop a = arrayOop(JNIHandles::resolve_non_null(array));
  assert(a->is_array(), "must be array");
  return a->length();
JNI_END


//
// Object Array Operations
//

JNI_ENTRY(jobjectArray, jni_NewObjectArray(JNIEnv *env, jsize length, jclass elementClass, jobject initialElement))
  JNIWrapper("NewObjectArray");
  KlassHandle ek(THREAD, java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(elementClass)));
  klassOop ako = Klass::cast(ek())->array_klass(CHECK_0);
  KlassHandle ak = KlassHandle(THREAD, ako);
  objArrayKlass::cast(ak())->initialize(CHECK_0);
  objArrayOop result = objArrayKlass::cast(ak())->allocate(length, CHECK_0);
  oop initial_value = JNIHandles::resolve(initialElement);
  if (initial_value != NULL) {  // array already initialized with NULL
    for (int index = 0; index < length; index++) {
      result->obj_at_put(index, initial_value);
    }
  }
  return (jobjectArray) JNIHandles::make_local(env, result);
JNI_END


JNI_ENTRY(jobject, jni_GetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index))
  JNIWrapper("GetObjectArrayElement");
  objArrayOop a = objArrayOop(JNIHandles::resolve_non_null(array));
  if (a->is_within_bounds(index)) {
    return JNIHandles::make_local(env, a->obj_at(index));
  } else {
    THROW_0(vmSymbols::java_lang_ArrayIndexOutOfBoundsException());
  }
JNI_END


JNI_ENTRY(void, jni_SetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject value))
  JNIWrapper("SetObjectArrayElement");
  objArrayOop a = objArrayOop(JNIHandles::resolve_non_null(array));
  oop v = JNIHandles::resolve(value);
  if (a->is_within_bounds(index)) {
    if (v == NULL || v->is_a(objArrayKlass::cast(a->klass())->element_klass())) {
      a->obj_at_put(index, v);
    } else {
      THROW(vmSymbols::java_lang_ArrayStoreException());  
    }
  } else {
    THROW(vmSymbols::java_lang_ArrayIndexOutOfBoundsException());
  }
JNI_END


#define DEFINE_NEWSCALARARRAY(Return,Allocator,Result) \
JNI_ENTRY(Return, \
          jni_New##Result##Array(JNIEnv *env, jsize len)) \
  JNIWrapper("New" XSTR(Result) "Array"); \
  oop obj= oopFactory::Allocator(len, CHECK_0); \
  return (Return) JNIHandles::make_local(env, obj); \
JNI_END

DEFINE_NEWSCALARARRAY(jbooleanArray, new_boolArray,   Boolean)
DEFINE_NEWSCALARARRAY(jbyteArray,    new_byteArray,   Byte)
DEFINE_NEWSCALARARRAY(jshortArray,   new_shortArray,  Short)
DEFINE_NEWSCALARARRAY(jcharArray,    new_charArray,   Char)
DEFINE_NEWSCALARARRAY(jintArray,     new_intArray,    Int)
DEFINE_NEWSCALARARRAY(jlongArray,    new_longArray,   Long)
DEFINE_NEWSCALARARRAY(jfloatArray,   new_singleArray, Float)
DEFINE_NEWSCALARARRAY(jdoubleArray,  new_doubleArray, Double)


// Return an address which will fault if the caller writes to it.

static char* get_bad_address() {
  static char* bad_address = NULL;
  if (bad_address == NULL) {
    size_t size = os::vm_allocation_granularity();
    bad_address = os::reserve_memory(size);
    if (bad_address != NULL) {
      os::commit_memory(bad_address, size);
      os::protect_memory(bad_address, size);
    }
  }
  return bad_address;
}


#define DEFINE_GETSCALARARRAYELEMENTS(ElementTag,ElementType,Result, Tag) \
JNI_QUICK_ENTRY(ElementType*, \
          jni_Get##Result##ArrayElements(JNIEnv *env, ElementType##Array array, jboolean *isCopy)) \
  JNIWrapper("Get" XSTR(Result) "ArrayElements"); \
  /* allocate an chunk of memory in c land */ \
  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  ElementType* result; \
  int len = a->length(); \
  if (len == 0) { \
    /* Empty array: legal but useless, can't return NULL. \
     * Return a pointer to something useless. \
     * Avoid asserts in typeArrayOop. */ \
    result = (ElementType*)get_bad_address(); \
  } else { \
    result = NEW_C_HEAP_ARRAY(ElementType, len); \
    /* copy the array to the c chunk */ \
    memcpy(result, a->Tag##_at_addr(0), sizeof(ElementType)*len); \
  } \
  if (isCopy) *isCopy = JNI_TRUE; \
  return result; \
JNI_END

DEFINE_GETSCALARARRAYELEMENTS(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_GETSCALARARRAYELEMENTS(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_GETSCALARARRAYELEMENTS(T_SHORT,   jshort,   Short,   short)
DEFINE_GETSCALARARRAYELEMENTS(T_CHAR,    jchar,    Char,    char)
DEFINE_GETSCALARARRAYELEMENTS(T_INT,     jint,     Int,     int)
DEFINE_GETSCALARARRAYELEMENTS(T_LONG,    jlong,    Long,    long)
DEFINE_GETSCALARARRAYELEMENTS(T_FLOAT,   jfloat,   Float,   float)
DEFINE_GETSCALARARRAYELEMENTS(T_DOUBLE,  jdouble,  Double,  double)


#define DEFINE_RELEASESCALARARRAYELEMENTS(ElementTag,ElementType,Result,Tag) \
JNI_QUICK_ENTRY(void, \
          jni_Release##Result##ArrayElements(JNIEnv *env, ElementType##Array array, \
                                             ElementType *buf, jint mode)) \
  JNIWrapper("Release" XSTR(Result) "ArrayElements"); \
  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  int len = a->length(); \
  if (len != 0) {   /* Empty array:  nothing to free or copy. */  \
    if ((mode == 0) || (mode == JNI_COMMIT)) { \
      memcpy(a->Tag##_at_addr(0), buf, sizeof(ElementType)*len); \
    } \
    if ((mode == 0) || (mode == JNI_ABORT)) { \
      FreeHeap(buf); \
    } \
  } \
JNI_END

DEFINE_RELEASESCALARARRAYELEMENTS(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_RELEASESCALARARRAYELEMENTS(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_RELEASESCALARARRAYELEMENTS(T_SHORT,   jshort,   Short,   short)
DEFINE_RELEASESCALARARRAYELEMENTS(T_CHAR,    jchar,    Char,    char)
DEFINE_RELEASESCALARARRAYELEMENTS(T_INT,     jint,     Int,     int)
DEFINE_RELEASESCALARARRAYELEMENTS(T_LONG,    jlong,    Long,    long)
DEFINE_RELEASESCALARARRAYELEMENTS(T_FLOAT,   jfloat,   Float,   float)
DEFINE_RELEASESCALARARRAYELEMENTS(T_DOUBLE,  jdouble,  Double,  double)

#define DEFINE_GETSCALARARRAYREGION(ElementTag,ElementType,Result, Tag) \
JNI_ENTRY(void, \
jni_Get##Result##ArrayRegion(JNIEnv *env, ElementType##Array array, jsize start, \
			       jsize len, ElementType *buf)) \
  JNIWrapper("Get" XSTR(Result) "ArrayRegion"); \
  typeArrayOop src = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  if (start < 0 || len < 0 || ((unsigned int)start + (unsigned int)len > (unsigned int)src->length())) { \
    THROW(vmSymbols::java_lang_ArrayIndexOutOfBoundsException()); \
  } else { \
    if (len > 0) { \
      int sc = typeArrayKlass::cast(src->klass())->scale(); \
      memcpy((u_char*) buf, \
             (u_char*) src->Tag##_at_addr(start), \
             len * sc); \
    } \
  } \
JNI_END

DEFINE_GETSCALARARRAYREGION(T_BOOLEAN, jboolean,Boolean, bool)
DEFINE_GETSCALARARRAYREGION(T_BYTE,    jbyte,   Byte,    byte)
DEFINE_GETSCALARARRAYREGION(T_SHORT,   jshort,  Short,   short)
DEFINE_GETSCALARARRAYREGION(T_CHAR,    jchar,   Char,    char)
DEFINE_GETSCALARARRAYREGION(T_INT,     jint,    Int,     int)
DEFINE_GETSCALARARRAYREGION(T_LONG,    jlong,   Long,    long)
DEFINE_GETSCALARARRAYREGION(T_FLOAT,   jfloat,  Float,   float)
DEFINE_GETSCALARARRAYREGION(T_DOUBLE,  jdouble, Double,  double)

#define DEFINE_SETSCALARARRAYREGION(ElementTag,ElementType,Result, Tag) \
JNI_ENTRY(void, \
jni_Set##Result##ArrayRegion(JNIEnv *env, ElementType##Array array, jsize start, \
			       jsize len, const ElementType *buf)) \
  JNIWrapper("Set" XSTR(Result) "ArrayRegion"); \
  typeArrayOop dst = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  if (start < 0 || len < 0 || ((unsigned int)start + (unsigned int)len > (unsigned int)dst->length())) { \
    THROW(vmSymbols::java_lang_ArrayIndexOutOfBoundsException()); \
  } else { \
    if (len > 0) { \
      int sc = typeArrayKlass::cast(dst->klass())->scale(); \
      memcpy((u_char*) dst->Tag##_at_addr(start), \
             (u_char*) buf, \
             len * sc); \
    } \
  } \
JNI_END

DEFINE_SETSCALARARRAYREGION(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_SETSCALARARRAYREGION(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_SETSCALARARRAYREGION(T_SHORT,   jshort,   Short,   short)
DEFINE_SETSCALARARRAYREGION(T_CHAR,    jchar,    Char,    char)
DEFINE_SETSCALARARRAYREGION(T_INT,     jint,     Int,     int)
DEFINE_SETSCALARARRAYREGION(T_LONG,    jlong,    Long,    long)
DEFINE_SETSCALARARRAYREGION(T_FLOAT,   jfloat,   Float,   float)
DEFINE_SETSCALARARRAYREGION(T_DOUBLE,  jdouble,  Double,  double)


//
// Interception of natives
//

static bool register_native(KlassHandle k, symbolHandle name, symbolHandle signature, address entry, TRAPS) {
  methodOop method = Klass::cast(k())->lookup_method(name(), signature());
  if (method == NULL || !method->is_native()) {
    ResourceMark rm;
    THROW_MSG_(vmSymbols::java_lang_NoSuchMethodError(), 
               methodOopDesc::name_and_sig_as_C_string(Klass::cast(k()), name(), signature()),
               false);
  }
  if (entry != NULL) {
    method->set_native_function(entry);
  } else {
    method->clear_native_function();
  }
  if (PrintJNIResolving) {
    ResourceMark rm(THREAD);
    tty->print_cr("[Registering JNI native method %s.%s]", 
      Klass::cast(method->method_holder())->external_name(), 
      method->name()->as_C_string());
  }
  return true;
}


JNI_ENTRY(jint, jni_RegisterNatives(JNIEnv *env, jclass clazz,
                                    const JNINativeMethod *methods, 
                                    jint nMethods))  
  JNIWrapper("RegisterNatives");
  KlassHandle h_k(thread, java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz)));

  for (int index = 0; index < nMethods; index++) {
    const char* meth_name = methods[index].name;
    const char* meth_sig = methods[index].signature;

    // The class should have been loaded (we have an instance of the class
    // passed in) so the method and signature should already be in the symbol
    // table.  If they're not there, the method doesn't exist.
    symbolHandle name(THREAD, 
                      SymbolTable::probe(meth_name, (int)strlen(meth_name)));
    symbolHandle signature(THREAD, 
                           SymbolTable::probe(meth_sig, (int)strlen(meth_sig)));

    if (name == NULL || signature == NULL) {
      // Must return negative value on failure
      THROW_MSG_(vmSymbols::java_lang_NoSuchMethodError(), meth_name, -1);
    }

    bool res = register_native(h_k, name, signature, 
                               (address) methods[index].fnPtr, THREAD);
    if (!res) return -1;
  }
  return 0;
JNI_END


JNI_ENTRY(jint, jni_UnregisterNatives(JNIEnv *env, jclass clazz))  
  JNIWrapper("UnregisterNatives");
  klassOop k   = java_lang_Class::as_klassOop(JNIHandles::resolve_non_null(clazz));
  //%note jni_2
  if (Klass::cast(k)->oop_is_instance()) {
    for (int index = 0; index < instanceKlass::cast(k)->methods()->length(); index++) {
      methodOop m = methodOop(instanceKlass::cast(k)->methods()->obj_at(index));
      if (m->is_native()) {
        m->clear_native_function();
        m->set_signature_handler(NULL);
      }
    }
  }
  return 0;
JNI_END

//
// Monitor functions
//

JNI_ENTRY(jint, jni_MonitorEnter(JNIEnv *env, jobject jobj))

  // If the object is null, we can't do anything with it
  if (jobj == NULL) {
    THROW_(vmSymbols::java_lang_NullPointerException(), JNI_ERR);
  }

  Handle obj(thread, JNIHandles::resolve_non_null(jobj));
  ObjectSynchronizer::jni_enter(obj, CHECK_(JNI_ERR));
  return JNI_OK;

JNI_END

JNI_ENTRY(jint, jni_MonitorExit(JNIEnv *env, jobject jobj))

  // Don't do anything with a null object
  if (jobj == NULL) {
    THROW_(vmSymbols::java_lang_NullPointerException(), JNI_ERR);
  }

  Handle obj(THREAD, JNIHandles::resolve_non_null(jobj));
  ObjectSynchronizer::jni_exit(obj(), CHECK_(JNI_ERR));

  return JNI_OK;
JNI_END

//
// Extensions
//

JNI_ENTRY(void, jni_GetStringRegion(JNIEnv *env, jstring string, jsize start, jsize len, jchar *buf))
  JNIWrapper("GetStringRegion");
  oop s = JNIHandles::resolve_non_null(string);
  int s_len = java_lang_String::length(s);
  if (start < 0 || len < 0 || start + len > s_len) {
    THROW(vmSymbols::java_lang_StringIndexOutOfBoundsException());
  } else {
    if (len > 0) {
      int s_offset = java_lang_String::offset(s);
      typeArrayOop s_value = java_lang_String::value(s);
      memcpy(buf, s_value->char_at_addr(s_offset+start), sizeof(jchar)*len);
    }
  }
JNI_END


JNI_ENTRY(void, jni_GetStringUTFRegion(JNIEnv *env, jstring string, jsize start, jsize len, char *buf))
  JNIWrapper("GetStringUTFRegion");
  oop s = JNIHandles::resolve_non_null(string);
  int s_len = java_lang_String::length(s);
  if (start < 0 || len < 0 || start + len > s_len) {
    THROW(vmSymbols::java_lang_StringIndexOutOfBoundsException());
  } else {
    //%note jni_7
    if (len > 0) {
      ResourceMark rm(THREAD);
      char *utf_region = java_lang_String::as_utf8_string(s, start, len);
      int utf_len = (int)strlen(utf_region);
      memcpy(buf, utf_region, utf_len);
      buf[utf_len] = 0;
    } else {
      // JDK null-terminates the buffer even in len is zero
      if (buf != NULL) {
        buf[0] = 0; 
      }
    }
  }
JNI_END


JNI_ENTRY(void*, jni_GetPrimitiveArrayCritical(JNIEnv *env, jarray array, jboolean *isCopy))
  JNIWrapper("GetPrimitiveArrayCritical");
  GC_locker::lock_critical(thread);
  if (isCopy != NULL) {
    *isCopy = JNI_FALSE;
  }
  oop a = JNIHandles::resolve_non_null(array);
  assert(a->is_array(), "just checking");
  BasicType type;
  if (a->is_objArray()) {
    type = T_OBJECT;
  } else {
    type = typeArrayKlass::cast(a->klass())->type();
  }
  return arrayOop(a)->base(type);
JNI_END


JNI_ENTRY(void, jni_ReleasePrimitiveArrayCritical(JNIEnv *env, jarray array, void *carray, jint mode))
  JNIWrapper("ReleasePrimitiveArrayCritical");
  // The array, carray and mode arguments are ignored
  GC_locker::unlock_critical(thread);
JNI_END


JNI_ENTRY(const jchar*, jni_GetStringCritical(JNIEnv *env, jstring string, jboolean *isCopy))
  JNIWrapper("GetStringCritical");
  GC_locker::lock_critical(thread);
  if (isCopy != NULL) {
    *isCopy = JNI_FALSE;
  }
  oop s = JNIHandles::resolve_non_null(string);
  int s_len = java_lang_String::length(s);
  typeArrayOop s_value = java_lang_String::value(s);
  int s_offset = java_lang_String::offset(s);
  if (s_len > 0) {
    return s_value->char_at_addr(s_offset);
  } else {
    return (jchar*) s_value->base(T_CHAR);
  }
JNI_END


JNI_ENTRY(void, jni_ReleaseStringCritical(JNIEnv *env, jstring str, const jchar *chars))
  JNIWrapper("ReleaseStringCritical");
  // The str and chars arguments are ignored
  GC_locker::unlock_critical(thread);
JNI_END


JNI_ENTRY(jweak, jni_NewWeakGlobalRef(JNIEnv *env, jobject ref))
  JNIWrapper("jni_NewWeakGlobalRef");
  Handle ref_handle(thread, JNIHandles::resolve(ref));
  return JNIHandles::make_weak_global(ref_handle);
JNI_END

// Must be JNI_ENTRY (with HandleMark) 
// When jvmpi is enabled, the agent might allocate handles on receiving 
// JNI_WEAK_GLOBALREF_FREE event.
JNI_ENTRY(void, jni_DeleteWeakGlobalRef(JNIEnv *env, jweak ref))
  JNIWrapper("jni_DeleteWeakGlobalRef");
  JNIHandles::destroy_weak_global(ref);
JNI_END


JNI_QUICK_ENTRY(jboolean, jni_ExceptionCheck(JNIEnv *env))
  JNIWrapper("jni_ExceptionCheck");
  jni_check_async_exceptions(thread);
  return (thread->has_pending_exception()) ? JNI_TRUE : JNI_FALSE;
JNI_END


// Initialization state for three routines below relating to
// java.nio.DirectBuffers
static          jint directBufferSupportInitializeStarted = 0;
static volatile jint directBufferSupportInitializeEnded   = 0;
static volatile jint directBufferSupportInitializeFailed  = 0;
static jclass    bufferClass                 = NULL;
static jclass    directBufferClass           = NULL;
static jclass    directByteBufferClass       = NULL;
static jmethodID directByteBufferConstructor = NULL;
static jfieldID  directBufferAddressField    = NULL;
static jfieldID  bufferCapacityField         = NULL;

static jclass lookupOne(JNIEnv* env, const char* name, TRAPS) {
  Handle loader;            // null (bootstrap) loader
  Handle protection_domain; // null protection domain

  symbolHandle sym = oopFactory::new_symbol_handle(name, CHECK_0);
  return find_class_from_class_loader(env, sym, true, loader, protection_domain, true, CHECK_0);
}

// These lookups are done with the NULL (bootstrap) ClassLoader to
// circumvent any security checks that would be done by jni_FindClass.
JNI_ENTRY(bool, lookupDirectBufferClasses(JNIEnv* env))
{
  if ((bufferClass           = lookupOne(env, "java/nio/Buffer", thread))           == NULL) { return false; }
  if ((directBufferClass     = lookupOne(env, "sun/nio/ch/DirectBuffer", thread))   == NULL) { return false; }
  if ((directByteBufferClass = lookupOne(env, "java/nio/DirectByteBuffer", thread)) == NULL) { return false; }
  return true;
}
JNI_END


static bool initializeDirectBufferSupport(JNIEnv* env) {
  if (directBufferSupportInitializeFailed) {
    return false;
  }

  if (Atomic::cmpxchg(1, &directBufferSupportInitializeStarted, 0) == 0) {
    if (!lookupDirectBufferClasses(env)) {
      directBufferSupportInitializeFailed = 1;
      return false;
    }

    // Make global references for these
    bufferClass           = (jclass) env->NewGlobalRef(bufferClass);
    directBufferClass     = (jclass) env->NewGlobalRef(directBufferClass);
    directByteBufferClass = (jclass) env->NewGlobalRef(directByteBufferClass);

    // Get needed field and method IDs
    directByteBufferConstructor = env->GetMethodID(directByteBufferClass, "<init>", "(JI)V");
    directBufferAddressField    = env->GetFieldID(bufferClass, "address", "J");
    bufferCapacityField         = env->GetFieldID(bufferClass, "capacity", "I");

    if ((directByteBufferConstructor == NULL) ||
        (directBufferAddressField    == NULL) ||
        (bufferCapacityField         == NULL)) {
      directBufferSupportInitializeFailed = 1;
      return false;
    }
    
    directBufferSupportInitializeEnded = 1;
  } else {
    while (!directBufferSupportInitializeEnded && !directBufferSupportInitializeFailed) {
      os::yield_all();
    }
  }

  return !directBufferSupportInitializeFailed;
}

extern "C" jobject JNICALL jni_NewDirectByteBuffer(JNIEnv *env, void* address, jlong capacity)
{
  // thread_from_jni_environment() will block if VM is gone.
  JavaThread* thread = JavaThread::thread_from_jni_environment(env);

  JNIWrapper("jni_NewDirectByteBuffer");

  if (!directBufferSupportInitializeEnded) {
    if (!initializeDirectBufferSupport(env)) {
      return NULL;
    }
  }

  // Being paranoid about accidental sign extension on address
  jlong addr = (jlong) ((uintptr_t) address);
  // NOTE that package-private DirectByteBuffer constructor currently
  // takes int capacity
  jint  cap  = (jint)  capacity;
  return env->NewObject(directByteBufferClass, directByteBufferConstructor, addr, cap);
}


extern "C" void* JNICALL jni_GetDirectBufferAddress(JNIEnv *env, jobject buf)
{
  // thread_from_jni_environment() will block if VM is gone.
  JavaThread* thread = JavaThread::thread_from_jni_environment(env);

  JNIWrapper("jni_GetDirectBufferAddress");

  if (!directBufferSupportInitializeEnded) {
    if (!initializeDirectBufferSupport(env)) {
      return 0;
    }
  }

  if ((buf != NULL) && (!env->IsInstanceOf(buf, directBufferClass))) {
    return 0;
  }

  return (void*) env->GetLongField(buf, directBufferAddressField);
}


extern "C" jlong JNICALL jni_GetDirectBufferCapacity(JNIEnv *env, jobject buf)
{
  // thread_from_jni_environment() will block if VM is gone.
  JavaThread* thread = JavaThread::thread_from_jni_environment(env);

  JNIWrapper("jni_GetDirectBufferCapacity");

  if (!directBufferSupportInitializeEnded) {
    if (!initializeDirectBufferSupport(env)) {
      return 0;
    }
  }

  if (buf == NULL) {
    return -1;
  }

  if (!env->IsInstanceOf(buf, directBufferClass)) {
    return -1;
  }

  // NOTE that capacity is currently an int in the implementation
  return env->GetIntField(buf, bufferCapacityField);
}


JNI_LEAF(jint, jni_GetVersion(JNIEnv *env))
  JNIWrapper("GetVersion");
  return CurrentVersion;
JNI_END

extern struct JavaVM_ main_vm;

JNI_LEAF(jint, jni_GetJavaVM(JNIEnv *env, JavaVM **vm))
  JNIWrapper("jni_GetJavaVM");
  *vm  = (JavaVM *)(&main_vm);
  return JNI_OK;
JNI_END

// Structure containing all jni functions
struct JNINativeInterface_ jni_NativeInterface = {
    NULL,
    NULL,
    NULL,

    NULL,

    jni_GetVersion,

    jni_DefineClass,
    jni_FindClass,

    jni_FromReflectedMethod,
    jni_FromReflectedField,

    jni_ToReflectedMethod,

    jni_GetSuperclass,
    jni_IsAssignableFrom,

    jni_ToReflectedField,

    jni_Throw,
    jni_ThrowNew,
    jni_ExceptionOccurred,
    jni_ExceptionDescribe,
    jni_ExceptionClear,
    jni_FatalError,

    jni_PushLocalFrame,
    jni_PopLocalFrame,

    jni_NewGlobalRef,
    jni_DeleteGlobalRef,
    jni_DeleteLocalRef,
    jni_IsSameObject,

    jni_NewLocalRef,
    jni_EnsureLocalCapacity,

    jni_AllocObject,
    jni_NewObject,
    jni_NewObjectV,
    jni_NewObjectA,

    jni_GetObjectClass,
    jni_IsInstanceOf,

    jni_GetMethodID,

    jni_CallObjectMethod,
    jni_CallObjectMethodV,
    jni_CallObjectMethodA,
    jni_CallBooleanMethod,
    jni_CallBooleanMethodV,
    jni_CallBooleanMethodA,
    jni_CallByteMethod,
    jni_CallByteMethodV,
    jni_CallByteMethodA,
    jni_CallCharMethod,
    jni_CallCharMethodV,
    jni_CallCharMethodA,
    jni_CallShortMethod,
    jni_CallShortMethodV,
    jni_CallShortMethodA,
    jni_CallIntMethod,
    jni_CallIntMethodV,
    jni_CallIntMethodA,
    jni_CallLongMethod,
    jni_CallLongMethodV,
    jni_CallLongMethodA,
    jni_CallFloatMethod,
    jni_CallFloatMethodV,
    jni_CallFloatMethodA,
    jni_CallDoubleMethod,
    jni_CallDoubleMethodV,
    jni_CallDoubleMethodA,
    jni_CallVoidMethod,
    jni_CallVoidMethodV,
    jni_CallVoidMethodA,

    jni_CallNonvirtualObjectMethod,
    jni_CallNonvirtualObjectMethodV,
    jni_CallNonvirtualObjectMethodA,
    jni_CallNonvirtualBooleanMethod,
    jni_CallNonvirtualBooleanMethodV,
    jni_CallNonvirtualBooleanMethodA,
    jni_CallNonvirtualByteMethod,
    jni_CallNonvirtualByteMethodV,
    jni_CallNonvirtualByteMethodA,
    jni_CallNonvirtualCharMethod,
    jni_CallNonvirtualCharMethodV,
    jni_CallNonvirtualCharMethodA,
    jni_CallNonvirtualShortMethod,
    jni_CallNonvirtualShortMethodV,
    jni_CallNonvirtualShortMethodA,
    jni_CallNonvirtualIntMethod,
    jni_CallNonvirtualIntMethodV,
    jni_CallNonvirtualIntMethodA,
    jni_CallNonvirtualLongMethod,
    jni_CallNonvirtualLongMethodV,
    jni_CallNonvirtualLongMethodA,
    jni_CallNonvirtualFloatMethod,
    jni_CallNonvirtualFloatMethodV,
    jni_CallNonvirtualFloatMethodA,
    jni_CallNonvirtualDoubleMethod,
    jni_CallNonvirtualDoubleMethodV,
    jni_CallNonvirtualDoubleMethodA,
    jni_CallNonvirtualVoidMethod,
    jni_CallNonvirtualVoidMethodV,
    jni_CallNonvirtualVoidMethodA,

    jni_GetFieldID,

    jni_GetObjectField,
    jni_GetBooleanField,
    jni_GetByteField,
    jni_GetCharField,
    jni_GetShortField,
    jni_GetIntField,
    jni_GetLongField,
    jni_GetFloatField,
    jni_GetDoubleField,

    jni_SetObjectField,
    jni_SetBooleanField,
    jni_SetByteField,
    jni_SetCharField,
    jni_SetShortField,
    jni_SetIntField,
    jni_SetLongField,
    jni_SetFloatField,
    jni_SetDoubleField,

    jni_GetStaticMethodID,

    jni_CallStaticObjectMethod,
    jni_CallStaticObjectMethodV,
    jni_CallStaticObjectMethodA,
    jni_CallStaticBooleanMethod,
    jni_CallStaticBooleanMethodV,
    jni_CallStaticBooleanMethodA,
    jni_CallStaticByteMethod,
    jni_CallStaticByteMethodV,
    jni_CallStaticByteMethodA,
    jni_CallStaticCharMethod,
    jni_CallStaticCharMethodV,
    jni_CallStaticCharMethodA,
    jni_CallStaticShortMethod,
    jni_CallStaticShortMethodV,
    jni_CallStaticShortMethodA,
    jni_CallStaticIntMethod,
    jni_CallStaticIntMethodV,
    jni_CallStaticIntMethodA,
    jni_CallStaticLongMethod,
    jni_CallStaticLongMethodV,
    jni_CallStaticLongMethodA,
    jni_CallStaticFloatMethod,
    jni_CallStaticFloatMethodV,
    jni_CallStaticFloatMethodA,
    jni_CallStaticDoubleMethod,
    jni_CallStaticDoubleMethodV,
    jni_CallStaticDoubleMethodA,
    jni_CallStaticVoidMethod,
    jni_CallStaticVoidMethodV,
    jni_CallStaticVoidMethodA,

    jni_GetStaticFieldID,

    jni_GetStaticObjectField,
    jni_GetStaticBooleanField,
    jni_GetStaticByteField,
    jni_GetStaticCharField,
    jni_GetStaticShortField,
    jni_GetStaticIntField,
    jni_GetStaticLongField,
    jni_GetStaticFloatField,
    jni_GetStaticDoubleField,

    jni_SetStaticObjectField,
    jni_SetStaticBooleanField,
    jni_SetStaticByteField,
    jni_SetStaticCharField,
    jni_SetStaticShortField,
    jni_SetStaticIntField,
    jni_SetStaticLongField,
    jni_SetStaticFloatField,
    jni_SetStaticDoubleField,

    jni_NewString,
    jni_GetStringLength,
    jni_GetStringChars,
    jni_ReleaseStringChars,

    jni_NewStringUTF,
    jni_GetStringUTFLength,
    jni_GetStringUTFChars,
    jni_ReleaseStringUTFChars,

    jni_GetArrayLength,
 
    jni_NewObjectArray,
    jni_GetObjectArrayElement,
    jni_SetObjectArrayElement,

    jni_NewBooleanArray,
    jni_NewByteArray,
    jni_NewCharArray,
    jni_NewShortArray,
    jni_NewIntArray,
    jni_NewLongArray,
    jni_NewFloatArray,
    jni_NewDoubleArray,

    jni_GetBooleanArrayElements,
    jni_GetByteArrayElements,
    jni_GetCharArrayElements,
    jni_GetShortArrayElements,
    jni_GetIntArrayElements,
    jni_GetLongArrayElements,
    jni_GetFloatArrayElements,
    jni_GetDoubleArrayElements,

    jni_ReleaseBooleanArrayElements,
    jni_ReleaseByteArrayElements,
    jni_ReleaseCharArrayElements,
    jni_ReleaseShortArrayElements,
    jni_ReleaseIntArrayElements,
    jni_ReleaseLongArrayElements,
    jni_ReleaseFloatArrayElements,
    jni_ReleaseDoubleArrayElements,

    jni_GetBooleanArrayRegion,
    jni_GetByteArrayRegion,
    jni_GetCharArrayRegion,
    jni_GetShortArrayRegion,
    jni_GetIntArrayRegion,
    jni_GetLongArrayRegion,
    jni_GetFloatArrayRegion,
    jni_GetDoubleArrayRegion,

    jni_SetBooleanArrayRegion,
    jni_SetByteArrayRegion,
    jni_SetCharArrayRegion,
    jni_SetShortArrayRegion,
    jni_SetIntArrayRegion,
    jni_SetLongArrayRegion,
    jni_SetFloatArrayRegion,
    jni_SetDoubleArrayRegion,

    jni_RegisterNatives,
    jni_UnregisterNatives,

    jni_MonitorEnter,
    jni_MonitorExit,

    jni_GetJavaVM,

    jni_GetStringRegion,
    jni_GetStringUTFRegion,

    jni_GetPrimitiveArrayCritical,
    jni_ReleasePrimitiveArrayCritical,

    jni_GetStringCritical,
    jni_ReleaseStringCritical,

    jni_NewWeakGlobalRef,
    jni_DeleteWeakGlobalRef,

    jni_ExceptionCheck,

    jni_NewDirectByteBuffer,
    jni_GetDirectBufferAddress,
    jni_GetDirectBufferCapacity
};


// For jvmti use to modify jni function table.
// Java threads in native contiues to run until it is transitioned
// to VM at safepoint. Before the transition or before it is blocked
// for safepoint it may access jni function table. VM could crash if
// any java thread access the jni function table in the middle of memcpy.
// To avoid this each function pointers are copied automically.
void copy_jni_function_table(const struct JNINativeInterface_ *new_jni_NativeInterface) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");
  intptr_t *a = (intptr_t *) &jni_NativeInterface;
  intptr_t *b = (intptr_t *) new_jni_NativeInterface;
  for (int i=0; i <  sizeof(struct JNINativeInterface_)/sizeof(void *); i++) {
    Atomic::store_ptr(*b++, a++);
  }
}

void quicken_jni_functions() {
  // Replace Get<Primitive>Field with fast versions
  if (UseFastJNIAccessors && !JvmtiExport::can_post_field_access()
      && !VerifyJNIFields && !TraceJNICalls && !CountJNICalls && !CheckJNICalls
#if defined(_WINDOWS) && defined(IA32) && defined(COMPILER2)
      // windows x86 currently needs SEH wrapper and the gain of the fast
      // versions currently isn't certain for server vm on uniprocessor.
      && os::is_MP()
#endif
  ) {
    address func;
    func = JNI_FastGetField::generate_fast_get_boolean_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetBooleanField = (GetBooleanField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_byte_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetByteField = (GetByteField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_char_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetCharField = (GetCharField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_short_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetShortField = (GetShortField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_int_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetIntField = (GetIntField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_long_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetLongField = (GetLongField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_float_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetFloatField = (GetFloatField_t)func;
    }
    func = JNI_FastGetField::generate_fast_get_double_field();
    if (func != (address)-1) {
      jni_NativeInterface.GetDoubleField = (GetDoubleField_t)func;
    }
  }
}

// Returns the function structure
const struct JNINativeInterface_* jni_functions() {
  if (CheckJNICalls) return jni_functions_check();
  return (const struct JNINativeInterface_*)&jni_NativeInterface;
}

// Returns the function structure
const struct JNINativeInterface_* jni_functions_nocheck() {
  return (const struct JNINativeInterface_*)&jni_NativeInterface;
}


// Invocation API


// Forward declaration
extern const struct JNIInvokeInterface_ jni_InvokeInterface;

// Global invocation API vars
jint vm_created = 0;
volatile jint num_vm_created = 0;     // number of VMs created so far
struct JavaVM_ main_vm = {&jni_InvokeInterface};


#define JAVASTACKSIZE (400 * 1024)    /* Default size of a thread java stack */
#define PROCSTACKSIZE 0               /* 0 means default size in HPI */
enum { VERIFY_NONE, VERIFY_REMOTE, VERIFY_ALL };



_JNI_IMPORT_OR_EXPORT_ jint JNICALL JNI_GetDefaultJavaVMInitArgs(void *args_) {
  JDK1_1InitArgs *args = (JDK1_1InitArgs *)args_;
  if (Threads::is_supported_jni_version(args->version)) {
    return JNI_OK;
  }
  // 1.1 style no longer supported in hotspot.
  return JNI_ERR;
}


_JNI_IMPORT_OR_EXPORT_ jint JNICALL JNI_CreateJavaVM(JavaVM **vm, void **penv, void *args) {
  // At the moment it's only possible to have one Java VM, 
  // since some of the runtime state is in global variables. 

  // We cannot use our mutex locks here, since they only work on
  // Threads. We do an atomic compare and exchange to ensure only
  // one thread can call this method at a time
  if (Atomic::add(1, &num_vm_created) > 1) {
    return JNI_ERR;
  }
  if (Atomic::xchg(1, &vm_created) == 1) {
    Atomic::dec(&num_vm_created);
    return JNI_ERR;
  }

  assert(vm_created == 1, "vm_created is true during the creation");

  /**
   * Certain errors during initialization are recoverable and do not
   * prevent this method from being called again at a later time 
   * (perhaps with different arguments).  However, at a certain 
   * point during initialization if an error occurs we cannot allow
   * this function to be called again (or it will crash).  In those
   * situations, the 'canTryAgain' flag is set to false, which disables
   * the decrementing of 'num_vm_created', such that any new call to 
   * JNI_CreateJavaVM will immediately fail using the above logic.
   */
  bool can_try_again = true;

  jint result = Threads::create_vm((JavaVMInitArgs*) args, &can_try_again);
  if (result == JNI_OK) {
    JavaThread *thread = JavaThread::current();
    /* thread is thread_in_vm here */
    *vm = (JavaVM *)(&main_vm);
    *(JNIEnv**)penv = thread->jni_environment();      

    // Tracks the time application was running before GC   
    RuntimeService::record_application_start();
   
    // Notify JVMTI
    if (JvmtiExport::should_post_thread_life()) {
       JvmtiExport::post_thread_start(thread);
    }
    // Check if we should compile all classes on bootclasspath
    NOT_PRODUCT(if (CompileTheWorld) ClassLoader::compile_the_world();)
    // Since this is not a JVM_ENTRY we have to set the thread state manually before leaving.
    ThreadStateTransition::transition(thread, _thread_in_vm, _thread_in_native); 
  } else {
    // Creation failed. We must reset vm_created
    vm_created = 0;
    *vm = 0;
    *(JNIEnv**)penv = 0;    
    if (can_try_again) {
      // Decrement only if it is ok to call JNI_CreateJavaVM again later.
      Atomic::dec(&num_vm_created);  
    }
  }

  return result;
}


_JNI_IMPORT_OR_EXPORT_ jint JNICALL JNI_GetCreatedJavaVMs(JavaVM **vm_buf, jsize bufLen, jsize *numVMs) {
  // See bug 4367188, the wrapper can sometimes cause VM crashes
  // JNIWrapper("GetCreatedJavaVMs");
  if (vm_created) {
    if (numVMs != NULL) *numVMs = 1;
    if (bufLen > 0)     *vm_buf = (JavaVM *)(&main_vm);
  } else {
    if (numVMs != NULL) *numVMs = 0;
  }
  return JNI_OK;
}

extern "C" {                 

jint JNICALL jni_DestroyJavaVM(JavaVM *vm) {
  if (!vm_created) return JNI_ERR;

  JNIWrapper("DestroyJavaVM");
  JNIEnv *env;
  JavaVMAttachArgs destroyargs;
  destroyargs.version = CurrentVersion;
  destroyargs.name = (char *)"DestroyJavaVM";
  destroyargs.group = NULL;
  jint res = vm->AttachCurrentThread((void **)&env, (void *)&destroyargs);
  if (res != JNI_OK) return res;

  // Since this is not a JVM_ENTRY we have to set the thread state manually before entering.
  JavaThread* thread = JavaThread::current();
  ThreadStateTransition::transition_from_native(thread, _thread_in_vm);
  if (Threads::destroy_vm()) {
    // Should not change thread state, VM is gone
    vm_created = false;
    return JNI_OK;
  } else {
    ThreadStateTransition::transition(thread, _thread_in_vm, _thread_in_native);
    return JNI_ERR;
  }
}


static jint attach_current_thread(JavaVM *vm, void **penv, void *_args, bool daemon) {
  JavaVMAttachArgs *args = (JavaVMAttachArgs *) _args;

  // Check below commented out from JDK1.2fcs as well
  /*
  if (args && (args->version != JNI_VERSION_1_1 || args->version != JNI_VERSION_1_2)) {
    return JNI_EVERSION;
  }
  */

  Thread* t = ThreadLocalStorage::get_thread_slow();
  if (t != NULL) {
    // If the thread has been attacted this operation is a no-op
    *(JNIEnv**)penv = ((JavaThread*) t)->jni_environment();
    return JNI_OK;
  }

  // Create a thread
  JavaThread* thread = new JavaThread();

  // Set correct safepoint info. The thread is going to call into Java when
  // initializing the Java level thread object. Hence, the correct state must
  // be set in order for the Safepoint code to deal with it correctly.  
  thread->set_thread_state(_thread_in_vm);    
  // Must do this before initialize_thread_local_storage
  thread->record_stack_base_and_size();    
  thread->initialize_thread_local_storage();

  if (!os::create_attached_thread(thread))
     return JNI_ERR;
  
  // Crucial that we do not have a safepoint check for this thread, since it has
  // not been added to the Thread list yet.
  { Threads_lock->lock_without_safepoint_check();    
    // This must be inside this lock in order to get FullGCALot to work properly, i.e., to
    // avoid this thread trying to do a GC before it is added to the thread-list
    thread->set_active_handles(JNIHandleBlock::allocate_block());  
    Threads::add(thread, daemon);
    Threads_lock->unlock();
  }
  // Create thread group and name info from attach arguments
  oop group = NULL;
  char* thread_name = NULL;
  if (args != NULL && Threads::is_supported_jni_version(args->version)) {
    group = JNIHandles::resolve(args->group);
    thread_name = args->name; // may be NULL
  }
  if (group == NULL) group = Universe::main_thread_group();

  // Create Java level thread object and attach it to this thread
  bool attach_failed = false;
  {
    EXCEPTION_MARK;
    HandleMark hm(THREAD);
    Handle thread_group(THREAD, group);
    thread->allocate_threadObj(thread_group, thread_name, daemon, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      CLEAR_PENDING_EXCEPTION;
      // cleanup outside the handle mark.
      attach_failed = true;
    }
  }

  if (attach_failed) {
    // Added missing cleanup
    thread->cleanup_failed_attach_current_thread();
    return JNI_ERR;
  }

  // Enable stack overflow checks
  thread->create_stack_guard_pages();

  // Set java thread status.
  java_lang_Thread::set_thread_status(thread->threadObj(), 
				      java_lang_Thread::RUNNABLE);

  // Notify the debugger
  if (JvmtiExport::should_post_thread_life()) {
    JvmtiExport::post_thread_start(thread);
  }
  // Notify jvmpi
  if (jvmpi::is_event_enabled(JVMPI_EVENT_THREAD_START)) {
    jvmpi::post_thread_start_event(thread);
  }
 
  *(JNIEnv**)penv = thread->jni_environment();

  // Now leaving the VM, so change thread_state. This is normally automatically taken care
  // of in the JVM_ENTRY. But in this situation we have to do it manually. Notice, that by
  // using ThreadStateTransition::transition, we do a callback to the safepoint code if 
  // needed.

  ThreadStateTransition::transition(thread, _thread_in_vm, _thread_in_native);

  return JNI_OK;
}


jint JNICALL jni_AttachCurrentThread(JavaVM *vm, void **penv, void *_args) {
  if (!vm_created) return JNI_ERR;

  JNIWrapper("AttachCurrentThread");
  return attach_current_thread(vm, penv, _args, false);
}


jint JNICALL jni_DetachCurrentThread(JavaVM *vm)  {
  VM_Exit::block_if_vm_exited();

  JNIWrapper("DetachCurrentThread");  

  // If the thread has been deattacted the operations is a no-op
  if (ThreadLocalStorage::thread() == NULL) return JNI_OK;

  JavaThread* thread = JavaThread::current();
  if (thread->has_last_Java_frame()) {
    // Can't detach a thread that's running java, that can't work.
    return JNI_ERR;
  }

  // Safepoint support. Have to do call-back to safepoint code, if in the
  // middel of a safepoint operation
  ThreadStateTransition::transition_from_native(thread, _thread_in_vm);
  
  // XXX: Note that JavaThread::exit() call below removes the guards on the
  // stack pages set up via enable_stack_{red,yellow}_zone() calls
  // above in jni_AttachCurrentThread. Unfortunately, while the setting
  // of the guards is visible in jni_AttachCurrentThread above,
  // the removal of the guards is buried below in JavaThread::exit()
  // here. The abstraction should be more symmetrically either exposed
  // or hidden (e.g. it could probably be hidden in the same
  // (platform-dependent) methods where we do alternate stack
  // maintenance work?)
  thread->exit(false);
  delete thread;

  return JNI_OK;
}


jint JNICALL jni_GetEnv(JavaVM *vm, void **penv, jint version) {
  if (!vm_created) {
     *penv = NULL;
     return JNI_EDETACHED;
  }

  if (JvmtiExport::is_jvmti_version(version)) {
    return JvmtiExport::get_jvmti_interface(vm, penv, version);
  }

  Thread* thread = ThreadLocalStorage::thread();
  if (thread != NULL && thread->is_Java_thread()) {
    if (Threads::is_supported_jni_version_including_1_1(version)) {
      *(JNIEnv**)penv = ((JavaThread*) thread)->jni_environment();
      return JNI_OK;

    } else if (version == JVMPI_VERSION_1 || 
               version == JVMPI_VERSION_1_1 ||
               (version == JVMPI_VERSION_1_2 && UseSuspendResumeThreadLists)) {
      *penv = (void* )jvmpi::GetInterface_1(version);	// version 1.X support
      return JNI_OK; 

    } else if (JvmtiExport::is_jvmdi_version(version)) {
      return JvmtiExport::get_jvmdi_interface(vm, penv, version);

    } else {
      *penv = NULL;
      return JNI_EVERSION;
    }
  } else {
    *penv = NULL;
    return JNI_EDETACHED;
  }
}


jint JNICALL jni_AttachCurrentThreadAsDaemon(JavaVM *vm, void **penv, void *_args) {
  if (!vm_created) return JNI_ERR;

  JNIWrapper("AttachCurrentThreadAsDaemon");
  return attach_current_thread(vm, penv, _args, true);
}


} // End extern "C"

const struct JNIInvokeInterface_ jni_InvokeInterface = {
    NULL,
    NULL,
    NULL,

    jni_DestroyJavaVM,
    jni_AttachCurrentThread,
    jni_DetachCurrentThread,
    jni_GetEnv,
    jni_AttachCurrentThreadAsDaemon
};

