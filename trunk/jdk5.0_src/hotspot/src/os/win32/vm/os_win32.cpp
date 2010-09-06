#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)os_win32.cpp	1.462 04/06/15 17:23:38 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#ifdef _WIN64
// Must be at least Windows 2000 or XP to use VectoredExceptions
#define _WIN32_WINNT 0x500
#endif

// do not include precompiled header file
# include "incls/_os_win32.cpp.incl"

#ifdef _DEBUG
#include <crtdbg.h>
#endif


#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <objidl.h>
#include <shlobj.h>

#include <malloc.h>
#include <signal.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>              // For _beginthreadex(), _endthreadex()
#include <imagehlp.h>             // For os::dll_address_to_function_name

/* for enumerating dll libraries */
#include <tlhelp32.h>
#include <vdmdbg.h>

// for timer info max values which include all bits
#define ALL_64_BITS CONST64(0xFFFFFFFFFFFFFFFF)

static HANDLE main_process;
static HANDLE main_thread;
static int    main_thread_id;

static FILETIME process_creation_time;
static FILETIME process_exit_time;
static FILETIME process_user_time;
static FILETIME process_kernel_time;

static LPTOP_LEVEL_EXCEPTION_FILTER previous_toplevel_exception_filter = NULL;
#ifdef _WIN64
PVOID  topLevelVectoredExceptionHandler = NULL;
#endif

#ifdef _M_IA64
#define __CPU__ ia64
#elif _M_AMD64
#define __CPU__ amd64
#else
#define __CPU__ i486
#endif

// save DLL module handle, used by GetModuleFileName

HINSTANCE vm_lib_handle;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      vm_lib_handle = hinst;
      if(ForceTimeHighResolution)
        timeBeginPeriod(1L);
      break;
    case DLL_PROCESS_DETACH:
      if(ForceTimeHighResolution)
        timeEndPeriod(1L);
#ifdef _WIN64
      if (topLevelVectoredExceptionHandler != NULL) {
        RemoveVectoredExceptionHandler(topLevelVectoredExceptionHandler);
        topLevelVectoredExceptionHandler = NULL;
      }
#endif
      break;
    default:
      break;
  }
  return true;
}

static inline double fileTimeAsDouble(FILETIME* time) {
  const double high  = (double) ((unsigned int) ~0);
  const double split = 10000000.0;
  double result = (time->dwLowDateTime / split) + 
                   time->dwHighDateTime * (high/split);
  return result;
}

// Implementation of os

bool os::getenv(const char* name, char* buffer, int len) {
 int result = GetEnvironmentVariable(name, buffer, len);
 return result > 0 && result < len;
}


// No setuid programs under Windows.
bool os::have_special_privileges() {
  return false;
}


void os::init_system_properties_values() {
  /* sysclasspath, java_home, dll_dir */
  {
      char *home_path;
      char *dll_path;
      char *pslash;
      char *bin = "\\bin";
      char home_dir[MAX_PATH];

      if (!getenv("_ALT_JAVA_HOME_DIR", home_dir, MAX_PATH)) {
          os::jvm_path(home_dir, sizeof(home_dir));
	  // Found the full path to jvm[_g].dll. 
	  // Now cut the path to <java_home>/jre if we can. 
          *(strrchr(home_dir, '\\')) = '\0';  /* get rid of \jvm.dll */
          pslash = strrchr(home_dir, '\\');
          if (pslash != NULL) {
              *pslash = '\0';                 /* get rid of \{client|server} */
              pslash = strrchr(home_dir, '\\');
              if (pslash != NULL)
                  *pslash = '\0';             /* get rid of \bin */
          }
      }

      home_path = NEW_C_HEAP_ARRAY(char, strlen(home_dir) + 1);
      if (home_path == NULL)
          return;
      strcpy(home_path, home_dir);
      Arguments::set_java_home(home_path);

      dll_path = NEW_C_HEAP_ARRAY(char, strlen(home_dir) + strlen(bin) + 1);
      if (dll_path == NULL)
          return;
      strcpy(dll_path, home_dir);
      strcat(dll_path, bin);
      Arguments::set_dll_dir(dll_path);

      if (!set_boot_path('\\', ';'))
	  return;
  }

  /* library_path */
  {
    /* Win32 library search order (See the documentation for LoadLibrary):
     *
     * 1. The directory from which application is loaded.
     * 2. The current directory
     * 3. System directory (GetSystemDirectory)
     * 4. Windows directory (GetWindowsDirectory)
     * 5. The PATH environment variable
     */

    char *library_path;
    char tmp[MAX_PATH];
    char *path_str = ::getenv("PATH");

    library_path = NEW_C_HEAP_ARRAY(char, MAX_PATH * 4 + (path_str ? strlen(path_str) : 0) + 10);

    library_path[0] = '\0';

    GetModuleFileName(NULL, tmp, sizeof(tmp));
    *(strrchr(tmp, '\\')) = '\0';
    strcat(library_path, tmp);

    strcat(library_path, ";.");

    GetSystemDirectory(tmp, sizeof(tmp));
    strcat(library_path, ";");
    strcat(library_path, tmp);

    GetWindowsDirectory(tmp, sizeof(tmp));
    strcat(library_path, ";");
    strcat(library_path, tmp);
  
    if (path_str) {
        strcat(library_path, ";");
        strcat(library_path, path_str);
    }

    Arguments::set_library_path(library_path);
    FREE_C_HEAP_ARRAY(char, library_path);
  }

  /* Default extensions directory */
  {
      char buf[MAX_PATH + 10];
      sprintf(buf, "%s\\lib\\ext", Arguments::get_java_home());
      Arguments::set_ext_dirs(buf);
  }

  /* Default endorsed standards directory. */
  {
    #define ENDORSED_DIR "\\lib\\endorsed"
    size_t len = strlen(Arguments::get_java_home()) + sizeof(ENDORSED_DIR);
    char * buf = NEW_C_HEAP_ARRAY(char, len);
    sprintf(buf, "%s%s", Arguments::get_java_home(), ENDORSED_DIR);
    Arguments::set_endorsed_dirs(buf);
    #undef ENDORSED_DIR
  }

  // Done
  return;
}

void os::breakpoint() {
  DebugBreak();
}

// Invoked from the BREAKPOINT Macro
extern "C" void breakpoint() {
  os::breakpoint();
}

// Returns an estimate of the current stack pointer. Result must be guaranteed
// to point into the calling threads stack, and be no lower than the current
// stack pointer.

address os::current_stack_pointer() {
  int dummy;
  address sp = (address)&dummy;
  return sp;
}

// os::current_stack_base()
//
//   Returns the base of the stack, which is the stack's
//   starting address.  This function must be called 
//   while running on the stack of the thread being queried.

address os::current_stack_base() {
  MEMORY_BASIC_INFORMATION minfo;
  address stack_bottom;
  size_t stack_size;

  VirtualQuery(&minfo, &minfo, sizeof(minfo));
  stack_bottom =  (address)minfo.AllocationBase;
  stack_size = minfo.RegionSize;

  // Add up the sizes of all the regions with the same
  // AllocationBase.
  while( 1 )
  {
    VirtualQuery(stack_bottom+stack_size, &minfo, sizeof(minfo));
    if ( stack_bottom == (address)minfo.AllocationBase )
      stack_size += minfo.RegionSize;
    else
      break;
  }

#ifdef _M_IA64
  // IA64 has memory and register stacks
  stack_size = stack_size / 2;
#endif
  return stack_bottom + stack_size;
}

size_t os::current_stack_size() {
  size_t sz;
  MEMORY_BASIC_INFORMATION minfo;
  VirtualQuery(&minfo, &minfo, sizeof(minfo));
  sz = (size_t)os::current_stack_base() - (size_t)minfo.AllocationBase;
  return sz;
}

LONG WINAPI topLevelExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo);

// Thread start routine for all new Java threads
static unsigned __stdcall _start(Thread* thread) {
  // Try to randomize the cache line index of hot stack frames.
  // This helps when threads of the same stack traces evict each other's
  // cache lines. The threads can be either from the same JVM instance, or
  // from different JVM instances. The benefit is especially true for
  // processors with hyperthreading technology.
  static int counter = 0;
  int pid = os::current_process_id();
  _alloca(((pid ^ counter++) & 7) * 128);

  OSThread* osthr = thread->osthread();
  assert(osthr->get_state() == RUNNABLE, "invalid os thread state");

#ifdef _WIN64
  // Win64 uses vectored exception.
  thread->run();
#else
  // Install a win32 structured exception handler around every thread created 
  // by VM, so VM can genrate error dump when an exception occurred in non-
  // Java thread (e.g. VM thread).
  __try {
     thread->run();
  } __except(topLevelExceptionFilter((_EXCEPTION_POINTERS*)_exception_info())) {
      // Nothing to do.
  }
#endif

  // One less thread is executing
  // When the VMThread gets here, the main thread may have already exited
  // which frees the CodeHeap containing the Atomic::add code
  if (thread != VMThread::vm_thread() && VMThread::vm_thread() != NULL) {
    Atomic::dec_ptr((intptr_t*)&os::win32::_os_thread_count);
  }

  return 0;
}

static OSThread* create_os_thread(Thread* thread, HANDLE thread_handle, int thread_id) {
  // Allocate the OSThread object
  OSThread* osthread = new OSThread(NULL, NULL);
  if (osthread == NULL) return NULL;

  // Initial state is ALLOCATED but not INITIALIZED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(ALLOCATED);
  }

  // Initialize support for Java interrupts
  HANDLE interrupt_event = CreateEvent(NULL, true, false, NULL);
  if (interrupt_event == NULL) {
    delete osthread;
    return NULL;
  }
  osthread->set_interrupt_event(interrupt_event);

  // Store info on the Win32 thread into the OSThread
  osthread->set_thread_handle(thread_handle);
  osthread->set_thread_id(thread_id);

  // Initial thread state is INITIALIZED, not SUSPENDED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(INITIALIZED);
  }

  return osthread;
}


bool os::create_attached_thread(Thread* anThread) {
  HANDLE thread;
  if (!DuplicateHandle(main_process, GetCurrentThread(), GetCurrentProcess(),
                       &thread, THREAD_ALL_ACCESS, false, 0)) {
    fatal("DuplicateHandle failed\n");
  }
  OSThread* osthread = create_os_thread(anThread, thread, (int)current_thread_id());
  if (osthread == NULL) {
     return false;
  }
  
  {
    MutexLockerEx ml(anThread->SR_lock(), Mutex::_no_safepoint_check_flag);
    anThread->clear_is_baby_thread();
    osthread->set_state(RUNNABLE);
  }
  anThread->set_osthread(osthread);
  return true;
}

bool os::create_main_thread(Thread* thread) {
  if (_starting_thread == NULL) {
    _starting_thread = create_os_thread(thread, main_thread, main_thread_id);
     if (_starting_thread == NULL) { 
        return false;
     }
  }
  // The primordial thread is runnable from the start)
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    thread->clear_is_baby_thread();
    _starting_thread->set_state(RUNNABLE);
  }
  thread->set_osthread(_starting_thread);
  return true;
}

// Allocate and initialize a new OSThread
// Note: thr_type and stack_size arguments are not used on Win32.
bool os::create_thread(Thread* thread, ThreadType thr_type, size_t stack_size) {
  unsigned thread_id;

  // Allocate the OSThread object
  OSThread* osthread = new OSThread(NULL, NULL);
  if (osthread == NULL) {
    return false;
  }

  // Initial state is ALLOCATED but not INITIALIZED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(ALLOCATED);
  }
  
  // Initialize support for Java interrupts
  HANDLE interrupt_event = CreateEvent(NULL, true, false, NULL);
  if (interrupt_event == NULL) {
    delete osthread;
    return NULL;
  }
  osthread->set_interrupt_event(interrupt_event);
  osthread->set_interrupted(false);
  
  if (os::win32::_os_thread_count > os::win32::_os_thread_limit) {
    // We got lots of threads. Check if we still have some address space left.
    // Need to be at least 5Mb of unreserved address space. We do check by
    // trying to reserve some.
    const size_t VirtualMemoryBangSize = 20*K*K;
    char* mem = os::reserve_memory(VirtualMemoryBangSize);
    if (mem == NULL) { 
      delete osthread;    
      return NULL;
    } else {
      // Release the memory again
      (void)os::release_memory(mem, VirtualMemoryBangSize);
    }
  }

  thread->set_osthread(osthread);
  
  // Create the Win32 thread
  HANDLE thread_handle =
    (HANDLE)_beginthreadex(NULL,
                           UseDefaultStackSize ? 0 : (unsigned)JavaThread::stack_size_at_create(),
                           (unsigned (__stdcall *)(void*)) _start,
                           thread,
                           CREATE_SUSPENDED,
                           &thread_id);
  if (thread_handle == NULL) {
    // Need to clean up stuff we've allocated so far
    CloseHandle(osthread->interrupt_event());
    thread->set_osthread(NULL);
    delete osthread;
    return NULL;
  }
  
  Atomic::inc_ptr((intptr_t*)&os::win32::_os_thread_count);

  // Store info on the Win32 thread into the OSThread
  osthread->set_thread_handle(thread_handle);
  osthread->set_thread_id(thread_id);

  // Initial thread state is INITIALIZED, not SUSPENDED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(INITIALIZED);
  }

  // The thread is returned suspended (in state INITIALIZED), and is started higher up in the call chain
  return true;
}


