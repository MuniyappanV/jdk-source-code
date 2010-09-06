#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)compileBroker.cpp	1.116 04/03/31 19:43:18 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_compileBroker.cpp.incl"

bool CompileBroker::_initialized = false;
volatile bool CompileBroker::_should_block = false;

// The installed compiler
AbstractCompiler* CompileBroker::_compiler = NULL;

// These counters are used for assigning id's to each compilation
uint CompileBroker::_compilation_id        = 0;
uint CompileBroker::_osr_compilation_id    = 0;
uint CompileBroker::_native_compilation_id = 0;

// Debugging information
int  CompileBroker::_last_compile_type     = no_compile;
char CompileBroker::_last_method_compiled[CompileBroker::name_buffer_length];

// Performance counters
PerfCounter* CompileBroker::_perf_total_compilation = NULL;
PerfCounter* CompileBroker::_perf_native_compilation = NULL;
PerfCounter* CompileBroker::_perf_osr_compilation = NULL;
PerfCounter* CompileBroker::_perf_standard_compilation = NULL;

PerfCounter* CompileBroker::_perf_total_bailout_count = NULL;
PerfCounter* CompileBroker::_perf_total_invalidated_count = NULL;
PerfCounter* CompileBroker::_perf_total_compile_count = NULL;
PerfCounter* CompileBroker::_perf_total_native_compile_count = NULL;
PerfCounter* CompileBroker::_perf_total_osr_compile_count = NULL;
PerfCounter* CompileBroker::_perf_total_standard_compile_count = NULL;

PerfCounter* CompileBroker::_perf_sum_osr_bytes_compiled = NULL;
PerfCounter* CompileBroker::_perf_sum_standard_bytes_compiled = NULL;
PerfCounter* CompileBroker::_perf_sum_nmethod_size = NULL;
PerfCounter* CompileBroker::_perf_sum_nmethod_code_size = NULL;

PerfStringVariable* CompileBroker::_perf_last_method = NULL;
PerfStringVariable* CompileBroker::_perf_last_failed_method = NULL;
PerfStringVariable* CompileBroker::_perf_last_invalidated_method = NULL;
PerfVariable*       CompileBroker::_perf_last_compile_type = NULL;
PerfVariable*       CompileBroker::_perf_last_compile_size = NULL;
PerfVariable*       CompileBroker::_perf_last_failed_type = NULL;
PerfVariable*       CompileBroker::_perf_last_invalidated_type = NULL;

// Timers and counters for generating statistics
elapsedTimer CompileBroker::_t_total_compilation;
elapsedTimer CompileBroker::_t_native_compilation;
elapsedTimer CompileBroker::_t_osr_compilation;
elapsedTimer CompileBroker::_t_standard_compilation;

int CompileBroker::_total_bailout_count          = 0;
int CompileBroker::_total_invalidated_count      = 0;
int CompileBroker::_total_compile_count          = 0;
int CompileBroker::_total_native_compile_count   = 0;
int CompileBroker::_total_osr_compile_count      = 0;
int CompileBroker::_total_standard_compile_count = 0;

int CompileBroker::_sum_osr_bytes_compiled       = 0;
int CompileBroker::_sum_standard_bytes_compiled  = 0;
int CompileBroker::_sum_nmethod_size             = 0;
int CompileBroker::_sum_nmethod_code_size        = 0;

CompileQueue* CompileBroker::_adapter_queue  = NULL;
CompileQueue* CompileBroker::_method_queue   = NULL;
CompileTask*  CompileBroker::_task_free_list = NULL;

CompilerThread*                 CompileBroker::_adapter_thread = NULL;
GrowableArray<CompilerThread*>* CompileBroker::_method_threads = NULL;

// CompileTaskWrapper
//
// Assign this task to the current thread.  Deallocate the task
// when the compilation is complete.
class CompileTaskWrapper : StackObj {
public:
  CompileTaskWrapper(CompileTask* task);
  ~CompileTaskWrapper();
};

CompileTaskWrapper::CompileTaskWrapper(CompileTask* task) {
  CompilerThread* thread = (CompilerThread*)JavaThread::current();
  thread->set_task(task);
  CompileLog*     log  = thread->log();
  if (log != NULL)  task->log_head(log);
}

CompileTaskWrapper::~CompileTaskWrapper() {
  CompilerThread* thread = (CompilerThread*)JavaThread::current();
  CompileTask* task = thread->task();
  CompileLog*  log  = thread->log();
  if (log != NULL)  task->log_tail(log);
  thread->set_task(NULL);
  if (task->is_blocking()) {
    MutexLocker notifier(task->lock(), thread);
    task->mark_complete();
    // Notify the waiting thread that the compilation has completed.
    task->lock()->notify_all();
  } else {
    task->mark_complete();

    // By convention, the compiling thread is responsible for
    // recycling a non-blocking CompileTask.
    CompileBroker::free_task(task);
  }
}


// ------------------------------------------------------------------
// CompileTask::initialize
void CompileTask::initialize(int compile_id,
                             methodHandle method,
                             int osr_bci,
                             methodHandle hot_method,
                             int hot_count,
                             const char* comment,
                             int adapter_kind,
                             bool is_blocking) {
  assert(!_lock->is_locked(), "bad locking");

  _compile_id = compile_id;
  _method = JNIHandles::make_global(method, false);
  _osr_bci = osr_bci;
  _is_blocking = is_blocking;
  _adapter_kind = adapter_kind;
  _num_inlined_bytecodes = 0;

  _is_complete = false;
  _is_success = false;
 
  _hot_method = NULL;
  _hot_count = hot_count;
  _time_queued = 0;  // tidy
  _comment = comment;

  if (LogCompilation) {
    _time_queued = os::elapsed_counter();
    if (hot_method.not_null()) {
      if (hot_method == method) {
        _hot_method = _method;
      } else {
        _hot_method = JNIHandles::make_global(hot_method, false);
      }
    }
  }

  _next = NULL;
}


// ------------------------------------------------------------------
// CompileTask::free
void CompileTask::free() {
  assert(!_lock->is_locked(), "Should not be locked when freed");
  if (_hot_method != NULL && _hot_method != _method) {
    JNIHandles::destroy_global(_hot_method, false);
  }
  JNIHandles::destroy_global(_method, false);
}


// ------------------------------------------------------------------
// CompileTask::print
void CompileTask::print() {
  tty->print("<CompileTask compile_id=%d ", _compile_id);
  if (_adapter_kind == ciEnv::i2c) {
    tty->print("adapter_kind=i2c ");
  } else if (_adapter_kind == ciEnv::c2i) {
    tty->print("adapter_kind=c2i ");
  }
  tty->print("method=");
  ((methodOop)JNIHandles::resolve(_method))->print_name(tty);
  tty->print_cr(" osr_bci=%d is_blocking=%s is_complete=%s is_success=%s>",
             _osr_bci, bool_to_str(_is_blocking),
             bool_to_str(_is_complete), bool_to_str(_is_success));
}

// ------------------------------------------------------------------
// CompileTask::print_line_on_error
//
// This function is called by fatal error handler when the thread 
// causing troubles is a compiler thread. 
//
// Do not grab any lock, do not allocate memory.
//
// Otherwise it's the same as CompileTask::print_line()
//
void CompileTask::print_line_on_error(outputStream* st, char* buf, int buflen) {
  methodOop method = (methodOop)JNIHandles::resolve(_method);

  // print compiler name
  st->print("%s:", CompileBroker::compiler()->name());

  // print compilation number or adapter information
  if (is_method_compile()) {
    st->print("%3d", compile_id());
  } else if (adapter_kind() == ciEnv::c2i) {
    st->print("C2I Adapter for ");
  } else {
    st->print("I2C Adapter for ");
  }

  // print method attributes
  const bool is_osr = osr_bci() != CompileBroker::standard_entry_bci;
  { const char blocking_char  = is_blocking()                      ? 'b' : ' ';
    const char compile_type   = is_osr ? '%' : method->is_native() ? '*' : ' ';
    const char sync_char      = method->is_synchronized()          ? 's' : ' ';
    const char exception_char = method->has_exception_handler()    ? '!' : ' ';
    const char tier_char      = 
      is_highest_tier_compile(comp_level())                        ? ' ' : ('0' + comp_level());
    st->print("%c%c%c%c%c ", compile_type, sync_char, exception_char, blocking_char, tier_char);
  }

  // Use buf to get method name and signature
  if (method) st->print("%s", method->name_and_sig_as_C_string(buf, buflen));

  // print osr_bci if any
  if (is_osr) st->print(" @ %d", osr_bci());

  // print method size
  st->print_cr(" (%d bytes)", method->code_size());
}

