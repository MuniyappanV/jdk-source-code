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

// VM_Version provides information about the VM.

class Abstract_VM_Version: AllStatic {
 protected:
  friend class VMStructs;
  static const char*  _s_vm_release;
  static const char*  _s_internal_vm_info_string;
  // These are set by machine-dependent initializations
  static bool         _supports_cx8;
  static unsigned int _logical_processors_per_package;
  static int          _vm_major_version;
  static int          _vm_minor_version;
  static int          _vm_build_number;
  static bool         _initialized;
  static int          _parallel_worker_threads;
  static bool         _parallel_worker_threads_initialized;

  static unsigned int nof_parallel_worker_threads(unsigned int num,
                                                  unsigned int dem,
                                                  unsigned int switch_pt);
 public:
  static void initialize();

  // Name
  static const char* vm_name();
  // Vendor
  static const char* vm_vendor();
  // VM version information string printed by launcher (java -version)
  static const char* vm_info_string();
  static const char* vm_release();
  static const char* vm_platform_string();

  static int vm_major_version()               { assert(_initialized, "not initialized"); return _vm_major_version; }
  static int vm_minor_version()               { assert(_initialized, "not initialized"); return _vm_minor_version; }
  static int vm_build_number()                { assert(_initialized, "not initialized"); return _vm_build_number; }

  // Gets the jvm_version_info.jvm_version defined in jvm.h
  static unsigned int jvm_version();

  // Internal version providing additional build information
  static const char* internal_vm_info_string();

  // does HW support an 8-byte compare-exchange operation?
  static bool supports_cx8()  {return _supports_cx8;}
  static unsigned int logical_processors_per_package() {
    return _logical_processors_per_package;
  }

  // Number of page sizes efficiently supported by the hardware.  Most chips now
  // support two sizes, thus this default implementation.  Processor-specific
  // subclasses should define new versions to hide this one as needed.  Note
  // that the O/S may support more sizes, but at most this many are used.
  static uint page_size_count() { return 2; }

  // Returns the number of parallel threads to be used for VM
  // work.  If that number has not been calculated, do so and
  // save it.  Returns ParallelGCThreads if it is set on the
  // command line.
  static unsigned int parallel_worker_threads();
  // Calculates and returns the number of parallel threads.  May
  // be VM version specific.
  static unsigned int calc_parallel_worker_threads();
};