// Free Win32 resources related to the OSThread
void os::free_thread(OSThread* osthread) {
  assert(osthread != NULL, "osthread not set");
  CloseHandle(osthread->thread_handle());
  CloseHandle(osthread->interrupt_event());
  delete osthread;
}


static int    has_performance_count = 0;
static jlong first_filetime;
static jlong initial_performance_count;
static jlong performance_frequency;


jlong as_long(LARGE_INTEGER x) {
  jlong result = 0; // initialization to avoid warning
  set_high(&result, x.HighPart);
  set_low(&result,  x.LowPart);
  return result;
}


jlong os::elapsed_counter() {
  LARGE_INTEGER count;  
  if (has_performance_count) {
    QueryPerformanceCounter(&count);
    return as_long(count) - initial_performance_count;
  } else {
    FILETIME wt;
    GetSystemTimeAsFileTime(&wt);
    return (jlong_from(wt.dwHighDateTime, wt.dwLowDateTime) - first_filetime);
  }
}


jlong os::elapsed_frequency() {
  if (has_performance_count) {
    return performance_frequency;
  } else {
   // the FILETIME time is the number of 100-nanosecond intervals since January 1,1601.
   return 10000000;
  }
}


bool os::is_MP() {
  return win32::is_MP();
}

julong os::physical_memory() {
  return win32::physical_memory();
}

julong os::allocatable_physical_memory(julong size) {
  return MIN2(size, (julong)1400*M);
}

int os::processor_count() {
  return win32::processor_count();
}

int os::active_processor_count() {
  UINT_PTR lpProcessAffinityMask = 0;
  UINT_PTR lpSystemAffinityMask = 0;
  int proc_count = win32::processor_count();
  if (proc_count <= sizeof(UINT_PTR) * BitsPerByte &&
      GetProcessAffinityMask(GetCurrentProcess(), &lpProcessAffinityMask, &lpSystemAffinityMask)) {
    // Nof active processors is number of bits in process affinity mask
    int bitcount = 0;
    while (lpProcessAffinityMask != 0) {
      lpProcessAffinityMask = lpProcessAffinityMask & (lpProcessAffinityMask-1);
      bitcount++;
    }
    return bitcount;
  } else {
    return proc_count;
  }
}

bool os::distribute_processes(uint length, uint* distribution) {
  // Not yet implemented.
  return false;
}

bool os::bind_to_processor(uint processor_id) {
  // Not yet implemented.
  return false;
}

static void initialize_performance_counter() {
  LARGE_INTEGER count;
  if (QueryPerformanceFrequency(&count)) {
    has_performance_count = 1;
    performance_frequency = as_long(count);
    QueryPerformanceCounter(&count);
    initial_performance_count = as_long(count);
  } else {
    has_performance_count = 0;
    FILETIME wt;
    GetSystemTimeAsFileTime(&wt);
    first_filetime = jlong_from(wt.dwHighDateTime, wt.dwLowDateTime);
  }
}


double os::elapsedTime() {
  return (double) elapsed_counter() / (double) elapsed_frequency();
}


// Windows format:
//   The FILETIME structure is a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601.
// Java format:
//   Java standards require the number of milliseconds since 1/1/1970 

// Constant offset - calculated using offset()
static jlong  _offset   = 116444736000000000;
// Fake time counter for reproducible results when debugging
static jlong  fake_time = 0;

#ifdef ASSERT
// Just to be safe, recalculate the offset in debug mode
static jlong _calculated_offset = 0;
static int   _has_calculated_offset = 0;

jlong offset() {
  if (_has_calculated_offset) return _calculated_offset;
  SYSTEMTIME java_origin;
  java_origin.wYear          = 1970;
  java_origin.wMonth         = 1; 
  java_origin.wDayOfWeek     = 0; // ignored 
  java_origin.wDay           = 1; 
  java_origin.wHour          = 0; 
  java_origin.wMinute        = 0; 
  java_origin.wSecond        = 0; 
  java_origin.wMilliseconds  = 0; 
  FILETIME jot;
  if (!SystemTimeToFileTime(&java_origin, &jot)) {
    fatal1("Error = %d\nWindows error", GetLastError());
  }
  _calculated_offset = jlong_from(jot.dwHighDateTime, jot.dwLowDateTime);
  _has_calculated_offset = 1;
  assert(_calculated_offset == _offset, "Calculated and constant time offsets must be equal");
  return _calculated_offset;
}
#else
jlong offset() {
  return _offset;
}
#endif

jlong windows_to_java_time(FILETIME wt) {
  jlong a = jlong_from(wt.dwHighDateTime, wt.dwLowDateTime);
  return (a - offset()) / 10000;
}

FILETIME java_to_windows_time(jlong l) {
  jlong a = (l * 10000) + offset();
  FILETIME result;
  result.dwHighDateTime = high(a); 
  result.dwLowDateTime  = low(a);
  return result;
}


jlong os::javaTimeMillis() {
  if (UseFakeTimers) {
    return fake_time++;
  } else {
    FILETIME wt;
    GetSystemTimeAsFileTime(&wt);
    return windows_to_java_time(wt);
  }
}

#define NANOS_PER_SEC         CONST64(1000000000)
#define NANOS_PER_MILLISEC    1000000
jlong os::javaTimeNanos() {
  if (!has_performance_count) { 
    return javaTimeMillis() * NANOS_PER_MILLISEC; // the best we can do.
  } else {
    LARGE_INTEGER current_count;  
    QueryPerformanceCounter(&current_count);
    double current = as_long(current_count);
    double freq = performance_frequency;
    jlong time = (jlong)((current/freq) * NANOS_PER_SEC);
    return time;
  }
}

void os::javaTimeNanos_info(jvmtiTimerInfo *info_ptr) {
  if (!has_performance_count) { 
    // javaTimeMillis() doesn't have much percision,
    // but it is not going to wrap -- so all 64 bits
    info_ptr->max_value = ALL_64_BITS;  

    // this is a wall clock timer, so may skip
    info_ptr->may_skip_backward = true; 
    info_ptr->may_skip_forward = true; 
  } else {
    jlong freq = performance_frequency;
    if (freq < NANOS_PER_SEC) {
      // the performance counter is 64 bits and we will
      // be multiplying it -- so no wrap in 64 bits
      info_ptr->max_value = ALL_64_BITS;
    } else if (freq > NANOS_PER_SEC) {
      // use the max value the counter can reach to
      // determine the max value which could be returned
      julong max_counter = (julong)ALL_64_BITS;
      info_ptr->max_value = (jlong)(max_counter / (freq / NANOS_PER_SEC));
    } else {
      // the performance counter is 64 bits and we will
      // be using it directly -- so no wrap in 64 bits
      info_ptr->max_value = ALL_64_BITS;
    }

    // using a counter, so no skipping
    info_ptr->may_skip_backward = false;  
    info_ptr->may_skip_forward = false;   
  }
  info_ptr->kind = JVMTI_TIMER_ELAPSED;                // elapsed not CPU time
}

void os::shutdown() {  

  // allow PerfMemory to attempt cleanup of any persistent resources
  perfMemory_exit();

  // flush buffered output, finish log files
  ostream_abort();

  // Check for abort hook
  abort_hook_t abort_hook = Arguments::abort_hook();
  if (abort_hook != NULL) {
    abort_hook();
  }
}

void os::abort(bool dump_core)
{
  os::shutdown();
  // no core dump on Windows
  ::exit(1);
}

// Die immediately, no exit hook, no abort hook, no cleanup.
void os::die() {
  _exit(-1);
}

// Directory routines copied from src/win32/native/java/io/dirent_md.c
//  * @(#)dirent_md.c	1.15 00/02/02
//
// The declarations for DIR and struct dirent are in jvm_win32.h.

/* Caller must have already run dirname through JVM_NativePath, which removes
   duplicate slashes and converts all instances of '/' into '\\'. */

DIR *
os::opendir(const char *dirname)
{
    assert(dirname != NULL, "just checking");	// hotspot change
    DIR *dirp = (DIR *)malloc(sizeof(DIR));
    DWORD fattr;				// hotspot change
    char alt_dirname[4] = { 0, 0, 0, 0 };

    if (dirp == 0) {
	errno = ENOMEM;
	return 0;
    }

    /*
     * Win32 accepts "\" in its POSIX stat(), but refuses to treat it
     * as a directory in FindFirstFile().  We detect this case here and
     * prepend the current drive name.
     */
    if (dirname[1] == '\0' && dirname[0] == '\\') {
	alt_dirname[0] = _getdrive() + 'A' - 1;
	alt_dirname[1] = ':';
	alt_dirname[2] = '\\';
	alt_dirname[3] = '\0';
	dirname = alt_dirname;
    }

    dirp->path = (char *)malloc(strlen(dirname) + 5);
    if (dirp->path == 0) {
	free(dirp);
	errno = ENOMEM;
	return 0;
    }
    strcpy(dirp->path, dirname);

    fattr = GetFileAttributes(dirp->path);
    if (fattr == 0xffffffff) {
	free(dirp->path);
	free(dirp);
	errno = ENOENT;
	return 0;
    } else if ((fattr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
	free(dirp->path);
	free(dirp);
	errno = ENOTDIR;
	return 0;
    }

    /* Append "*.*", or possibly "\\*.*", to path */
    if (dirp->path[1] == ':'
	&& (dirp->path[2] == '\0'
	    || (dirp->path[2] == '\\' && dirp->path[3] == '\0'))) {
	/* No '\\' needed for cases like "Z:" or "Z:\" */
	strcat(dirp->path, "*.*");
    } else {
	strcat(dirp->path, "\\*.*");
    }

    dirp->handle = FindFirstFile(dirp->path, &dirp->find_data);
    if (dirp->handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
	    free(dirp->path);
	    free(dirp);
	    errno = EACCES;
	    return 0;
	}
    }
    return dirp;
}

/* parameter dbuf unused on Windows */

struct dirent *
os::readdir(DIR *dirp, dirent *dbuf)
{
    assert(dirp != NULL, "just checking");	// hotspot change
    if (dirp->handle == INVALID_HANDLE_VALUE) {
	return 0;
    }

    strcpy(dirp->dirent.d_name, dirp->find_data.cFileName);

    if (!FindNextFile(dirp->handle, &dirp->find_data)) {
	if (GetLastError() == ERROR_INVALID_HANDLE) {
	    errno = EBADF;
	    return 0;
	}
	FindClose(dirp->handle);
	dirp->handle = INVALID_HANDLE_VALUE;
    }

    return &dirp->dirent;
}

int
os::closedir(DIR *dirp)
{
    assert(dirp != NULL, "just checking");	// hotspot change
    if (dirp->handle != INVALID_HANDLE_VALUE) {
	if (!FindClose(dirp->handle)) {
	    errno = EBADF;
	    return -1;
	}
	dirp->handle = INVALID_HANDLE_VALUE;
    }
    free(dirp->path);
    free(dirp);
    return 0;
}

const char* os::dll_file_extension() { return ".dll"; }

const char * os::get_temp_directory()
{
    static char path_buf[MAX_PATH];
    if (GetTempPath(MAX_PATH, path_buf)>0)
      return path_buf;
    else{
      path_buf[0]='\0';
      return path_buf;
    }
}

//-----------------------------------------------------------
// Helper functions for fatal error handler

// The following library functions are resolved dynamically at runtime:

// PSAPI functions, for Windows NT, 2000, XP

// psapi.h doesn't come with Visual Studio 6; it can be downloaded as Platform 
// SDK from Microsoft.  Here are the definitions copied from psapi.h
typedef struct _MODULEINFO {
    LPVOID lpBaseOfDll;
    DWORD SizeOfImage;
    LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;

static BOOL  (WINAPI *_EnumProcessModules)  ( HANDLE, HMODULE *, DWORD, LPDWORD );
static DWORD (WINAPI *_GetModuleFileNameEx) ( HANDLE, HMODULE, LPTSTR, DWORD );
static BOOL  (WINAPI *_GetModuleInformation)( HANDLE, HMODULE, LPMODULEINFO, DWORD );

// ToolHelp Functions, for Windows 95, 98 and ME

static HANDLE(WINAPI *_CreateToolhelp32Snapshot)(DWORD,DWORD) ;
static BOOL  (WINAPI *_Module32First)           (HANDLE,LPMODULEENTRY32) ;
static BOOL  (WINAPI *_Module32Next)            (HANDLE,LPMODULEENTRY32) ;

bool _has_psapi;
bool _has_toolhelp;

static bool _init_psapi() {
  HINSTANCE psapi = LoadLibrary( "PSAPI.DLL" ) ;
  if( psapi == NULL ) return false ;

  _EnumProcessModules = CAST_TO_FN_PTR(
      BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD),
      GetProcAddress(psapi, "EnumProcessModules")) ;
  _GetModuleFileNameEx = CAST_TO_FN_PTR(
      DWORD (WINAPI *)(HANDLE, HMODULE, LPTSTR, DWORD),
      GetProcAddress(psapi, "GetModuleFileNameExA"));
  _GetModuleInformation = CAST_TO_FN_PTR(
      BOOL (WINAPI *)(HANDLE, HMODULE, LPMODULEINFO, DWORD),
      GetProcAddress(psapi, "GetModuleInformation"));

  _has_psapi = (_EnumProcessModules && _GetModuleFileNameEx && _GetModuleInformation);
  return _has_psapi;
}