// ------------------------------------------------------------------
// CompileTask::print_line
void CompileTask::print_line() {
  Thread *thread = Thread::current();
  methodHandle method(thread,
                      (methodOop)JNIHandles::resolve(method_handle()));
  ResourceMark rm(thread);

  ttyLocker ttyl;  // keep the following output all in one block

  // print compiler name if requested
  if (CIPrintCompilerName) tty->print("%s:", CompileBroker::compiler()->name());

  // print compilation number or adapter information
  if (is_method_compile()) {
    tty->print("%3d", compile_id());
  } else if (adapter_kind() == ciEnv::c2i) {
    tty->print("C2I Adapter for ");
  } else {
    tty->print("I2C Adapter for ");
  }

  // print method attributes
  const bool is_osr = osr_bci() != CompileBroker::standard_entry_bci;
  { const char blocking_char  = is_blocking()                      ? 'b' : ' ';
    const char compile_type   = is_osr ? '%' : method->is_native() ? '*' : ' ';
    const char sync_char      = method->is_synchronized()          ? 's' : ' ';
    const char exception_char = method->has_exception_handler()    ? '!' : ' ';
    const char tier_char      = 
      is_highest_tier_compile(comp_level())                        ? ' ' : ('0' + comp_level());
    tty->print("%c%c%c%c%c ", compile_type, sync_char, exception_char, blocking_char, tier_char);
  }

  // print method name
  method->print_short_name(tty);

  // print osr_bci if any
  if (is_osr) tty->print(" @ %d", osr_bci());

  // print method size
  tty->print_cr(" (%d bytes)", method->code_size());
}

// ------------------------------------------------------------------
// CompileTask::log_head
void CompileTask::log_head(CompileLog* log) {
  Thread* thread = Thread::current();
  methodHandle method(thread,
                      (methodOop)JNIHandles::resolve(method_handle()));
  ResourceMark rm(thread);

  // <task id='9' method='M' osr_bci='X' level='1' blocking='1' stamp='1.234'>
  log->begin_head("task");
  if (_compile_id != 0)   log->print(" compile_id='%d'", _compile_id);
  if (_adapter_kind == ciEnv::i2c) {
    log->print(" compile_kind='i2c'");
  } else if (_adapter_kind == ciEnv::c2i) {
    log->print(" compile_kind='c2i'");
  } else if (method->is_native()) {
    log->print(" compile_kind='c2n'");  // same as nmethod::compile_kind
  } else if (_osr_bci != CompileBroker::standard_entry_bci) {
    log->print(" compile_kind='osr'");  // same as nmethod::compile_kind
  } // else compile_kind='c2c'
  if (!method.is_null())  log->method(method);
  if (_osr_bci != CompileBroker::standard_entry_bci) {
    log->print(" osr_bci='%d'", _osr_bci);
  }
  if (_comp_level != CompLevel_highest_tier) {
    log->print(" level='%d'", _comp_level);
  }
  if (_is_blocking) {
    log->print(" blocking='1'");
  }
  log->stamp();
  log->end_head();
  log->begin_elem("task_queued stamp='%.3f'",
                  (double) _time_queued / (double) os::elapsed_frequency());
  if (_comment != NULL) {
    log->print(" comment='%s'", _comment);
  }
  if (_hot_method != NULL) {
    methodHandle hot(thread,
                     (methodOop)JNIHandles::resolve(_hot_method));
    if (hot() != method()) {
      log->method(hot);
    }
  }
  if (_hot_count != 0) {
    log->print(" hot_count='%d'", _hot_count);
  }
  log->end_elem();
}

// ------------------------------------------------------------------
// CompileTask::log_tail
void CompileTask::log_tail(CompileLog* log) {
  Thread* thread = Thread::current();
  methodHandle method(thread,
                      (methodOop)JNIHandles::resolve(method_handle()));
  ResourceMark rm(thread);

  // <task_done ... stamp='1.234'>  </task>
  nmethod* nm = method->code();
  log->begin_elem("task_done success='%d' nmsize='%d' count='%d'",
                  _is_success, nm == NULL ? 0 : nm->instructions_size(),
                  method->invocation_count());
  int bec = method->backedge_count();
  if (bec != 0)  log->print(" backedge_count='%d'", bec);
  // Note:  "_is_complete" is about to be set, but is not.
  if (_num_inlined_bytecodes != 0) {
    log->print(" inlined_bytes='%d'", _num_inlined_bytecodes);
  }
  log->stamp();
  log->end_elem();
  log->tail("task");
  log->clear_identities();   // next task will have different CI
  if (log->unflushed_count() > 2000) {
    log->flush();
  }
  log->mark_file_end();
}



// ------------------------------------------------------------------
// CompileQueue::add
//
// Add a CompileTask to a CompileQueue
void CompileQueue::add(CompileTask* task) {
  assert(lock()->owned_by_self(), "must own lock");

  task->set_next(NULL);

  if (_last == NULL) {
    // The compile queue is empty.
    assert(_first == NULL, "queue is empty");
    _first = task;
    _last = task;
  } else {
    // Append the task to the queue.
    assert(_last->next() == NULL, "not last");
    _last->set_next(task);
    _last = task;
  }

  // Mark the method as being in the compile queue.
  // We do not set this bit for adapter compilation, so
  // duplicate adapter compilation entries may enter the
  // queue.
  if (task->is_method_compile()) {
    ((methodOop)JNIHandles::resolve(task->method_handle()))
        ->set_queued_for_compilation();
  }

  if (CIPrintCompileQueue) {
    print();
  }

  // Notify CompilerThreads that a task is available.
  lock()->notify();
}


// ------------------------------------------------------------------
// CompileQueue::get
//
// Get the next CompileTask from a CompileQueue
CompileTask* CompileQueue::get() {
  MutexLocker locker(lock());
  
  // Wait for an available CompileTask.
  while (_first == NULL) {
    // There is no work to be done right now.  Wait.
    lock()->wait();
  }

  CompileTask* task = _first;

  // Update queue first and last
  _first =_first->next();
  if (_first == NULL) {
    _last = NULL;
  }

  return task;

}


// ------------------------------------------------------------------
// CompileQueue::print
void CompileQueue::print() {
  tty->print_cr("Contents of %s", name());
  tty->print_cr("----------------------");
  CompileTask* task = _first;
  while (task != NULL) {
    task->print_line();
    task = task->next();
  }
  tty->print_cr("----------------------");
}

CompilerCounters::CompilerCounters(const char* thread_name, int instance, TRAPS) {

  _current_method[0] = '\0';
  _compile_type = CompileBroker::no_compile;

  if (UsePerfData) {
    ResourceMark rm;

    // create the thread instance name space string - don't create an
    // instance subspace if instance is -1 - keeps the adapterThread
    // counters  from having a ".0" namespace.
    const char* thread_i = (instance == -1) ? thread_name :
                      PerfDataManager::name_space(thread_name, instance);
                      

    char* name = PerfDataManager::counter_name(thread_i, "method");
    _perf_current_method =
               PerfDataManager::create_string_variable(SUN_CI, name,
                                                       cmname_buffer_length,
                                                       _current_method, CHECK);

    name = PerfDataManager::counter_name(thread_i, "type");
    _perf_compile_type = PerfDataManager::create_variable(SUN_CI, name,
                                                          PerfData::U_None,
                                                         (jlong)_compile_type,
                                                          CHECK);

    name = PerfDataManager::counter_name(thread_i, "time");
    _perf_time = PerfDataManager::create_counter(SUN_CI, name,
                                                 PerfData::U_Ticks, CHECK);

    name = PerfDataManager::counter_name(thread_i, "compiles");
    _perf_compiles = PerfDataManager::create_counter(SUN_CI, name,
                                                     PerfData::U_Events, CHECK);
  }
}


