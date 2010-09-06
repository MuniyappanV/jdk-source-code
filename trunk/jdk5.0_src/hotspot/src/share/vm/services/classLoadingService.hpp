#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)classLoadingService.hpp	1.3 04/02/20 01:46:18 JVM"
#endif
//
// Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
// SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
//

class instanceKlass;

// VM monitoring and management support for the Class Loading subsystem
class ClassLoadingService : public AllStatic {
private:
  // Counters for classes loaded from class files
  static PerfCounter*  _classes_loaded_count;  
  static PerfCounter*  _classes_unloaded_count;
  static PerfCounter*  _classbytes_loaded;
  static PerfCounter*  _classbytes_unloaded;

  // Counters for classes loaded from shared archive
  static PerfCounter*  _shared_classes_loaded_count;
  static PerfCounter*  _shared_classes_unloaded_count;
  static PerfCounter*  _shared_classbytes_loaded;
  static PerfCounter*  _shared_classbytes_unloaded;

  static PerfVariable* _class_methods_size;
 
  static size_t compute_class_size(instanceKlass* k);

public:
  static void init();

  static bool get_verbose() { return TraceClassLoading; }
  static bool set_verbose(bool verbose);
  static void reset_trace_class_unloading();

  static jlong loaded_class_count() {
    return _classes_loaded_count->get_value() + _shared_classes_loaded_count->get_value();
  }
  static jlong unloaded_class_count() {
    return _classes_unloaded_count->get_value() + _shared_classes_unloaded_count->get_value();
  }
  static jlong loaded_class_bytes() {
    if (UsePerfData) {
      return _classbytes_loaded->get_value() + _shared_classbytes_loaded->get_value();
    } else {
      return -1;
    }
  }
  static jlong unloaded_class_bytes() {
    if (UsePerfData) {
      return _classbytes_unloaded->get_value() + _shared_classbytes_unloaded->get_value();
    } else {
      return -1;
    }
  }

  static jlong loaded_shared_class_count() {
    return _shared_classes_loaded_count->get_value();
  }
  static jlong unloaded_shared_class_count() {
    return _shared_classes_unloaded_count->get_value();
  }
  static jlong loaded_shared_class_bytes() {
    if (UsePerfData) {
      return _shared_classbytes_loaded->get_value();
    } else {
      return -1;
    }
  }
  static jlong unloaded_shared_class_bytes() {
    if (UsePerfData) {
      return _shared_classbytes_unloaded->get_value();
    } else {
      return -1;
    }
  }
  static jlong class_method_data_size() {
    return (UsePerfData ? _class_methods_size->get_value() : -1);
  }

  static void notify_class_loaded(instanceKlass* k, bool shared_class);
  static void notify_class_unloaded(instanceKlass* k, bool shared_class);
  static void add_class_method_size(int size) {
    if (UsePerfData) {
      _class_methods_size->inc(size);
    }
  }
};

// FIXME: make this piece of code to be shared by M&M and JVMTI
class LoadedClassesEnumerator : public StackObj {
private:
  static GrowableArray<KlassHandle>* _loaded_classes;
  // _current_thread is for creating a KlassHandle with a faster version constructor
  static Thread*                     _current_thread;

  GrowableArray<KlassHandle>* _klass_handle_array;

public:
  LoadedClassesEnumerator(Thread* cur_thread);

  int num_loaded_classes()         { return _klass_handle_array->length(); }
  KlassHandle get_klass(int index) { return _klass_handle_array->at(index); }

  static void add_loaded_class(klassOop k) {
    // FIXME: For now - don't include array klasses
    // The spec is unclear at this point to count array klasses or not
    // and also indirect creation of array of super class and secondaries
    //
    // for (klassOop l = k; l != NULL; l = Klass::cast(l)->array_klass_or_null()) {
    //  KlassHandle h(_current_thread, l);
    //  _loaded_classes->append(h);
    // }
    KlassHandle h(_current_thread, k);
    _loaded_classes->append(h);
  }
};