static bool _init_toolhelp() {
  HINSTANCE kernel32 = LoadLibrary("Kernel32.DLL") ;
  if (kernel32 == NULL) return false ;

  _CreateToolhelp32Snapshot = CAST_TO_FN_PTR(
      HANDLE(WINAPI *)(DWORD,DWORD),
      GetProcAddress(kernel32, "CreateToolhelp32Snapshot"));
  _Module32First = CAST_TO_FN_PTR(
      BOOL(WINAPI *)(HANDLE,LPMODULEENTRY32),
      GetProcAddress(kernel32, "Module32First" ));
  _Module32Next = CAST_TO_FN_PTR(
      BOOL(WINAPI *)(HANDLE,LPMODULEENTRY32),
      GetProcAddress(kernel32, "Module32Next" ));

  _has_toolhelp = (_CreateToolhelp32Snapshot && _Module32First && _Module32Next);
  return _has_toolhelp;
}

#ifdef _M_AMD64
// Helper routine which returns true if address in
// within the NTDLL address space.
//
static bool _addr_in_ntdll( address addr )
{
  HMODULE hmod;
  MODULEINFO minfo;

  hmod = GetModuleHandle("NTDLL.DLL");
  if ( hmod == NULL ) return false;
  if ( !_GetModuleInformation( GetCurrentProcess(), hmod,
                               &minfo, sizeof(MODULEINFO)) )
    return false;

  if ( (addr >= minfo.lpBaseOfDll) &&
       (addr < (address)((uintptr_t)minfo.lpBaseOfDll + (uintptr_t)minfo.SizeOfImage)))
    return true;
  else
    return false;
}
#endif


// Enumerate all modules for a given process ID
//
// Notice that Windows 95/98/Me and Windows NT/2000/XP have 
// different API for doing this. We use PSAPI.DLL on NT based
// Windows and ToolHelp on 95/98/Me.

// Callback function that is called by enumerate_modules() on
// every DLL module. 
// Input parameters:
//    int       pid,
//    char*     module_file_name,
//    address   module_base_addr,
//    unsigned  module_size,
//    void*     param
typedef int (*EnumModulesCallbackFunc)(int, char *, address, unsigned, void *);

// enumerate_modules for Windows NT, using PSAPI 
static int _enumerate_modules_winnt( int pid, EnumModulesCallbackFunc func, void * param)
{
  HANDLE   hProcess ;

# define MAX_NUM_MODULES 128
  HMODULE     modules[MAX_NUM_MODULES];
  static char filename[ MAX_PATH ];
  int         result = 0;

  if (!_has_psapi) return 0;

  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                         FALSE, pid ) ;
  if (hProcess == NULL) return 0;

  DWORD size_needed;
  if (!_EnumProcessModules(hProcess, modules, 
                           sizeof(modules), &size_needed)) {
      CloseHandle( hProcess );
      return 0;
  }

  // number of modules that are currently loaded
  int num_modules = size_needed / sizeof(HMODULE);

  for (int i = 0; i < MIN2(num_modules, MAX_NUM_MODULES); i++) {
    // Get Full pathname:
    if(!_GetModuleFileNameEx(hProcess, modules[i],
                             filename, sizeof(filename))) {
        filename[0] = '\0';
    }

    MODULEINFO modinfo;
    if (!_GetModuleInformation(hProcess, modules[i],
                               &modinfo, sizeof(modinfo))) {
        modinfo.lpBaseOfDll = NULL;
        modinfo.SizeOfImage = 0;
    }

    // Invoke callback function
    result = func(pid, filename, (address)modinfo.lpBaseOfDll, 
                  modinfo.SizeOfImage, param);
    if (result) break;
  }

  CloseHandle( hProcess ) ;
  return result;
}


// enumerate_modules for Windows 95/98/ME, using TOOLHELP
static int _enumerate_modules_windows( int pid, EnumModulesCallbackFunc func, void *param)
{
  HANDLE                hSnapShot ;
  static MODULEENTRY32  modentry ;
  int                   result = 0;

  if (!_has_toolhelp) return 0;

  // Get a handle to a Toolhelp snapshot of the system
  hSnapShot = _CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid ) ;
  if( hSnapShot == INVALID_HANDLE_VALUE ) {
      return FALSE ;
  }

  // iterate through all modules
  modentry.dwSize = sizeof(MODULEENTRY32) ;
  bool not_done = _Module32First( hSnapShot, &modentry ) ;

  while( not_done ) {
    // invoke the callback
    result=func(pid, modentry.szExePath, (address)modentry.modBaseAddr, 
                modentry.modBaseSize, param);
    if (result) break;

    modentry.dwSize = sizeof(MODULEENTRY32) ;
    not_done = _Module32Next( hSnapShot, &modentry );
  }

  CloseHandle(hSnapShot);
  return result;
}

int enumerate_modules( int pid, EnumModulesCallbackFunc func, void * param )
{
  // Get current process ID if caller doesn't provide it.
  if (!pid) pid = os::current_process_id();

  if (os::win32::is_nt()) return _enumerate_modules_winnt  (pid, func, param);
  else                    return _enumerate_modules_windows(pid, func, param);
}

struct _modinfo {
   address addr;
   char*   full_path;   // point to a char buffer
   int     buflen;      // size of the buffer
   address base_addr;
};

static int _locate_module_by_addr(int pid, char * mod_fname, address base_addr, 
                                  unsigned size, void * param) {
   struct _modinfo *pmod = (struct _modinfo *)param;
   if (!pmod) return -1;

   if (base_addr     <= pmod->addr && 
       base_addr+size > pmod->addr) {
     // if a buffer is provided, copy path name to the buffer
     if (pmod->full_path) {
       jio_snprintf(pmod->full_path, pmod->buflen, "%s", mod_fname);
     }
     pmod->base_addr = base_addr;
     return 1;
   }
   return 0;
}

bool os::dll_address_to_library_name(address addr, char* buf,
                                     int buflen, int* offset) {
// NOTE: the reason we don't use SymGetModuleInfo() is it doesn't always
//       return the full path to the DLL file, sometimes it returns path
//       to the corresponding PDB file (debug info); sometimes it only 
//       returns partial path, which makes life painful.

   struct _modinfo mi;
   mi.addr      = addr;
   mi.full_path = buf;
   mi.buflen    = buflen;
   int pid = os::current_process_id();
   if (enumerate_modules(pid, _locate_module_by_addr, (void *)&mi)) {
      // buf already contains path name
      if (offset) *offset = addr - mi.base_addr;
      return true;
   } else {
      if (buf) buf[0] = '\0';
      if (offset) *offset = -1;
      return false;
   }
}

bool os::dll_address_to_function_name(address addr, char *buf,
                                      int buflen, int *offset) {
  // Unimplemented on Windows - in order to use SymGetSymFromAddr(),
  // we need to initialize imagehlp/dbghelp, then load symbol table 
  // for every module. That's too much work to do after a fatal error.
  // For an example on how to implement this function, see 1.4.2.
  if (offset)  *offset  = -1;
  if (buf) buf[0] = '\0';
  return false;
}

// save the start and end address of jvm.dll into param[0] and param[1]
static int _locate_jvm_dll(int pid, char* mod_fname, address base_addr, 
                    unsigned size, void * param) {
   if (!param) return -1;

   if (base_addr     <= (address)_locate_jvm_dll &&
       base_addr+size > (address)_locate_jvm_dll) {
         ((address*)param)[0] = base_addr;
         ((address*)param)[1] = base_addr + size;
         return 1;
   }
   return 0;
}

address vm_lib_location[2];    // start and end address of jvm.dll

// check if addr is inside jvm.dll
bool os::address_is_in_vm(address addr) {
  if (!vm_lib_location[0] || !vm_lib_location[1]) {
    int pid = os::current_process_id();
    if (!enumerate_modules(pid, _locate_jvm_dll, (void *)vm_lib_location)) {
      assert(false, "Can't find jvm module.");
      return false;
    }
  }

  return (vm_lib_location[0] <= addr) && (addr < vm_lib_location[1]);
}

// print module info; param is outputStream*
static int _print_module(int pid, char* fname, address base, 
                         unsigned size, void* param) {
   if (!param) return -1;

   outputStream* st = (outputStream*)param;

   address end_addr = base + size;
   st->print(PTR_FORMAT " - " PTR_FORMAT " \t%s\n", base, end_addr, fname);
   return 0;
}

void os::print_dll_info(outputStream *st) {
   int pid = os::current_process_id();
   st->print_cr("Dynamic libraries:");
   enumerate_modules(pid, _print_module, (void *)st);
}

void os::print_os_info(outputStream* st) {
   st->print("OS:");

   OSVERSIONINFOEX osvi;
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if (!GetVersionEx((OSVERSIONINFO *)&osvi)) {
      st->print_cr("N/A");
      return;
   }

   int os_vers = osvi.dwMajorVersion * 1000 + osvi.dwMinorVersion;

   if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
     switch (os_vers) {
       case 3051: st->print(" Windows NT 3.51"); break;
       case 4000: st->print(" Windows NT 4.0"); break;
       case 5000: st->print(" Windows 2000"); break;
       case 5001: st->print(" Windows XP"); break;
       case 5002: st->print(" Windows Server 2003 family"); break;
       default: // future windows, print out its major and minor versions
                st->print(" Windows NT %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
     }
   } else {
     switch (os_vers) {
       case 4000: st->print(" Windows 95"); break;
       case 4010: st->print(" Windows 98"); break;
       case 4090: st->print(" Windows Me"); break;
       default: // future windows, print out its major and minor versions
                st->print(" Windows %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
     }
   }

   st->print(" Build %d", osvi.dwBuildNumber);
   st->print(" %s", osvi.szCSDVersion);           // service pack
   st->cr();
}

void os::print_memory_info(outputStream* st) {
  st->print("Memory:");
  st->print(" %dk page", os::vm_page_size()>>10);

  // FIXME: GlobalMemoryStatus() may return incorrect value if total memory
  // is larger than 4GB
  MEMORYSTATUS ms;
  GlobalMemoryStatus(&ms);

  st->print(", physical %uk", ms.dwTotalPhys >> 10);
  st->print("(%uk free)", ms.dwAvailPhys >> 10);

  st->print(", swap %uk", ms.dwTotalPageFile >> 10);
  st->print("(%uk free)", ms.dwAvailPageFile >> 10);
  st->cr();
}

void os::print_siginfo(outputStream *st, void *siginfo) {
  EXCEPTION_RECORD* er = (EXCEPTION_RECORD*)siginfo;
  st->print("siginfo:");
  st->print(" ExceptionCode=0x%x", er->ExceptionCode);

  if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && 
      er->NumberParameters >= 2) {
      switch (er->ExceptionInformation[0]) {
      case 0: st->print(", reading address"); break;
      case 1: st->print(", writing address"); break;
      default: st->print(", ExceptionInformation=" INTPTR_FORMAT,
                            er->ExceptionInformation[0]);
      }
      st->print(" " INTPTR_FORMAT, er->ExceptionInformation[1]);
  } else {
      int num = er->NumberParameters;
      if (num > 0) {
        st->print(", ExceptionInformation=");
        for (int i = 0; i < num; i++) {
          st->print(INTPTR_FORMAT " ", er->ExceptionInformation[i]);
        }
      }
  }
  st->cr();
}

static char saved_jvm_path[MAX_PATH] = {0};

// Find the full path to the current module, jvm.dll or jvm_g.dll
void os::jvm_path(char *buf, jint buflen) {
  // Error checking.
  if (buflen < MAX_PATH) {
    assert(false, "must use a large-enough buffer");
    buf[0] = '\0';
    return;
  }
  // Lazy resolve the path to current module.
  if (saved_jvm_path[0] != 0) {
    strcpy(buf, saved_jvm_path);
    return;
  }

  GetModuleFileName(vm_lib_handle, buf, buflen);
#ifndef PRODUCT
  // Typical value for this path on Windows is 
  // "<JAVA_HOME>\jre\bin\{client|server}\jvm[_g].dll".
  // If "/jre/bin/" appears at the right place in the string, 
  // then assume we are installed in a JDK and we're done.
  // Otherwise, check for a JAVA_HOME environment variable and fix
  // up the path so it looks like jvm.dll is installed there 
  // (append a fake suffix "jre\bin\hotspot\jvm.dll").
  const char *p = buf + strlen(buf) - 1;
  for (int count = 0; p > buf && count < 4; ++count) {
    for (--p; p > buf && *p != '\\'; --p)
      /* empty */ ;
  }

  //  On Win98 GetModuleFileName() returns the path in the upper case.
  if (_strnicmp(p, "\\jre\\bin\\", 9) != 0 ) { // Not in JDK
    // Look for JAVA_HOME in the environment.
    char java_home_var[MAX_PATH-16];
    if (getenv("JAVA_HOME", java_home_var, MAX_PATH-16)) {
      // Check the current module name "jvm.dll" or "jvm_g.dll".
      p = strrchr(buf, '\\');
      assert(_strnicmp(p, "\\jvm",4) == 0, "invalid library name");
      p = (_strnicmp(p,"\\jvm_g",6) == 0 ) ? "_g" : "";
      strcpy(buf, java_home_var);
      sprintf(buf + strlen(buf), "\\jre\\bin\\hotspot\\jvm%s.dll", p);
    }
  }
#endif // #ifndef PRODUCT

  strcpy(saved_jvm_path, buf);
}