// ------------------------------------------------------------------
// CompileBroker::compilation_init
//
// Initialize the Compilation object
void CompileBroker::compilation_init(AbstractCompiler* compiler) {
  _last_method_compiled[0] = '\0';

  // Set the interface to the current compiler.
  _compiler = compiler;

  // Initialize the CompileTask free list
  _task_free_list = NULL;

  // Start the CompilerThreads
  init_compiler_threads(compiler_count());


  // totalTime performance counter is always created as it is required
  // by the implementation of java.lang.management.CompilationMBean.
  {
    EXCEPTION_MARK;
    _perf_total_compilation =
                 PerfDataManager::create_counter(JAVA_CI, "totalTime",
                                                 PerfData::U_Ticks, CHECK);
  }


  if (UsePerfData) {

    EXCEPTION_MARK;

    // create the jvmstat performance counters
    _perf_native_compilation =
                 PerfDataManager::create_counter(SUN_CI, "nativeTime",
                                                 PerfData::U_Ticks, CHECK);

    _perf_osr_compilation =
                 PerfDataManager::create_counter(SUN_CI, "osrTime",
                                                 PerfData::U_Ticks, CHECK);

    _perf_standard_compilation =
                 PerfDataManager::create_counter(SUN_CI, "standardTime",
                                                 PerfData::U_Ticks, CHECK);

    _perf_total_bailout_count =
                 PerfDataManager::create_counter(SUN_CI, "totalBailouts",
                                                 PerfData::U_Events, CHECK);

    _perf_total_invalidated_count =
                 PerfDataManager::create_counter(SUN_CI, "totalInvalidates",
                                                 PerfData::U_Events, CHECK);

    _perf_total_compile_count =
                 PerfDataManager::create_counter(SUN_CI, "totalCompiles",
                                                 PerfData::U_Events, CHECK);

    _perf_total_native_compile_count =
                 PerfDataManager::create_counter(SUN_CI, "nativeCompiles",
                                                 PerfData::U_Events, CHECK);

    _perf_total_osr_compile_count =
                 PerfDataManager::create_counter(SUN_CI, "osrCompiles",
                                                 PerfData::U_Events, CHECK);

    _perf_total_standard_compile_count =
                 PerfDataManager::create_counter(SUN_CI, "standardCompiles",
                                                 PerfData::U_Events, CHECK);

    _perf_sum_osr_bytes_compiled =
                 PerfDataManager::create_counter(SUN_CI, "osrBytes",
                                                 PerfData::U_Bytes, CHECK);

    _perf_sum_standard_bytes_compiled =
                 PerfDataManager::create_counter(SUN_CI, "standardBytes",
                                                 PerfData::U_Bytes, CHECK);

    _perf_sum_nmethod_size =
                 PerfDataManager::create_counter(SUN_CI, "nmethodSize",
                                                 PerfData::U_Bytes, CHECK);

    _perf_sum_nmethod_code_size =
                 PerfDataManager::create_counter(SUN_CI, "nmethodCodeSize",
                                                 PerfData::U_Bytes, CHECK);

    _perf_last_method =
                 PerfDataManager::create_string_variable(SUN_CI, "lastMethod",
                                       CompilerCounters::cmname_buffer_length,
                                       "", CHECK);

    _perf_last_failed_method =
            PerfDataManager::create_string_variable(SUN_CI, "lastFailedMethod",
                                       CompilerCounters::cmname_buffer_length,
                                       "", CHECK);

    _perf_last_invalidated_method =
        PerfDataManager::create_string_variable(SUN_CI, "lastInvalidatedMethod",
                                     CompilerCounters::cmname_buffer_length,
                                     "", CHECK);

    _perf_last_compile_type =
             PerfDataManager::create_variable(SUN_CI, "lastType",
                                              PerfData::U_None,
                                              (jlong)CompileBroker::no_compile,
                                              CHECK);

    _perf_last_compile_size =
             PerfDataManager::create_variable(SUN_CI, "lastSize",
                                              PerfData::U_Bytes,
                                              (jlong)CompileBroker::no_compile,
                                              CHECK);


    _perf_last_failed_type =
             PerfDataManager::create_variable(SUN_CI, "lastFailedType",
                                              PerfData::U_None,
                                              (jlong)CompileBroker::no_compile,
                                              CHECK);

    _perf_last_invalidated_type =
         PerfDataManager::create_variable(SUN_CI, "lastInvalidatedType",
                                          PerfData::U_None,
                                          (jlong)CompileBroker::no_compile,
                                          CHECK);
  }

  _initialized = true;
}



// ------------------------------------------------------------------
// CompileBroker::make_compiler_thread
CompilerThread* CompileBroker::make_compiler_thread(const char* name, CompileQueue* queue, CompilerCounters* counters, TRAPS) {
  CompilerThread* compiler_thread = NULL;

  klassOop k =
    SystemDictionary::resolve_or_fail(vmSymbolHandles::java_lang_Thread(),
                                      true, CHECK_0);
  instanceKlassHandle klass (THREAD, k);
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK_0);
  Handle string = java_lang_String::create_from_str(name, CHECK_0);    

  // Initialize thread_oop to put it into the system threadGroup    
  Handle thread_group (THREAD,  Universe::system_thread_group());
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, thread_oop, 
                       klass, 
                       vmSymbolHandles::object_initializer_name(), 
                       vmSymbolHandles::threadgroup_string_void_signature(), 
                       thread_group, 
                       string, 
                       CHECK_0);  

  {
    MutexLocker mu(Threads_lock, THREAD);
    compiler_thread = new CompilerThread(queue, counters);
    // At this point the new CompilerThread data-races with this startup
    // thread (which I believe is the primoridal thread and NOT the VM
    // thread).  This means Java bytecodes being executed at startup can 
    // queue compile jobs which will run at whatever default priority the
    // newly created CompilerThread runs at.

   
    // At this point it may be possible that no osthread was created for the
    // JavaThread due to lack of memory. We would have to throw an exception
    // in that case. However, since this must work and we do not allow
    // exceptions anyway, check and abort if this fails.

    if (compiler_thread == NULL || compiler_thread->osthread() == NULL){
      vm_exit_during_initialization("java.lang.OutOfMemoryError", 
                                    "unable to create new native thread");
    }
 
    java_lang_Thread::set_thread(thread_oop(), compiler_thread);      

    // Note that this only sets the JavaThread _priority field, which by
    // definition is limited to Java priorities and not OS priorities.
    // The os-priority is set in the CompilerThread startup code itself
    java_lang_Thread::set_priority(thread_oop(), MaxPriority);
    // CLEANUP PRIORITIES: This -if- statement hids a bug whereby the compiler
    // threads never have their OS priority set.  The assumption here is to
    // enable the Performance group to do flag tuning, figure out a suitable
    // CompilerThreadPriority, and then remove this 'if' statement (and
    // comment) and unconditionally set the priority.

    // Compiler Threads should be at the highest Priority
    if ( CompilerThreadPriority != -1 )
      os::set_native_priority( compiler_thread, CompilerThreadPriority );
    else
      os::set_native_priority( compiler_thread, os::java_to_os_priority[MaxPriority]);

      // Note that I cannot call os::set_priority because it expects Java
      // priorities and I am *explicitly* using OS priorities so that it's
      // possible to set the compiler thread priority higher than any Java
      // thread.
    
    java_lang_Thread::set_daemon(thread_oop());
    
    compiler_thread->set_threadObj(thread_oop());
    Threads::add(compiler_thread);  
    Thread::start(compiler_thread);
  }
  // Let go of Threads_lock before yielding
  os::yield(); // make sure that the compiler thread is started early (especially helpful on SOLARIS)

  return compiler_thread;
}


// ------------------------------------------------------------------
// CompileBroker::init_compiler_threads
//
// Initialize the compilation queue
void CompileBroker::init_compiler_threads(int compiler_count) {
  EXCEPTION_MARK;

  if (!CICompileAdaptersInThread && compiler()->needs_adapters()) {
    _adapter_queue = new CompileQueue("AdapterQueue", AdapterCompileQueue_lock);
    CompilerCounters* counters = new CompilerCounters("adapterThread", -1, CHECK);
    _adapter_thread = make_compiler_thread("AdapterThread", _adapter_queue, counters, CHECK);
  }

  _method_queue  = new CompileQueue("MethodQueue",  MethodCompileQueue_lock);
  _method_threads =
    new (ResourceObj::C_HEAP) GrowableArray<CompilerThread*>(compiler_count, true);

  char name_buffer[256];
  int i;
  for (i = 0; i < compiler_count; i++) {
    // Create a name for our thread.
    sprintf(name_buffer, "CompilerThread%d", i);
    CompilerCounters* counters = new CompilerCounters("compilerThread", i, CHECK);

    CompilerThread* new_thread = make_compiler_thread(name_buffer, _method_queue, counters, CHECK);
    _method_threads->append(new_thread);
  }
  if (UsePerfData) {
    PerfDataManager::create_constant(SUN_CI, "threads", PerfData::U_Bytes,
                                     compiler_count, CHECK);
  }
}

// ------------------------------------------------------------------
// CompileBroker::is_idle
bool CompileBroker::is_idle() {
  if (!_method_queue->is_empty()) {
    return false;
  } else {
    int num_threads = _method_threads->length();
    for (int i=0; i<num_threads; i++) {
      if (_method_threads->at(i)->task() != NULL) {
        return false;
      }
    }

    // No pending or active compilations.
    return true;
  }
}