void os::print_jni_name_prefix_on(outputStream* st, int args_size) {
#ifndef _WIN64
  st->print("_");
#endif
}


void os::print_jni_name_suffix_on(outputStream* st, int args_size) {
#ifndef _WIN64
  st->print("@%d", args_size  * sizeof(int));
#endif
}

// sun.misc.Signal
// NOTE that this is a workaround for an apparent kernel bug where if
// a signal handler for SIGBREAK is installed then that signal handler
// takes priority over the console control handler for CTRL_CLOSE_EVENT.
// See bug 4416763.
static void (*sigbreakHandler)(int) = NULL;

static void UserHandler(int sig, void *siginfo, void *context) {
  os::signal_notify(sig);
  // We need to reinstate the signal handler each time... 
  os::signal(sig, (void*)UserHandler);
}

void* os::user_handler() {
  return (void*) UserHandler;
}

void* os::signal(int signal_number, void* handler) {
  if ((signal_number == SIGBREAK) && (!ReduceSignalUsage)) {
    void (*oldHandler)(int) = sigbreakHandler;
    sigbreakHandler = (void (*)(int)) handler;
    return (void*) oldHandler;
  } else {
    return (void*)::signal(signal_number, (void (*)(int))handler);
  }
}

void os::signal_raise(int signal_number) {
  raise(signal_number);
}

// The Win32 C runtime library maps all console control events other than ^C
// into SIGBREAK, which makes it impossible to distinguish ^BREAK from close,
// logoff, and shutdown events.  We therefore install our own console handler
// that raises SIGTERM for the latter cases.
//
static BOOL WINAPI consoleHandler(DWORD event) {
  switch(event) {
    case CTRL_C_EVENT:
      if (is_error_reported()) {
        // Ctrl-C is pressed during error reporting, likely because the error
        // handler fails to abort. Let VM die immediately.
        os::die();
      }

      os::signal_raise(SIGINT);
      return TRUE;
      break;
    case CTRL_BREAK_EVENT:
      if (sigbreakHandler != NULL) {
        (*sigbreakHandler)(SIGBREAK);
      }
      return TRUE;
      break;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      os::signal_raise(SIGTERM);
      return TRUE;
      break;
    default:
      break;
  }
  return FALSE;
}

/*
 * The following code is moved from os.cpp for making this
 * code platform specific, which it is by its very nature.
 */

// Return maximum OS signal used + 1 for internal use only
// Used as exit signal for signal_thread
int os::sigexitnum_pd(){
  return NSIG;
}

// a counter for each possible signal value, including signal_thread exit signal
static volatile jint pending_signals[NSIG+1] = { 0 };
static HANDLE sig_sem;

void os::signal_init_pd() {
  // Initialize signal structures
  memset((void*)pending_signals, 0, sizeof(pending_signals));
  
  sig_sem = ::CreateSemaphore(NULL, 0, NSIG+1, NULL);

  // Programs embedding the VM do not want it to attempt to receive
  // events like CTRL_LOGOFF_EVENT, which are used to implement the
  // shutdown hooks mechanism introduced in 1.3.  For example, when
  // the VM is run as part of a Windows NT service (i.e., a servlet
  // engine in a web server), the correct behavior is for any console
  // control handler to return FALSE, not TRUE, because the OS's
  // "final" handler for such events allows the process to continue if
  // it is a service (while terminating it if it is not a service).
  // To make this behavior uniform and the mechanism simpler, we
  // completely disable the VM's usage of these console events if -Xrs
  // (=ReduceSignalUsage) is specified.  This means, for example, that
  // the CTRL-BREAK thread dump mechanism is also disabled in this
  // case.  See bugs 4323062, 4345157, and related bugs.
  
  if (!ReduceSignalUsage) {
    // Add a CTRL-C handler
    SetConsoleCtrlHandler(consoleHandler, TRUE);
  }
}

void os::signal_notify(int signal_number) {
  BOOL ret;

  Atomic::inc(&pending_signals[signal_number]);
  ret = ::ReleaseSemaphore(sig_sem, 1, NULL);
  assert(ret != 0, "ReleaseSemaphore() failed");
}  

static int check_pending_signals(bool wait_for_signal) {
  DWORD ret;
  while (true) {
    for (int i = 0; i < NSIG + 1; i++) {
      jint n = pending_signals[i];
      if (n > 0 && n == Atomic::cmpxchg(n - 1, &pending_signals[i], n)) {
        return i;
      }
    }
    if (!wait_for_signal) {
      return -1;
    }

    JavaThread *thread = JavaThread::current();

    ThreadBlockInVM tbivm(thread);

    bool threadIsSuspended;
    do {
      thread->set_suspend_equivalent();
      // cleared by handle_special_suspend_equivalent_condition() or java_suspend_self()
      ret = ::WaitForSingleObject(sig_sem, INFINITE);
      assert(ret == WAIT_OBJECT_0, "WaitForSingleObject() failed");

      // were we externally suspended while we were waiting?
      threadIsSuspended = thread->handle_special_suspend_equivalent_condition();
      if (threadIsSuspended) {
        //
        // The semaphore has been incremented, but while we were waiting
        // another thread suspended us. We don't want to continue running
        // while suspended because that would surprise the thread that
        // suspended us.
        //
        ret = ::ReleaseSemaphore(sig_sem, 1, NULL);
        assert(ret != 0, "ReleaseSemaphore() failed");

        thread->java_suspend_self();
      }
    } while (threadIsSuspended);
  }
}

int os::signal_lookup() {
  return check_pending_signals(false);
}

int os::signal_wait() {
  return check_pending_signals(true);
}

// Implicit OS exception handling

LONG Handle_Exception(struct _EXCEPTION_POINTERS* exceptionInfo, address handler) {
  JavaThread* thread = JavaThread::current();
  // Save pc in thread
#ifdef _M_IA64
  thread->set_saved_exception_pc((address)exceptionInfo->ContextRecord->StIIP);
  // Set pc to handler
  exceptionInfo->ContextRecord->StIIP = (DWORD64)handler;  
#elif _M_AMD64
  thread->set_saved_exception_pc((address)exceptionInfo->ContextRecord->Rip);
  // Set pc to handler
  exceptionInfo->ContextRecord->Rip = (DWORD64)handler;  
#else
  thread->set_saved_exception_pc((address)exceptionInfo->ContextRecord->Eip);
  // Set pc to handler
  exceptionInfo->ContextRecord->Eip = (LONG)handler;  
#endif

  // Continue the execution
  return EXCEPTION_CONTINUE_EXECUTION;
}


// Used for PostMortemDump
extern "C" void safepoints();
extern "C" void find(int x);
extern "C" void events();

// According to Windows API documentation, an illegal instruction sequence should generate
// the 0xC000001C exception code. However, real world experience shows that occasionnaly
// the execution of an illegal instruction can generate the exception code 0xC000001E. This
// seems to be an undocumented feature of Win NT 4.0 (and probably other Windows systems).

#define EXCEPTION_ILLEGAL_INSTRUCTION_2 0xC000001E

// From "Execution Protection in the Windows Operating System" draft 0.35
// Once a system header becomes available, the "real" define should be
// included or copied here.
#define EXCEPTION_INFO_EXEC_VIOLATION 0x08

#define def_excpt(val) #val, val

struct siglabel {
  char *name;
  int   number;
};

struct siglabel exceptlabels[] = {
    def_excpt(EXCEPTION_ACCESS_VIOLATION),
    def_excpt(EXCEPTION_DATATYPE_MISALIGNMENT),
    def_excpt(EXCEPTION_BREAKPOINT),
    def_excpt(EXCEPTION_SINGLE_STEP),
    def_excpt(EXCEPTION_ARRAY_BOUNDS_EXCEEDED),
    def_excpt(EXCEPTION_FLT_DENORMAL_OPERAND),
    def_excpt(EXCEPTION_FLT_DIVIDE_BY_ZERO),
    def_excpt(EXCEPTION_FLT_INEXACT_RESULT),
    def_excpt(EXCEPTION_FLT_INVALID_OPERATION),
    def_excpt(EXCEPTION_FLT_OVERFLOW),
    def_excpt(EXCEPTION_FLT_STACK_CHECK),
    def_excpt(EXCEPTION_FLT_UNDERFLOW),
    def_excpt(EXCEPTION_INT_DIVIDE_BY_ZERO),
    def_excpt(EXCEPTION_INT_OVERFLOW),
    def_excpt(EXCEPTION_PRIV_INSTRUCTION),
    def_excpt(EXCEPTION_IN_PAGE_ERROR),
    def_excpt(EXCEPTION_ILLEGAL_INSTRUCTION),
    def_excpt(EXCEPTION_ILLEGAL_INSTRUCTION_2),
    def_excpt(EXCEPTION_NONCONTINUABLE_EXCEPTION),
    def_excpt(EXCEPTION_STACK_OVERFLOW),
    def_excpt(EXCEPTION_INVALID_DISPOSITION),
    def_excpt(EXCEPTION_GUARD_PAGE),
    def_excpt(EXCEPTION_INVALID_HANDLE),
    NULL, 0
};

const char * os::exception_name(int exception_code, char *buf, int size) {
  for (int i = 0; exceptlabels[i].name != NULL; i++) {
    if (exceptlabels[i].number == exception_code) {
       jio_snprintf(buf, size, "%s", exceptlabels[i].name);
       return buf;
    }
  }

  return NULL;
}

//-----------------------------------------------------------------------------
LONG Handle_IDiv_Exception(struct _EXCEPTION_POINTERS* exceptionInfo) {
  // handle exception caused by idiv; should only happen for -MinInt/-1
  // (division by zero is handled explicitly)
#ifdef _M_IA64
  assert(0, "Fix Handle_IDiv_Exception");
#elif _M_AMD64
  PCONTEXT ctx = exceptionInfo->ContextRecord;
  address pc = (address)ctx->Rip;
  NOT_PRODUCT(Events::log("idiv overflow exception at " INTPTR_FORMAT , pc));
  assert(pc[0] == 0xF7, "not an idiv opcode");
  assert((pc[1] & ~0x7) == 0xF8, "cannot handle non-register operands");
  assert(ctx->Rax == min_jint, "unexpected idiv exception");
  // set correct result values and continue after idiv instruction
  ctx->Rip = (DWORD)pc + 2;        // idiv reg, reg  is 2 bytes
  ctx->Rax = (DWORD)min_jint;      // result
  ctx->Rdx = (DWORD)0;             // remainder
  // Continue the execution
#else
  PCONTEXT ctx = exceptionInfo->ContextRecord;
  address pc = (address)ctx->Eip;
  NOT_PRODUCT(Events::log("idiv overflow exception at " INTPTR_FORMAT , pc));
  assert(pc[0] == 0xF7, "not an idiv opcode");
  assert((pc[1] & ~0x7) == 0xF8, "cannot handle non-register operands");
  assert(ctx->Eax == min_jint, "unexpected idiv exception");
  // set correct result values and continue after idiv instruction
  ctx->Eip = (DWORD)pc + 2;        // idiv reg, reg  is 2 bytes
  ctx->Eax = (DWORD)min_jint;      // result
  ctx->Edx = (DWORD)0;             // remainder
  // Continue the execution
#endif
  return EXCEPTION_CONTINUE_EXECUTION;
}

//-----------------------------------------------------------------------------

LONG WINAPI topLevelExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo) {
  if (InterceptOSException) return EXCEPTION_CONTINUE_SEARCH;  
  DWORD exception_code = exceptionInfo->ExceptionRecord->ExceptionCode;
#ifdef _M_IA64
  address pc = (address) exceptionInfo->ContextRecord->StIIP;
#elif _M_AMD64
  address pc = (address) exceptionInfo->ContextRecord->Rip;
#else
  address pc = (address) exceptionInfo->ContextRecord->Eip;
#endif
  Thread* t = ThreadLocalStorage::get_thread_slow();          // slow & steady

#ifndef _WIN64
  // Execution protection violation - win32 running on AMD64 only
  // Handled first to avoid misdiagnosis as a "normal" access violation;
  // This is safe to do because we have a new/unique ExceptionInformation
  // code for this condition.
  if (exception_code == EXCEPTION_ACCESS_VIOLATION) {
    PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;
    int exception_subcode = (int) exceptionRecord->ExceptionInformation[0];
    address addr = (address) exceptionRecord->ExceptionInformation[1];
    
    if (exception_subcode == EXCEPTION_INFO_EXEC_VIOLATION) {
      int page_size = os::vm_page_size();
      
      // Make sure the pc and the faulting address are sane.
      //
      // If an instruction spans a page boundary, and the page containing
      // the beginning of the instruction is executable but the following
      // page is not, the pc and the faulting address might be slightly
      // different - we still want to unguard the 2nd page in this case.
      //
      // 15 bytes seems to be a (very) safe value for max instruction size.
      bool pc_is_near_addr = 
        (pointer_delta((void*) addr, (void*) pc, sizeof(char)) < 15);
      bool instr_spans_page_boundary =
        (align_size_down((intptr_t) pc ^ (intptr_t) addr,
                         (intptr_t) page_size) > 0);

      if (pc == addr || (pc_is_near_addr && instr_spans_page_boundary)) {
        static volatile address last_addr =
          (address) os::non_memory_address_word();

        // In conservative mode, don't unguard unless the address is in the VM
        if (UnguardOnExecutionViolation > 0 && addr != last_addr &&
            (UnguardOnExecutionViolation > 1 || os::address_is_in_vm(addr))) {
          
          // Unguard and retry
          address page_start =
            (address) align_size_down((intptr_t) addr, (intptr_t) page_size);
          bool res = os::unguard_memory((char*) page_start, page_size);
          
          if (PrintMiscellaneous && Verbose) {
            char buf[256];
            jio_snprintf(buf, sizeof(buf), "Execution protection violation "
                         "at " INTPTR_FORMAT
                         ", unguarding " INTPTR_FORMAT ": %s", addr,
                         page_start, (res ? "success" : strerror(errno)));
            tty->print_raw_cr(buf);
          }

          // Set last_addr so if we fault again at the same address, we don't
          // end up in an endless loop.
          // 
          // There are two potential complications here.  Two threads trapping
          // at the same address at the same time could cause one of the
          // threads to think it already unguarded, and abort the VM.  Likely
          // very rare.
          // 
          // The other race involves two threads alternately trapping at
          // different addresses and failing to unguard the page, resulting in
          // an endless loop.  This condition is probably even more unlikely
          // than the first.
          //
          // Although both cases could be avoided by using locks or thread
          // local last_addr, these solutions are unnecessary complication:
          // this handler is a best-effort safety net, not a complete solution.
          // It is disabled by default and should only be used as a workaround
          // in case we missed any no-execute-unsafe VM code.

          last_addr = addr;

          return EXCEPTION_CONTINUE_EXECUTION;
        }
      }

      // Last unguard failed or not unguarding
      tty->print_raw_cr("Execution protection violation");
      VMError err(t, exception_code, addr, exceptionInfo->ExceptionRecord,
                  exceptionInfo->ContextRecord);
      err.report_and_die();
    }
  }
#endif // _WIN64

  if (t != NULL && t->is_Java_thread()) {
    JavaThread* thread = (JavaThread*) t;
    bool in_java = thread->thread_state() == _thread_in_Java;

    // Handle potential stack overflows up front.
    if (exception_code == EXCEPTION_STACK_OVERFLOW) {
      if (os::uses_stack_guard_pages()) {
#ifdef _M_IA64
        //
        // If it's a legal stack address continue, Windows will map it in. 
        //
        PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;
        address addr = (address) exceptionRecord->ExceptionInformation[1];
        if (addr > thread->stack_yellow_zone_base() && addr < thread->stack_base() ) 
          return EXCEPTION_CONTINUE_EXECUTION; 

        // The register save area is the same size as the memory stack
        // and starts at the page just above the start of the memory stack.
        // If we get a fault in this area, we've run out of register
        // stack.  If we are in java, try throwing a stack overflow exception.
        if (addr > thread->stack_base() && 
                      addr <= (thread->stack_base()+thread->stack_size()) ) {
	  char buf[256];
	  jio_snprintf(buf, 256,
		       "Register stack overflow, addr:%p, stack_base:%p\n",
		       addr, thread->stack_base() );
	  tty->print_raw_cr(buf);
	  // If not in java code, return and hope for the best.
	  return in_java ? Handle_Exception(exceptionInfo,
            SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW))
            :  EXCEPTION_CONTINUE_EXECUTION;
        }
#endif
        if (thread->stack_yellow_zone_enabled()) {
          // Yellow zone violation.  The o/s has unprotected the first yellow
          // zone page for us.  Note:  must call disable_stack_yellow_zone to
          // update the enabled status, even if the zone contains only one page.
          thread->disable_stack_yellow_zone();
	  // If not in java code, return and hope for the best.
	  return in_java ? Handle_Exception(exceptionInfo,
            SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW))
            :  EXCEPTION_CONTINUE_EXECUTION;
        } else {
          // Fatal red zone violation. 
          thread->disable_stack_red_zone();
          tty->print_raw_cr("An unrecoverable stack overflow has occurred.");
          VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
          err.report_and_die();
          return EXCEPTION_CONTINUE_SEARCH;
        }
      } else if (in_java) {
        // JVM-managed guard pages cannot be used on win95/98.  The o/s provides
        // a one-time-only guard page, which it has released to us.  The next
        // stack overflow on this thread will result in an ACCESS_VIOLATION.
        return Handle_Exception(exceptionInfo,
          SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW));
      } else {
        // Can only return and hope for the best.  Further stack growth will
        // result in an ACCESS_VIOLATION.
	return EXCEPTION_CONTINUE_EXECUTION;
      }
    } else if (exception_code == EXCEPTION_ACCESS_VIOLATION) {
      // Either stack overflow or null pointer exception.
      if (in_java) {
        PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;
	address addr = (address) exceptionRecord->ExceptionInformation[1];
	address stack_end = thread->stack_base() - thread->stack_size();
	if (addr < stack_end && addr >= stack_end - os::vm_page_size()) {
	  // Stack overflow.
	  assert(!os::uses_stack_guard_pages(),
	    "should be caught by red zone code above.");
	  return Handle_Exception(exceptionInfo,
	    SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW));
	}
#ifndef CORE
	//
	// Check for safepoint polling and implicit null
	// We only expect null pointers in the stubs (vtable)
	// the rest are checked explicitly now.
	//
	CodeBlob* cb = CodeCache::find_blob(pc);
	if (cb != NULL) {
          if (SafepointPolling && os::is_poll_address(addr) && cb->is_nmethod()) {

            // Look up the relocation information
            assert( ((nmethod*)cb)->is_at_poll_or_poll_return(pc),
              "relocation type does not indicate safepoint polling");
            assert( ((NativeInstruction*)pc)->is_safepoint_poll(),
              "Only polling locations are used for safepoint");

            // Handle a Safepoint Poll
            if (((nmethod*)cb)->is_at_poll_return(pc)) {
#ifndef PRODUCT
              if( TraceSafepoint )
                tty->print("... found polling page return exception at pc = " INTPTR_FORMAT "\n",
                  (intptr_t)pc);
#endif

#ifdef COMPILER1
              return Handle_Exception(exceptionInfo,
                Runtime1::entry_for(Runtime1::polling_page_return_handler_id));
#else
              assert(OptoRuntime::polling_page_return_handler_blob() != NULL,
                "stub not created yet");
              return Handle_Exception(exceptionInfo,
                (address)OptoRuntime::polling_page_return_handler_blob()->instructions_begin());
#endif
            }

            // Handle a Safepoint Poll
            else {
#ifndef PRODUCT
              if( TraceSafepoint )
                tty->print("... found polling page safepoint exception at pc = " INTPTR_FORMAT "\n",
                  (intptr_t)pc);
#endif

#ifdef COMPILER1
              return Handle_Exception(exceptionInfo,
                Runtime1::entry_for(Runtime1::polling_page_safepoint_handler_id));
#else
              assert(OptoRuntime::polling_page_safepoint_handler_blob() != NULL,
                "stub not created yet");
              return Handle_Exception(exceptionInfo,
                (address)OptoRuntime::polling_page_safepoint_handler_blob()->instructions_begin());
#endif
            }
          }
        }
#endif // ndef CORE
        {
#ifdef _WIN64
          //
	  // If it's a legal stack address map the entire region in
          //
          PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;
	  address addr = (address) exceptionRecord->ExceptionInformation[1];
	  if (addr > thread->stack_yellow_zone_base() && addr < thread->stack_base() ) {
                  addr = (address)((uintptr_t)addr & 
                         (~((uintptr_t)os::vm_page_size() - (uintptr_t)1)));
                  os::commit_memory( (char *)addr, thread->stack_base() - addr );
	          return EXCEPTION_CONTINUE_EXECUTION;
          }
          else 
#endif
          {
	    // Null pointer exception.
#ifdef _M_IA64
            // We catch register stack overflows in compiled code by doing
            // an explicit compare and executing a st8(G0, G0) if the
            // BSP enters into our guard area.  We test for the overflow
            // condition and fall into the normal null pointer exception 
            // code if BSP hasn't overflowed.
            if ( in_java ) {
              if(thread->register_stack_overflow()) {
                assert((address)exceptionInfo->ContextRecord->IntS3 == 
                                thread->register_stack_limit(), 
                               "GR7 doesn't contain register_stack_limit");
                // Disable the yellow zone which sets the state that 
                // we've got a stack overflow problem.
                if (thread->stack_yellow_zone_enabled()) {
                  thread->disable_stack_yellow_zone();
                }
                // Give us some room to process the exception
                thread->disable_register_stack_guard();
                // Update GR7 with the new limit so we can continue running
                // compiled code.
                exceptionInfo->ContextRecord->IntS3 = 
                               (ULONGLONG)thread->register_stack_limit();
                return Handle_Exception(exceptionInfo, 
                       SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW));
              } else {
#ifndef CORE
		//
		// Check for implicit null
		// We only expect null pointers in the stubs (vtable)
		// the rest are checked explicitly now.
		//
		CodeBlob* cb = CodeCache::find_blob(pc);
		if (cb != NULL) {
		  if (VtableStubs::stub_containing(pc) != NULL) {
		    if (((uintptr_t)addr) < os::vm_page_size() ) {
		      // an access to the first page of VM--assume it is a null pointer
		      return Handle_Exception(exceptionInfo,
			SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_NULL));
		    }
		  }
		}
#endif
	      }
            } // in_java

            // IA64 doesn't use implicit null checking yet. So we shouldn't
            // get here.
            tty->print_raw_cr("Access violation, possible null pointer exception");
            VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
            err.report_and_die();
#else /* !IA64 */

            // Windows 98 reports faulting addresses incorrectly
	    if (!MacroAssembler::needs_explicit_null_check((intptr_t)addr) ||
                !os::win32::is_nt()) {
	      return Handle_Exception(exceptionInfo,
		SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_NULL));
	    }
	    VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
	    err.report_and_die();
#endif
          }
	}
      }

#ifdef _WIN64
      // Special care for fast JNI field accessors.
      // jni_fast_Get<Primitive>Field can trap at certain pc's if a GC kicks
      // in and the heap gets shrunk before the field access.
      if (exception_code == EXCEPTION_ACCESS_VIOLATION) {
        address addr = JNI_FastGetField::find_slowcase_pc(pc);
        if (addr != (address)-1) {
          return Handle_Exception(exceptionInfo, addr);
        }
      }
#endif

#ifdef _M_AMD64
      // Windows will sometimes generate an access violation
      // when we call malloc.  Since we use VectoredExceptions
      // on 64 bit platforms, we see this exception.  We must
      // pass this exception on so Windows can recover.
      // We check to see if the pc of the fault is in NTDLL.DLL
      // if so, we pass control on to Windows for handling.
      if ( _addr_in_ntdll(pc) ) return EXCEPTION_CONTINUE_SEARCH;