// ------------------------------------------------------------------
// CompileBroker::compile_method
//
// Request compilation of a method.
nmethod* CompileBroker::compile_method_base(methodHandle method, 
                                            int osr_bci,
                                            int comp_level,
                                            methodHandle hot_method, 
                                            int hot_count,
                                            const char* comment, 
                                            TRAPS) {
  // do nothing if compiler is not available
  if (!_initialized || !compiler()->is_initialized()) {
    return NULL;
  }

  guarantee(!method->is_abstract(), "cannot compile abstract methods");
  assert(method->method_holder()->klass_part()->oop_is_instance(),
         "sanity check");
  assert(!instanceKlass::cast(method->method_holder())->is_not_initialized(),
         "method holder must be initialized");

  nmethod*     result     = NULL;
  uint         compile_id = 0;
  CompileTask* task       = NULL;
  bool         blocking   = false;

  if (CIPrintRequests) {
    tty->print("request: ");
    method->print_short_name(tty);
    if (osr_bci != InvocationEntryBci) {
      tty->print(" osr_bci: %d", osr_bci);
    }
    tty->print(" comment: %s count: %d", comment, hot_count);
    if (!hot_method.is_null()) {
      tty->print(" hot: ");
      if (hot_method() != method()) {
          hot_method->print_short_name(tty);
      } else {
        tty->print("yes");
      }
    }
    tty->cr();
  }

  // A request has been made for compilation.  Before we do any
  // real work, check to see if the method has been compiled
  // in the meantime with a definitive result.
  if (check_compilation_result(method, osr_bci, comp_level, &result)) {
    return result;
  }

  // If this method is already in the compile queue, then
  // we do not block the current thread.
  if (compilation_is_in_queue(method, osr_bci)) {
    // We may want to decay our counter a bit here to prevent
    // multiple denied requests for compilation.  This is an
    // open compilation policy issue. Note: The other possibility,
    // in the case that this is a blocking compile request, is to have
    // all subsequent blocking requesters wait for completion of
    // ongoing compiles. Note that in this case we'll need a protocol
    // for freeing the associated compile tasks. [Or we could have
    // a single static monitor on which all these waiters sleep.]
    return NULL;
  }

  // Before we acquire our lock, make sure that the compilation
  // isn't prohibited in a straightforward way.
  if (compilation_is_prohibited(method, osr_bci)) {
    return NULL;
  }

  // Acquire our lock.
  {
    MutexLocker locker(_method_queue->lock(), THREAD);

    // Make sure the method has not slipped into the queues since
    // last we checked; note that those checks were "fast bail-outs".
    // Here we need to be more careful, see 14012000 below.
    if (compilation_is_in_queue(method, osr_bci)) {
      return NULL;
    }

    // We need to check again to see if the compilation has
    // completed.  A previous compilation may have registered
    // some result.
    if (check_compilation_result(method, osr_bci, comp_level, &result)) {
      return result;
    }

    // We now know that this compilation is not pending, complete,
    // or prohibited.  Assign a compile_id to this compilation
    // and check to see if it is in our [Start..Stop) range.
    compile_id = assign_compile_id(method, osr_bci);
    if (compile_id == 0) {
      // The compilation falls outside the allowed range.
      return NULL;
    }

    // Should this thread wait for completion of the compile?
    blocking = is_compile_blocking(method, osr_bci);

    // We will enter the compilation in the queue.
    // 14012000: Note that this sets the queued_for_compile bits in
    // the target method. We can now reason that a method cannot be
    // queued for compilation more than once, as follows:
    // Before a thread queues a task for compilation, it first acquires
    // the compile queue lock, then checks if the method's queued bits
    // are set or it has already been compiled. Thus there can not be two
    // instances of a compilation task for the same method on the
    // compilation queue. Consider now the case where the compilation
    // thread has already removed a task for that method from the queue
    // and is in the midst of compiling it. In this case, the
    // queued_for_compile bits must be set in the method (and these
    // will be visible to the current thread, since the bits were set
    // under protection of the compile queue lock, which we hold now.
    // When the compilation completes, the compiler thread first sets
    // the compilation result and then clears the queued_for_compile
    // bits. Neither of these actions are protected by a barrier (or done
    // under the protection of a lock), so the only guarantee we have
    // (on machines with TSO (Total Store Order)) is that these values
    // will update in that order. As a result, the only combinations of
    // these bits that the current thread will see are, in temporal order:
    // <RESULT, QUEUE> :
    //     <0, 1> : in compile queue, but not yet compiled
    //     <1, 1> : compiled but queue bit not cleared
    //     <1, 0> : compiled and queue bit cleared
    // Because we first check the queue bits then check the result bits,
    // we are assured that we cannot introduce a duplicate task.
    // Note that if we did the tests in the reverse order (i.e. check
    // result then check queued bit), we could get the result bit before
    // the compilation completed, and the queue bit after the compilation
    // completed, and end up introducing a "duplicate" (redundant) task.
    // In that case, the compiler thread should first check if a method
    // has already been compiled before trying to compile it.
    // NOTE: in the event that there are multiple compiler threads and
    // there is de-optimization/recompilation, things will get hairy,
    // and in that case it's best to protect both the testing (here) of
    // these bits, and their updating (here and elsewhere) under a
    // common lock.
    task = create_compile_task(_method_queue,
                               compile_id, method,
                               osr_bci,
                               hot_method, hot_count, comment,
                               ciEnv::not_adapter,
                               blocking);
    task->set_comp_level(comp_level);
  }

  if (blocking) {
    return wait_for_completion(task);
  } else {
    return NULL;
  }
}


nmethod* CompileBroker::compile_method(methodHandle method, int osr_bci,
                                       methodHandle hot_method, int hot_count,
                                       const char* comment, TRAPS) {
  // make sure arguments make sense
  assert(method->method_holder()->klass_part()->oop_is_instance(), "not an instance method");
  assert(osr_bci == InvocationEntryBci || (0 <= osr_bci && osr_bci < method->code_size()), "bci out of range");
  assert(!method->is_abstract() && (osr_bci == InvocationEntryBci || !method->is_native()), "cannot compile abstract/native methods");
  assert(!instanceKlass::cast(method->method_holder())->is_not_initialized(), "method holder must be initialized");

  int comp_level = CompilationPolicy::policy()->compilation_level(method, osr_bci);

  // return quickly if possible
  if (osr_bci == InvocationEntryBci) {
    // standard compilation
    nmethod* method_code = method->code();
    if (method_code != NULL) return method_code;
    if (method->is_not_compilable(comp_level)) return NULL;
  } else {
    // osr compilation
    assert(comp_level == CompLevel_full_optimization, 
           "all OSR compiles are assumed to be at a single compilation lavel");
    nmethod* nm = method->lookup_osr_nmethod_for(osr_bci);
    if (nm != NULL) return nm;
    if (method->is_not_osr_compilable()) return NULL;
  }

  // some prerequisites that are compiler specific
#ifdef COMPILER1
  NEEDS_CLEANUP
  if (HAS_PENDING_EXCEPTION) {
    if (PENDING_EXCEPTION != Universe::vm_exception()) CLEAR_PENDING_EXCEPTION;
    return NULL;
  }
#else // COMPILER2
  method->constants()->resolve_string_constants(CHECK_0);
  // Resolve all classes seen in the signature of the method
  // we are compiling.
  methodOopDesc::load_signature_classes(method, CHECK_0);
#endif // COMPILER1 or COMPILER2

  // If the method is native, do the lookup in the thread requesting
  // the compilation. Native lookups can load code, which is not per-
  // mitted during compilation.
  //
  // Note: A native method implies non-osr compilation which is
  //       checked with an assertion at the entry of this method.
  if (method->is_native()) {
    bool in_base_library;
    address adr = NativeLookup::lookup(method, in_base_library, THREAD);
    if (PENDING_EXCEPTION) {
      // In case of an exception looking up the method, we just forget
      // about it. The interpreter will kick-in and throw the exception.
      method->set_not_compilable(); // implies is_not_osr_compilable()
      CLEAR_PENDING_EXCEPTION;
      return NULL;
    }
  }

  // If evolution kicked in before, just return.
  if (method->is_old_version()) {
    return NULL;
  }

  // JVMTI and JVMPI -- post_compile_event requires jmethod_id() that may require
  // a lock the compiling thread can not acquire. Prefetch it here.
  if (JvmtiExport::should_post_compiled_method_load() || jvmpi::enabled()) { 
    method->jmethod_id(); 
  }

  // do the compilation
  compile_method_base(method, osr_bci, comp_level, hot_method, hot_count, comment, CHECK_0);

  // return requested nmethod
  return osr_bci  == InvocationEntryBci ? method->code() : method->lookup_osr_nmethod_for(osr_bci);
}