#endif

      // Stack overflow or null pointer exception in native code.
      VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
      err.report_and_die();
      return EXCEPTION_CONTINUE_SEARCH;
    }

    if (in_java) {
      switch (exception_code) {
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return Handle_Exception(exceptionInfo, SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_DIVIDE_BY_ZERO));

      case EXCEPTION_INT_OVERFLOW:
        return Handle_IDiv_Exception(exceptionInfo);
      
#ifndef CORE
      // Only used for safepoints in compiler
      case EXCEPTION_ILLEGAL_INSTRUCTION: // fall-through
      case EXCEPTION_ILLEGAL_INSTRUCTION_2: {       
#ifdef _M_IA64
        assert(0, "Fix Exception Filter");
        {
#else
#ifdef _M_AMD64
        NativeInstruction *inst  = (NativeInstruction *)exceptionInfo->ContextRecord->Rip;
#else
        NativeInstruction *inst  = (NativeInstruction *)exceptionInfo->ContextRecord->Eip;
#endif
        if (inst->is_illegal()) {   
#ifdef COMPILER1
          return Handle_Exception(exceptionInfo, Runtime1::entry_for(Runtime1::illegal_instruction_handler_id));
#else
          assert(OptoRuntime::illegal_exception_handler_blob() != NULL, "stub not created yet");
          return Handle_Exception(exceptionInfo, (address)OptoRuntime::illegal_exception_handler_blob()->instructions_begin());
#endif
#endif // else _M_IA64
        }        
        break;      
      }
#endif      
      } // switch
    }
  }

  if (previous_toplevel_exception_filter != NULL) {
    // Call an already existing toplevel exception handler
    return previous_toplevel_exception_filter(exceptionInfo);
  } else if (exception_code != EXCEPTION_BREAKPOINT) {    
#ifndef _WIN64
    VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
    err.report_and_die();
#else
    // Itanium Windows uses a VectoredExceptionHandler
    // Which means that C++ programatic exception handlers (try/except)
    // will get here.  Continue the search for the right except block if
    // the exception code is not a fatal code.
    switch ( exception_code ) {
      case EXCEPTION_ACCESS_VIOLATION:
      case EXCEPTION_STACK_OVERFLOW:
      case EXCEPTION_ILLEGAL_INSTRUCTION:
      case EXCEPTION_ILLEGAL_INSTRUCTION_2:
      case EXCEPTION_INT_OVERFLOW:
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
      {  VMError err(t, exception_code, pc, exceptionInfo->ExceptionRecord, exceptionInfo->ContextRecord);
         err.report_and_die();
      }
        break;
      default:
        break;
    }
#endif
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

#ifndef _WIN64
// Special care for fast JNI accessors.
// jni_fast_Get<Primitive>Field can trap at certain pc's if a GC kicks in and
// the heap gets shrunk before the field access.
// Need to install our own structured exception handler since native code may
// install its own.
LONG WINAPI fastJNIAccessorExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo) {
  DWORD exception_code = exceptionInfo->ExceptionRecord->ExceptionCode;
  if (exception_code == EXCEPTION_ACCESS_VIOLATION) {
    address pc = (address) exceptionInfo->ContextRecord->Eip;
    address addr = JNI_FastGetField::find_slowcase_pc(pc);
    if (addr != (address)-1) {
      return Handle_Exception(exceptionInfo, addr);
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

#define DEFINE_FAST_GETFIELD(Return,Fieldname,Result) \
Return JNICALL jni_fast_Get##Result##Field_wrapper(JNIEnv *env, jobject obj, jfieldID fieldID) { \
  __try { \
    return (*JNI_FastGetField::jni_fast_Get##Result##Field_fp)(env, obj, fieldID); \
  } __except(fastJNIAccessorExceptionFilter((_EXCEPTION_POINTERS*)_exception_info())) { \
  } \
  return 0; \
}

DEFINE_FAST_GETFIELD(jboolean, bool,   Boolean)
DEFINE_FAST_GETFIELD(jbyte,    byte,   Byte)
DEFINE_FAST_GETFIELD(jchar,    char,   Char)
DEFINE_FAST_GETFIELD(jshort,   short,  Short)
DEFINE_FAST_GETFIELD(jint,     int,    Int)
DEFINE_FAST_GETFIELD(jlong,    long,   Long)
DEFINE_FAST_GETFIELD(jfloat,   float,  Float)
DEFINE_FAST_GETFIELD(jdouble,  double, Double)

address os::win32::fast_jni_accessor_wrapper(BasicType type) {
  switch (type) {
    case T_BOOLEAN: return (address)jni_fast_GetBooleanField_wrapper;
    case T_BYTE:    return (address)jni_fast_GetByteField_wrapper;
    case T_CHAR:    return (address)jni_fast_GetCharField_wrapper;
    case T_SHORT:   return (address)jni_fast_GetShortField_wrapper;
    case T_INT:     return (address)jni_fast_GetIntField_wrapper;
    case T_LONG:    return (address)jni_fast_GetLongField_wrapper;
    case T_FLOAT:   return (address)jni_fast_GetFloatField_wrapper;
    case T_DOUBLE:  return (address)jni_fast_GetDoubleField_wrapper;
    default:        ShouldNotReachHere();
  }
  return (address)-1;
}
#endif

// Virtual Memory

int os::vm_page_size() { return os::win32::vm_page_size(); }
int os::vm_allocation_granularity() {
  return os::win32::vm_allocation_granularity();
}


// On win32, one cannot release just a part of reserved memory, it's an
// all or nothing deal.  When we split a reservation, we must break the
// reservation into two reservations.
void os::split_reserved_memory(char *base, size_t size, size_t split,
                              bool realloc) {
  if (size > 0) {
    release_memory(base, size);
    if (realloc) {
      reserve_memory(split, base);
    }
    if (size != split) {
      reserve_memory(size - split, base + split);
    }
  }
}


// Reserve memory at an arbitrary address, only if that area is
// available (and not reserved for something else).
char* os::attempt_reserve_memory_at(size_t bytes, char* requested_addr) {
  // Windows os::reserve_memory() fails of the requested address range is
  // not avilable.
  return reserve_memory(bytes, requested_addr);
}


// ISM not avaliable for win32
char* os::reserve_memory_special(size_t bytes) {
  ShouldNotReachHere();
  return NULL;
}
 
void os::print_statistics() {
}

bool os::commit_memory(char* addr, size_t bytes) {
  if (bytes == 0) {
    // Don't bother the OS with noops.
    return true;
  }
  assert((size_t) addr % os::vm_page_size() == 0, "commit on page boundaries");
  assert(bytes % os::vm_page_size() == 0, "commit in page-sized chunks");
  // Don't attempt to print anything if the OS call fails. We're
  // probably low on resources, so the print itself may cause crashes.
  return VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE) != NULL;
}

bool os::commit_memory(char* addr, size_t size, size_t alignment_hint) {
  return commit_memory(addr, size);
}

bool os::uncommit_memory(char* addr, size_t bytes) {
  if (bytes == 0) {
    // Don't bother the OS with noops.
    return true;
  }
  assert((size_t) addr % os::vm_page_size() == 0, "uncommit on page boundaries");
  assert(bytes % os::vm_page_size() == 0, "uncommit in page-sized chunks");
  return VirtualFree(addr, bytes, MEM_DECOMMIT);
}


bool os::release_memory(char* addr, size_t bytes) {
  return VirtualFree(addr, 0, MEM_RELEASE);
}


bool os::protect_memory(char* addr, size_t bytes) {
  DWORD old_status;
  return VirtualProtect(addr, bytes, PAGE_READONLY, &old_status);
}


bool os::guard_memory(char* addr, size_t bytes) {
  DWORD old_status;
  return VirtualProtect(addr, bytes, PAGE_EXECUTE_READWRITE | PAGE_GUARD, &old_status);
}


bool os::unguard_memory(char* addr, size_t bytes) {
  DWORD old_status;
  return VirtualProtect(addr, bytes, PAGE_EXECUTE_READWRITE, &old_status);
}

#define MAX_ERROR_COUNT 100
#define SYS_THREAD_ERROR 0xffffffffUL

void os::pd_start_thread(Thread* thread) {
  DWORD ret = ResumeThread(thread->osthread()->thread_handle());
  // Returns previous suspend state:
  // 0:  Thread was not suspended
  // 1:  Thread is running now
  // >1: Thread is still suspended.
  assert(ret != SYS_THREAD_ERROR, "StartThread failed"); // should propagate back
}

//Helper function trying to suspend thread for a reasonable 
//number of iterations. This becomes neccessary because 
//SuspendThread may fail to suspend a thread that makes 
//Windows kernel call.  
static DWORD try_to_suspend(void* handle) { 
    DWORD ret = 0;
    int err_count = 0; 
    while(SuspendThread(handle) == SYS_THREAD_ERROR) { 
        if(err_count++ >= MAX_ERROR_COUNT) {
            return SYS_THREAD_ERROR;
        }
        Sleep(0); 
    } 
    return ret;
}

// Suspend a thread by one level. The VM code tracks the
// nesting despite Win32 SuspendThread support for nesting.
// This aids in debugging and assertions and more shared code.
// SuspendThread returns previous suspend count if successful
// pd_suspend_thread returns 0 on success.

int os::pd_suspend_thread(Thread* thread, bool fence) {
    HANDLE handle = thread->osthread()->thread_handle();    
    DWORD ret;
    if (fence) {
      ThreadCritical tc;
      ret = try_to_suspend(handle);
    } else {
      ret = try_to_suspend(handle);
    }
    assert(ret != SYS_THREAD_ERROR, "SuspendThread failed"); // should propagate back
    assert(ret == 0, "Win32 nested suspend");

    return ret;
}

// Resume a thread by one level.  This method assumes that consecutive
// suspends nest and require matching resumes to fully resume.  Note that
// this is different from Java's Thread.resume, which always resumes any
// number of nested suspensions.  The ability to nest suspensions is used
// by other facilities like safe points.  
// Multiple-level suspends are handled by common code in the Thread class.
// Resuming a thread that is not suspended is a no-op.
// ResumeThread returns -1 failure, 0 not suspended, else
// previous suspend count, i.e. 1 for now restarted.
// pd_resume_thread returns 0 on success
// vm_resume() handles nesting suspend/resume requests, pd_resume_thread()
// is only called when nesting level reaches 0. 

int os::pd_resume_thread(Thread* thread) {
  OSThread* osthread = thread->osthread();
  DWORD ret = ResumeThread(osthread->thread_handle());
  assert(ret != SYS_THREAD_ERROR, "ResumeThread failed"); // should propagate back
  assert(ret != 0, "Cannot resume unsuspended thread");
  if (ret == SYS_THREAD_ERROR) {
    return -1;
  }
  return 0;
}

// we need to be able to suspend ourself while at the same (atomic) time
// giving up the SR_lock -- we do this by using the
// SR_lock to implement a suspend_self
int os::pd_self_suspend_thread(Thread* thread) {
  thread->SR_lock()->wait(Mutex::_no_safepoint_check_flag);
  return 0;
}

size_t os::read(int fd, void *buf, unsigned int nBytes) { 
  return ::read(fd, buf, nBytes);
}

class HighResolutionInterval {
  // The default timer resolution seems to be 10 milliseconds.
  // (Where is this written down?)
  // If someone wants to sleep for only a fraction of the default,
  // then we set the timer resolution down to 1 millisecond for 
  // the duration of their interval.
  // We carefully set the resolution back, since otherwise we 
  // seem to incur an overhead (3%?) that we don't need.
private:
    jlong resolution;
public:
  HighResolutionInterval(jlong ms) {
    resolution = ms % 10L;
    if (resolution != 0) {
      MMRESULT result = timeBeginPeriod(1L);
    }
  }
  ~HighResolutionInterval() {
    if (resolution != 0) {
      MMRESULT result = timeEndPeriod(1L);
    }
    resolution = 0L;
  }
};

int os::sleep(Thread* thread, jlong ms, bool interruptable) {
  jlong limit = (jlong) MAXDWORD;

  while(ms > limit) {
    int res;
    if ((res = sleep(thread, limit, interruptable)) != OS_TIMEOUT)
      return res;
    ms -= limit;
  }

  assert(thread == Thread::current(),  "thread consistency check");
  OSThread* osthread = thread->osthread();
  OSThreadWaitState osts(osthread, false /* not Object.wait() */);
  int result;
  if (interruptable) {    
    assert(thread->is_Java_thread(), "must be java thread");
    ThreadBlockInVM tbivm((JavaThread*) thread);
    HANDLE events[1];
    events[0] = osthread->interrupt_event();     
    HighResolutionInterval *phri=NULL;
    if(!ForceTimeHighResolution)
      phri = new HighResolutionInterval( ms );
    if (WaitForMultipleObjects(1, events, FALSE, (DWORD)ms) == WAIT_TIMEOUT) {
      result = OS_TIMEOUT;
    } else {
      osthread->set_interrupted(false);
      ResetEvent(osthread->interrupt_event());
      result = OS_INTRPT;
    }
    delete phri; //if it is NULL, harmless

#if 0
    // XXX - This code was not exercised during the Merlin RC1
    // pre-integration test cycle so it has been removed to
    // reduce risk.
    //
    // were we externally suspended while we were waiting?
    if (((JavaThread *) thread)->is_external_suspend_with_lock()) {
      //
      // While we were waiting in WaitForMultipleObjects() another thread
      // suspended us. We don't want to continue running while suspended
      // because that would surprise the thread that suspended us.
      //
      ((JavaThread *) thread)->java_suspend_self();
    }
#endif
  } else {    
    assert(!thread->is_Java_thread(), "must not be java thread");
    Sleep((long) ms);
    result = OS_TIMEOUT;
  }
  return result;
}

// Sleep forever; naked call to OS-specific sleep; use with CAUTION
void os::infinite_sleep() {
  while (true) {    // sleep forever ...
    Sleep(100000);  // ... 100 seconds at a time
  }
}

void os::yield() {  
  // Yields to all threads with same priority  
  Sleep(0);
}

void os::yield_all(int attempts) {
  // Yields to all threads, including threads with lower priorities    
  Sleep(1);
}

// Win32 only gives you access to seven real priorities at a time,
// so we compress Java's ten down to seven.  It would be better
// if we dynamically adjusted relative priorities. 

int os::java_to_os_priority[MaxPriority + 1] = {
  THREAD_PRIORITY_IDLE,                         // 0  Entry should never be used
  THREAD_PRIORITY_LOWEST,                       // 1  MinPriority
  THREAD_PRIORITY_LOWEST,                       // 2
  THREAD_PRIORITY_BELOW_NORMAL,                 // 3
  THREAD_PRIORITY_BELOW_NORMAL,                 // 4
  THREAD_PRIORITY_NORMAL,                       // 5  NormPriority
  THREAD_PRIORITY_ABOVE_NORMAL,                 // 6
  THREAD_PRIORITY_ABOVE_NORMAL,                 // 7
  THREAD_PRIORITY_HIGHEST,                      // 8
  THREAD_PRIORITY_HIGHEST,                      // 9  NearMaxPriority
  THREAD_PRIORITY_TIME_CRITICAL                 // 10 MaxPriority
};

OSReturn os::set_native_priority(Thread* thread, int priority) {
  if (!UseThreadPriorities) return OS_OK;
  bool ret = SetThreadPriority(thread->osthread()->thread_handle(), priority);
  return ret ? OS_OK : OS_ERR;
}

OSReturn os::get_native_priority(const Thread* const thread, int* priority_ptr) {
  if ( !UseThreadPriorities ) {
    *priority_ptr = java_to_os_priority[NormPriority];
    return OS_OK;
  }
  int os_prio = GetThreadPriority(thread->osthread()->thread_handle());
  if (os_prio == THREAD_PRIORITY_ERROR_RETURN) {
    assert(false, "GetThreadPriority failed");
    return OS_ERR;
  }
  *priority_ptr = os_prio;
  return OS_OK;
}


// Hint to the underlying OS that a task switch would not be good.
// Void return because it's a hint and can fail.
void os::hint_no_preempt() {}

void os::interrupt(Thread* thread) {
  assert(!thread->is_Java_thread() || Thread::current() == thread || Threads_lock->owned_by_self(),
         "possibility of dangling Thread pointer");

  OSThread* osthread = thread->osthread();
  osthread->set_interrupted(true);
  // More than one thread can get here with the same value of osthread,
  // resulting in multiple notifications.  We do, however, want the store
  // to interrupted() to be visible to other threads before we post
  // the interrupt event.
  OrderAccess::release();
  SetEvent(osthread->interrupt_event());
  // For JSR166:  unpark after setting status
  if (thread->is_Java_thread()) 
    ((JavaThread*)thread)->parker()->unpark();

}


bool os::is_interrupted(Thread* thread, bool clear_interrupted) {
  assert(!thread->is_Java_thread() || Thread::current() == thread || Threads_lock->owned_by_self(),
         "possibility of dangling Thread pointer");

  OSThread* osthread = thread->osthread();
  bool interrupted;
  interrupted = osthread->interrupted();
  if (clear_interrupted == true) {
    osthread->set_interrupted(false);
    ResetEvent(osthread->interrupt_event());
  } // Otherwise leave the interrupted state alone

  return interrupted;
}


ExtendedPC os::fetch_top_frame(Thread* thread, intptr_t** sp, intptr_t** fp) {
  CONTEXT context;
  context.ContextFlags = CONTEXT_CONTROL;
  ExtendedPC addr;
#ifdef _M_IA64
  if (GetThreadContext(thread->osthread()->thread_handle(), &context)) {
    *sp  = (intptr_t*)context.IntSp;
    *fp  = (intptr_t*)context.RsBSP;
    addr = ExtendedPC((address)context.StIIP);
  } else {
    ShouldNotReachHere();    
  }
  return addr;
#elif _M_AMD64
  if (GetThreadContext(thread->osthread()->thread_handle(), &context)) {
    *sp  = (intptr_t*)context.Rsp;
    *fp  = (intptr_t*)context.Rbp;
    addr = ExtendedPC((address)context.Rip);
  } else {
    ShouldNotReachHere();    
  }
  return addr;
#else
  if (GetThreadContext(thread->osthread()->thread_handle(), &context)) {
    *sp  = (jint*)context.Esp;
    *fp  = (jint*)context.Ebp;
    addr = ExtendedPC((address)context.Eip);
  } else {
    ShouldNotReachHere();    
  }
  return addr;
#endif
}

#ifndef CORE
static bool _set_thread_pc(Thread* thread, ExtendedPC old_pc, ExtendedPC new_pc) {
  CONTEXT context;
  context.ContextFlags = CONTEXT_CONTROL;  
  HANDLE handle = thread->osthread()->thread_handle();
#ifdef _M_IA64
  assert(0, "Fix set_thread_pc");
  return false;
#else
  if (GetThreadContext(handle, &context)) {
#ifdef _M_AMD64
    if (context.Rip != (uintptr_t)old_pc.pc()) return false;
    context.Rip = (uintptr_t)new_pc.pc();
#else
    if (context.Eip != (unsigned long)old_pc.pc()) return false;
    context.Eip = (unsigned long)new_pc.pc();
#endif
    if (!SetThreadContext(handle, &context)) {    
      // SetThreadContext failed
      ShouldNotReachHere();
      return false;
    }
  } else {
    // GetThreadContext failed
    ShouldNotReachHere();
    return false;
  }    
  return true;
#endif
}

bool os::set_thread_pc_and_resume(JavaThread* thread, ExtendedPC old_pc, ExtendedPC new_pc) {
  assert(thread->is_in_compiled_safepoint(), "only for compiled safepoint");

  bool rslt = _set_thread_pc(thread, old_pc, new_pc);
  thread->safepoint_state()->notify_set_thread_pc_result(rslt);
  thread->vm_resume();
  return rslt;
}
#endif

// Get's a pc (hint) for a running thread. Currently used only for profiling.
ExtendedPC os::get_thread_pc(Thread* thread) {
  CONTEXT context;
  context.ContextFlags = CONTEXT_CONTROL;  
  HANDLE handle = thread->osthread()->thread_handle();
#ifdef _M_IA64
  assert(0, "Fix get_thread_pc");
  return ExtendedPC(NULL);
#else
  if (GetThreadContext(handle, &context)) {
#ifdef _M_AMD64
    return ExtendedPC((address) context.Rip);
#else
    return ExtendedPC((address) context.Eip);
#endif
  } else {
    return ExtendedPC(NULL);
  }
#endif
}

// GetCurrentThreadId() returns DWORD
intx os::current_thread_id()          { return GetCurrentThreadId(); }

static int _initial_pid = 0;

int os::current_process_id()
{
  return (_initial_pid ? _initial_pid : _getpid());
}

int    os::win32::_vm_page_size       = 0;
int    os::win32::_vm_allocation_granularity = 0;
int    os::win32::_processor_type     = 0;
// Processor level is not available on non-NT systems, use vm_version instead
int    os::win32::_processor_level    = 0;
int    os::win32::_processor_count    = 0;
julong os::win32::_physical_memory    = 0;
size_t os::win32::_default_stack_size = 0;

         intx os::win32::_os_thread_limit    = 0;
volatile intx os::win32::_os_thread_count    = 0;

bool   os::win32::_is_nt              = false;


void os::win32::initialize_system_info() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  _vm_page_size    = si.dwPageSize;
  _vm_allocation_granularity = si.dwAllocationGranularity;
  _processor_type  = si.dwProcessorType;
  _processor_level = si.wProcessorLevel;
  _processor_count = si.dwNumberOfProcessors;

  MEMORYSTATUS ms;
  // also returns dwAvailPhys (free physical memory bytes), dwTotalVirtual, dwAvailVirtual,
  // dwMemoryLoad (% of memory in use)
  GlobalMemoryStatus(&ms);
  _physical_memory = ms.dwTotalPhys;

  OSVERSIONINFO oi;
  oi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&oi);
  switch(oi.dwPlatformId) {
    case VER_PLATFORM_WIN32_WINDOWS: _is_nt = false; break;
    case VER_PLATFORM_WIN32_NT:      _is_nt = true;  break;
    default: fatal("Unknown platform");
  }  

  _default_stack_size = os::current_stack_size(); 
  assert(_default_stack_size > (size_t) _vm_page_size, "invalid stack size");
  assert((_default_stack_size & (_vm_page_size - 1)) == 0,
    "stack size not a multiple of page size");

  initialize_performance_counter();

  // Win95/Win98 scheduler bug work-around. The Win95/98 scheduler is
  // known to deadlock the system, if the VM issues to thread operations with
  // a too high frequency, e.g., such as changing the priorities. 
  // The 6000 seems to work well - no deadlocks has been notices on the test
  // programs that we have seen experience this problem.
  if (!os::win32::is_nt()) {    
    StarvationMonitorInterval = 6000;
  }
}


void os::win32::setmode_streams() {
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
}


int os::message_box(const char* title, const char* message) {
  int result = MessageBox(NULL, message, title,
                          MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL | MB_DEFAULT_DESKTOP_ONLY);
  return result == IDYES;
}

int os::allocate_thread_local_storage() {
  return TlsAlloc();
}


void os::free_thread_local_storage(int index) {
  TlsFree(index);
}


void os::thread_local_storage_at_put(int index, void* value) {
  TlsSetValue(index, value);
  assert(thread_local_storage_at(index) == value, "Just checking");
}


void* os::thread_local_storage_at(int index) {
  return TlsGetValue(index);
}


#ifndef PRODUCT
#ifndef _WIN64
// Helpers to check whether NX protection is enabled
int nx_exception_filter(_EXCEPTION_POINTERS *pex) {
  if (pex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
      pex->ExceptionRecord->NumberParameters > 0 &&
      pex->ExceptionRecord->ExceptionInformation[0] ==
      EXCEPTION_INFO_EXEC_VIOLATION) {
    return EXCEPTION_EXECUTE_HANDLER;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void nx_check_protection() {
  // If NX is enabled we'll get an exception calling into code on the stack
  char code[] = { (char)0xC3 }; // ret
  void *code_ptr = (void *)code;
  __try {
    __asm call code_ptr
  } __except(nx_exception_filter((_EXCEPTION_POINTERS*)_exception_info())) {
    tty->print_raw_cr("NX protection detected.");
  }
}
#endif // _WIN64
#endif // PRODUCT

// this is called _before_ the global arguments have been parsed
void os::init(void) {
  _initial_pid = _getpid();

  init_random(1234567);

  win32::initialize_system_info();
  win32::setmode_streams();

  // For better scalability on MP systems (must be called after initialize_system_info)
#ifndef PRODUCT
  if (win32::is_MP()) {    
    NoYieldsInMicrolock = true;
  }
#endif
  // Initialize main_process and main_thread
  main_process = GetCurrentProcess();  // Remember main_process is a pseudo handle
  if (!DuplicateHandle(main_process, GetCurrentThread(), main_process,
                       &main_thread, THREAD_ALL_ACCESS, false, 0)) {
    fatal("DuplicateHandle failed\n");
  }
  main_thread_id = (int) GetCurrentThreadId();
}
 
// To install functions for atexit processing
extern "C" {
  static void perfMemory_exit_helper() {
    perfMemory_exit();
  }
}


// this is called _after_ the global arguments have been parsed
jint os::init_2(void) {
  // Allocate a single page and mark it as readable for safepoint polling
  if( SafepointPolling ) {
    address polling_page = (address)VirtualAlloc(NULL, os::vm_page_size(), MEM_RESERVE, PAGE_READONLY);
    guarantee( polling_page != NULL, "Reserve Failed for polling page");

    address return_page  = (address)VirtualAlloc(polling_page, os::vm_page_size(), MEM_COMMIT, PAGE_READONLY);
    guarantee( return_page != NULL, "Commit Failed for polling page");

    os::set_polling_page( polling_page );

#ifndef PRODUCT
    if( Verbose )
      tty->print("[SafePoint Polling address: " INTPTR_FORMAT "]\n", (intptr_t)polling_page);
#endif
  }

  // Setup Windows Exceptions

  // On Itanium systems, Structured Exception Handling does not
  // work since stack frames must be walkable by the OS.  Since
  // much of our code is dynamically generated, and we do not have
  // proper unwind .xdata sections, the system simply exits
  // rather than delivering the exception.  To work around
  // this we use VectorExceptions instead.
#ifdef _WIN64
  topLevelVectoredExceptionHandler = AddVectoredExceptionHandler( 1, topLevelExceptionFilter);
#else
  if (UseTopLevelExceptionFilter && !UseNewOSExceptions) {
    previous_toplevel_exception_filter = SetUnhandledExceptionFilter(topLevelExceptionFilter);
  }  
#endif

  // for debugging float code generation bugs
  if (ForceFloatExceptions) {
#ifndef  _WIN64
    static long fp_control_word = 0;
    __asm { fstcw fp_control_word }
    // see Intel PPro Manual, Vol. 2, p 7-16
    const long precision = 0x20;
    const long underflow = 0x10;
    const long overflow  = 0x08;
    const long zero_div  = 0x04;
    const long denorm    = 0x02;
    const long invalid   = 0x01;
    fp_control_word |= invalid;
    __asm { fldcw fp_control_word }
#endif
  }

  // Initialize HPI.
  jint hpi_result = hpi::initialize();
  if (hpi_result != JNI_OK) { return hpi_result; }

  // If stack_commit_size is 0, windows will reserve the default size, 
  // but only commit a small portion of it.
  size_t stack_commit_size = round_to(ThreadStackSize*K, os::vm_page_size());
  size_t default_reserve_size = os::win32::default_stack_size();
  size_t actual_reserve_size = stack_commit_size;
  if (stack_commit_size < default_reserve_size) {
    // If stack_commit_size == 0, we want this too
    actual_reserve_size = default_reserve_size;
  }

  JavaThread::set_stack_size_at_create(stack_commit_size);

  // Calculate theoretical max. size of Threads to guard gainst artifical
  // out-of-memory situations, where all available address-space has been
  // reserved by thread stacks.
  assert(actual_reserve_size != 0, "Must have a stack");

  // Calculate the thread limit when we should start doing Virtual Memory 
  // banging. Currently when the threads will have used all but 200Mb of space.
  //
  // TODO: consider performing a similar calculation for commit size instead 
  // as reserve size, since on a 64-bit platform we'll run into that more
  // often than running out of virtual memory space.  We can use the 
  // lower value of the two calculations as the os_thread_limit.
  size_t max_address_space = ((size_t)1 << (BitsPerOop - 1)) - (200 * K * K);
  win32::_os_thread_limit = (intx)(max_address_space / actual_reserve_size);

  // at exit methods are called in the reverse order of their registration.
  // there is no limit to the number of functions registered. atexit does
  // not set errno.

  if (PerfAllowAtExitRegistration) {
    // only register atexit functions if PerfAllowAtExitRegistration is set.
    // atexit functions can be delayed until process exit time, which
    // can be problematic for embedded VM situations. Embedded VMs should
    // call DestroyJavaVM() to assure that VM resources are released.

    // note: perfMemory_exit_helper atexit function may be removed in
    // the future if the appropriate cleanup code can be added to the
    // VM_Exit VMOperation's doit method.
    if (atexit(perfMemory_exit_helper) != 0) {
      warning("os::init_2 atexit(perfMemory_exit_helper) failed");
    }
  }

  // initialize PSAPI or ToolHelp for fatal error handler
  if (win32::is_nt()) _init_psapi();
  else _init_toolhelp();

#ifndef _WIN64
  // Print something if NX is enabled (win32 on AMD64)
  NOT_PRODUCT(if (PrintMiscellaneous && Verbose) nx_check_protection());
#endif

  return JNI_OK;
}


// Mark the polling page as unreadable
void os::make_polling_page_unreadable(void) {
  if( !SafepointPolling )
    return;

  DWORD old_status;
  if( !VirtualProtect((char *)_polling_page, os::vm_page_size(), PAGE_NOACCESS, &old_status) )
    fatal("Could not disable polling page");
};

// Mark the polling page as readable
void os::make_polling_page_readable(void) {
  if( !SafepointPolling )
    return;

  DWORD old_status;
  if( !VirtualProtect((char *)_polling_page, os::vm_page_size(), PAGE_READONLY, &old_status) )
    fatal("Could not enable polling page");
};


int os::stat(const char *path, struct stat *sbuf) {
  char pathbuf[MAX_PATH];
  if (strlen(path) > MAX_PATH - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  hpi::native_path(strcpy(pathbuf, path));
  return ::stat(pathbuf, sbuf);
}


// JVMPI code

#define FT2INT64(ft) \
  ((jlong)((jlong)(ft).dwHighDateTime << 32 | (julong)(ft).dwLowDateTime))


// current_thread_cpu_time(bool) and thread_cpu_time(Thread*, bool)
// are used by JVM M&M and JVMTI to get user+sys or user CPU time
// of a thread.
//
// current_thread_cpu_time() and thread_cpu_time(Thread*) returns
// the fast estimate available on the platform.

// current_thread_cpu_time() is not optimized for Windows yet
jlong os::current_thread_cpu_time() {
  // return user + sys since the cost is the same
  return os::thread_cpu_time(Thread::current(), true /* user+sys */);
}

jlong os::thread_cpu_time(Thread* thread) {
  // consistent with what current_thread_cpu_time() returns.
  return os::thread_cpu_time(thread, true /* user+sys */);
}

jlong os::current_thread_cpu_time(bool user_sys_cpu_time) {
  return os::thread_cpu_time(Thread::current(), user_sys_cpu_time);
}

jlong os::thread_cpu_time(Thread* thread, bool user_sys_cpu_time) {
  // This code is copy from clasic VM -> hpi::sysThreadCPUTime
  // If this function changes, os::is_thread_cpu_time_supported() should too
  if (os::win32::is_nt()) {
    FILETIME CreationTime;
    FILETIME ExitTime;
    FILETIME KernelTime;
    FILETIME UserTime;

    if ( GetThreadTimes(thread->osthread()->thread_handle(),
                    &CreationTime, &ExitTime, &KernelTime, &UserTime) == 0)
      return -1;
    else
      if (user_sys_cpu_time) {
        return (FT2INT64(UserTime) + FT2INT64(KernelTime)) * 100;
      } else {
        return FT2INT64(UserTime) * 100;
      }
  } else {
    return (jlong) timeGetTime() * 1000000;
  }
}

void os::current_thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = ALL_64_BITS;        // the max value -- all 64 bits
  info_ptr->may_skip_backward = false;      // GetThreadTimes returns absolute time
  info_ptr->may_skip_forward = false;       // GetThreadTimes returns absolute time
  info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;   // user+system time is returned
}

void os::thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = ALL_64_BITS;        // the max value -- all 64 bits
  info_ptr->may_skip_backward = false;      // GetThreadTimes returns absolute time
  info_ptr->may_skip_forward = false;       // GetThreadTimes returns absolute time
  info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;   // user+system time is returned
}

bool os::is_thread_cpu_time_supported() {
  // see os::thread_cpu_time
  if (os::win32::is_nt()) {
    FILETIME CreationTime;
    FILETIME ExitTime;
    FILETIME KernelTime;
    FILETIME UserTime;

    if ( GetThreadTimes(GetCurrentThread(),
                    &CreationTime, &ExitTime, &KernelTime, &UserTime) == 0)
      return false;
    else
      return true;
  } else {
    return false;
  }
}

bool os::thread_is_running(JavaThread* tp) {
#ifdef _WIN64
  assert(0, "Fix thread_is_running");
  return  false;
#else
  // this code is a copy from classic VM -> hpi::sysThreadIsRunning
  unsigned int sum = 0;
  unsigned int *p;
  CONTEXT context;
  
  context.ContextFlags = CONTEXT_FULL;
  GetThreadContext(tp->osthread()->thread_handle(), &context);
  p = (unsigned int*)&context.SegGs;
  while (p <= (unsigned int*)&context.SegSs) {
    sum += *p;
    p++;
  }
  if (sum == tp->last_sum()) {
    return false;
  }
  tp->set_last_sum(sum);
  return true;
#endif
}


// DontYieldALot=false by default: dutifully perform all yields as requested by JVM_Yield()
bool os::dont_yield() {
  return DontYieldALot;
}

// Is a (classpath) directory empty?
bool os::dir_is_empty(const char* path) {
  WIN32_FIND_DATA fd;
  HANDLE f = FindFirstFile(path, &fd);
  if (f == INVALID_HANDLE_VALUE) {
    return true;
  }
  FindClose(f);
  return false;
}


// Map a block of memory.
char* os::map_memory(int fd, const char* file_name, size_t file_offset,
                     char *addr, size_t bytes, bool read_only,
                     bool allow_exec) {
  HANDLE hFile;
  char* base;

  hFile = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == NULL) {
    if (PrintMiscellaneous && Verbose) {
      DWORD err = GetLastError();
      tty->print_cr("CreateFile() failed: GetLastError->%ld.");
    }
    return NULL;
  }

  if (allow_exec) {
    // CreateFileMapping/MapViewOfFileEx can't map executable memory
    // unless it comes from a PE image (which the shared archive is not.)
    // Even VirtualProtect refuses to give execute access to mapped memory
    // that was not previously executable.
    // 
    // Instead, stick the executable region in anonymous memory.  Yuck.
    // Penalty is that ~4 pages will not be shareable - in the future
    // we might consider DLLizing the shared archive with a proper PE
    // header so that mapping executable + sharing is possible.

    base = (char*) VirtualAlloc(addr, bytes, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
    if (base == NULL) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("VirtualAlloc() failed: GetLastError->%ld.", err);
      }
      CloseHandle(hFile);
      return NULL;
    }
    
    DWORD bytes_read;
    OVERLAPPED overlapped;
    overlapped.Offset = (DWORD)file_offset;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = NULL;
    // ReadFile guarantees that if the return value is true, the requested
    // number of bytes were read before returning.
    bool res = ReadFile(hFile, base, (DWORD)bytes, &bytes_read, &overlapped);
    if (!res) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("ReadFile() failed: GetLastError->%ld.", err);
      }
      release_memory(base, bytes);
      CloseHandle(hFile);
      return NULL;
    }
  } else {
    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_WRITECOPY, 0, 0,
                                    NULL /*file_name*/);
    if (hMap == NULL) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("CreateFileMapping() failed: GetLastError->%ld.");
      }
      CloseHandle(hFile);
      return NULL;
    }
    
    DWORD access = read_only ? FILE_MAP_READ : FILE_MAP_COPY;
    base = (char*)MapViewOfFileEx(hMap, access, 0, (DWORD)file_offset,
                                  (DWORD)bytes, addr);
    if (base == NULL) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("MapViewOfFileEx() failed: GetLastError->%ld.", err);
      }
      CloseHandle(hMap);
      CloseHandle(hFile);
      return NULL;
    }
    
    if (CloseHandle(hMap) == 0) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("CloseHandle(hMap) failed: GetLastError->%ld.", err);
      }
      CloseHandle(hFile);
      return base;
    }
  }

  if (allow_exec) {
    DWORD old_protect;
    DWORD exec_access = read_only ? PAGE_EXECUTE_READ : PAGE_EXECUTE_READWRITE;
    bool res = VirtualProtect(base, bytes, exec_access, &old_protect);

    if (!res) {
      if (PrintMiscellaneous && Verbose) {
        DWORD err = GetLastError();
        tty->print_cr("VirtualProtect() failed: GetLastError->%ld.", err);
      }
      // Don't consider this a hard error, on IA32 even if the
      // VirtualProtect fails, we should still be able to execute
      CloseHandle(hFile);
      return base;
    }
  }

  if (CloseHandle(hFile) == 0) {
    if (PrintMiscellaneous && Verbose) {
      DWORD err = GetLastError();
      tty->print_cr("CloseHandle(hFile) failed: GetLastError->%ld.", err);
    }
    return base;
  }

  return base;
}