// ------------------------------------------------------------------
// CompileBroker::compile_adapter_for
//
// Invoke the compiler on a method
#ifdef COMPILER2
BasicAdapter* CompileBroker::compile_adapter_for(methodHandle method,
                                               int adapter_kind,
                                               bool blocking) {
  assert(compiler()->needs_adapters(), "compiler must need adapters");
  BasicAdapter* result = NULL;
  CompileTask* task = NULL;

  if (CICompileAdaptersInThread) {
    // Launch this adapter compilation in the requesting thread.

    // Check once more to see if entries ahead of us in the queue
    // resulted in a compilation of this adapter.
    bool adapter_exists = check_adapter_result(method,
                                               adapter_kind,
                                               &result);
    if (!adapter_exists) {

      task = allocate_task();
      task->initialize(0, method, standard_entry_bci,
                       methodHandle(), 0, "adapter",
                       adapter_kind, true);

      // Compile the adapter.
      invoke_compiler_on_adapter(task);
      check_adapter_result(method, task->adapter_kind(), &result);

      free_task(task);
    }

    return result;
  }

  // Put the request for the adapter compilation in the adapter queue.

  // Acquire our lock.
  {
    MutexLocker locker(_adapter_queue->lock());

    // We check to see if the adapter has already been generated.
    if (check_adapter_result(method, adapter_kind, &result)) {
      return result;
    }

    // We do not check to ensure that adapter compilations are
    // not in the queue.  This means duplicate adapter compilations
    // can be in the queue simultaneously.

    // We will enter the compilation in the queue.  Externally
    // requested adapter compilations are always blocking.
    task = create_compile_task(_adapter_queue,
                               0, method,
                               standard_entry_bci,
                               methodHandle(), 0, "adapter",
                               adapter_kind,
                               blocking);
  }

  if (blocking) {
    return wait_for_adapter_completion(task);
  } else {
    return NULL;
  }
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::check_compilation_result
//
// See if compilation of this method is already complete.
bool CompileBroker::check_compilation_result(methodHandle method,
                                             int          osr_bci,
                                             int          comp_level,
                                             nmethod**    result) {
  bool is_osr = (osr_bci != standard_entry_bci);
  if (is_osr) {
    if (method->is_not_osr_compilable()) {
      *result = NULL;
      return true;
    } else {
      *result = method->lookup_osr_nmethod_for(osr_bci);
      return (*result != NULL);
    }
  } else {
    if (method->is_not_compilable(comp_level)) {
      *result = NULL;
      return true;
    } else {
      *result = method->code();
      return (*result != NULL);
    }
  }
}


// ------------------------------------------------------------------
// CompileBroker::check_adapter_result
//
// See if compilation of this adapter is already complete.
#ifdef COMPILER2
bool CompileBroker::check_adapter_result(methodHandle   method,
                                       int            adapter_kind,
                                       BasicAdapter** result) {
  if (adapter_kind == ciEnv::i2c) {
    AdapterInfo i2c_info(method, true);
    *result = I2CAdapterGenerator::_cache->lookup(&i2c_info);
  } else {
    AdapterInfo c2i_info(method, false);
    *result = C2IAdapterGenerator::_cache->lookup(&c2i_info);
  }
  return (*result != NULL);
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::compilation_is_in_queue
//
// See if this compilation is already requested.
//
// Implementation note: there is only a single "is in queue" bit
// for each method.  This means that the check below is overly
// conservative in the sense that an osr compilation in the queue
// will block a normal compilation from entering the queue (and vice
// versa).  This can be remedied by a full queue search to disambiguate
// cases.  If it is deemed profitible, this may be done.
bool CompileBroker::compilation_is_in_queue(methodHandle method,
                                          int          osr_bci) {
  return method->queued_for_compilation();
}


// ------------------------------------------------------------------
// CompileBroker::compilation_is_prohibited
//
// See if this compilation is not allowed.
bool CompileBroker::compilation_is_prohibited(methodHandle method, int osr_bci) {
  // The compile-only option may prohibit this compilation.
  if (is_not_compile_only(method)) {
    return true;
  }
  
  bool is_native = method->is_native();
  // Some compilers may not support the compilation of natives.
  if (is_native &&
      (!CICompileNatives || !compiler()->supports_native())) {
    method->set_not_compilable();
    return true;
  }
  
  bool is_osr = (osr_bci != standard_entry_bci);
  // Some compilers may not support on stack replacement.
  if (is_osr &&
      (!CICompileOSR || !compiler()->supports_osr())) {
    method->set_not_osr_compilable();
    return true;
  }

  // The method may be explicitly excluded by the user.
  if (CompilerOracle::should_exclude(method)) {
    // This does not happen silently...
    ResourceMark rm;
    tty->print("### Excluding compile: ");
    method->print_short_name(tty);
    tty->cr();
    method->set_not_compilable();
  }

  return false;
}


// ------------------------------------------------------------------
// CompileBroker::is_not_compile_only
//
// Check to see if the compile-only option prohibits this compilation.
bool CompileBroker::is_not_compile_only(methodHandle method) {
  ResourceMark rm;
  return
    Arguments::GetCheckCompileOnly() &&
    !Arguments::CompileMethod(method->klass_name()->as_C_string(),
                             method->name()->as_C_string());
}


// ------------------------------------------------------------------
// CompileBroker::assign_compile_id
//
// Assign a serialized id number to this compilation request.  If the
// number falls out of the allowed range, return a 0.  Native and OSR
// compilations may be numbered separately from regular compilations
// if certain debugging flags are used.
uint CompileBroker::assign_compile_id(methodHandle method, int osr_bci) {
  assert(_method_queue->lock()->owner() == JavaThread::current(),
         "must hold the compilation queue lock");
  bool is_osr = (osr_bci != standard_entry_bci);
  bool is_native = method->is_native();
  uint id;
  if (CICountOSR && is_osr) {
    id = ++_osr_compilation_id;
    if ((uint)CIStartOSR <= id && id < (uint)CIStopOSR) {
      return id;
    }
  } else if (CICountNative && is_native) {
    id = ++_native_compilation_id;
    if ((uint)CIStartNative <= id && id < (uint)CIStopNative) {
      return id;
    }
  } else {
    id = ++_compilation_id;
    if ((uint)CIStart <= id && id < (uint)CIStop) {
      return id;
    }
  }
  
  // Method was not in the appropriate compilation range.
  method->set_not_compilable();
  return 0;
}


// ------------------------------------------------------------------
// CompileBroker::is_compile_blocking
//
// Should the current thread be blocked until this compilation request
// has been fulfilled?
bool CompileBroker::is_compile_blocking(methodHandle method, int osr_bci) {
  return !BackgroundCompilation;
}


// ------------------------------------------------------------------
// CompileBroker::preload_classes
void CompileBroker::preload_classes(methodHandle method, TRAPS) {
  // Move this code over from c1_Compiler.cpp
  ShouldNotReachHere();
}


// ------------------------------------------------------------------
// CompileBroker::create_compile_task
//
// Create a CompileTask object representing the current request for
// compilation.  Add this task to the queue.
CompileTask* CompileBroker::create_compile_task(CompileQueue* queue,
                                              int           compile_id,
                                              methodHandle  method,
                                              int           osr_bci,
                                              methodHandle  hot_method,
                                              int           hot_count,
                                              const char*   comment,
                                              int           adapter_kind,
                                              bool          blocking) {
  CompileTask* new_task = allocate_task();
  new_task->initialize(compile_id, method, osr_bci,
                       hot_method, hot_count, comment,
                       adapter_kind, blocking);
  queue->add(new_task);
  return new_task;
}


// ------------------------------------------------------------------
// CompileBroker::allocate_task
//
// Allocate a CompileTask, from the free list if possible.
CompileTask* CompileBroker::allocate_task() {
  MutexLocker locker(CompileTaskAlloc_lock);
  CompileTask* task = NULL;
  if (_task_free_list != NULL) {
    task = _task_free_list;
    _task_free_list = task->next();
    task->set_next(NULL);
  } else {
    task = new CompileTask();
    task->set_next(NULL);
  }
  return task;
}


// ------------------------------------------------------------------
// CompileBroker::free_task
//
// Add a task to the free list.
void CompileBroker::free_task(CompileTask* task) {
  MutexLocker locker(CompileTaskAlloc_lock);
  task->free();
  task->set_next(_task_free_list);
  _task_free_list = task;
}


// ------------------------------------------------------------------
// CompileBroker::wait_for_completion
//
// Wait for the given method CompileTask to complete.
nmethod* CompileBroker::wait_for_completion(CompileTask* task) {
  if (CIPrintCompileQueue) {
    tty->print_cr("BLOCKING FOR COMPILE");
  }

  assert(task->is_blocking(), "can only wait on blocking task");

  JavaThread *thread = JavaThread::current();
  thread->set_blocked_on_compilation(true);
  
  methodHandle method(thread,
                      (methodOop)JNIHandles::resolve(task->method_handle()));
  {
    MutexLocker waiter(task->lock(), thread);

    while (!task->is_complete())
      task->lock()->wait();
  }
  // It is harmless to check this status without the lock, because
  // completion is a stable property (until the task object is recycled).
  assert(task->is_complete(), "Compilation should have completed");

  nmethod* result;

  // Ignore boolean return value of check compilation result.  We
  // know that the compilation thinks it is finished.
  check_compilation_result(method, task->osr_bci(), task->comp_level(), &result);
  thread->set_blocked_on_compilation(false);

  // By convention, the waiter is responsible for recycling a
  // blocking CompileTask. Since there is only one waiter ever
  // waiting on a CompileTask, we know that no one else will
  // be using this CompileTask; we can free it.
  free_task(task);

  return result;
}


// ------------------------------------------------------------------
// CompileBroker::wait_for_adapter_completion
//
// Wait for the given adapter CompileTask to complete.
#ifdef COMPILER2
BasicAdapter* CompileBroker::wait_for_adapter_completion(CompileTask* task) {
  if (CIPrintCompileQueue) {
    tty->print_cr("BLOCKING FOR ADAPTER COMPILE");
  }

  assert(task->is_blocking(), "can only wait on blocking task");

  JavaThread *thread = JavaThread::current();
  thread->set_blocked_on_compilation(true);
  
  methodHandle method(thread, (methodOop)JNIHandles::resolve(task->method_handle()));
  {
    MutexLocker waiter(task->lock(), thread);

    while (!task->is_complete())
      task->lock()->wait();
  }
  // It is harmless to check this status without the lock, because
  // completion is a stable property (until the task object is recycled).
  assert(task->is_complete(), "Adaption should have completed");

  BasicAdapter* result;

  // Adapter compilation.
  check_adapter_result(method, task->adapter_kind(), &result);
  assert(result != NULL, "adapter compilations never bail out");
    
  // By convention, the waiter is responsible for recycling a
  // blocking CompileTask.
  free_task(task);
  thread->set_blocked_on_compilation(false);

  return result;
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::compiler_thread_loop
//
// The main loop run by a CompilerThread.
void CompileBroker::compiler_thread_loop() {
  JavaThread* thread = JavaThread::current();
  CompileQueue* queue = ((CompilerThread*)thread)->queue();

  {
    MutexLocker locker(queue->lock(), thread);
    if (!compiler()->is_initialized()) {
      compiler()->initialize();
    }
  }

  // Open a log.
  if (LogCompilation) {
    init_compiler_thread_log();
  }
  CompileLog* log = ((CompilerThread*)thread)->log();
  if (log != NULL) {
    log->begin_elem("start_compile_thread thread='" UINTX_FORMAT "' process='%d'",
                    os::current_thread_id(),
                    os::current_process_id());
    log->stamp();
    log->end_elem();
  }

  while (true) {
    {
      // We need this HandleMark to avoid leaking VM handles.
      HandleMark hm(thread);
      if (CodeCache::unallocated_capacity() < CodeCacheMinimumFreeSpace) {
        // The CodeCache is full.  Print out warning and disable compilation.
        UseInterpreter = true;
        if (UseCompiler || AlwaysCompileLoopMethods ) {
          if (log != NULL) {
            log->begin_elem("code_cache_full");
            log->stamp();
            log->end_elem();
          }
#ifndef PRODUCT
          warning("CodeCache is full. Compiler has been disabled");
          if (CompileTheWorld || ExitOnFullCodeCache) {
            before_exit(thread);
            exit_globals(); // will delete tty
            exit(CompileTheWorld ? 0 : 1);
          }
#endif
          UseCompiler               = false;    
          AlwaysCompileLoopMethods  = false;
        }
      }
      
      CompileTask* task = queue->get();

      // Give compiler threads an extra quanta.  They tend to be bursty and
      // this helps the compiler to finish up the job.
      if( CompilerThreadHintNoPreempt )
        os::hint_no_preempt();

      // trace per thread time and compile statistics
      CompilerCounters* counters = ((CompilerThread*)thread)->counters();
      PerfTraceTimedEvent(counters->time_counter(), counters->compile_counter());

      // Assign the task to the current thread.  Mark this compilation
      // thread as active for the profiler.
      CompileTaskWrapper ctw(task);
      methodHandle method(thread,
                     (methodOop)JNIHandles::resolve(task->method_handle()));
    
      // Never compile a method if breakpoints are present in it
      if (task->is_method_compile() && method()->number_of_breakpoints() == 0) {
        // Compile the method.
        if (UseCompiler || AlwaysCompileLoopMethods) {
#ifdef COMPILER1
          // Allow repeating compilations for the purpose of benchmarking
          // compile speed. This is not useful for customers.
          if (CompilationRepeat != 0) {
            int compile_count = CompilationRepeat;
            while (compile_count > 0) {
              invoke_compiler_on_method(task);
              if (method->code() != NULL) {
                method->code()->make_zombie();
                method->set_code(NULL);
              }
              compile_count--;
            }
          }
#endif
          invoke_compiler_on_method(task);
        } else {
          // After compilation is disabled, remove remaining methods from queue
          method->clear_queued_for_compilation();
        }
      } else {
#ifdef COMPILER1
        ShouldNotReachHere();
#else // COMPILER2
        // Adapter compilation.
        BasicAdapter* result;

        // Check once more to see if entries ahead of us in the queue
        // resulted in a compilation of this adapter.
        bool adapter_exists = check_adapter_result(method,
                                                   task->adapter_kind(),
                                                   &result);
        if (!adapter_exists) {
          // Compile the adapter.
          invoke_compiler_on_adapter(task);
        }
#endif // COMPILER1 or COMPILER2
      }
    }
  }
}


// ------------------------------------------------------------------
// CompileBroker::init_compiler_thread_log
//
// Set up state required by +LogCompilation.
void CompileBroker::init_compiler_thread_log() {
    CompilerThread* thread = (CompilerThread*)JavaThread::current();
    char  fileBuf[4*K];
    FILE* fp = NULL;
    char* file = NULL;
    intx thread_id = os::current_thread_id();
    for (int try_temp_dir = 1; try_temp_dir >= 0; try_temp_dir--) {
      const char* dir = (try_temp_dir ? os::get_temp_directory() : NULL);
      if (dir == NULL)  dir = "";
      sprintf(fileBuf, "%shs_c" UINTX_FORMAT "_pid%u.log",
              dir, thread_id, os::current_process_id());
      fp = fopen(fileBuf, "at");
      if (fp != NULL) {
        file = NEW_C_HEAP_ARRAY(char, strlen(fileBuf)+1);
        strcpy(file, fileBuf);
        break;
      }
    }
    if (fp == NULL) {
      warning("Cannot open log file: %s", fileBuf);
    } else {
      if (LogCompilation && Verbose)
        tty->print_cr("Opening compilation log %s", file);
      CompileLog* log = new(ResourceObj::C_HEAP) CompileLog(file, fp, thread_id);
      thread->init_log(log);
    }
}

// ------------------------------------------------------------------
// CompileBroker::set_should_block
//
// Set _should_block.
// Call this from the VM, with Threads_lock held and a safepoint requested.
void CompileBroker::set_should_block() {
  assert(Threads_lock->owner() == Thread::current(), "must have threads lock");
  assert(SafepointSynchronize::is_at_safepoint(), "must be at a safepoint already");
#ifndef PRODUCT
  if (PrintCompilation && (Verbose || WizardMode))
    tty->print_cr("notifying compiler thread pool to block");
#endif
  _should_block = true;
}

// ------------------------------------------------------------------
// CompileBroker::maybe_block
//
// Call this from the compiler at convenient points, to poll for _should_block.
void CompileBroker::maybe_block() {
  if (_should_block) {
#ifndef PRODUCT
    if (PrintCompilation && (Verbose || WizardMode))
      tty->print_cr("compiler thread " INTPTR_FORMAT " poll detects block request", Thread::current());
#endif
    ThreadInVMfromNative tivfn(JavaThread::current());
  }
}


// ------------------------------------------------------------------
// CompileBroker::invoke_compiler_on_method
//
// Compile a method.  If the method needs adapters, compile the
// adapters as well.
void CompileBroker::invoke_compiler_on_method(CompileTask* task) {
  if (PrintCompilation) {
    ResourceMark rm;
    task->print_line();
  }
  assert(task->is_method_compile(), "must be method compile");
  elapsedTimer time;

  CompilerThread* thread = (CompilerThread*)JavaThread::current();
  ResourceMark rm(thread);

  uint compile_id = task->compile_id();
  methodHandle method(thread,
                      (methodOop)JNIHandles::resolve(task->method_handle()));
  int osr_bci = task->osr_bci();

  // Common flags.
  bool is_native = method->is_native();
  bool is_osr = (osr_bci != standard_entry_bci);

  // Save information about this method in case of failure.
  set_last_compile(thread, method, is_osr, is_native);

  JNIEnv* env = thread->jni_environment();

  // Allocate a new set of JNI handles.
  push_jni_handle_block();
  jobject target_handle = JNIHandles::make_local(env, method());
  int compilable = ciEnv::MethodCompilable;
  {
    int system_dictionary_modification_counter;
    {
      MutexLocker locker(Compile_lock, thread);
      system_dictionary_modification_counter = SystemDictionary::number_of_modifications();
    }

    ThreadToNativeFromVM ttn(thread);
    HandleMark  handle_mark(thread);

    ciEnv ci_env(env, 
                 system_dictionary_modification_counter, 
                 check_break_at(method, compile_id, is_osr, is_native), 
                 task->comp_level());
    if (LogCompilation && !CompilerOracle::should_log(method))
      ci_env.set_log(NULL);
    ciMethod* target = ci_env.get_method_from_handle(target_handle);

    // Eagerly generate i2c and c2i adapters before compiling the method.
#ifdef COMPILER2
    if (CIEagerAdapters && compiler()->needs_adapters()) {
      eager_compile_i2c_adapters(&ci_env, target);
      eager_compile_c2i_adapters(&ci_env, target);
    }
#endif // COMPILER2

    TraceTime t1("compilation", &time);

    compiler()->compile_method(&ci_env, target, osr_bci);

    if (!ci_env.failing() && !ci_env.method_registered()) {
      //assert(false, "compiler should always document failure");
      // The compiler elected, without comment, not to register a result.
      // Do not attempt further compilations of this method.
      ci_env.record_method_not_compilable("compile failed");
    }

    if (ci_env.failing()) {
      // Copy this bit to the enclosing block:
      compilable = ci_env.compilable();
      if (PrintCompilation) {
        const char* reason = ci_env.failure_reason();
        if (compilable == ciEnv::MethodCompilable_not_at_tier) {
          if (is_highest_tier_compile(ci_env.comp_level())) {
            // Already at highest tier, promote to not compilable.
            compilable = ciEnv::MethodCompilable_never;
          } else {
            tty->print_cr("%3d   COMPILE SKIPPED: %s (retry at different tier)", compile_id, reason);
          }
        }

        if (compilable == ciEnv::MethodCompilable_never) {
          tty->print_cr("%3d   COMPILE SKIPPED: %s (not retryable)", compile_id, reason);
        } else if (compilable == ciEnv::MethodCompilable) {
          tty->print_cr("%3d   COMPILE SKIPPED: %s", compile_id, reason);
        }
      }
    } else {
      task->mark_success();
      task->set_num_inlined_bytecodes(ci_env.num_inlined_bytecodes());
    }

  }
  pop_jni_handle_block();

  collect_statistics(thread, time, task);

  if (compilable == ciEnv::MethodCompilable_never) {
    if (is_osr) {
      method->set_not_osr_compilable();
    } else {
      method->set_not_compilable();
    }
  } else if (compilable == ciEnv::MethodCompilable_not_at_tier) {
    method->set_not_compilable(task->comp_level());
  }

  // Note that the queued_for_compilation bits are cleared without
  // protection of a mutex. [They were set by the requester thread,
  // when adding the task to the complie queue -- at which time the
  // compile queue lock was held. Subsequently, we acquired the compile
  // queue lock to get this task off the compile queue; thus (to belabour
  // the point somewhat) our clearing of the bits must be occurring
  // only after the setting of the bits. See also 14012000 above.
  method->clear_queued_for_compilation();
}


// ------------------------------------------------------------------
// CompileBroker::eager_compile_i2c_adapters
#ifdef COMPILER2
void CompileBroker::eager_compile_i2c_adapters(ciEnv*    ci_env,
                                               ciMethod* target) {

  // We compile one i2c adapter for the target method if it has
  // not already been generated.
  bool needs_adapter = false;
  {
    VM_ENTRY_MARK;
    methodHandle method(THREAD, target->get_methodOop());
    AdapterInfo i2c_info(method, true);
    needs_adapter = (I2CAdapterGenerator::_cache->lookup(&i2c_info) == NULL);
  }
  if (needs_adapter)
    compiler()->compile_adapter(ci_env, target, ciEnv::i2c);
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::eager_compile_c2i_adapters
#ifdef COMPILER2
void CompileBroker::eager_compile_c2i_adapters(ciEnv*    ci_env,
                                               ciMethod* target) {
  // We compile one c2i adapter for the target method if it has
  // not already been generated.
  bool needs_adapter = false;
  {
    VM_ENTRY_MARK;
    methodHandle method(THREAD, target->get_methodOop());
    AdapterInfo c2i_info(method, false);
    needs_adapter = (C2IAdapterGenerator::_cache->lookup(&c2i_info) == NULL);
  }
  if (needs_adapter)
    compiler()->compile_adapter(ci_env, target, ciEnv::c2i);
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::invoke_compile_on_adapter
//
// Compile a single adapter.
#ifdef COMPILER2
void CompileBroker::invoke_compiler_on_adapter(CompileTask* task) {
  if (PrintAdapterCompilation) {
    ResourceMark rm;
    task->print_line();
  }
  JavaThread* thread = JavaThread::current();
  ResourceMark rm(thread);
  methodHandle method(thread, (methodOop)JNIHandles::resolve(task->method_handle()));
  int adapter_kind = task->adapter_kind();
  JNIEnv* env = thread->jni_environment();

  jobject target_handle;
  push_jni_handle_block();
  target_handle = JNIHandles::make_local(env, method());
  int num_mods = 0;
  {
    MutexLocker locker(Compile_lock, thread);
    num_mods = SystemDictionary::number_of_modifications();
  }
  {
    ThreadToNativeFromVM ttn(thread);
    HandleMark  handle_mark(thread);

    ciEnv ci_env(env, num_mods, false, CompLevel_full_optimization);
    if (LogCompilation && !CompilerOracle::should_log(methodHandle()))
      ci_env.set_log(NULL);
    ciMethod* target = ci_env.get_method_from_handle(target_handle);

    assert(!CIEagerAdapters || adapter_kind != ciEnv::i2c,
           "CIEagerAdapters prevents demand for i2c adapters");
    compiler()->compile_adapter(&ci_env, target, adapter_kind);
  }

  pop_jni_handle_block();

  BasicAdapter *result = NULL;
  check_adapter_result(method, adapter_kind, &result);
  assert(result != NULL, "must be able to generate adapter");

  task->mark_success();
}
#endif // COMPILER2


// ------------------------------------------------------------------
// CompileBroker::set_last_compile
//
// Record this compilation for debugging purposes.
void CompileBroker::set_last_compile(CompilerThread* thread, methodHandle method, bool is_osr, bool is_native) {
  ResourceMark rm;
  char* method_name = method->name()->as_C_string();
  strncpy(_last_method_compiled, method_name, CompileBroker::name_buffer_length);
  char current_method[CompilerCounters::cmname_buffer_length];
  size_t maxLen = CompilerCounters::cmname_buffer_length;

  if (UsePerfData) {
    const char* class_name = method->method_holder()->klass_part()->name()->as_C_string();

    size_t s1len = strlen(class_name);
    size_t s2len = strlen(method_name);

    // check if we need to truncate the string
    if (s1len + s2len + 2 > maxLen) {

      // the strategy is to lop off the leading characters of the
      // class name and the trailing characters of the method name.

      if (s2len + 2 > maxLen) {
        // lop of the entire class name string, let snprintf handle
        // truncation of the method name.
        class_name += s1len; // null string
      }
      else {
        // lop off the extra characters from the front of the class name
        class_name += ((s1len + s2len + 2) - maxLen);
      }
    }

    jio_snprintf(current_method, maxLen, "%s %s", class_name, method_name);
  }

  if (CICountOSR && is_osr) {
    _last_compile_type = osr_compile;
  } else if (CICountNative && is_native) {
    _last_compile_type = native_compile;
  } else {
    _last_compile_type = normal_compile;
  }

  if (UsePerfData) {
    CompilerCounters* counters = thread->counters();
    counters->set_current_method(current_method);
    counters->set_compile_type((jlong)_last_compile_type);
  }
}


// ------------------------------------------------------------------
// CompileBroker::push_jni_handle_block
//
// Push on a new block of JNI handles.
void CompileBroker::push_jni_handle_block() {
  JavaThread* thread = JavaThread::current();

  // Allocate a new block for JNI handles.
  // Inlined code from jni_PushLocalFrame()
  JNIHandleBlock* java_handles = thread->active_handles();
  JNIHandleBlock* compile_handles = JNIHandleBlock::allocate_block(thread);
  assert(compile_handles != NULL && java_handles != NULL, "should not be NULL");
  compile_handles->set_pop_frame_link(java_handles);  // make sure java handles get gc'd.
  thread->set_active_handles(compile_handles);
}


// ------------------------------------------------------------------
// CompileBroker::pop_jni_handle_block
//
// Pop off the current block of JNI handles.
void CompileBroker::pop_jni_handle_block() {
  JavaThread* thread = JavaThread::current();

  // Release our JNI handle block
  JNIHandleBlock* compile_handles = thread->active_handles();
  JNIHandleBlock* java_handles = compile_handles->pop_frame_link();
  thread->set_active_handles(java_handles);
  compile_handles->set_pop_frame_link(NULL);
  JNIHandleBlock::release_block(compile_handles, thread); // may block
}


// ------------------------------------------------------------------
// CompileBroker::check_break_at
//
// Should the compilation break at the current compilation.
bool CompileBroker::check_break_at(methodHandle method, int compile_id, bool is_osr, bool is_native) {
  if (CICountOSR && is_osr && (compile_id == CIBreakAtOSR)) {
    return true;
  } else if (CICountNative && is_native && (compile_id == CIBreakAtNative)) {
    return true;
  } else if( CompilerOracle::should_break_at(method) ) { // break when compiling
    return true;
  } else {
    return (compile_id == CIBreakAt);
  }
}

// ------------------------------------------------------------------
// CompileBroker::collect_statistics
//
// Collect statistics about the compilation.

void CompileBroker::collect_statistics(CompilerThread* thread, elapsedTimer time, CompileTask* task) {
  bool success = task->is_success();
  methodHandle method = (methodOop)JNIHandles::resolve(task->method_handle());
  int osr_bci = task->osr_bci();
  uint compile_id = task->compile_id();
  int comp_level = task->comp_level();
  bool is_osr = (task->osr_bci() != standard_entry_bci);
  bool is_native = method->is_native();
  nmethod* code = NULL;
  check_compilation_result(method, osr_bci, comp_level, &code);
  CompilerCounters* counters = thread->counters();

  MutexLocker locker(CompileStatistics_lock);
  
  // _perf variables are production performance counters which are
  // updated regardless of the setting of the CITime and CITimeEach flags
  //
  if (!success) {
    _total_bailout_count++;
    if (UsePerfData) {
      _perf_last_failed_method->set_value(counters->current_method());
      _perf_last_failed_type->set_value(counters->compile_type());
      _perf_total_bailout_count->inc();
    }
  } else if (code == NULL) {
    if (UsePerfData) {
      _perf_last_invalidated_method->set_value(counters->current_method());
      _perf_last_invalidated_type->set_value(counters->compile_type());
      _perf_total_invalidated_count->inc();
    }
    _total_invalidated_count++;
  } else {
    // Compilation succeeded

    // update compilation ticks - used by the implementation of
    // java.lang.management.CompilationMBean
    _perf_total_compilation->inc(time.ticks());

    if (CITime) {
      _t_total_compilation.add(time);
      if (is_native) {
        _t_native_compilation.add(time);
      } else if (is_osr) {
        _t_osr_compilation.add(time);
        _sum_osr_bytes_compiled += method->code_size() + task->num_inlined_bytecodes();
      } else {
        _t_standard_compilation.add(time);
        _sum_standard_bytes_compiled += method->code_size() + task->num_inlined_bytecodes();
      }
    }

    if (UsePerfData) {
      // save the name of the last method compiled
      _perf_last_method->set_value(counters->current_method());
      _perf_last_compile_type->set_value(counters->compile_type());
      _perf_last_compile_size->set_value(method->code_size() +
                                         task->num_inlined_bytecodes());
      if (is_native) {
        _perf_native_compilation->inc(time.ticks());
      } else if (is_osr) {
        _perf_osr_compilation->inc(time.ticks());
        _perf_sum_osr_bytes_compiled->inc(method->code_size() + task->num_inlined_bytecodes());
      } else {
        _perf_standard_compilation->inc(time.ticks());
        _perf_sum_standard_bytes_compiled->inc(method->code_size() + task->num_inlined_bytecodes());
      }
    }

    if (CITimeEach) {
      float bytes_per_sec = 1.0 * (method->code_size() + task->num_inlined_bytecodes()) / time.seconds();
      tty->print_cr("%3d   seconds: %f bytes/sec : %f (bytes %d + %d inlined)",
                    compile_id, time.seconds(), bytes_per_sec, method->code_size(), task->num_inlined_bytecodes());
    }

    // Collect counts of successful compilations
    _sum_nmethod_size += code->total_size();
    _sum_nmethod_code_size += code->code_size();;
    _total_compile_count++;

    if (UsePerfData) {
      _perf_sum_nmethod_size->inc(code->total_size());
      _perf_sum_nmethod_code_size->inc(code->code_size());
      _perf_total_compile_count->inc();
    }

    if (is_native) {
      if (UsePerfData) _perf_total_native_compile_count->inc();
      _total_native_compile_count++;
    } else if (is_osr) {
      if (UsePerfData) _perf_total_osr_compile_count->inc();
      _total_osr_compile_count++;
    } else {
      if (UsePerfData) _perf_total_standard_compile_count->inc();
      _total_standard_compile_count++;
    }
  }
  // set the current method for the thread to null
  if (UsePerfData) counters->set_current_method("");
}



void CompileBroker::print_times() {
  tty->cr();
  tty->print_cr("Accumulated compiler times (for compiled methods only)");
  tty->print_cr("------------------------------------------------");
               //0000000000111111111122222222223333333333444444444455555555556666666666
               //0123456789012345678901234567890123456789012345678901234567890123456789
  tty->print_cr("  Total compilation time   : %6.3f s", CompileBroker::_t_total_compilation.seconds());
  tty->print_cr("    Standard compilation   : %6.3f s, Average : %2.3f",
                CompileBroker::_t_standard_compilation.seconds(),
                CompileBroker::_t_standard_compilation.seconds() / CompileBroker::_total_standard_compile_count);
  tty->print_cr("    On stack replacement   : %6.3f s, Average : %2.3f", CompileBroker::_t_osr_compilation.seconds(), CompileBroker::_t_osr_compilation.seconds() / CompileBroker::_total_osr_compile_count);
  tty->print_cr("    Native methods         : %6.3f s, Average : %2.3f", CompileBroker::_t_native_compilation.seconds(), CompileBroker::_t_native_compilation.seconds() / CompileBroker::_total_native_compile_count);
#ifdef COMPILER1
  compiler()->print_timers();
#endif

  tty->cr();
  tty->print_cr("  Total compiled bytecodes : %6d bytes", CompileBroker::_sum_osr_bytes_compiled + CompileBroker::_sum_standard_bytes_compiled);
  tty->print_cr("    Standard compilation   : %6d bytes", CompileBroker::_sum_standard_bytes_compiled);
  tty->print_cr("    On stack replacement   : %6d bytes", CompileBroker::_sum_osr_bytes_compiled);
  int bps = (CompileBroker::_sum_osr_bytes_compiled + CompileBroker::_sum_standard_bytes_compiled) / CompileBroker::_t_total_compilation.seconds();
  tty->print_cr("  Average compilation speed: %6d bytes/s", bps);
  tty->cr();
  tty->print_cr("  nmethod code size        : %6d bytes", CompileBroker::_sum_nmethod_code_size);
  tty->print_cr("  nmethod total size       : %6d bytes", CompileBroker::_sum_nmethod_size);
}


// Debugging output for failure
void CompileBroker::print_last_compile() {
  if (_compiler != NULL && _last_method_compiled != NULL && _last_compile_type != no_compile) {
    if (_last_compile_type == osr_compile) {
      tty->print_cr("Last parse:  [osr]%d+++%s",
                    _osr_compilation_id, _last_method_compiled);
    } else if (_last_compile_type == native_compile) {
      tty->print_cr("Last parse:  [native]%d+++%s",
                    _native_compilation_id, _last_method_compiled);
    } else {
      tty->print_cr("Last parse:  %d+++%s",
                    _compilation_id, _last_method_compiled);
    }
  }
}


void CompileBroker::print_compiler_threads() {
  NOT_PRODUCT(tty->print_cr("Compiler thread printing unimplemented.");)
}