// Unmap a block of memory.
// Returns 0=success, otherwise non-zero.

bool os::unmap_memory(char* addr, size_t bytes) {
  BOOL result = UnmapViewOfFile(addr);
  if (result == 0) {
    if (PrintMiscellaneous && Verbose) {
      DWORD err = GetLastError();
      tty->print_cr("UnmapViewOfFile() failed: GetLastError->%ld.", err);
    }
    return false;
  }
  return true;
}


//--------------------------------------------------------------------------------------------------
// Non-product code

#ifdef PRODUCT
bool os::check_heap(bool force) { return true; }
#else
static int mallocDebugIntervalCounter = 0;
static int mallocDebugCounter = 0;
bool os::check_heap(bool force) {
  if (++mallocDebugCounter < MallocVerifyStart && !force) return true;
  if (++mallocDebugIntervalCounter >= MallocVerifyInterval || force) {
    // Note: HeapValidate executes two hardware breakpoints when it finds something
    // wrong; at these points, eax contains the address of the offending block (I think).
    // To get to the exlicit error message(s) below, just continue twice.
    HANDLE heap = GetProcessHeap();
    { HeapLock(heap);
      PROCESS_HEAP_ENTRY phe;
      phe.lpData = NULL;
      while (HeapWalk(heap, &phe) != 0) {
        if ((phe.wFlags & PROCESS_HEAP_ENTRY_BUSY) &&
            !HeapValidate(heap, 0, phe.lpData)) {
          tty->print_cr("C heap has been corrupted (time: %d allocations)", mallocDebugCounter);
          tty->print_cr("corrupted block near address %#x, length %d", phe.lpData, phe.cbData);
          fatal("corrupted C heap");
        }
      }
      int err = GetLastError();
      if (err != ERROR_NO_MORE_ITEMS && err != ERROR_CALL_NOT_IMPLEMENTED) {
        fatal1("heap walk aborted with error %d", err);
      }
      HeapUnlock(heap);
    }
    mallocDebugIntervalCounter = 0;
  }
  return true;
}
#endif


#ifndef PRODUCT
bool os::find(address addr) {
  // Nothing yet
  return false;
}
#endif 
