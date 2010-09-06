#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)os_solaris.cpp	1.323 04/06/18 11:32:41 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// do not include  precompiled  header file
# include "incls/_os_solaris.cpp.incl"

// put OS-includes here
# include <dlfcn.h>
# include <errno.h>
# include <link.h>
# include <poll.h>
# include <pthread.h>
# include <pwd.h>
# include <schedctl.h>
# include <signal.h>
# include <stdio.h>
# include <alloca.h>
# include <sys/filio.h>
# include <sys/ipc.h>
# include <sys/lwp.h>
# include <sys/lwp.h>
# include <sys/machelf.h>     // for elf Sym structure used by dladdr1
# include <sys/mman.h>
# include <sys/processor.h>
# include <sys/procset.h>
# include <sys/resource.h>
# include <sys/shm.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/systeminfo.h>
# include <sys/time.h>
# include <sys/types.h>
# include <sys/utsname.h>
# include <thread.h>
# include <unistd.h>
# include <sys/priocntl.h>
# include <sys/rtpriocntl.h>
# include <sys/tspriocntl.h>
# include <sys/iapriocntl.h>
# include <sys/loadavg.h>

# define _STRUCTURED_PROC 1  //  this gets us the new structured proc interfaces of 5.6 & later
# include <sys/procfs.h>     //  see comment in <sys/procfs.h>

#define MAX_PATH (2 * K)

// for timer info max values which include all bits
#define ALL_64_BITS CONST64(0xFFFFFFFFFFFFFFFF)

/*
  MPSS Changes Start.
  The JVM binary needs to be built and run on pre-Solaris 9
  systems, but the constants needed by MPSS are only in Solaris 9
  header files.  They are textually replicated here to allow
  building on earlier systems.  Once building on Solaris 8 is 
  no longer a requirement, these #defines can be replaced by ordinary
  system .h inclusion.

  In earlier versions of the  JDK and Solaris, we used ISM for large pages.
  But ISM requires shared memory to achieve this and thus has many caveats.
  MPSS is a fully transparent and is a cleaner way to get large pages.
  Although we still require keeping ISM for backward compatiblitiy as well as
  giving the opportunity to use large pages on older systems it is
  recommended that MPSS be used for Solaris 9 and above.

*/

struct S9_memcntl_mha {
  uint_t          mha_cmd;        /* command(s) */
  uint_t          mha_flags;
  size_t          mha_pagesize;
};
#define S9_MC_HAT_ADVISE   7       /* advise hat map size */
#define S9_MHA_MAPSIZE_VA  0x1     /* set preferred page size */
#define S9_MAP_ALIGN       0x200   /* addr specifies alignment */
// MPSS Changes End.

// see thr_setprio(3T) for the basis of these numbers
#define MinimumPriority 0
#define NormalPriority  64
#define MaximumPriority 127

// Values for ThreadPriorityPolicy == 1
int prio_policy1[MaxPriority+1] = { -99999, 0, 16, 32, 48, 64, 
                                        80, 96, 112, 124, 127 };

address os::Solaris::handler_start;  // start pc of thr_sighndlrinfo
address os::Solaris::handler_end;    // end pc of thr_sighndlrinfo

address os::Solaris::_main_stack_base = NULL;  // 4352906 workaround


// "default" initializers for missing libc APIs
extern "C" {
  static int lwp_mutex_init(mutex_t *mx, int scope, void *arg) { memset(mx, 0, sizeof(mutex_t)); return 0; }
  static int lwp_mutex_destroy(mutex_t *mx)                 { return 0; }

  static int lwp_cond_init(cond_t *cv, int scope, void *arg){ memset(cv, 0, sizeof(cond_t)); return 0; }
  static int lwp_cond_destroy(cond_t *cv)                   { return 0; }
}

// "default" initializers for pthread-based synchronization
extern "C" {
  static int pthread_mutex_default_init(mutex_t *mx, int scope, void *arg) { memset(mx, 0, sizeof(mutex_t)); return 0; }

  static int pthread_cond_default_init(cond_t *cv, int scope, void *arg){ memset(cv, 0, sizeof(cond_t)); return 0; }
}

// Thread Local Storage
// This is common to all Solaris platforms so it is defined here,
// in this common file.
// The declarations are in the os_cpu threadLS*.hpp files.
//
// Static member initialization for TLS
Thread* ThreadLocalStorage::_get_thread_cache[ThreadLocalStorage::_pd_cache_size] = {NULL};

#ifndef PRODUCT
#define _PCT(n,d)       ((100.0*(double)(n))/(double)(d))

int ThreadLocalStorage::_tcacheHit = 0;
int ThreadLocalStorage::_tcacheMiss = 0;

void ThreadLocalStorage::print_statistics() {
  int total = _tcacheMiss+_tcacheHit;
  tty->print_cr("Thread cache hits %d misses %d total %d percent %f\n",
                _tcacheHit, _tcacheMiss, total, _PCT(_tcacheHit, total));
}
#undef _PCT
#endif // PRODUCT

Thread* ThreadLocalStorage::get_thread_via_cache_slowly(uintptr_t raw_id,
                                                        int index) {
  Thread *thread = get_thread_slow();
  if (thread != NULL) {
    address sp = os::current_stack_pointer();
    guarantee(thread->_stack_base == NULL ||
              (sp <= thread->_stack_base &&
                 sp >= thread->_stack_base - thread->_stack_size) ||
               is_error_reported(),
              "sp must be inside of selected thread stack");

    thread->_self_raw_id = raw_id;  // mark for quick retrieval
    _get_thread_cache[ index ] = thread;
  }
  return thread;
}


static const double all_zero[ sizeof(Thread) / sizeof(double) + 1 ] = {0};
#define NO_CACHED_THREAD ((Thread*)all_zero)

void ThreadLocalStorage::pd_set_thread(Thread* thread) {

#ifdef ASSERT
  Thread* old_thread = get_thread_slow();

  if (thread == NULL) {
    // delete not_current_thread cannot clear the TLS so catch this case
    assert(old_thread == Thread::current(), "caller must be current thread");
  }
  // implied else:
  // We cannot assert(old_thread == NULL, ...) because we might be
  // initializing a recycled thread from the thread library. If the
  // recycled thread was once a Thread, then the assert() is okay.
  // However, if it was previously an internal thread library thread,
  // then there is no guarantee what the old_thread value might be.
  //
  // We cannot assert() that the caller is the current thread on
  // this initial set because we have not yet defined the current
  // thread value in thread local storage.
#endif

  // Store the new value before updating the cache to prevent a race
  // between get_thread_via_cache_slowly() and this store operation.
  os::thread_local_storage_at_put(ThreadLocalStorage::thread_index(), thread);

  // Update thread cache with new thread if setting on thread create,
  // or NO_CACHED_THREAD (zeroed) thread if resetting thread on exit.
  uintptr_t raw = pd_raw_thread_id();
  int ix = pd_cache_index(raw);
  _get_thread_cache[ix] = thread == NULL ? NO_CACHED_THREAD : thread;
}

void ThreadLocalStorage::pd_init() {
  for (int i = 0; i < _pd_cache_size; i++) {
    _get_thread_cache[i] = NO_CACHED_THREAD;
  }
}

// Invalidate all the caches (happens to be the same as pd_init).
void ThreadLocalStorage::pd_invalidate_all() { pd_init(); }

#undef NO_CACHED_THREAD

// END Thread Local Storage

static inline size_t adjust_stack_size(address base, size_t size) {
  if ((ssize_t)size < 0) {
    // 4759953: Compensate for ridiculous stack size.
    size = max_intx;
  }
  if (size > (size_t)base) {
    // 4812466: Make sure size doesn't allow the stack to wrap the address space.
    size = (size_t)base;
  }
  return size;
}

static inline stack_t get_stack_info() {
  stack_t st;
  int retval = thr_stksegment(&st);
  st.ss_size = adjust_stack_size((address)st.ss_sp, st.ss_size);
  assert(retval == 0, "incorrect return value from thr_stksegment");
  assert((address)&st < (address)st.ss_sp, "Invalid stack base returned");
  assert((address)&st > (address)st.ss_sp-st.ss_size, "Invalid stack size returned");
  return st;
}

address os::current_stack_base() {
  bool is_primordial_thread = thr_main();

  // Workaround 4352906, avoid calls to thr_stksegment by
  // thr_main after the first one (it looks like we trash
  // some data, causing the value for ss_sp to be incorrect).
  if (!is_primordial_thread || os::Solaris::_main_stack_base == NULL) {
    stack_t st = get_stack_info();
    if (is_primordial_thread) {
      // cache initial value of stack base
      os::Solaris::_main_stack_base = (address)st.ss_sp;
    }
    return (address)st.ss_sp;
  } else {
    guarantee(os::Solaris::_main_stack_base != NULL, "Attempt to use null cached stack base");
    return os::Solaris::_main_stack_base;
  }
}

size_t os::current_stack_size() {
  size_t size;

  if(!thr_main()) {
    size = get_stack_info().ss_size;
  } else {
    struct rlimit limits;
    getrlimit(RLIMIT_STACK, &limits);
    size = adjust_stack_size(os::Solaris::_main_stack_base, (size_t)limits.rlim_cur);
  }

  return size;
}

// interruptible infrastructure

// setup_interruptible saves the thread state before going into an
// interruptible system call.  
// The saved state is used to restore the thread to
// its former state whether or not an interrupt is received.  
// Used by os_sleep, and classloader os::read 
// hpi calls skip this layer and stay in _thread_in_native

void os::Solaris::setup_interruptible(JavaThread* thread) {
 
  JavaThreadState thread_state = thread->thread_state();

  assert(thread_state != _thread_blocked, "Coming from the wrong thread");
  assert(thread_state != _thread_in_native, "Native threads skip setup_interruptible");
  OSThread* osthread = thread->osthread();
  osthread->set_saved_interrupt_thread_state(thread_state);
  thread->frame_anchor()->make_walkable(thread);
  ThreadStateTransition::transition(thread, thread_state, _thread_blocked);
}

JavaThread* os::Solaris::setup_interruptible() {
  JavaThread* thread = (JavaThread*)ThreadLocalStorage::thread();
  setup_interruptible(thread);
  return thread;
}

#ifdef ASSERT

JavaThread* os::Solaris::setup_interruptible_native() {
  JavaThread* thread = (JavaThread*)ThreadLocalStorage::thread();
  JavaThreadState thread_state = thread->thread_state();
  assert(thread_state == _thread_in_native, "Assumed thread_in_native");
  return thread;
}

void os::Solaris::cleanup_interruptible_native(JavaThread* thread) {
  JavaThreadState thread_state = thread->thread_state();
  assert(thread_state == _thread_in_native, "Assumed thread_in_native");
}
#endif

// cleanup_interruptible reverses the effects of setup_interruptible

void os::Solaris::cleanup_interruptible(JavaThread* thread) {
  OSThread* osthread = thread->osthread();

  ThreadStateTransition::transition(thread, _thread_blocked, osthread->saved_interrupt_thread_state());
}


static int _processors_online = 0; 
int os::Solaris::_processor_count = 0;

         jint os::Solaris::_os_thread_limit = 0;
volatile jint os::Solaris::_os_thread_count = 0;

julong os::Solaris::_physical_memory = 0;

julong os::physical_memory() {
   return Solaris::physical_memory();
}

julong os::allocatable_physical_memory(julong size) {
#ifdef _LP64
   return size;
#else
   julong result = MIN2(size, (julong)3835*M);
   if (!is_allocatable(result)) {
     // Memory allocations will be aligned but the alignment
     // is not known at this point.  Alignments will 
     // be at most to LargePageSizeInBytes.  Protect 
     // allocations from alignments up to illegal
     // values. If at this point 2G is illegal.
     julong reasonable_size = (julong)2*G - 2 * LargePageSizeInBytes;
     result =  MIN2(size, reasonable_size);
   }
   return result;
#endif
}

static hrtime_t first_hrtime = 0;
static const hrtime_t hrtime_hz = 1000*1000*1000;
const int LOCK_BUSY = 1;
const int LOCK_FREE = 0;
const int LOCK_INVALID = -1;
static volatile hrtime_t max_hrtime = 0;
static volatile int max_hrtime_lock = LOCK_FREE;     // Update counter with LSB as lock-in-progress


void os::Solaris::initialize_system_info() {
  _processor_count = sysconf(_SC_NPROCESSORS_CONF);
  _processors_online = sysconf (_SC_NPROCESSORS_ONLN); 
  _physical_memory = (julong)sysconf(_SC_PHYS_PAGES) * (julong)sysconf(_SC_PAGESIZE);
}

int os::processor_count() {
  return Solaris::processor_count();
}

int os::active_processor_count() {
  int online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  pid_t pid = getpid();
  psetid_t pset = PS_NONE;
  // Are we running in a processor set?
  if (pset_bind(PS_QUERY, P_PID, pid, &pset) == 0) {
    if (pset != PS_NONE) {
      uint_t pset_cpus;
      // Query number of cpus in processor set
      if (pset_info(pset, NULL, &pset_cpus, NULL) == 0) {
	assert(pset_cpus > 0 && pset_cpus <= online_cpus, "sanity check");
	_processors_online = pset_cpus;
	return pset_cpus;
      }
    }
  }
  // Otherwise return number of online cpus
  return online_cpus;
}

static bool find_processors_in_pset(psetid_t        pset,
                                    processorid_t** id_array,
                                    uint_t*         id_length) {
  bool result = false;
  // Find the number of processors in the processor set.
  if (pset_info(pset, NULL, id_length, NULL) == 0) {
    // Make up an array to hold their ids.
    *id_array = NEW_C_HEAP_ARRAY(processorid_t, *id_length);
    // Fill in the array with their processor ids.
    if (pset_info(pset, NULL, id_length, *id_array) == 0) {
      result = true;
    }
  }
  return result;
}

// Callers of find_processors_online() must tolerate imprecise results -- 
// the system configuration can change asynchronously because of DR
// or explicit psradm operations.  
//
// We also need to take care that the loop (below) terminates as the
// number of processors online can change between the _SC_NPROCESSORS_ONLN
// request and the loop that builds the list of processor ids.   Unfortunately
// there's no reliable way to determine the maximum valid processor id, 
// so we use a manifest constant, MAX_PROCESSOR_ID, instead.  See p_online 
// man pages, which claim the processor id set is "sparse, but
// not too sparse".  MAX_PROCESSOR_ID is used to ensure that we eventually
// exit the loop.  
//
// In the future we'll be able to use sysconf(_SC_CPUID_MAX), but that's
// not available on S8.0.

static bool find_processors_online(processorid_t** id_array,
                                   uint*           id_length) {
  const processorid_t MAX_PROCESSOR_ID = 100000 ;         
  // Find the number of processors online.
  *id_length = sysconf(_SC_NPROCESSORS_ONLN);
  // Make up an array to hold their ids.
  *id_array = NEW_C_HEAP_ARRAY(processorid_t, *id_length);
  // Processors need not be numbered consecutively.
  long found = 0;
  processorid_t next = 0;
  while (found < *id_length && next < MAX_PROCESSOR_ID) {
    processor_info_t info;
    if (processor_info(next, &info) == 0) {
      // NB, PI_NOINTR processors are effectively online ...
      if (info.pi_state == P_ONLINE || info.pi_state == P_NOINTR) {
        (*id_array)[found] = next;
        found += 1;
      }
    }
    next += 1;
  }
  if (found < *id_length) { 
      // The loop above didn't identify the expected number of processors.
      // We could always retry the operation, calling sysconf(_SC_NPROCESSORS_ONLN)
      // and re-running the loop, above, but there's no guarantee of progress 
      // if the system configuration is in flux.  Instead, we just return what
      // we've got.  Note that in the worst case find_processors_online() could
      // return an empty set.  (As a fall-back in the case of the empty set we
      // could just return the ID of the current processor).  
      *id_length = found ; 
  }

  return true;
}

static bool assign_distribution(processorid_t* id_array,
                                uint           id_length,
                                uint*          distribution,
                                uint           distribution_length) {
  // We assume we can assign processorid_t's to uint's.
  assert(sizeof(processorid_t) == sizeof(uint),
         "can't convert processorid_t to uint");
  // Quick check to see if we won't succeed.
  if (id_length < distribution_length) {
    return false;
  }
  // Assign processor ids to the distribution.
  // Try to shuffle processors to distribute work across boards,
  // assuming 4 processors per board.
  const uint processors_per_board = ProcessDistributionStride;
  // Find the maximum processor id.
  processorid_t max_id = 0;
  for (uint m = 0; m < id_length; m += 1) {
    max_id = MAX2(max_id, id_array[m]);
  }
  // The next id, to limit loops.
  const processorid_t limit_id = max_id + 1;
  // Make up markers for available processors.
  bool* available_id = NEW_C_HEAP_ARRAY(bool, limit_id);
  for (uint c = 0; c < limit_id; c += 1) {
    available_id[c] = false;
  }
  for (uint a = 0; a < id_length; a += 1) {
    available_id[id_array[a]] = true;
  }
  // Step by "boards", then by "slot", copying to "assigned".
  // NEEDS_CLEANUP: The assignment of processors should be stateful,
  //                remembering which processors have been assigned by
  //                previous calls, etc., so as to distribute several
  //                independent calls of this method.  What we'd like is
  //                It would be nice to have an API that let us ask
  //                how many processes are bound to a processor,
  //                but we don't have that, either.
  //                In the short term, "board" is static so that 
  //                subsequent distributions don't all start at board 0.
  static uint board = 0;
  uint assigned = 0;
  // Until we've found enough processors ....
  while (assigned < distribution_length) {
    // ... find the next available processor in the board.
    for (uint slot = 0; slot < processors_per_board; slot += 1) {
      uint try_id = board * processors_per_board + slot;
      if ((try_id < limit_id) && (available_id[try_id] == true)) {
        distribution[assigned] = try_id;
        available_id[try_id] = false;
        assigned += 1;
        break;
      }
    }
    board += 1;
    if (board * processors_per_board + 0 >= limit_id) {
      board = 0;
    }
  }
  if (available_id != NULL) {
    FREE_C_HEAP_ARRAY(bool, available_id);
  }
  return true;
}

bool os::distribute_processes(uint length, uint* distribution) {
  bool result = false;
  // Find the processor id's of all the available CPUs.
  processorid_t* id_array  = NULL;
  uint           id_length = 0;
  // There are some races between querying information and using it, 
  // since processor sets can change dynamically.
  psetid_t pset = PS_NONE;
  // Are we running in a processor set?
  if ((pset_bind(PS_QUERY, P_PID, P_MYID, &pset) == 0) && pset != PS_NONE) {
    result = find_processors_in_pset(pset, &id_array, &id_length);
  } else {
    result = find_processors_online(&id_array, &id_length);
  }
  if (result == true) {
    if (id_length >= length) {
      result = assign_distribution(id_array, id_length, distribution, length);
    } else {
      result = false;
    }
  }
  if (id_array != NULL) {
    FREE_C_HEAP_ARRAY(processorid_t, id_array);
  }
  return result;
}

bool os::bind_to_processor(uint processor_id) {
  // We assume that a processorid_t can be stored in a uint.
  assert(sizeof(uint) == sizeof(processorid_t),
         "can't convert uint to processorid_t");
  int bind_result =
    processor_bind(P_LWPID,                       // bind LWP.
                   P_MYID,                        // bind current LWP.
                   (processorid_t) processor_id,  // id.
                   NULL);                         // don't return old binding.
  return (bind_result == 0);
}

bool os::getenv(const char* name, char* buffer, int len) {
  char* val = ::getenv( name );
  if ( val == NULL 
  ||   strlen(val) + 1  >  len ) {
    if (len > 0)  buffer[0] = 0; // return a null string
    return false;
  }
  strcpy( buffer, val );
  return true;
}


// Return true if user is running as root.

bool os::have_special_privileges() {
  static bool init = false;
  static bool privileges = false;
  if (!init) {
    privileges = (getuid() != geteuid()) || (getgid() != getegid());
    init = true;
  }
  return privileges;
}


static char* get_property(char* name, char* buffer, int buffer_size) {
  if (os::getenv(name, buffer, buffer_size)) {
    return buffer;
  }
  static char empty[] = "";
  return empty;
}


void os::init_system_properties_values() {
  char arch[12];
  sysinfo(SI_ARCHITECTURE, arch, sizeof(arch));

  // The next steps are taken in the product version:
  //
  // Obtain the JAVA_HOME value from the location of libjvm[_g].so.
  // This library should be located at:
  // <JAVA_HOME>/jre/lib/<arch>/{client|server}/libjvm[_g].so.
  //
  // If "/jre/lib/" appears at the right place in the path, then we 
  // assume libjvm[_g].so is installed in a JDK and we use this path. 
  //
  // Otherwise exit with message: "Could not create the Java virtual machine."
  //
  // The following extra steps are taken in the debugging version:
  //
  // If "/jre/lib/" does NOT appear at the right place in the path
  // instead of exit check for $JAVA_HOME environment variable.
  //
  // If it is defined and we are able to locate $JAVA_HOME/jre/lib/<arch>,
  // then we append a fake suffix "hotspot/libjvm[_g].so" to this path so
  // it looks like libjvm[_g].so is installed there
  // <JAVA_HOME>/jre/lib/<arch>/hotspot/libjvm[_g].so.
  //
  // Otherwise exit. 
  //
  // Important note: if the location of libjvm.so changes this 
  // code needs to be changed accordingly.

  // The next few definitions allow the code to be verbatim:
#define malloc(n) (char*)NEW_C_HEAP_ARRAY(char, (n))
#define getenv(n) ::getenv(n)

#define DEFAULT_LD_LIBRARY_PATH "/usr/lib" /* See ld.so.1(1) */
#define EXTENSIONS_DIR "/lib/ext"
#define ENDORSED_DIR "/lib/endorsed"

  {
    /* sysclasspath, java_home, dll_dir */
    {
        char *home_path;
	char *dll_path;
	char *pslash;
        char buf[MAXPATHLEN];
	os::jvm_path(buf, sizeof(buf));

	// Found the full path to libjvm.so. 
	// Now cut the path to <java_home>/jre if we can. 
	*(strrchr(buf, '/')) = '\0';  /* get rid of /libjvm.so */
	pslash = strrchr(buf, '/');
	if (pslash != NULL)
	    *pslash = '\0';           /* get rid of /{client|server|hotspot} */
	dll_path = malloc(strlen(buf) + 1);
	if (dll_path == NULL)
	    return;
	strcpy(dll_path, buf);
        Arguments::set_dll_dir(dll_path);

	if (pslash != NULL) {
	    pslash = strrchr(buf, '/');
	    if (pslash != NULL) {
		*pslash = '\0';       /* get rid of /<arch> */ 
		pslash = strrchr(buf, '/');
		if (pslash != NULL)
		    *pslash = '\0';   /* get rid of /lib */
	    }
	}

	home_path = malloc(strlen(buf) + 1);
	if (home_path == NULL)
	    return;
	strcpy(home_path, buf);
        Arguments::set_java_home(home_path);
        
	if (!set_boot_path('/', ':'))
	    return;
    }

    /* Where to look for native libraries */
    {
	/* Get the user setting of LD_LIBRARY_PATH */
	char *v = getenv("LD_LIBRARY_PATH");

	if (v == NULL) {
	    /* This is never the case. */
	    v = (char *) malloc(sizeof(DEFAULT_LD_LIBRARY_PATH));
	    strcpy(v, DEFAULT_LD_LIBRARY_PATH);
	} else {
	    char *ld_library_path = (char *)
		malloc(sizeof(DEFAULT_LD_LIBRARY_PATH) 	/* enough for '\0' */ +
		       1 				/* colon */ +
		       strlen(v));
	    strcpy(ld_library_path, v);
	    strcat(ld_library_path, ":" DEFAULT_LD_LIBRARY_PATH);
	    v = ld_library_path;
	}
        Arguments::set_library_path(v);
    }

    /* Extensions directories */
    {
	char * buf;
	buf = malloc(strlen(Arguments::get_java_home()) + sizeof(EXTENSIONS_DIR));
	sprintf(buf, "%s" EXTENSIONS_DIR, Arguments::get_java_home());
        Arguments::set_ext_dirs(buf);
    }

    /* Endorsed standards default directory. */
    {
	char * buf = malloc(strlen(Arguments::get_java_home()) + sizeof(ENDORSED_DIR));
	sprintf(buf, "%s" ENDORSED_DIR, Arguments::get_java_home());
        Arguments::set_endorsed_dirs(buf);
    }
  }

#undef malloc
#undef getenv
#undef DEFAULT_LD_LIBRARY_PATH
#undef EXTENSIONS_DIR
#undef ENDORSED_DIR

}

void os::breakpoint() {
  BREAKPOINT;
}

bool os::obsolete_option(const JavaVMOption *option)
{
  if (!strncmp(option->optionString, "-Xt", 3)) {
    return true;
  } else if (!strncmp(option->optionString, "-Xtm", 4)) {
    return true;
  } else if (!strncmp(option->optionString, "-Xverifyheap", 12)) {
    return true;
  } else if (!strncmp(option->optionString, "-Xmaxjitcodesize", 16)) {
    return true;
  } 
  return false;
}

bool os::Solaris::valid_stack_address(Thread* thread, address sp) {
  address  stackStart  = (address)thread->stack_base();
  address  stackEnd    = (address)(stackStart - (address)thread->stack_size());
  if (sp < stackStart && sp >= stackEnd ) return true;
  return false;
}

#ifndef PRODUCT
void os::Solaris::Event::verify() {
  guarantee(!Universe::is_fully_initialized() ||
	    !Universe::heap()->is_in_reserved(this),
	    "Event must be in C heap only.");
}

void os::Solaris::OSMutex::verify() {
  guarantee(!Universe::is_fully_initialized() ||
            !Universe::heap()->is_in_reserved(oop(this)),
	    "OSMutex must be in C heap only.");
}

void os::Solaris::OSMutex::verify_locked() {
  int my_id = thr_self();
  assert(_is_owned, "OSMutex should be locked");
  assert(_owner == my_id, "OSMutex should be locked by me");
}
#endif

extern "C" void breakpoint() {
  // use debugger to set breakpoint here
}

// Returns an estimate of the current stack pointer. Result must be guaranteed to
// point into the calling threads stack, and be no lower than the current stack 
// pointer.
address os::current_stack_pointer() {
  int dummy;
  address sp = (address)&dummy + 8;	// %%%% need to confirm if this is right
  return sp;
}

static thread_t main_thread;

// Thread start routine for all new Java threads
extern "C" {
  static void* _start(void* data) {
    // Try to randomize the cache line index of hot stack frames.
    // This helps when threads of the same stack traces evict each other's
    // cache lines. The threads can be either from the same JVM instance, or
    // from different JVM instances. The benefit is especially true for
    // processors with hyperthreading technology.
    static int counter = 0;
    int pid = os::current_process_id();
    alloca(((pid ^ counter++) & 7) * 128);

    // %%%%% should do the os::initialize_thread actions in here
    int prio;
    Thread* thread = (Thread*)data;
    OSThread* osthr = thread->osthread();

    osthr->set_lwp_id( _lwp_self() );  // Store lwp in case we are bound
    // If the creator called set priority before we started,
    // we need to call set priority now that we have an lwp.  
    // Get the priority from libthread and set the priority 
    // for the new Solaris lwp.
    if ( osthr->thread_id() != -1 ) { 
      if ( UseThreadPriorities ) {
        thr_getprio(osthr->thread_id(), &prio);
        if (ThreadPriorityVerbose) {
           tty->print_cr("Starting Thread " INTPTR_FORMAT ", LWP is " INTPTR_FORMAT ", setting priority: %d\n", 
			 osthr->thread_id(), osthr->lwp_id(), prio );
        }
        os::set_native_priority(thread, prio);
      }
    }
    else if (ThreadPriorityVerbose) {
      warning("Can't set priority in _start routine, thread id hasn't been set\n");
    }

    assert(osthr->get_state() == RUNNABLE, "invalid os thread state");

    // initialize signal mask for this thread
    os::Solaris::hotspot_sigmask(thread);

    
    thread->run();

    // One less thread is executing
    // When the VMThread gets here, the main thread may have already exited
    // which frees the CodeHeap containing the Atomic::dec code
    if (thread != VMThread::vm_thread() && VMThread::vm_thread() != NULL) {
      Atomic::dec(&os::Solaris::_os_thread_count);
    }

    if (UseDetachedThreads) {
      thr_exit(NULL);
      ShouldNotReachHere();
    }
    return NULL;
  }
}


static OSThread* create_os_thread(Thread* thread, thread_t thread_id) {
  // Allocate the OSThread object
  OSThread* osthread = new OSThread(NULL, NULL);
  if (osthread == NULL) return NULL;

  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);  
    // Initial state is ALLOCATED but not INITIALIZED
    osthread->set_state(ALLOCATED);
    
    // Store info on the Solaris thread into the OSThread
    osthread->set_thread_id(thread_id);
    osthread->set_lwp_id(_lwp_self());

    if ( ThreadPriorityVerbose ) {
      tty->print_cr("In create_os_thread, Thread " INTPTR_FORMAT ", LWP is " INTPTR_FORMAT "\n",
                    osthread->thread_id(), osthread->lwp_id() );
    }
    
    // Initial thread state is INITIALIZED, not SUSPENDED
    osthread->set_state(INITIALIZED);
  }
  return osthread;
}

void os::Solaris::hotspot_sigmask(Thread* thread) {

  //Save caller's signal mask
  sigset_t sigmask;
  thr_sigsetmask(SIG_SETMASK, NULL, &sigmask);
  OSThread *osthread = thread->osthread();
  osthread->set_caller_sigmask(sigmask);
  
  thr_sigsetmask(SIG_UNBLOCK, os::Solaris::unblocked_signals(), NULL);
  if (!ReduceSignalUsage) {
    if (thread->is_VM_thread()) {
      // Only the VM thread handles BREAK_SIGNAL ...
      thr_sigsetmask(SIG_UNBLOCK, vm_signals(), NULL);
    } else {
      // ... all other threads block BREAK_SIGNAL
      assert(!sigismember(vm_signals(), SIGINT), "SIGINT should not be blocked");
      thr_sigsetmask(SIG_BLOCK, vm_signals(), NULL);
    }
  }
}

bool os::create_attached_thread(Thread* thread) {
  OSThread* osthread = create_os_thread(thread, thr_self());
  if (osthread == NULL) {
     return false; 
  }
  
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    thread->clear_is_baby_thread();
    osthread->set_state(RUNNABLE);   
  }
  thread->set_osthread(osthread);

  // initialize signal mask for this thread
  // and save the caller's signal mask 
  os::Solaris::hotspot_sigmask(thread);
  
  return true;
}

bool os::create_main_thread(Thread* thread) {
  if (_starting_thread == NULL) {
    _starting_thread = create_os_thread(thread, main_thread);
     if (_starting_thread == NULL) {
        return false; 
     }
  }

  // The primodial thread is runnable from the start
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);  
    thread->clear_is_baby_thread();
    _starting_thread->set_state(RUNNABLE);
  }
  thread->set_osthread(_starting_thread);

  // initialize signal mask for this thread
  // and save the caller's signal mask 
  os::Solaris::hotspot_sigmask(thread);

  return true;
}

// _T2_libthread is true if we believe we are running with the newer
// SunSoft lwp/libthread.so (2.8 patch, 2.9 default)
bool os::Solaris::_T2_libthread = false;

bool os::create_thread(Thread* thread, ThreadType thr_type, size_t stack_size) {
  // Allocate the OSThread object
  OSThread* osthread = new OSThread(NULL, NULL);
  if (osthread == NULL) {
    return false;
  }

  if ( ThreadPriorityVerbose ) {
    char *thrtyp;
    switch ( thr_type ) {
      case vm_thread:
        thrtyp = (char *)"vm";
        break;
      case cms_thread:
        thrtyp = (char *)"cms";
        break;
      case pgc_thread:
        thrtyp = (char *)"pgc";
        break;
      case java_thread:
        thrtyp = (char *)"java";
        break;
      case compiler_thread:
        thrtyp = (char *)"compiler";
        break;
      case watcher_thread:
        thrtyp = (char *)"watcher";
        break;
      case gc_thread:
        thrtyp = (char *)"gc";
        break;
      default:
        thrtyp = (char *)"unknown";
        break;
    }
    tty->print_cr("In create_thread, creating a %s thread\n", thrtyp); 
  }

  // Calculate stack size if it's not specified by caller.
  if (stack_size == 0) {
    // The default stack size 1M (2M for LP64).
    stack_size = (BytesPerWord >> 2) * K * K;

    switch (thr_type) {
    case os::java_thread:
      // Java threads use ThreadStackSize which default value can be changed with the flag -Xss
      if (JavaThread::stack_size_at_create() > 0) stack_size = JavaThread::stack_size_at_create();
      break;
    case os::compiler_thread:
      if (CompilerThreadStackSize > 0) {
        stack_size = (size_t)(CompilerThreadStackSize * K);
        break;
      } // else fall through:
        // use VMThreadStackSize if CompilerThreadStackSize is not defined
    case os::vm_thread: 
    case os::gc_thread:
    case os::pgc_thread: 
    case os::cms_thread: 
    case os::watcher_thread: 
      if (VMThreadStackSize > 0) stack_size = (size_t)(VMThreadStackSize * K);
      break;
    }
  }
  stack_size = MAX2(stack_size, os::Solaris::min_stack_allowed);
  
  // Initial state is ALLOCATED but not INITIALIZED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(ALLOCATED);
  }
  
  if (os::Solaris::_os_thread_count > os::Solaris::_os_thread_limit) {
    // We got lots of threads. Check if we still have some address space left.
    // Need to be at least 5Mb of unreserved address space. We do check by
    // trying to reserve some.
    const size_t VirtualMemoryBangSize = 20*K*K;
    char* mem = os::reserve_memory(VirtualMemoryBangSize);
    if (mem == NULL) { 
      delete osthread;    
      return false;
    } else {
      // Release the memory again
      os::release_memory(mem, VirtualMemoryBangSize);
    }
  }

  // Setup osthread because the child thread may need it.
  thread->set_osthread(osthread);

  // Create the Solaris thread
  // explicit THR_BOUND for T2_libthread case in case 
  // that assumption is not accurate, but our alternate signal stack
  // handling is based on it which must have bound threads
  thread_t tid;
  long     flags = (UseDetachedThreads ? THR_DETACHED : 0) | THR_SUSPENDED
                   | ((UseBoundThreads || os::Solaris::T2_libthread() || 
                       (thr_type == vm_thread) ||
                       (thr_type == cms_thread) ||
                       (thr_type == pgc_thread) ||
                       (thr_type == compiler_thread && BackgroundCompilation)) ?
                      THR_BOUND : 0);
  int      status;

  // 4376845 -- libthread/kernel don't provide enough LWPs to utilize all CPUs.
  //
  // On multiprocessors systems, libthread sometimes under-provisions our
  // process with LWPs.  On a 30-way systems, for instance, we could have
  // 50 user-level threads in ready state and only 2 or 3 LWPs assigned
  // to our process.  This can result in under utilization of PEs. 
  // I suspect the problem is related to libthread's LWP
  // pool management and to the kernel's SIGBLOCKING "last LWP parked" 
  // upcall policy. 
  // 
  // The following code is palliative -- it attempts to ensure that our 
  // process has sufficient LWPs to take advantage of multiple PEs.
  // Proper long-term cures include using user-level threads bound to LWPs 
  // (THR_BOUND) or using LWP-based synchronization.  Note that there is a 
  // slight timing window with respect to sampling _os_thread_count, but
  // the race is benign.  Also, we should periodically recompute
  // _processors_online as the min of SC_NPROCESSORS_ONLN and the 
  // the number of PEs in our partition.  You might be tempted to use
  // THR_NEW_LWP here, but I'd recommend against it as that could
  // result in undesirable growth of the libthread's LWP pool.
  // The fix below isn't sufficient; for instance, it doesn't take into count
  // LWPs parked on IO.  It does, however, help certain CPU-bound benchmarks.
  //
  // Some pathologies this scheme doesn't handle:
  // *	Threads can block, releasing the LWPs.  The LWPs can age out. 
  //	When a large number of threads become ready again there aren't
  //	enough LWPs available to service them.  This can occur when the
  //	number of ready threads oscillates. 
  // *	LWPs/Threads park on IO, thus taking the LWP out of circulation.
  // 
  // Finally, we should call thr_setconcurrency() periodically to refresh
  // the LWP pool and thwart the LWP age-out mechanism. 
  // The "+3" term provides a little slop -- we want to slightly overprovision.

  if (AdjustConcurrency && os::Solaris::_os_thread_count < (_processors_online+3)) { 
	if (!(flags & THR_BOUND)) { 
		thr_setconcurrency (os::Solaris::_os_thread_count); 	// avoid starvation
	}
  }
  // Although this doesn't hurt, we should warn of undefined behavior
  // when using unbound T1 threads with schedctl().  This should never 
  // happen, as the compiler and VM threads are always created bound
  DEBUG_ONLY(  
      if ((VMThreadHintNoPreempt || CompilerThreadHintNoPreempt) &&
          (!os::Solaris::T2_libthread() && (!(flags & THR_BOUND))) &&
	  ((thr_type == vm_thread) || (thr_type == cms_thread) ||
	   (thr_type == pgc_thread) || (thr_type == compiler_thread && BackgroundCompilation))) {
	 warning("schedctl behavior undefined when Compiler/VM/GC Threads are Unbound");
      }
  );


  // Mark that we don't have an lwp or thread id yet. 
  // In case we attempt to set the priority before the thread starts.  
  osthread->set_lwp_id(-1);
  osthread->set_thread_id(-1);

  status = thr_create(NULL, stack_size, _start, thread, flags, &tid);
  if (status != 0) {
    if (PrintMiscellaneous && (Verbose || WizardMode)) {
      perror("os::create_thread");
    }
    thread->set_osthread(NULL);
    // Need to clean up stuff we've allocated so far
    delete osthread;
    return false;
  }
  
  Atomic::inc(&os::Solaris::_os_thread_count);
  
  // Store info on the Solaris thread into the OSThread
  osthread->set_thread_id(tid);

  // Remember that we created this thread so we can set priority on it 
  osthread->set_vm_created();

  // Set the default thread priority otherwise use NormalPriority

  if ( UseThreadPriorities ) {
     thr_setprio(tid, (DefaultThreadPriority == -1) ? 
                                           java_to_os_priority[NormPriority] : 
                                           DefaultThreadPriority);
  }

  // Initial thread state is INITIALIZED, not SUSPENDED
  {
    MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag);
    osthread->set_state(INITIALIZED);
  }
  
  // The thread is returned suspended (in state INITIALIZED), and is started higher up in the call chain
  return true;
}

/* defined for >= Solaris 10. This allows builds on earlier versions
 *  of Solaris to take advantage of the newly reserved Solaris JVM signals 
 *  With SIGJVM1, SIGJVM2, INTERRUPT_SIGNAL is SIGJVM1, ASYNC_SIGNAL is SIGJVM2
 *  and -XX:+UseAltSigs does nothing since these should have no conflict 
 */
#if !defined(SIGJVM1)
#define SIGJVM1 39
#define SIGJVM2 40
#endif

debug_only(static bool signal_sets_initialized = false);
static sigset_t unblocked_sigs, vm_sigs, allowdebug_blocked_sigs;
int os::Solaris::_SIGinterrupt = INTERRUPT_SIGNAL;
int os::Solaris::_SIGasync = ASYNC_SIGNAL;

bool os::Solaris::is_sig_ignored(int sig) {
      struct sigaction oact;
      sigaction(sig, (struct sigaction*)NULL, &oact);
      void* ohlr = oact.sa_sigaction ? CAST_FROM_FN_PTR(void*,  oact.sa_sigaction)
                                     : CAST_FROM_FN_PTR(void*,  oact.sa_handler);
      if (ohlr == CAST_FROM_FN_PTR(void*, SIG_IGN))
           return true;
      else
           return false;
}

void os::Solaris::signal_sets_init() {
  // Should also have an assertion stating we are still single-threaded.
  assert(!signal_sets_initialized, "Already initialized");
  // Fill in signals that are necessarily unblocked for all threads in
  // the VM. Currently, we unblock the following signals:
  // SHUTDOWN{1,2,3}_SIGNAL: for shutdown hooks support (unless over-ridden
  //                         by -Xrs (=ReduceSignalUsage));
  // BREAK_SIGNAL which is unblocked only by the VM thread and blocked by all
  // other threads. The "ReduceSignalUsage" boolean tells us not to alter
  // the dispositions or masks wrt these signals.
  // Programs embedding the VM that want to use the above signals for their
  // own purposes must, at this time, use the "-Xrs" option to prevent
  // interference with shutdown hooks and BREAK_SIGNAL thread dumping.
  // (See bug 4345157, and other related bugs).
  // In reality, though, unblocking these signals is really a nop, since
  // these signals are not blocked by default.
  sigemptyset(&unblocked_sigs);
  sigemptyset(&allowdebug_blocked_sigs);
  sigaddset(&unblocked_sigs, SIGILL);
  sigaddset(&unblocked_sigs, SIGSEGV);
  sigaddset(&unblocked_sigs, SIGBUS);
  sigaddset(&unblocked_sigs, SIGFPE);

  // Note: SIGRTMIN is a macro that calls sysconf() so it will
  // dynamically detect SIGRTMIN value for the system at runtime, not buildtime
  if (SIGJVM1 < SIGRTMIN) {
    os::Solaris::set_SIGinterrupt(SIGJVM1);
    os::Solaris::set_SIGasync(SIGJVM2);
  } else if (UseAltSigs) {
    os::Solaris::set_SIGinterrupt(ALT_INTERRUPT_SIGNAL);
    os::Solaris::set_SIGasync(ALT_ASYNC_SIGNAL);
  } else {
    os::Solaris::set_SIGinterrupt(INTERRUPT_SIGNAL);
    os::Solaris::set_SIGasync(ASYNC_SIGNAL);
  }

  sigaddset(&unblocked_sigs, os::Solaris::SIGinterrupt());
  sigaddset(&unblocked_sigs, os::Solaris::SIGasync());

  if (!ReduceSignalUsage) {
   if (!os::Solaris::is_sig_ignored(SHUTDOWN1_SIGNAL)) {
      sigaddset(&unblocked_sigs, SHUTDOWN1_SIGNAL);
      sigaddset(&allowdebug_blocked_sigs, SHUTDOWN1_SIGNAL);
   }
   if (!os::Solaris::is_sig_ignored(SHUTDOWN2_SIGNAL)) {
      sigaddset(&unblocked_sigs, SHUTDOWN2_SIGNAL);
      sigaddset(&allowdebug_blocked_sigs, SHUTDOWN2_SIGNAL);
   }
   if (!os::Solaris::is_sig_ignored(SHUTDOWN3_SIGNAL)) {
      sigaddset(&unblocked_sigs, SHUTDOWN3_SIGNAL);
      sigaddset(&allowdebug_blocked_sigs, SHUTDOWN3_SIGNAL);
   }
  }
  // Fill in signals that are blocked by all but the VM thread.
  sigemptyset(&vm_sigs);
  if (!ReduceSignalUsage)
    sigaddset(&vm_sigs, BREAK_SIGNAL);
  debug_only(signal_sets_initialized = true);

}

// These are signals that are unblocked while a thread is running Java.
// (For some reason, they get blocked by default.)
sigset_t* os::Solaris::unblocked_signals() {
  assert(signal_sets_initialized, "Not initialized");
  return &unblocked_sigs;
}

// These are the signals that are blocked while a (non-VM) thread is
// running Java. Only the VM thread handles these signals.
sigset_t* os::Solaris::vm_signals() {
  assert(signal_sets_initialized, "Not initialized");
  return &vm_sigs;
}

// These are signals that are blocked during cond_wait to allow debugger in
sigset_t* os::Solaris::allowdebug_blocked_signals() {
  assert(signal_sets_initialized, "Not initialized");
  return &allowdebug_blocked_sigs;
}

// First crack at OS-specific initialization, from inside the new thread.
void os::initialize_thread() {
  if (thr_main()) {
    JavaThread* jt = (JavaThread *)Thread::current();
    assert(jt != NULL,"Sanity check");
    // Use 2MB to allow for Solaris 7 64 bit mode.
    size_t stack_size = JavaThread::stack_size_at_create() == 0
                        ? 2048*K : JavaThread::stack_size_at_create();

    // There are rare cases when we may have already used more than
    // the basic stack size allotment before this method is invoked.
    // Attempt to allow for a normally sized java_stack.
    size_t current_stack_offset = (size_t)(jt->stack_base() - (address)&stack_size);
    stack_size += ReservedSpace::page_align_size_down(current_stack_offset);

    if (stack_size > jt->stack_size()) {
      NOT_PRODUCT(
        struct rlimit limits;
        getrlimit(RLIMIT_STACK, &limits);
	size_t size = adjust_stack_size(jt->stack_base(), (size_t)limits.rlim_cur);
        assert(size == jt->stack_size(), "Stack size problem in main thread");
      )
      tty->print_cr(
        "Stack size of %d Kb exceeds current limit of %d Kb.\n"
        "(Stack sizes are rounded up to a multiple of the system page size.)\n"
        "See limit(1) to increase the stack size limit.",
        stack_size / K, jt->stack_size() / K);
      vm_exit(1);
    }
    assert(jt->stack_size() >= stack_size,
          "Attempt to map more stack than was allocated");
    jt->set_stack_size(stack_size);
  }

   // 5/22/01: Right now alternate signal stacks do not handle
   // throwing stack overflow exceptions, see bug 4463178
   // Until a fix is found for this, T2 will NOT imply alternate signal
   // stacks.
   // If using T2 libthread threads, install an alternate signal stack.
   // Because alternate stacks associate with LWPs on Solaris,
   // see sigaltstack(2), if using UNBOUND threads, or if UseBoundThreads
   // we prefer to explicitly stack bang. 
   // If not using T2 libthread, but using UseBoundThreads any threads
   // (primordial thread, jni_attachCurrentThread) we do not create,
   // probably are not bound, therefore they can not have an alternate
   // signal stack. Since our stack banging code is generated and
   // is shared across threads, all threads must be bound to allow
   // using alternate signal stacks.  The alternative is to interpose
   // on _lwp_create to associate an alt sig stack with each LWP,
   // and this could be a problem when the JVM is embedded.
   // We would prefer to use alternate signal stacks with T2
   // Since there is currently no accurate way to detect T2
   // we do not. Assuming T2 when running T1 causes sig 11s or assertions
   // on installing alternate signal stacks 

  
   // 05/09/03: removed alternate signal stack support for Solaris
   // The alternate signal stack mechanism is no longer needed to
   // handle stack overflow. This is now handled by allocating
   // guard pages (red zone) and stackbanging.
   // Initially the alternate signal stack mechanism was removed because
   // it did not work with T1 llibthread. Alternate 
   // signal stacks MUST have all threads bound to lwps. Applications
   // can create their own threads and attach them without their being
   // bound under T1. This is frequently the case for the primordial thread.
   // If we were ever to reenable this mechanism we would need to
   // use the dynamic check for T2 libthread.

  os::Solaris::init_thread_fpu_state();
}



// Free Solaris resources related to the OSThread
void os::free_thread(OSThread* osthread) {
  assert(osthread != NULL, "os::free_thread but osthread not set");


  // We are told to free resources of the argument thread, 
  // but we can only really operate on the current thread.
  // The main thread must take the VMThread down synchronously
  // before the main thread exits and frees up CodeHeap
  guarantee((Thread::current()->osthread() == osthread
     || (osthread == VMThread::vm_thread()->osthread())), "os::free_thread but not current thread");
  if (Thread::current()->osthread() == osthread) {
    // Restore caller's signal mask
    sigset_t sigmask = osthread->caller_sigmask();
    thr_sigsetmask(SIG_SETMASK, &sigmask, NULL);
  }
  delete osthread;
}

 // ***************************************************************
 // Platform dependent Suspend and Resume
 // ***************************************************************

// this is for the normal thr_suspend case for non-self suspension
// Returns 0 for success
int os::pd_suspend_thread(Thread* thread, bool fence) {
    // fence off shared mutexes prior to suspension of target to avoid
    // any deadlocks

    OSThread* osthread = thread->osthread();
    long sts;
    if (fence) {
      ThreadCritical tc;

      sts = thr_suspend(osthread->thread_id());
    } else {
      sts = thr_suspend(osthread->thread_id());
    }
    return sts;
}

// Resume a thread by one level.  
// Return 0 for success
// Nesting of suspend/resume is handled
// by Thread::suspend_thread_impl, Thread::resume_thread_impl
// resume_thread
int os::pd_resume_thread(Thread* thread) {
    return thr_continue(thread->osthread()->thread_id()); 
}

// we need to be able to suspend ourself while at the same (atomic) time
// giving up the SR_lock -- we do this by using the
// SR_lock to implement a suspend_self
int os::pd_self_suspend_thread(Thread* thread) {
    thread->SR_lock()->wait(Mutex::_no_safepoint_check_flag);
    return 0;
}

void os::pd_start_thread(Thread* thread) {
  int status = thr_continue(thread->osthread()->thread_id());
  assert(status == 0, "thr_continue failed");
}
  

intx os::current_thread_id() {
  return (intx)thr_self();
}

static pid_t _initial_pid = 0;

int os::current_process_id() {
  return (int)(_initial_pid ? _initial_pid : getpid());
}

int os::allocate_thread_local_storage() {
  // %%%       in Win32 this allocates a memory segment pointed to by a
  //           register.  Dan Stein can implement a similar feature in
  //           Solaris.  Alternatively, the VM can do the same thing
  //           explicitly: malloc some storage and keep the pointer in a
  //           register (which is part of the thread's context) (or keep it
  //           in TLS).
  // %%%       In current versions of Solaris, thr_self and TSD can
  //	       be accessed via short sequences of displaced indirections.
  //	       The value of thr_self is available as %g7(36).
  //	       The value of thr_getspecific(k) is stored in %g7(12)(4)(k*4-4),
  //	       assuming that the current thread already has a value bound to k.
  //	       It may be worth experimenting with such access patterns,
  //	       and later having the parameters formally exported from a Solaris
  //	       interface.  I think, however, that it will be faster to
  //	       maintain the invariant that %g2 always contains the
  //	       JavaThread in Java code, and have stubs simply
  //	       treat %g2 as a caller-save register, preserving it in a %lN.
  thread_key_t tk;
  if (thr_keycreate( &tk, NULL ) )
    fatal1("os::allocate_thread_local_storage: thr_keycreate failed (%s)", strerror(errno));
  return int(tk);
}

void os::free_thread_local_storage(int index) {
  // %%% don't think we need anything here
  // if ( pthread_key_delete((pthread_key_t) tk) )
  //   fatal("os::free_thread_local_storage: pthread_key_delete failed");
}

#define SMALLINT 32   // libthread allocate for tsd_common is a version specific
                      // small number - point is NO swap space available
void os::thread_local_storage_at_put(int index, void* value) {
  // %%% this is used only in threadLocalStorage.cpp
  if (thr_setspecific((thread_key_t)index, value)) {
    if (errno == ENOMEM) {
       vm_exit_out_of_memory(SMALLINT, "thr_setspecific: out of swap space");
    } else {
      fatal1("os::thread_local_storage_at_put: thr_setspecific failed (%s)", strerror(errno));
    }
  } else { 
      ThreadLocalStorage::set_thread_in_slot ((Thread *) value) ; 
  }
}

// This function could be called before TLS is initialized, for example, when 
// VM receives an async signal or when VM causes a fatal error during
// initialization. Return NULL if thr_getspecific() fails.
void* os::thread_local_storage_at(int index) {
  // %%% this is used only in threadLocalStorage.cpp
  void* r = NULL;
  return thr_getspecific((thread_key_t)index, &r) != 0 ? NULL : r;
}


const int NANOSECS_PER_MILLISECS = 1000000;
// gethrtime can move backwards if read from one cpu and then a different cpu
// getTimeNanos is guaranteed to not move backward on Solaris
// local spinloop created as faster for a CAS on an int than
// a CAS on a 64bit jlong. Also Atomic::cmpxchg for jlong is not
// supported on sparc v8 or pre supports_cx8 intel boxes.
// oldgetTimeNanos for systems which do not support CAS on 64bit jlong
// i.e. sparc v8 and pre supports_cx8 (i486) intel boxes
inline hrtime_t oldgetTimeNanos() {
  int gotlock = LOCK_INVALID;
  hrtime_t newtime = gethrtime();
  
  for (;;) {
// grab lock for max_hrtime
    int curlock = max_hrtime_lock;
    if (curlock & LOCK_BUSY)  continue;
    if (gotlock = Atomic::cmpxchg(LOCK_BUSY, &max_hrtime_lock, LOCK_FREE) != LOCK_FREE) continue;
    if (newtime > max_hrtime) {
      max_hrtime = newtime;
    } else {
      newtime = max_hrtime;
    }
    // release lock
    max_hrtime_lock = LOCK_FREE;
    return newtime;
  }
}
// gethrtime can move backwards if read from one cpu and then a different cpu
// getTimeNanos is guaranteed to not move backward on Solaris
inline hrtime_t getTimeNanos() {
  if (VM_Version::supports_cx8()) {
    bool retry = false;
    hrtime_t newtime = gethrtime();
    hrtime_t oldmaxtime = max_hrtime;
    hrtime_t retmaxtime = oldmaxtime;
    while ((newtime > retmaxtime) && (retry == false || retmaxtime != oldmaxtime)) {
      oldmaxtime = retmaxtime;
      retmaxtime = Atomic::cmpxchg(newtime, (volatile jlong *)&max_hrtime, oldmaxtime);
      retry = true;
    }
    return (newtime > retmaxtime) ? newtime : retmaxtime;
  } else {
    return oldgetTimeNanos();
  }
}

// Time since start-up in seconds to a fine granularity.
// Used by VMSelfDestructTimer and the MemProfiler.
double os::elapsedTime() {
  return (double)(getTimeNanos() - first_hrtime) / (double)hrtime_hz;
}

jlong os::elapsed_counter() {
  return (jlong)(getTimeNanos() - first_hrtime);
}

jlong os::elapsed_frequency() {
   return hrtime_hz;
}

// Used internally for comparisons only
// getTimeMillis guaranteed to not move backwards on Solaris
jlong getTimeMillis() {
  jlong nanotime = getTimeNanos();
  return (jlong)(nanotime / NANOSECS_PER_MILLISECS);
}

// Must return millis since Jan 1 1970 for JVM_CurrentTimeMillis
jlong os::javaTimeMillis() {
  timeval t;
  static const char* aNull = 0;
  if (gettimeofday( &t, &aNull) == -1)
    fatal1("os::javaTimeMillis: gettimeofday (%s)", strerror(errno));
  return jlong(t.tv_sec) * 1000  +  jlong(t.tv_usec) / 1000;
}

jlong os::javaTimeNanos() {
  return (jlong)getTimeNanos();
}

void os::javaTimeNanos_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = ALL_64_BITS;      // gethrtime() uses all 64 bits
  info_ptr->may_skip_backward = false;    // not subject to resetting or drifting
  info_ptr->may_skip_forward = false;     // not subject to resetting or drifting
  info_ptr->kind = JVMTI_TIMER_ELAPSED;   // elapsed not CPU time
}

// Note: os::shutdown() might be called very early during initialization, or
// called from signal handler. Before adding something to os::shutdown(), make
// sure it is async-safe and can handle partially initialized VM.
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

// Note: os::abort() might be called very early during initialization, or
// called from signal handler. Before adding something to os::abort(), make
// sure it is async-safe and can handle partially initialized VM.
void os::abort(bool dump_core) {
  os::shutdown();
  if (dump_core) {
#ifndef PRODUCT
    fdStream out(defaultStream::output_fd());
    out.print_raw("Current thread is ");
    char buf[16];
    jio_snprintf(buf, sizeof(buf), UINTX_FORMAT, os::current_thread_id());
    out.print_raw_cr(buf);
    out.print_raw_cr("Dumping core ...");
#endif
    ::abort(); // dump core (for debugging)
  }

  ::exit(1);
}

// Die immediately, no exit hook, no abort hook, no cleanup.
void os::die() {
  _exit(-1);
}

// DLL functions

const char* os::dll_file_extension() { return ".so"; }

const char* os::get_temp_directory() { return "/tmp/"; }

// check if addr is inside libjvm[_g].so
bool os::address_is_in_vm(address addr) {
  static address libjvm_base_addr;
  Dl_info dlinfo;

  if (libjvm_base_addr == NULL) {
    dladdr(CAST_FROM_FN_PTR(void *, os::address_is_in_vm), &dlinfo);
    libjvm_base_addr = (address)dlinfo.dli_fbase;
    assert(libjvm_base_addr !=NULL, "Cannot obtain base address for libjvm");
  }

  if (dladdr((void *)addr, &dlinfo)) {
    if (libjvm_base_addr == (address)dlinfo.dli_fbase) return true;
  }

  return false;
}

typedef int (*dladdr1_func_type) (void *, Dl_info *, void **, int);
static dladdr1_func_type dladdr1_func = NULL;

bool os::dll_address_to_function_name(address addr, char *buf,
                                      int buflen, int * offset) {
  Dl_info dlinfo;

  // dladdr1_func was initialized in os::init()
  if (dladdr1_func){
      // yes, we have dladdr1

      // Support for dladdr1 is checked at runtime; it may be
      // available even if the vm is built on a machine that does
      // not have dladdr1 support.  Make sure there is a value for
      // RTLD_DL_SYMENT.
      #ifndef RTLD_DL_SYMENT
      #define RTLD_DL_SYMENT 1
      #endif
      Sym * info;
      if (dladdr1_func((void *)addr, &dlinfo, (void **)&info,
                       RTLD_DL_SYMENT)) {
          if (buf) jio_snprintf(buf, buflen, "%s", dlinfo.dli_sname);
          if (offset) *offset = addr - (address)dlinfo.dli_saddr;

          // check if the returned symbol really covers addr
          return ((char *)dlinfo.dli_saddr + info->st_size > (char *)addr);
      } else {
          if (buf) buf[0] = '\0';
          if (offset) *offset  = -1;
          return false;
      }
  } else {
      // no, only dladdr is available
      if(dladdr((void *)addr, &dlinfo)) {
          if (buf) jio_snprintf(buf, buflen, dlinfo.dli_sname);
          if (offset) *offset = addr - (address)dlinfo.dli_saddr;
          return true;
      } else {
          if (buf) buf[0] = '\0';
          if (offset) *offset  = -1;
          return false;
      }
  }
}

bool os::dll_address_to_library_name(address addr, char* buf,
                                     int buflen, int* offset) {
  Dl_info dlinfo;

  if (dladdr((void*)addr, &dlinfo)){
     if (buf) jio_snprintf(buf, buflen, "%s", dlinfo.dli_fname);
     if (offset) *offset = addr - (address)dlinfo.dli_fbase;
     return true;
  } else {
     if (buf) buf[0] = '\0';
     if (offset) *offset = -1;
     return false;
  }
}

// Prints the names and full paths of all opened dynamic libraries
// for current process
void os::print_dll_info(outputStream * st) {
    Dl_info dli;
    void *handle;
    Link_map *map;
    Link_map *p;

    st->print_cr("Dynamic libraries:"); st->flush();

    if (!dladdr(CAST_FROM_FN_PTR(void *, os::print_dll_info), &dli)) {
        st->print_cr("Error: Cannot print dynamic libraries.");
        return;
    }
    handle = dlopen(dli.dli_fname, RTLD_LAZY);
    if (handle == NULL) {
        st->print_cr("Error: Cannot print dynamic libraries.");
        return;
    }
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    if (map == NULL) {
        st->print_cr("Error: Cannot print dynamic libraries.");
        return;
    }

    while (map->l_prev != NULL)
        map = map->l_prev;

    while (map != NULL) {
        st->print_cr(PTR_FORMAT " \t%s", map->l_addr, map->l_name);
        map = map->l_next;
    }

    dlclose(handle);
}

bool _print_ascii_file(const char* filename, outputStream* st) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
     return false;
  }

  char buf[32];
  int bytes;
  while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
    st->print_raw(buf, bytes);
  }

  close(fd);

  return true;
}

void os::print_os_info(outputStream* st) {
  st->print("OS:");

  if (!_print_ascii_file("/etc/release", st)) {
    st->print("Solaris");
  }
  st->cr();

  // kernel
  st->print("uname:");
  struct utsname name;
  uname(&name);
  st->print(name.sysname); st->print(" ");
  st->print(name.release); st->print(" ");
  st->print(name.version); st->print(" ");
  st->print(name.machine);

  // libthread
  if (os::Solaris::T2_libthread()) st->print("  (T2 libthread)");
  else st->print("  (T1 libthread)");
  st->cr();

  // rlimit
  st->print("rlimit:");
  struct rlimit rlim;

  st->print(" STACK ");
  getrlimit(RLIMIT_STACK, &rlim);
  if (rlim.rlim_cur == RLIM_INFINITY) st->print("infinity");
  else st->print("%uk", rlim.rlim_cur >> 10);

  st->print(", CORE ");
  getrlimit(RLIMIT_CORE, &rlim);
  if (rlim.rlim_cur == RLIM_INFINITY) st->print("infinity");
  else st->print("%uk", rlim.rlim_cur >> 10);

  st->print(", NOFILE ");
  getrlimit(RLIMIT_NOFILE, &rlim);
  if (rlim.rlim_cur == RLIM_INFINITY) st->print("infinity");
  else st->print("%d", rlim.rlim_cur);

  st->print(", AS ");
  getrlimit(RLIMIT_AS, &rlim);
  if (rlim.rlim_cur == RLIM_INFINITY) st->print("infinity");
  else st->print("%uk", rlim.rlim_cur >> 10);
  st->cr();

  // load average
  st->print("load average:");
  double loadavg[3];
  getloadavg(loadavg, 3);
  st->print("%0.02f %0.02f %0.02f", loadavg[0], loadavg[1], loadavg[2]);
  st->cr();
}

void os::print_memory_info(outputStream* st) {
  st->print("Memory:");
  st->print(" %dk page", os::vm_page_size()>>10);
  st->print(", physical " UINT64_FORMAT "k", os::physical_memory()>>10);
  julong avail_memory = (julong)sysconf(_SC_AVPHYS_PAGES) * os::vm_page_size();
  st->print("(" UINT64_FORMAT "k free)", avail_memory >> 10);
  st->cr();
}

void os::print_siginfo(outputStream* st, void* siginfo) {
  st->print("siginfo:");

  siginfo_t *si = (siginfo_t*)siginfo;
  st->print("si_signo=%d", si->si_signo);
  st->print(", si_errno=%d", si->si_errno);
  st->print(", si_code=%d", si->si_code);
  switch (si->si_signo) {
  case SIGILL:
  case SIGFPE:
  case SIGSEGV:
  case SIGBUS:
    st->print(", si_addr=" PTR_FORMAT, si->si_addr);
    break;
  }
  st->cr();
}

static char saved_jvm_path[MAXPATHLEN] = {0};

// Find the full path to the current module, libjvm.so or libjvm_g.so
void os::jvm_path(char *buf, jint buflen) {
  // Error checking.
  if (buflen < MAXPATHLEN) {
    assert(false, "must use a large-enough buffer");
    buf[0] = '\0';
    return;
  }
  // Lazy resolve the path to current module.
  if (saved_jvm_path[0] != 0) {
    strcpy(buf, saved_jvm_path);
    return;
  }

  Dl_info dlinfo;
  int ret = dladdr(CAST_FROM_FN_PTR(void *, os::jvm_path), &dlinfo);
  assert(ret != 0, "cannot locate libjvm");
  realpath((char *)dlinfo.dli_fname, buf);

#ifndef PRODUCT
  // Support for the gamma launcher.  Typical value for buf is
  // "<JAVA_HOME>/jre/lib/<arch>/<vmtype>/libjvm.so".  If "/jre/lib/" appears at
  // the right place in the string, then assume we are installed in a JDK and
  // we're done.  Otherwise, check for a JAVA_HOME environment variable and fix
  // up the path so it looks like libjvm.so is installed there (append a
  // fake suffix hotspot/libjvm.so).
  const char *p = buf + strlen(buf) - 1;
  for (int count = 0; p > buf && count < 5; ++count) {
    for (--p; p > buf && *p != '/'; --p)
      /* empty */ ;
  }

  if (strncmp(p, "/jre/lib/", 9) != 0) {
    // Look for JAVA_HOME in the environment.
    char* java_home_var = ::getenv("JAVA_HOME");
    if (java_home_var != NULL && java_home_var[0] != 0) {
      char cpu_arch[12];
      sysinfo(SI_ARCHITECTURE, cpu_arch, sizeof(cpu_arch));
#ifdef _LP64
      // If we are on sparc running a 64-bit vm, look in jre/lib/sparcv9.
      if (strcmp(cpu_arch, "sparc") == 0) {
	strcat(cpu_arch, "v9");
      }
#endif
      // Check the current module name "libjvm.so" or "libjvm_g.so".
      p = strrchr(buf, '/');
      assert(strstr(p, "/libjvm") == p, "invalid library name");
      p = strstr(p, "_g") ? "_g" : "";

      realpath(java_home_var, buf);
      sprintf(buf + strlen(buf), "/jre/lib/%s", cpu_arch);
      if (0 == access(buf, F_OK)) {
	// Use current module name "libjvm[_g].so" instead of 
	// "libjvm"debug_only("_g")".so" since for fastdebug version
	// we should have "libjvm.so" but debug_only("_g") adds "_g"!
	// It is used when we are choosing the HPI library's name 
	// "libhpi[_g].so" in hpi::initialize_get_interface().
	sprintf(buf + strlen(buf), "/hotspot/libjvm%s.so", p);
      } else {
        // Go back to path of .so
        realpath((char *)dlinfo.dli_fname, buf);
      }
    }
  } 
#endif // #ifndef PRODUCT

  strcpy(saved_jvm_path, buf);
}

 
void os::print_jni_name_prefix_on(outputStream* st, int args_size) {
  // no prefix required, not even "_"
}


void os::print_jni_name_suffix_on(outputStream* st, int args_size) {
  // no suffix required
}


// sun.misc.Signal

extern "C" {
  static void UserHandler(int sig, void *siginfo, void *context) {
    // Ctrl-C is pressed during error reporting, likely because the error
    // handler fails to abort. Let VM die immediately.
    if (sig == SIGINT && is_error_reported()) {
       os::die();
    }

    os::signal_notify(sig);
    // We do not need to reinstate the signal handler each time...
  }
}

void* os::user_handler() {
  return CAST_FROM_FN_PTR(void*, UserHandler);
}
extern "C" {
  typedef void       (*sa_handler_t)(int);
  typedef void      (*sa_sigaction_t)(int, siginfo_t *, void *);
}

void* os::signal(int signal_number, void* handler) {
  struct sigaction sigAct, oldSigAct;
  sigfillset(&(sigAct.sa_mask));
  sigAct.sa_flags = SA_RESTART & ~SA_RESETHAND;
  sigAct.sa_handler = CAST_TO_FN_PTR(sa_handler_t, handler);

  if (sigaction(signal_number, &sigAct, &oldSigAct))
    // -1 means registration failed
    return (void *)-1;

  return CAST_FROM_FN_PTR(void*, oldSigAct.sa_handler);
}

void os::signal_raise(int signal_number) {
  raise(signal_number);
}

/*
 * The following code is moved from os.cpp for making this
 * code platform specific, which it is by its very nature.
 */

// a counter for each possible signal value
#define OLDMAXSIGNUM 32
static int Maxsignum = 0;
static int Sigexit = 0;
static int Maxlibjsigsigs;
static jint *pending_signals = NULL;
static int *preinstalled_sigs = NULL;
static struct sigaction *chainedsigactions = NULL;
static sema_t sig_sem;
typedef int (*version_getting_t)();
version_getting_t os::Solaris::get_libjsig_version = NULL;
static int libjsigversion = NULL;

int os::sigexitnum_pd() {
  assert(Sigexit > 0, "signal memory not yet initialized");
  return Sigexit;
}

void os::Solaris::init_signal_mem() {
  // Initialize signal structures
  Maxsignum = SIGRTMAX;
  Sigexit = Maxsignum+1;
  assert(Maxsignum >0, "Unable to obtain max signal number");

  Maxlibjsigsigs = Maxsignum;

  // pending_signals has one int per signal
  // The additional signal is for SIGEXIT - exit signal to signal_thread
  pending_signals = (jint *)os::malloc(sizeof(jint) * (Sigexit+1));
  memset(pending_signals, 0, (sizeof(jint) * (Sigexit+1)));

  if (UseSignalChaining) {
     chainedsigactions = (struct sigaction *)malloc(sizeof(struct sigaction) 
       * (Maxsignum + 1));
     memset(chainedsigactions, 0, (sizeof(struct sigaction) * (Maxsignum + 1)));
     preinstalled_sigs = (int *)os::malloc(sizeof(int) * (Maxsignum + 1));
     memset(preinstalled_sigs, 0, (sizeof(int) * (Maxsignum + 1)));
  }
}

void os::signal_init_pd() {
  int ret;

  ret = ::sema_init(&sig_sem, 0, NULL, NULL);
  assert(ret == 0, "sema_init() failed");
}

void os::signal_notify(int signal_number) {
  int ret;

  Atomic::inc(&pending_signals[signal_number]);
  ret = ::sema_post(&sig_sem);
  assert(ret == 0, "sema_post() failed");
}   

static int check_pending_signals(bool wait_for_signal) {
  int ret;
  while (true) {
    for (int i = 0; i < Sigexit + 1; i++) {
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
      ret = ::sema_wait(&sig_sem);
      assert(ret == 0, "sema_wait() failed");

      // were we externally suspended while we were waiting?
      threadIsSuspended = thread->handle_special_suspend_equivalent_condition();
      if (threadIsSuspended) {
        //
        // The semaphore has been incremented, but while we were waiting
        // another thread suspended us. We don't want to continue running
        // while suspended because that would surprise the thread that
        // suspended us.
        //
        ret = ::sema_post(&sig_sem);
        assert(ret == 0, "sema_post() failed");

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

// Virtual Memory

static int page_size = -1;

int os::vm_page_size() {
  assert(page_size != -1, "must call os::init");
  return page_size;
}

// Solaris allocates memory by pages.
int os::vm_allocation_granularity() {
  assert(page_size != -1, "must call os::init");
  return page_size;
}

// The mmap functions commit_memory, uncommit_memory, 
// release_memory, guard memory, unguard memory just return true for
// objects using ISM. 
bool os::commit_memory(char* addr, size_t bytes) {
  size_t size = bytes;
   if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
     return true;
   } else {
     return 
       NULL != Solaris::mmap_chunk(addr, size, MAP_PRIVATE|MAP_FIXED,
				   PROT_READ | PROT_WRITE | PROT_EXEC);
   }
}

bool os::commit_memory(char* addr, size_t bytes, size_t alignment_hint) {
  if (commit_memory(addr, bytes)) {
    if (UseMPSS && alignment_hint > (size_t)vm_page_size()) {
      // Since this is a hint, ignore any failures.
      (void)Solaris::set_mpss_range(addr, bytes, alignment_hint);
    }
    return true;
  }
  return false;
}

bool os::uncommit_memory(char* addr, size_t bytes) {
  size_t size = bytes;
  if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
    return true;
  } else {
    // Map uncommitted pages PROT_NONE so we fail early if we touch an
    // uncommitted page. Otherwise, the read/write might succeed if we
    // have enough swap space to back the physical page.
    return
      NULL != Solaris::mmap_chunk(addr, size,
                                  MAP_PRIVATE|MAP_FIXED|MAP_NORESERVE, 
				  PROT_NONE);
  }
}


// Reserve memory at an arbitrary address, only if that area is
// available (and not reserved for something else).

char* os::attempt_reserve_memory_at(size_t bytes, char* requested_addr) {
  const int max_tries = 5;
  char* base[max_tries];
  size_t size[max_tries];
  const size_t gap = 0x400000;

  // Assert that this block is a multiple of 4MB (which is the minimum 
  // size increment for the Java heap).  A multiple of the page size 
  // would probably do. 
  assert(bytes % (4*1024*1024) == 0, "reserving unexpected size block");

  // Repeatedly allocate blocks until the block is allocated at the
  // right spot. Give up after max_tries.

  int i;
  for (i = 0; i < max_tries; ++i) {
    base[i] = reserve_memory(bytes);

    if (base[i] != NULL) {
      // Is this the block we wanted?
      if (base[i] == requested_addr) {
        size[i] = bytes;
        break;
      }

      // Does this overlap the block we wanted? Give back the overlapped
      // parts and try again.

      size_t top_overlap = requested_addr + (bytes + gap) - base[i];
      if (top_overlap >= 0 && top_overlap < bytes) {
        unmap_memory(base[i], top_overlap);
        base[i] += top_overlap;
        size[i] = bytes - top_overlap;
      } else {
        size_t bottom_overlap = base[i] + bytes - requested_addr;
        if (bottom_overlap >= 0 && bottom_overlap < bytes) {
          unmap_memory(requested_addr, bottom_overlap);
          size[i] = bytes - bottom_overlap;
        } else {
          size[i] = bytes;
        }
      }
    }
  }

  // Give back the unused reserved pieces.

  for (int j = 0; j < i; ++j) {
    if (base[j] != NULL) {
      unmap_memory(base[j], size[j]);
    }
  }

  return (i < max_tries) ? requested_addr : NULL;
}


bool os::release_memory(char* addr, size_t bytes) {
  size_t size = bytes;
  if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
    return true;
  } else {
    return munmap(addr, size) == 0;
  }
}

// Protect memory (make it read-only. (Used to pass readonly pages through
// JNI GetArray<type>Elements with empty arrays.)
bool os::protect_memory(char* addr, size_t bytes) {
  if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
    fatal("protect_memory should not happen within an ISM region.");
  } else {
    int retVal = mprotect(addr, (size_t)bytes, PROT_READ);
    return retVal == 0;
  }
}

// guard_memory and unguard_memory only happens within stack guard pages.
// Since ISM pertains only to the heap, guard and unguard memory should not
/// happen with an ISM region.
bool os::guard_memory(char* addr, size_t bytes) {
  if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
    fatal("guard_memory should not happen within an ISM region.");
  } else {
    int retVal = mprotect(addr, (size_t)bytes, PROT_NONE);
    return retVal == 0;
  }
}

bool os::unguard_memory(char* addr, size_t bytes) {
  if ((UseISM || UsePermISM) && os::Solaris::largepage_range(addr, bytes)) { 
    fatal("unguard_memory should not happen within an ISM region");
  } else {
    int retVal = mprotect(addr, (size_t)bytes, PROT_READ|PROT_WRITE|PROT_EXEC);
    return retVal == 0;
  }
}

// large page init 
char* os::Solaris::_largepage_start_addr = NULL; 
char* os::Solaris::_largepage_end_addr = NULL; 

// Checks to see if the address is within the large pages 
bool os::Solaris::largepage_range(char* addr, size_t size) { 
  // Make sure address and size are within large page segment 
  if ( (os::Solaris::largepage_start_addr() <= addr) &&  
       (addr + size <= os::Solaris::largepage_end_addr()) ) { 
    return true;
  } else {
    assert(addr + size <= os::Solaris::largepage_start_addr() || 
	   addr >= os::Solaris::largepage_end_addr(), "overlaps large page segment"); 
    return false;
  }
}

bool os::Solaris::largepage_region_is_available() {
  return os::Solaris::largepage_start_addr() == NULL &&
	 os::Solaris::largepage_end_addr() == NULL;
}

void os::Solaris::mpss_sanity_check() {
  // MPSS only works in Solaris 9 and above.  Check to make sure MPSS works
  // on this system.
  char* addr = 
    Solaris::mmap_chunk((char *) LargePageSizeInBytes, LargePageSizeInBytes,
			MAP_PRIVATE | S9_MAP_ALIGN, 
			PROT_READ | PROT_WRITE | PROT_EXEC);
  
  // If the OS does not support MPSS, silently revert back to using default
  // page size.  Previously, an error message was printed out saying that 
  // MPSS does not work on the current system, but we need to implement a 
  // way to only print this out on Solaris 9 and above.
  if (addr == NULL || 
      !set_mpss_range(addr, LargePageSizeInBytes, LargePageSizeInBytes) ||
      LargePageSizeInBytes % os::vm_page_size() != 0) { 
    UseMPSS = false;
  }
  if (addr != NULL) {
    if (!uncommit_memory(addr, LargePageSizeInBytes)) 
      debug_only(warning("os::uncommit_memory failed"));
  }
}

bool os::Solaris::set_mpss_range(caddr_t start, size_t bytes, size_t align) {
  // Signal to OS that we want large pages for addresses 
  // from addr, addr + bytes
  struct S9_memcntl_mha mpss_struct;
  mpss_struct.mha_cmd = S9_MHA_MAPSIZE_VA;
  mpss_struct.mha_pagesize = align;
  mpss_struct.mha_flags = 0;
  if (memcntl(start, bytes, S9_MC_HAT_ADVISE, 
	      (caddr_t) &mpss_struct, 0, 0) < 0) {
    debug_only(warning("Attempt to use MPSS failed."));
    return false;
  }
  return true;
}

char* os::reserve_memory_special(size_t bytes) {
  size_t size = bytes;
  char* retAddr = NULL;
  int shmid;
  key_t ismKey;

  assert(os::Solaris::largepage_region_is_available(),
    "A large page region already exists");

  // UsePermISM will use the file and int combo to determine the key
  // so that subsequent use of permanent ISM will be mapped to the same
  // memory region. Only using UsePermISM, only 1 VM can be running
  // at the same time or else the VMs will try to share the memory region.
  if (UsePermISM) {
    ismKey = ftok("/dev/zero", 23098);
  } else {
    ismKey = IPC_PRIVATE;
  }
  // Create a large shared memory region to attach to based on size.
  // Currently, size is the total size of the heap
  shmid = shmget(ismKey, size, SHM_R | SHM_W | IPC_CREAT);
  if (shmid == -1){
    tty->print("Could not get ISM region: %s\n", strerror(errno));
    vm_exit(1);
  }
      
  // Attach to the region
  retAddr = (char *) shmat(shmid, 0, SHM_SHARE_MMU | SHM_R | SHM_W);
  if (retAddr == (char *) -1) {
    tty->print("Could not attach to ISM region: %s\n", strerror(errno));
    vm_exit(1);
  }

  // Removing the shmid is OK here because the actual
  // removal is based on a reference count on the number of processes that
  // are attached to the shared memory segment.  The VM continues to be 
  // attached even though the shmid has been removed from the shm tables.
  // The reference count is decremented when we shmdt or the process
  // terminates, whichever is earlier.
  if (UseISM && !UsePermISM) {
    shmid_ds *buf = NULL;
    if (shmctl(shmid, IPC_RMID, buf) != 0) {
      fatal1("Unable to remove ISM segment with shmid=%d", shmid);
    }
  }
  
  // Save the start and end address for the large page region based on the 
  // size of the heap. This is done so that future mmap calls for addresses
  // within the region are ignored.  The end address needs to be  
  // a multiple of LargePageSizeInBytes. 
  os::Solaris::set_largepage_start_addr(retAddr); 
  os::Solaris::set_largepage_end_addr((char *) (retAddr + round_to(size, LargePageSizeInBytes)));
  
  return retAddr;
}

static int os_sleep(jlong millis, bool interruptible) {
  const jlong limit = INT_MAX;
  jlong prevtime;
  int res;
  
  while (millis > limit) {
    if ((res = os_sleep(limit, interruptible)) != OS_OK)
      return res;
    millis -= limit;
  } 

  // Restart interrupted polls with new parameters until the proper delay 
  // has been completed.

  prevtime = getTimeMillis();

  while (millis > 0) {
    jlong newtime;

  if (!interruptible) {
    // Following assert fails for os::yield_all:
    // assert(!thread->is_Java_thread(), "must not be java thread");
    res = poll(NULL, 0, millis);
  } else {
    assert(Thread::current()->is_Java_thread(), "must be java thread");
    INTERRUPTIBLE_NORESTART_VM(poll(NULL, 0, millis), res, 
      os::Solaris::clear_interrupted);
  }
  // INTERRUPTIBLE_NORESTART_VM returns res == OS_INTRPT for thread.Interrupt

    if((res == OS_ERR) && (errno == EINTR)) {
      newtime = getTimeMillis();
      assert(newtime >= prevtime, "time moving backwards");
    /* Doing prevtime and newtime in microseconds doesn't help precision,
       and trying to round up to avoid lost milliseconds can result in a
       too-short delay. */
      millis -= newtime - prevtime;
      if(millis <= 0)
	return OS_OK;
      prevtime = newtime;
    } else
      return res;
  }

  return OS_OK;
}

int os::Solaris::naked_sleep() {
  // %% make the sleep time an interger flag. for now use 1 millisec.
  return os_sleep(1, false);
}

// Read calls from inside the vm need to perform state transitions
size_t os::read(int fd, void *buf, unsigned int nBytes) {
  INTERRUPTIBLE_RETURN_INT_VM(::read(fd, buf, nBytes), os::Solaris::clear_interrupted);
}

int os::sleep(Thread* thread, jlong millis, bool interruptible) {
  assert(thread == Thread::current(),  "thread consistency check");

  // On Solaris machines (especially 2.5.1) we found that sometimes the VM gets into a live lock
  // situation with a JavaThread being starved out of a lwp. The kernel doesn't seem to generate
  // a SIGWAITING signal which would enable the threads library to create a new lwp for the starving
  // thread. We suspect that because the Watcher thread keeps waking up at periodic intervals the kernel
  // is fooled into believing that the system is making progress. In the code below we block the
  // the watcher thread while safepoint is in progress so that it would not appear as though the
  // system is making progress.
  if (thread->is_Watcher_thread() && SafepointSynchronize::is_synchronizing() && !Arguments::has_profile()) {
    // We now try to acquire the threads lock. Since this lock is held by the VM thread during
    // the entire safepoint, the watcher thread will  line up here during the safepoint.
    Threads_lock->lock_without_safepoint_check();
    Threads_lock->unlock(); 
  }

  if (millis <= 0) {
    // NOTE: workaround for bug 4338139
    if (thread->is_Java_thread()) {
      ThreadBlockInVM tbivm((JavaThread*) thread);

      thr_yield();

#if 0
      // XXX - This code was not exercised during the Merlin RC1
      // pre-integration test cycle so it has been removed to
      // reduce risk.
      //
      // were we externally suspended while we were waiting?
      if (((JavaThread *) thread)->is_external_suspend_with_lock()) {
        //
        // While we were waiting in thr_yield() another thread suspended
        // us. We don't want to continue running while suspended because
        // that would surprise the thread that suspended us.
        //
        ((JavaThread *) thread)->java_suspend_self();
      }
#endif
      return 0;
    }

    thr_yield();
    return 0;
  }

  OSThreadWaitState osts(thread->osthread(), false /* not Object.wait() */);

  return os_sleep(millis, interruptible);
}

// Sleep forever; naked call to OS-specific sleep; use with CAUTION
void os::infinite_sleep() {
  while (true) {    // sleep forever ...
    ::sleep(100);   // ... 100 seconds at a time
  }
}

// Used to convert frequent JVM_Yield() to nops
bool os::dont_yield() {
  if (DontYieldALot) {  
    static hrtime_t last_time = 0;
    hrtime_t diff = getTimeNanos() - last_time;
    
    if (diff < DontYieldALotInterval * 1000000)
      return true;
    
    last_time += diff;
    
    return false;
  }
  else {
    return false;
  }
}

void os::yield() {  
  // Yields to all threads with same or greater priority
  os::sleep(Thread::current(), 0, false);
}


// On Solaris we found that yield_all doesn't always yield to all other threads.
// There have been cases where there is a thread ready to execute but it doesn't
// get an lwp as the VM thread continues to spin with sleeps of 1 millisecond.
// The 1 millisecond wait doesn't seem long enough for the kernel to issue a
// SIGWAITING signal which will cause a new lwp to be created. So we count the
// number of times yield_all is called in the one loop and increase the sleep
// time after 8 attempts. If this fails too we increase the concurrency level
// so that the starving thread would get an lwp

void os::yield_all(int attempts) {
  // Yields to all threads, including threads with lower priorities
  if (attempts == 0) {
    os::sleep(Thread::current(), 1, false);
  } else {
    int iterations = attempts % 30;
    if (iterations == 0) {
      int noofLWPS = thr_getconcurrency();
      if (noofLWPS < (Threads::number_of_threads() + 2)) {
        thr_setconcurrency(thr_getconcurrency() + 1);
      }
    } else if (iterations < 25) {
      os::sleep(Thread::current(), 1, false);
    } else {
      os::sleep(Thread::current(), 10, false);
    }
  }
}

// Called from the tight loops to possibly influence time-sharing heuristics
void os::loop_breaker(int attempts) {
  os::yield_all(attempts);
}


// Interface for setting lwp priorities.  If we are using T2 libthread,
// which forces the use of BoundThreads or we manually set UseBoundThreads,
// all of our threads will be assigned to real lwp's.  Using the thr_setprio
// function is meaningless in this mode so we must adjust the real lwp's priority
// The routines below implement the getting and setting of lwp priorities. 
//
// Note: There are three priority scales used on Solaris.  Java priotities
//       which range from 1 to 10, libthread "thr_setprio" scale which range 
//       from 0 to 127, and the current scheduling class of the process we 
//       are running in.  This is typically from -60 to +60.  
//       The setting of the lwp priorities in done after a call to thr_setprio 
//       so Java priorities are mapped to libthread priorities and we map from 
//       the latter to lwp priorities.  We don't keep priorities stored in 
//       Java priorities since some of our worker threads want to set priorities 
//       higher than all Java threads.
//
// For related information:
// (1)	man -s 2 priocntl
// (2)	man -s 4 priocntl
// (3)	man dispadmin
// =    librt.so
// =    libthread/common/rtsched.c - thrp_setlwpprio().
// =	ps -cL <pid> ... to validate priority.
// =	sched_get_priority_min and _max
//		pthread_create
//		sched_setparam
//		pthread_setschedparam
//
// Assumptions:
// +	We assume that all threads in the process belong to the same
//		scheduling class.   IE. an homogenous process.
// +	Must be root or in IA group to change change "interactive" attribute.
// 		Priocntl() will fail silently.  The only indication of failure is when
// 		we read-back the value and notice that it hasn't changed.
// +	Interactive threads enter the runq at the head, non-interactive at the tail.
// +	For RT, change timeslice as well.  Invariant:
//		constant "priority integral"
//		Konst == TimeSlice * (60-Priority)
//		Given a priority, compute appropriate timeslice.
// +	Higher numerical values have higher priority.

// sched class attributes 
typedef struct {
	int   schedPolicy;		// classID
	int   maxPrio;
	int   minPrio;
} SchedInfo;


static SchedInfo tsLimits, iaLimits, rtLimits;

#ifdef ASSERT
static int  ReadBackValidate = 1; 
#endif
static int  myClass	= 0; 
static int  myMin	= 0; 
static int  myMax	= 0; 
static int  myCur	= 0; 
static bool priocntl_enable = false;


// Call the version of priocntl suitable for all supported versions 
// of Solaris. We need to call through this wrapper so that we can 
// build on Solaris 9 and run on Solaris 8, 9 and 10.
//
// This code should be removed if we ever stop supporting Solaris 8
// and earlier releases.

static long priocntl_stub(int pcver, idtype_t idtype, id_t id, int cmd, caddr_t arg);
typedef long (*priocntl_type)(int pcver, idtype_t idtype, id_t id, int cmd, caddr_t arg);
static priocntl_type priocntl_ptr = priocntl_stub;

// Stub to set the value of the real pointer, and then call the real
// function.

static long priocntl_stub(int pcver, idtype_t idtype, id_t id, int cmd, caddr_t arg) {
  // Try Solaris 8- name only.
  priocntl_type tmp = (priocntl_type)dlsym(RTLD_DEFAULT, "__priocntl");
  guarantee(tmp != NULL, "priocntl function not found.");
  priocntl_ptr = tmp;
  return (*priocntl_ptr)(PC_VERSION, idtype, id, cmd, arg);
}


// lwp_priocntl_init
//
// Try to determine the priority scale for our process. 
//
// Return errno or 0 if OK. 
//
static 
int 	lwp_priocntl_init ()
{
  int rslt; 
  pcinfo_t ClassInfo;
  pcparms_t ParmInfo; 
  int i;

  if (!UseThreadPriorities) return 0;

  // We are using Bound threads, we need to determine our priority ranges
  if (os::Solaris::T2_libthread() || UseBoundThreads) {
    // If ThreadPriorityPolicy is 1, switch tables
    if (ThreadPriorityPolicy == 1) {
      for (i = 0 ; i < MaxPriority+1; i++) 
        os::java_to_os_priority[i] = prio_policy1[i];
    }
  }
  // Not using Bound Threads, set to ThreadPolicy 1
  else {
    for ( i = 0 ; i < MaxPriority+1; i++ ) {
      os::java_to_os_priority[i] = prio_policy1[i];
    }
    return 0;
  }


  // Get IDs for a set of well-known scheduling classes.
  // TODO-FIXME: GETCLINFO returns the current # of classes in the
  // the system.  We should have a loop that iterates over the
  // classID values, which are known to be "small" integers. 

  strcpy(ClassInfo.pc_clname, "TS");
  ClassInfo.pc_cid = -1;
  rslt = (*priocntl_ptr)(PC_VERSION, P_ALL, 0, PC_GETCID, (caddr_t)&ClassInfo); 
  if (rslt < 0) return errno;
  assert(ClassInfo.pc_cid != -1, "cid for TS class is -1");
  tsLimits.schedPolicy = ClassInfo.pc_cid;
  tsLimits.maxPrio = ((tsinfo_t*)ClassInfo.pc_clinfo)->ts_maxupri;
  tsLimits.minPrio = -tsLimits.maxPrio;

  strcpy(ClassInfo.pc_clname, "IA");
  ClassInfo.pc_cid = -1;
  rslt = (*priocntl_ptr)(PC_VERSION, P_ALL, 0, PC_GETCID, (caddr_t)&ClassInfo); 
  if (rslt < 0) return errno;
  assert(ClassInfo.pc_cid != -1, "cid for IA class is -1");
  iaLimits.schedPolicy = ClassInfo.pc_cid;
  iaLimits.maxPrio = ((iainfo_t*)ClassInfo.pc_clinfo)->ia_maxupri;
  iaLimits.minPrio = -iaLimits.maxPrio;

  strcpy(ClassInfo.pc_clname, "RT");
  ClassInfo.pc_cid = -1;
  rslt = (*priocntl_ptr)(PC_VERSION, P_ALL, 0, PC_GETCID, (caddr_t)&ClassInfo); 
  if (rslt < 0) return errno;
  assert(ClassInfo.pc_cid != -1, "cid for RT class is -1");
  rtLimits.schedPolicy = ClassInfo.pc_cid;
  rtLimits.maxPrio = ((rtinfo_t*)ClassInfo.pc_clinfo)->rt_maxpri;
  rtLimits.minPrio = 0;

    
  // Query our "current" scheduling class.  
  // This will normally be IA,TS or, rarely, RT. 
  memset (&ParmInfo, 0, sizeof(ParmInfo)); 
  ParmInfo.pc_cid = PC_CLNULL;
  rslt = (*priocntl_ptr) (PC_VERSION, P_PID, P_MYID, PC_GETPARMS, (caddr_t)&ParmInfo );
  if ( rslt < 0 ) return errno; 
  myClass = ParmInfo.pc_cid;

  // We now know our scheduling classId, get specific information
  // the class.
  ClassInfo.pc_cid = myClass;
  ClassInfo.pc_clname[0] = 0;
  rslt = (*priocntl_ptr) (PC_VERSION, (idtype)0, 0, PC_GETCLINFO, (caddr_t)&ClassInfo );
  if ( rslt < 0 ) return errno; 
    
  if (ThreadPriorityVerbose) 
    tty->print_cr ("lwp_priocntl_init: Class=%d(%s)...", myClass, ClassInfo.pc_clname);

  memset(&ParmInfo, 0, sizeof(pcparms_t));
  ParmInfo.pc_cid = PC_CLNULL;
  rslt = (*priocntl_ptr)(PC_VERSION, P_PID, P_MYID, PC_GETPARMS, (caddr_t)&ParmInfo);   
  if (rslt < 0) return errno; 

  if (ParmInfo.pc_cid == rtLimits.schedPolicy) {
    myMin = rtLimits.minPrio; 
    myMax = rtLimits.maxPrio; 
  } else if (ParmInfo.pc_cid == iaLimits.schedPolicy) {
    iaparms_t *iaInfo  = (iaparms_t*)ParmInfo.pc_clparms;
    myMin = iaLimits.minPrio; 
    myMax = iaLimits.maxPrio; 
    myMax = MIN2(myMax, (int)iaInfo->ia_uprilim); 	// clamp - restrict
  } else if (ParmInfo.pc_cid == tsLimits.schedPolicy) {
    tsparms_t *tsInfo  = (tsparms_t*)ParmInfo.pc_clparms; 
    myMin = tsLimits.minPrio; 
    myMax = tsLimits.maxPrio; 
    myMax = MIN2(myMax, (int)tsInfo->ts_uprilim); 	// clamp - restrict
  } else {
    // No clue - punt
    if (ThreadPriorityVerbose) 
      tty->print_cr ("Unknown scheduling class: %s ... \n", ClassInfo.pc_clname); 
    return EINVAL; 	// no clue, punt
  }

  if (ThreadPriorityVerbose) 
	tty->print_cr ("Thread priority Range: [%d..%d]\n", myMin, myMax); 

  priocntl_enable = true;  // Enable changing priorities
  return 0;
}

#define	IAPRI(x)	((iaparms_t *)((x).pc_clparms))
#define	RTPRI(x)	((rtparms_t *)((x).pc_clparms))
#define	TSPRI(x)	((tsparms_t *)((x).pc_clparms))	


// scale_to_lwp_priority 
// 
// Convert from the libthread "thr_setprio" scale to our current 
// lwp scheduling class scale.
//
static
int	scale_to_lwp_priority (int rMin, int rMax, int x)
{
  int v; 

  if (x == 127) return rMax; 		// avoid round-down 
    v = (((x*(rMax-rMin)))/128)+rMin; 
  return v; 
}


// set_lwp_priority
// 
// Set the priority of the lwp.  This call should only be made
// when using bound threads (T2 threads are bound by default).
//
int	set_lwp_priority (int ThreadID, int lwpid, int newPrio ) 
{
  int rslt; 
  int Actual, Expected, prv; 
  pcparms_t ParmInfo; 			// for GET-SET
#ifdef ASSERT
  pcparms_t ReadBack;			// for readback
#endif

  // Set priority via PC_GETPARMS, update, PC_SETPARMS
  // Query current values.
  // TODO: accelerate this by eliminating the PC_GETPARMS call.
  // Cache "pcparms_t" in global ParmCache.  
  // TODO: elide set-to-same-value

  // If something went wrong on init, don't change priorities.
  if ( !priocntl_enable ) { 
    if (ThreadPriorityVerbose) 
      tty->print_cr("Trying to set priority but init failed, ignoring");
    return EINVAL;
  }


  // If lwp hasn't started yet, just return
  // the _start routine will call us again.
  if ( lwpid <= 0 ) {
    if (ThreadPriorityVerbose) {
      tty->print_cr ("deferring the set_lwp_priority of thread " INTPTR_FORMAT " to %d, lwpid not set",
                     ThreadID, newPrio); 
    }
    return 0;
  }

  if (ThreadPriorityVerbose) {
    tty->print_cr ("set_lwp_priority(" INTPTR_FORMAT "@" INTPTR_FORMAT " %d) ",
                   ThreadID, lwpid, newPrio); 
  }

  memset(&ParmInfo, 0, sizeof(pcparms_t));
  ParmInfo.pc_cid = PC_CLNULL;
  rslt = (*priocntl_ptr)(PC_VERSION, P_LWPID, lwpid, PC_GETPARMS, (caddr_t)&ParmInfo); 	
  if (rslt < 0) return errno; 

  if (ParmInfo.pc_cid == rtLimits.schedPolicy) {
    rtparms_t *rtInfo  = (rtparms_t*)ParmInfo.pc_clparms;
    rtInfo->rt_pri     = scale_to_lwp_priority (rtLimits.minPrio, rtLimits.maxPrio, newPrio);
    rtInfo->rt_tqsecs  = RT_NOCHANGE; 
    rtInfo->rt_tqnsecs = RT_NOCHANGE; 
    if (ThreadPriorityVerbose) {
      tty->print_cr("RT: %d->%d\n", newPrio, rtInfo->rt_pri); 
    }
  } else if (ParmInfo.pc_cid == iaLimits.schedPolicy) {
    iaparms_t *iaInfo  = (iaparms_t*)ParmInfo.pc_clparms;
    int maxClamped     = MIN2(iaLimits.maxPrio, (int)iaInfo->ia_uprilim);
    iaInfo->ia_upri    = scale_to_lwp_priority(iaLimits.minPrio, maxClamped, newPrio);
    iaInfo->ia_uprilim = IA_NOCHANGE; 
    iaInfo->ia_nice    = IA_NOCHANGE; 
    iaInfo->ia_mode    = IA_NOCHANGE; 
    if (ThreadPriorityVerbose) { 
      tty->print_cr ("IA: [%d...%d] %d->%d\n", 
               iaLimits.minPrio, maxClamped, newPrio, iaInfo->ia_upri); 
    }
  } else if (ParmInfo.pc_cid == tsLimits.schedPolicy) {
    tsparms_t *tsInfo  = (tsparms_t*)ParmInfo.pc_clparms; 
    int maxClamped     = MIN2(tsLimits.maxPrio, (int)tsInfo->ts_uprilim);
    prv                = tsInfo->ts_upri; 
    tsInfo->ts_upri    = scale_to_lwp_priority(tsLimits.minPrio, maxClamped, newPrio);
    tsInfo->ts_uprilim = IA_NOCHANGE; 
    if (ThreadPriorityVerbose) { 
      tty->print_cr ("TS: %d [%d...%d] %d->%d\n", 
               prv, tsLimits.minPrio, maxClamped, newPrio, tsInfo->ts_upri); 
    }
    if (prv == tsInfo->ts_upri) return 0; 
  } else {
    if ( ThreadPriorityVerbose ) {
      tty->print_cr ("Unknown scheduling class\n"); 
    }
      return EINVAL; 	// no clue, punt
  }

  rslt = (*priocntl_ptr)(PC_VERSION, P_LWPID, lwpid, PC_SETPARMS, (caddr_t)&ParmInfo); 
  if (ThreadPriorityVerbose && rslt) { 
    tty->print_cr ("PC_SETPARMS ->%d %d\n", rslt, errno); 
  }
  if (rslt < 0) return errno; 

#ifdef ASSERT
  // Sanity check: read back what we just attempted to set. 
  // In theory it could have changed in the interim ...
  //
  // The priocntl system call is tricky.
  // Sometimes it'll validate the priority value argument and
  // return EINVAL if unhappy.  At other times it fails silently.
  // Readbacks are prudent.

  if (!ReadBackValidate) return 0; 

  memset(&ReadBack, 0, sizeof(pcparms_t));
  ReadBack.pc_cid = PC_CLNULL;
  rslt = (*priocntl_ptr)(PC_VERSION, P_LWPID, lwpid, PC_GETPARMS, (caddr_t)&ReadBack);
  assert(rslt >= 0, "priocntl failed");
  Actual = Expected = 0xBAD; 
  assert(ParmInfo.pc_cid == ReadBack.pc_cid, "cid's don't match");
  if (ParmInfo.pc_cid == rtLimits.schedPolicy) {  
    Actual   = RTPRI(ReadBack)->rt_pri;  
    Expected = RTPRI(ParmInfo)->rt_pri; 
  } else if (ParmInfo.pc_cid == iaLimits.schedPolicy) {
    Actual   = IAPRI(ReadBack)->ia_upri;  
    Expected = IAPRI(ParmInfo)->ia_upri; 
  } else if (ParmInfo.pc_cid == tsLimits.schedPolicy) {
    Actual   = TSPRI(ReadBack)->ts_upri;  
    Expected = TSPRI(ParmInfo)->ts_upri; 
  } else {
    if ( ThreadPriorityVerbose ) {
      tty->print_cr("set_lwp_priority: unexpected class in readback: %d\n", ParmInfo.pc_cid); 
    }
  }

  if (Actual != Expected) { 
    if ( ThreadPriorityVerbose ) {
      tty->print_cr ("set_lwp_priority(%d %d) Class=%d: actual=%d vs expected=%d\n",
             lwpid, newPrio, ReadBack.pc_cid, Actual, Expected); 
    }
  }
#endif
			
  return 0;
}



// Solaris only gives access to 128 real priorities at a time,
// so we expand Java's ten to fill this range.  This would be better
// if we dynamically adjusted relative priorities. 
//
// The ThreadPriorityPolicy option allows us to select 2 different
// priority scales.
//
// ThreadPriorityPolicy=0
// Since the Solaris' default priority is MaximumPriority, we do not  
// set a priority lower than Max unless a priority lower than
// NormPriority is requested.
// 
// ThreadPriorityPolicy=1
// This mode causes the priority table to get filled with  
// linear values.  NormPriority get's mapped to 50% of the
// Maximum priority an so on.  This will cause VM threads
// to get unfair treatment against other Solaris processes
// which do not explicitly alter their thread priorities.
// 


int os::java_to_os_priority[MaxPriority + 1] = {
  -99999,         // 0 Entry should never be used

  0,              // 1 MinPriority
  32,             // 2
  64,             // 3

  96,             // 4
  127,            // 5 NormPriority  
  127,            // 6   

  127,            // 7
  127,            // 8
  127,            // 9 NearMaxPriority

  127             // 10 MaxPriority
};


OSReturn os::set_native_priority(Thread* thread, int newpri) {
  assert(newpri >= MinimumPriority && newpri <= MaximumPriority, "bad priority mapping");
  if ( !UseThreadPriorities ) return OS_OK;
  int status = thr_setprio(thread->osthread()->thread_id(), newpri);
  if ( os::Solaris::T2_libthread() || (UseBoundThreads && thread->osthread()->is_vm_created()) ) 
    status |= (set_lwp_priority (thread->osthread()->thread_id(), 
                    thread->osthread()->lwp_id(), newpri ));
  return (status == 0) ? OS_OK : OS_ERR;
}


OSReturn os::get_native_priority(const Thread* const thread, int *priority_ptr) {
  int p;
  if ( !UseThreadPriorities ) {
    *priority_ptr = NormalPriority;
    return OS_OK;
  }
  int status = thr_getprio(thread->osthread()->thread_id(), &p);
  if (status != 0) {
    return OS_ERR;
  }
  *priority_ptr = p;
  return OS_OK;
}


// Hint to the underlying OS that a task switch would not be good.
// Void return because it's a hint and can fail.
void os::hint_no_preempt() {
  schedctl_start(schedctl_init());
}

void os::interrupt(Thread* thread) {
  assert(Thread::current() == thread || Threads_lock->owned_by_self(), "possibility of dangling Thread pointer");

  OSThread* osthread = thread->osthread();

  int isInterrupted = osthread->interrupted();
  if (!isInterrupted) {
      osthread->set_interrupted(true);
      OrderAccess::fence();
      osthread->interrupt_event()->unpark();
  } 
  
  // For JSR166:  unpark after setting status but before thr_kill -dl
  if (thread->is_Java_thread()) {
    ((JavaThread*)thread)->parker()->unpark();
  }

  if (!isInterrupted) {
    int status = thr_kill(osthread->thread_id(), os::Solaris::SIGinterrupt());
    assert(status == 0, "thr_kill != 0");
  } 
}    


bool os::is_interrupted(Thread* thread, bool clear_interrupted) {
  assert(Thread::current() == thread || Threads_lock->owned_by_self(), "possibility of dangling Thread pointer");

  OSThread* osthread = thread->osthread();

  bool res = osthread->interrupted();

  // NOTE that since there is no "lock" around these two operations,
  // there is the possibility that the interrupted flag will be
  // "false" but that the interrupt event will be set. This is
  // intentional. The effect of this is that Object.wait() will appear
  // to have a spurious wakeup, which is not harmful, and the
  // possibility is so rare that it is not worth the added complexity
  // to add yet another lock. It has also been recommended not to put
  // the interrupted flag into the os::Solaris::Event structure,
  // because it hides the issue.
  if (res && clear_interrupted) {
    osthread->set_interrupted(false);
    osthread->interrupt_event()->reset();
  }
  return res;
}

#ifndef CORE
// Use signals to set up pc for a suspended thread. Only used for compiled
// safepoint.
bool os::set_thread_pc_and_resume(JavaThread* thread, ExtendedPC old_addr, ExtendedPC new_addr) {
  assert(thread->is_vm_suspended(), "must be suspended");
  assert(thread->is_in_compiled_safepoint(), "only for compiled safepoint");

  bool result;
  const int time_to_wait = 100; // 100ms wait for initial response.
  OSThread* osthread = thread->osthread();
  SetThreadPC_Callback cb(old_addr, new_addr, Interrupt_lock);

  // true - target thread is suspended
  int status = cb.interrupt(thread, true, time_to_wait);

  if (cb.is_done() ) {
    result = cb.result();
  } else {
    DEBUG_ONLY(tty->print_cr("Failed to set pc for thread: %d got %d status", 
                              osthread->thread_id(), status);)
    result = false;
  }
  return result;
}
#endif

ExtendedPC os::fetch_top_frame(Thread* thread, intptr_t** ret_younger_sp, intptr_t** ret_sp) {
  assert(ProfilerLight || thread->is_vm_suspended(), "must be suspended");
  
  return os::Solaris::fetch_top_frame_fast(thread, ret_younger_sp, ret_sp);
}


void os::print_statistics() {
}

int os::message_box(const char* title, const char* message) {
  int i;
  fdStream err(defaultStream::error_fd());
  for (i = 0; i < 78; i++) err.print_raw("=");
  err.cr();
  err.print_raw_cr(title);
  for (i = 0; i < 78; i++) err.print_raw("-");
  err.cr();
  err.print_raw_cr(message);
  for (i = 0; i < 78; i++) err.print_raw("=");
  err.cr();

  char buf[16];
  // Prevent process from exiting upon "read error" without consuming all CPU
  while (::read(0, buf, sizeof(buf)) <= 0) { ::sleep(100); }

  return buf[0] == 'y' || buf[0] == 'Y';
}

// A lightweight implementation that does not suspend the target thread and
// thus returns only a hint. Used for profiling only!
ExtendedPC os::get_thread_pc(Thread* thread) {
  // Make sure that it is called by the watcher and the Threads lock is owned.
  assert(Thread::current()->is_Watcher_thread(), "Must be watcher and own Threads_lock");
  // For now, is only used to profile the VM Thread
  assert(thread->is_VM_thread(), "Can only be called for VMThread");
  ExtendedPC epc;

  GetThreadPC_Callback  cb(ProfileVM_lock); 
  OSThread *osthread = thread->osthread();
  const int time_to_wait = 400; // 400ms wait for initial response
  int status = cb.interrupt(thread, false, time_to_wait);   // target is not suspended

  if (cb.is_done() ) {
    epc = cb.addr();
  } else {
    DEBUG_ONLY(tty->print_cr("Failed to get pc for thread: %d got %d status", 
                              osthread->thread_id(), status););
    // epc is already NULL
  }
  return epc;
}  


// This does not do anything on Solaris. This is basically a hook for being
// able to use structured exception handling (thread-local exception filters) on, e.g., Win32.
void os::os_exception_wrapper(java_call_t f, JavaValue* value, methodHandle* method, JavaCallArguments* args, Thread* thread) {
  f(value, method, args, thread);  
}

// This routine may be used by user applications as a "hook" to catch signals.
// The user-defined signal handler must pass unrecognized signals to this
// routine, and if it returns true (non-zero), then the signal handler must
// return immediately.  If the flag "abort_if_unrecognized" is true, then this
// routine will never retun false (zero), but instead will execute a VM panic
// routine kill the process.
//
// If this routine returns false, it is OK to call it again.  This allows
// the user-defined signal handler to perform checks either before or after
// the VM performs its own checks.  Naturally, the user code would be making
// a serious error if it tried to handle an exception (such as a null check
// or breakpoint) that the VM was generating for its own correct operation.
//
// This routine may recognize any of the following kinds of signals:
// SIGBUS, SIGSEGV, SIGILL, SIGFPE, BREAK_SIGNAL, SIGPIPE, os::Solaris::SIGasync
// It should be consulted by handlers for any of those signals.
// It explicitly does not recognize os::Solaris::SIGinterrupt
//
// The caller of this routine must pass in the three arguments supplied
// to the function referred to in the "sa_sigaction" (not the "sa_handler")
// field of the structure passed to sigaction().  This routine assumes that
// the sa_flags field passed to sigaction() includes SA_SIGINFO and SA_RESTART.
//
// Note that the VM will print warnings if it detects conflicting signal
// handlers, unless invoked with the option "-XX:+AllowUserSignalHandlers".
//
extern "C" int JVM_handle_solaris_signal(int signo, siginfo_t* siginfo, void* ucontext, int abort_if_unrecognized);


void signalHandler(int sig, siginfo_t* info, void* ucVoid) {
  JVM_handle_solaris_signal(sig, info, ucVoid, true);
}

/* Do not delete - if guarantee is ever removed,  a signal handler (even empty)
   is needed to provoke threads blocked on IO to return an EINTR 
   Note: this explicitly does NOT call JVM_handle_solaris_signal and
   does NOT participate in signal chaining due to requirement for
   NOT setting SA_RESTART to make EINTR work. */
extern "C" void sigINTRHandler(int sig, siginfo_t* info, void* ucVoid) {
   if (UseSignalChaining) {
      struct sigaction *actp = os::Solaris::get_chained_signal_action(sig);
      if (actp && actp->sa_handler) {
        vm_exit_during_initialization("Signal chaining detected for VM interrupt signal, try -XX:+UseAltSigs");
      }
   }
}

// This boolean allows users to forward their own non-matching signals
// to JVM_handle_solaris_signal, harmlessly.
bool os::Solaris::signal_handlers_are_installed = false;

// For signal-chaining
bool os::Solaris::libjsig_is_loaded = false;
typedef struct sigaction *(*get_signal_t)(int);
get_signal_t os::Solaris::get_signal_action = NULL;

struct sigaction* os::Solaris::get_chained_signal_action(int sig) {
  struct sigaction *actp = NULL;
 
  if ((libjsig_is_loaded)  && (sig <= Maxlibjsigsigs)) {
    // Retrieve the old signal handler from libjsig
    actp = (*get_signal_action)(sig);
  }
  if (actp == NULL) {
    // Retrieve the preinstalled signal handler from jvm
    actp = get_preinstalled_handler(sig);
  }

  return actp;
}

bool os::Solaris::chained_handler(struct sigaction *actp, int sig,
                                  siginfo_t *siginfo, void *context) {
  // Call the old signal handler
  if (actp->sa_handler == SIG_DFL) {
    // It's more reasonable to let jvm treat it as an unexpected exception
    // instead of taking the default action.
    return false;
  } else if (actp->sa_handler != SIG_IGN) {
    if ((actp->sa_flags & SA_NODEFER) == 0) {
      // automaticlly block the signal
      sigaddset(&(actp->sa_mask), sig);
    }

    sa_handler_t hand;
    sa_sigaction_t sa;
    bool siginfo_flag_set = (actp->sa_flags & SA_SIGINFO) != 0;
    // retrieve the chained handler
    if (siginfo_flag_set) {
      sa = actp->sa_sigaction;
    } else {
      hand = actp->sa_handler;
    }

    if ((actp->sa_flags & SA_RESETHAND) != 0) {
      actp->sa_handler = SIG_DFL;
    }

    // try to honor the signal mask
    sigset_t oset;
    thr_sigsetmask(SIG_SETMASK, &(actp->sa_mask), &oset);

    // call into the chained handler
    if (siginfo_flag_set) {
      (*sa)(sig, siginfo, context);
    } else {
      (*hand)(sig);
    }

    // restore the signal mask
    thr_sigsetmask(SIG_SETMASK, &oset, 0);
  }
  // Tell jvm's signal handler the signal is taken care of.
  return true;
}

struct sigaction* os::Solaris::get_preinstalled_handler(int sig) {
  assert((chainedsigactions != (struct sigaction *)NULL) && (preinstalled_sigs != (int *)NULL) , "signals not yet initialized");
  if (preinstalled_sigs[sig] != 0) {
    return &chainedsigactions[sig];
  }
  return NULL;
}

void os::Solaris::save_preinstalled_handler(int sig, struct sigaction& oldAct) {
  
  assert(sig > 0 && sig <= Maxsignum, "vm signal out of expected range");
  assert((chainedsigactions != (struct sigaction *)NULL) && (preinstalled_sigs != (int *)NULL) , "signals not yet initialized");
  chainedsigactions[sig] = oldAct;
  preinstalled_sigs[sig] = 1;
}

void os::Solaris::set_signal_handler(int sig, bool set_installed, bool oktochain) {
  // Check for overwrite.
  struct sigaction oldAct;
  sigaction(sig, (struct sigaction*)NULL, &oldAct);
  void* oldhand = oldAct.sa_sigaction ? CAST_FROM_FN_PTR(void*,  oldAct.sa_sigaction)
				      : CAST_FROM_FN_PTR(void*,  oldAct.sa_handler);
  if (oldhand != CAST_FROM_FN_PTR(void*, SIG_DFL) &&
      oldhand != CAST_FROM_FN_PTR(void*, SIG_IGN) &&
      oldhand != CAST_FROM_FN_PTR(void*, signalHandler)) {
    if (AllowUserSignalHandlers || !set_installed) {
      // Do not overwrite; user takes responsibility to forward to us.
      return;
    } else if (UseSignalChaining) {
      if (oktochain) {
        // save the old handler in jvm
        save_preinstalled_handler(sig, oldAct);
      } else {
        vm_exit_during_initialization("Signal chaining not allowed for VM interrupt signal, try -XX:+UseAltSigs.");
      }
      // libjsig also interposes the sigaction() call below and saves the
      // old sigaction on it own.
    } else {
      fatal2("Encountered unexpected pre-existing sigaction handler %#lx for signal %d.", (long)oldhand, sig);
    }
  }

  struct sigaction sigAct;
  sigfillset(&(sigAct.sa_mask));
  sigAct.sa_handler = SIG_DFL;

  sigAct.sa_sigaction = signalHandler;
  // Handle SIGSEGV on alternate signal stack if
  // not using stack banging
  if (!UseStackBanging && sig == SIGSEGV) {
    sigAct.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
  // Interruptible i/o requires SA_RESTART cleared so EINTR 
  // is returned instead of restarting system calls
  } else if (sig == os::Solaris::SIGinterrupt()) {
    memset((char *)&(sigAct.sa_mask), 0, sizeof (sigset_t));
    sigAct.sa_handler = NULL;
    sigAct.sa_flags = SA_SIGINFO;
    sigAct.sa_sigaction = sigINTRHandler;
  } else {
    sigAct.sa_flags = SA_SIGINFO | SA_RESTART;
  }

  sigaction(sig, &sigAct, &oldAct);

  void* oldhand2 = oldAct.sa_sigaction ? CAST_FROM_FN_PTR(void*, oldAct.sa_sigaction)
				       : CAST_FROM_FN_PTR(void*, oldAct.sa_handler);
  assert(oldhand2 == oldhand, "no concurrent signal handler installation");
}


void os::Solaris::install_signal_handlers() {
  bool libjsigdone = false;
  signal_handlers_are_installed = true;

  // signal-chaining
  typedef void (*signal_setting_t)();
  signal_setting_t begin_signal_setting = NULL;
  signal_setting_t end_signal_setting = NULL;
  begin_signal_setting = CAST_TO_FN_PTR(signal_setting_t,
                           dlsym(RTLD_DEFAULT, "JVM_begin_signal_setting"));
  if (begin_signal_setting != NULL) {
    end_signal_setting = CAST_TO_FN_PTR(signal_setting_t,
                           dlsym(RTLD_DEFAULT, "JVM_end_signal_setting"));
    get_signal_action = CAST_TO_FN_PTR(get_signal_t,
                          dlsym(RTLD_DEFAULT, "JVM_get_signal_action"));
    get_libjsig_version = CAST_TO_FN_PTR(version_getting_t, 
                          dlsym(RTLD_DEFAULT, "JVM_get_libjsig_version"));
    libjsig_is_loaded = true;
    if (os::Solaris::get_libjsig_version != NULL) { 
       libjsigversion =  (*os::Solaris::get_libjsig_version)(); 
    }
    assert(UseSignalChaining, "should enable signal-chaining");
  }
  if (libjsig_is_loaded) {
    // Tell libjsig jvm is setting signal handlers
    (*begin_signal_setting)();
  }

  set_signal_handler(SIGSEGV, true, true);
  set_signal_handler(SIGPIPE, true, true);
  set_signal_handler(SIGBUS, true, true);
  set_signal_handler(SIGILL, true, true);
  set_signal_handler(SIGFPE, true, true);


  if (os::Solaris::SIGinterrupt() > OLDMAXSIGNUM || os::Solaris::SIGasync() > OLDMAXSIGNUM) {

    // Pre-1.4.1 Libjsig limited to signal chaining signals <= 32 so
    // can not register overridable signals which might be > 32
    if (libjsig_is_loaded && libjsigversion <= JSIG_VERSION_1_4_1) {
    // Tell libjsig jvm has finished setting signal handlers
      (*end_signal_setting)();
      libjsigdone = true;
    }
  }

  // Never ok to chain our SIGinterrupt
  set_signal_handler(os::Solaris::SIGinterrupt(), true, false);
  set_signal_handler(os::Solaris::SIGasync(), true, true);

  if (libjsig_is_loaded && !libjsigdone) {
    // Tell libjsig jvm finishes setting signal handlers
    (*end_signal_setting)();
  }
}


void report_error(const char* file_name, int line_no, const char* title, const char* format, ...);

const char * signames[] = {
  "SIG0",
  "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP",
  "SIGABRT", "SIGEMT", "SIGFPE", "SIGKILL", "SIGBUS",
  "SIGSEGV", "SIGSYS", "SIGPIPE", "SIGALRM", "SIGTERM",
  "SIGUSR1", "SIGUSR2", "SIGCLD", "SIGPWR", "SIGWINCH",
  "SIGURG", "SIGPOLL", "SIGSTOP", "SIGTSTP", "SIGCONT",
  "SIGTTIN", "SIGTTOU", "SIGVTALRM", "SIGPROF", "SIGXCPU",
  "SIGXFSZ", "SIGWAITING", "SIGLWP", "SIGFREEZE", "SIGTHAW",
  "SIGCANCEL", "SIGLOST"
};

const char * os::exception_name(int exception_code, char* buf, int size) {
  if (0 < exception_code && exception_code <= SIGRTMAX) {
     // signal
     if (exception_code < sizeof(signames)/sizeof(const char*)) {
        jio_snprintf(buf, size, "%s", signames[exception_code]);
     } else {
        jio_snprintf(buf, size, "SIG%d", exception_code);
     }
     return buf;
  } else {
     return NULL;
  }
}

// (Static) wrappers for the new libthread API
int_fnP_thread_t_iP_uP_stack_tP_gregset_t os::Solaris::_thr_getstate;
int_fnP_thread_t_i_gregset_t os::Solaris::_thr_setstate;
int_fnP_thread_t_i os::Solaris::_thr_setmutator;
int_fnP_thread_t os::Solaris::_thr_suspend_mutator;
int_fnP_thread_t os::Solaris::_thr_continue_mutator;

static address resolve_symbol(const char *name) {
  address addr;

  addr = (address) dlsym(RTLD_DEFAULT, name);
  if(addr == NULL) {
    // RTLD_DEFAULT was not defined on some early versions of 2.5.1
    addr = (address) dlsym(RTLD_NEXT, name);    
    if(addr == NULL) {
      fatal(dlerror());
    }
  }
  return addr;
}



// isT2_libthread()
//
// Routine to determine if we are currently using the new T2 libthread.
//
// We determine if we are using T2 by reading /proc/self/lstatus and
// looking for a thread with the ASLWP bit set.  If we find this status
// bit set, we must assume that we are NOT using T2.  The T2 team
// has approved this algorithm.
//
// We need to determine if we are running with the new T2 libthread
// since setting native thread priorities is handled differently 
// when using this library.  All threads created using T2 are bound 
// threads. Calling thr_setprio is meaningless in this case. 
//
bool isT2_libthread() {
  int i, rslt;
  static prheader_t * lwpArray = NULL;             
  static int lwpSize = 0;
  static int lwpFile = -1;
  lwpstatus_t * that;
  int aslwpcount;
  char lwpName [128];
  bool isT2 = false;

#define ADR(x)  ((uintptr_t)(x))
#define LWPINDEX(ary,ix)   ((lwpstatus_t *)(((ary)->pr_entsize * (ix)) + (ADR((ary) + 1))))

  aslwpcount = 0;
  lwpSize = 16*1024;
  lwpArray = ( prheader_t *)NEW_C_HEAP_ARRAY (char, lwpSize);
  lwpFile = open ("/proc/self/lstatus", O_RDONLY, 0);
  if (lwpArray == NULL) {
      if ( ThreadPriorityVerbose ) warning ("Couldn't allocate T2 Check array\n");
      return(isT2);
  }
  if (lwpFile < 0) {
      if ( ThreadPriorityVerbose ) warning ("Couldn't open /proc/self/lstatus\n");
      return(isT2);
  }
  for (;;) {
    lseek (lwpFile, 0, SEEK_SET);
    rslt = read (lwpFile, lwpArray, lwpSize);
    if ((lwpArray->pr_nent * lwpArray->pr_entsize) <= lwpSize) {
      break;
    }
    FREE_C_HEAP_ARRAY(char, lwpArray);
    lwpSize = lwpArray->pr_nent * lwpArray->pr_entsize;
    lwpArray = ( prheader_t *)NEW_C_HEAP_ARRAY (char, lwpSize);
    if (lwpArray == NULL) {
        if ( ThreadPriorityVerbose ) warning ("Couldn't allocate T2 Check array\n");
        return(isT2);
    }
  }

  // We got a good snapshot - now iterate over the list.
  for (i = 0; i < lwpArray->pr_nent; i++ ) {
    that = LWPINDEX(lwpArray,i);
    if (that->pr_flags & PR_ASLWP) {
      aslwpcount++;
    }
  }
  if ( aslwpcount == 0 ) isT2 = true;

  FREE_C_HEAP_ARRAY(char, lwpArray);
  close (lwpFile);
  if ( ThreadPriorityVerbose ) {
    if ( isT2 ) tty->print_cr("We are running with a T2 libthread\n");
    else tty->print_cr("We are not running with a T2 libthread\n");
  }
  return (isT2);
}


void os::Solaris::libthread_init() {
  address func = (address)dlsym(RTLD_DEFAULT, "_thr_suspend_allmutators");

  // Determine if we are running with the new T2 libthread
  os::Solaris::set_T2_libthread(isT2_libthread());

  lwp_priocntl_init();

  // RTLD_DEFAULT was not defined on some early versions of 5.5.1
  if(func == NULL) {
    func = (address) dlsym(RTLD_NEXT, "_thr_suspend_allmutators");  
    // Guarantee that this VM is running on an new enough OS (5.6 or
    // later) that it will have a new enough libthread.so.
    guarantee(func != NULL, "libthread.so is too old.");
  }

  // Initialize the new libthread getstate API wrappers
  func = resolve_symbol("thr_getstate");
  os::Solaris::set_thr_getstate(CAST_TO_FN_PTR(int_fnP_thread_t_iP_uP_stack_tP_gregset_t, func));

  func = resolve_symbol("thr_setstate");
  os::Solaris::set_thr_setstate(CAST_TO_FN_PTR(int_fnP_thread_t_i_gregset_t, func));
    
  func = resolve_symbol("thr_setmutator");
  os::Solaris::set_thr_setmutator(CAST_TO_FN_PTR(int_fnP_thread_t_i, func));

  func = resolve_symbol("thr_suspend_mutator");
  os::Solaris::set_thr_suspend_mutator(CAST_TO_FN_PTR(int_fnP_thread_t, func));

  func = resolve_symbol("thr_continue_mutator");
  os::Solaris::set_thr_continue_mutator(CAST_TO_FN_PTR(int_fnP_thread_t, func));

  int size;
  void (*handler_info_func)(address *, int *);
  handler_info_func = CAST_TO_FN_PTR(void (*)(address *, int *), resolve_symbol("thr_sighndlrinfo"));
  handler_info_func(&handler_start, &size);
  handler_end = handler_start + size;
}


int_fnP_mutex_tP os::Solaris::_mutex_lock;
int_fnP_mutex_tP os::Solaris::_mutex_trylock;
int_fnP_mutex_tP os::Solaris::_mutex_unlock;
int_fnP_mutex_tP_i_vP os::Solaris::_mutex_init;
int_fnP_mutex_tP os::Solaris::_mutex_destroy;
int os::Solaris::_mutex_scope = USYNC_THREAD;

int_fnP_cond_tP_mutex_tP_timestruc_tP os::Solaris::_cond_timedwait;
int_fnP_cond_tP_mutex_tP os::Solaris::_cond_wait;
int_fnP_cond_tP os::Solaris::_cond_signal;
int_fnP_cond_tP os::Solaris::_cond_broadcast;
int_fnP_cond_tP_i_vP os::Solaris::_cond_init;
int_fnP_cond_tP os::Solaris::_cond_destroy;
int os::Solaris::_cond_scope = USYNC_THREAD;

void os::Solaris::synchronization_init() {
  if(UseLWPSynchronization) {
    os::Solaris::set_mutex_lock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("_lwp_mutex_lock")));
    os::Solaris::set_mutex_trylock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("_lwp_mutex_trylock")));
    os::Solaris::set_mutex_unlock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("_lwp_mutex_unlock")));
    os::Solaris::set_mutex_init(lwp_mutex_init);
    os::Solaris::set_mutex_destroy(lwp_mutex_destroy);
    os::Solaris::set_mutex_scope(USYNC_PROCESS);
    
    os::Solaris::set_cond_timedwait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP_timestruc_tP, resolve_symbol("_lwp_cond_timedwait")));
    os::Solaris::set_cond_wait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP, resolve_symbol("_lwp_cond_wait")));
    os::Solaris::set_cond_signal(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("_lwp_cond_signal")));
    os::Solaris::set_cond_broadcast(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("_lwp_cond_broadcast")));
    os::Solaris::set_cond_init(lwp_cond_init);        
    os::Solaris::set_cond_destroy(lwp_cond_destroy);        
    os::Solaris::set_cond_scope(USYNC_PROCESS);
  }
  else {
    os::Solaris::set_mutex_scope(USYNC_THREAD);
    os::Solaris::set_cond_scope(USYNC_THREAD);

    if(UsePthreads) {
      os::Solaris::set_mutex_lock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("pthread_mutex_lock")));
      os::Solaris::set_mutex_trylock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("pthread_mutex_trylock")));
      os::Solaris::set_mutex_unlock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("pthread_mutex_unlock")));
      os::Solaris::set_mutex_init(pthread_mutex_default_init);
      os::Solaris::set_mutex_destroy(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("pthread_mutex_destroy")));
      
      os::Solaris::set_cond_timedwait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP_timestruc_tP, resolve_symbol("pthread_cond_timedwait")));
      os::Solaris::set_cond_wait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP, resolve_symbol("pthread_cond_wait")));
      os::Solaris::set_cond_signal(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("pthread_cond_signal")));
      os::Solaris::set_cond_broadcast(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("pthread_cond_broadcast")));
      os::Solaris::set_cond_init(pthread_cond_default_init);    
      os::Solaris::set_cond_destroy(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("pthread_cond_destroy")));
    }   
    else {
      os::Solaris::set_mutex_lock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("mutex_lock")));
      os::Solaris::set_mutex_trylock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("mutex_trylock")));
      os::Solaris::set_mutex_unlock(CAST_TO_FN_PTR(int_fnP_mutex_tP, resolve_symbol("mutex_unlock")));
      os::Solaris::set_mutex_init(::mutex_init);
      os::Solaris::set_mutex_destroy(::mutex_destroy);
      
      os::Solaris::set_cond_timedwait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP_timestruc_tP, resolve_symbol("cond_timedwait")));
      os::Solaris::set_cond_wait(CAST_TO_FN_PTR(int_fnP_cond_tP_mutex_tP, resolve_symbol("cond_wait")));
      os::Solaris::set_cond_signal(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("cond_signal")));
      os::Solaris::set_cond_broadcast(CAST_TO_FN_PTR(int_fnP_cond_tP, resolve_symbol("cond_broadcast")));
      os::Solaris::set_cond_init(::cond_init);    
      os::Solaris::set_cond_destroy(::cond_destroy);        
    }
  }
}

int os::Solaris::_dev_zero_fd = -1;

// this is called _before_ the global arguments have been parsed
void os::init(void) {
  _initial_pid = getpid();

  max_hrtime = first_hrtime = gethrtime();

  init_random(1234567);

  page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1)
    fatal1("os_solaris.cpp: os::init: sysconf failed (%s)", strerror(errno));

  Solaris::initialize_system_info();

  int fd = open("/dev/zero", O_RDWR);
  if (fd < 0) {
    fatal1("os::init: cannot open /dev/zero (%s)", strerror(errno));
  } else {
    Solaris::set_dev_zero_fd(fd);

    // Close on exec, child won't inherit.
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  }

  // check if dladdr1() exists; dladdr1 can provide more information than
  // dladdr for os::dll_address_to_function_name. It comes with SunOS 5.9 
  // and is available on linker patches for 5.7 and 5.8.
  // libdl.so must have been loaded, this call is just an entry lookup
  void * hdl = dlopen("libdl.so", RTLD_NOW);
  if (hdl) 
    dladdr1_func = CAST_TO_FN_PTR(dladdr1_func_type, dlsym(hdl, "dladdr1"));

  // (Solaris only) this switches to calls that actually do locking. 
  ThreadCritical::initialize(); 

  main_thread = thr_self();

  // Constant minimum stack size allowed. It must be at least
  // the minimum of what the OS supports (thr_min_stack()), and
  // enough to allow the thread to get to user bytecode execution.
  Solaris::min_stack_allowed = MAX2(thr_min_stack(), Solaris::min_stack_allowed);
  // If the pagesize of the VM is greater than 8K determine the appropriate
  // number of initial guard pages.  The user can change this with the
  // command line arguments, if needed.
  if (vm_page_size() > 8*K) {
    StackYellowPages = 1;
    StackRedPages = 1;
    StackShadowPages = round_to((StackShadowPages*8*K), vm_page_size());
  }
}

// To install functions for atexit system call
extern "C" {
  static void perfMemory_exit_helper() {
    perfMemory_exit();
  }
}

// this is called _after_ the global arguments have been parsed
jint os::init_2(void) {
  
  // Allocate a single page and mark it as readable for safepoint polling
  if( SafepointPolling ) {
    address polling_page = (address)Solaris::mmap_chunk( NULL, page_size, MAP_PRIVATE, PROT_READ );
    os::set_polling_page( polling_page );

#ifndef PRODUCT
    if( Verbose )
      tty->print("[SafePoint Polling address: " INTPTR_FORMAT "]\n", (intptr_t)polling_page);
#endif
  }

  // Check to make sure we can use MPSS on this version of OS
  if (UseMPSS) {
    Solaris::mpss_sanity_check();
  }      

  // Check minimum allowable stack size for thread creation and to initialize
  // the java system classes, including StackOverflowError - depends on page
  // size.  Add a page for compiler2 recursion in main thread.
  // Add in BytesPerWord times page size to account for VM stack during
  // class initialization depending on 32 or 64 bit VM.
  guarantee((Solaris::min_stack_allowed >=
    (StackYellowPages+StackRedPages+StackShadowPages+BytesPerWord
     COMPILER2_ONLY(+1)) * page_size),
    "need to increase Solaris::min_stack_allowed on this platform");

  size_t threadStackSizeInBytes = ThreadStackSize * K;
  if (threadStackSizeInBytes != 0 &&
    threadStackSizeInBytes < Solaris::min_stack_allowed) {
    tty->print_cr("\nThe stack size specified is too small, Specify at least %dk",
		  Solaris::min_stack_allowed/K);
    return JNI_ERR;
  }

  // For Solaris 10+, there will be a 64kb page size, which makes 
  // the usable default stack size quite a bit less.  Increase the
  // stack for 64kb (or any > than 8kb) pages, this increases
  // virtual memory fragmentation (since we're not creating the
  // stack on a power of 2 boundary.  The real fix for this
  // should be to fix the guard page mechanism.

  if (vm_page_size() > 8*K) {
      threadStackSizeInBytes = (threadStackSizeInBytes != 0) 
         ? threadStackSizeInBytes + 
           ((StackYellowPages + StackRedPages) * vm_page_size())
	 : 0;
      ThreadStackSize = threadStackSizeInBytes/K;
  }

  // Make the stack size a multiple of the page size so that
  // the yellow/red zones can be guarded.
  JavaThread::set_stack_size_at_create(round_to(threadStackSizeInBytes,
        vm_page_size()));

  Solaris::libthread_init();
  Solaris::signal_sets_init();
  Solaris::init_signal_mem();
  if (UseTopLevelExceptionFilter) {
    Solaris::install_signal_handlers();
  }
  if (libjsigversion < JSIG_VERSION_1_4_1) {
    Maxlibjsigsigs = OLDMAXSIGNUM;
  }

  // initialize synchronization primitives to use either thread or
  // lwp synchronization (controlled by UseLWPSynchronization)
  Solaris::synchronization_init();

  if (MaxFDLimit) {
    // set the number of file descriptors to max. print out error 
    // if getrlimit/setrlimit fails but continue regardless. 
    struct rlimit nbr_files;
    int status = getrlimit(RLIMIT_NOFILE, &nbr_files);
    if (status != 0) {
      if (PrintMiscellaneous && (Verbose || WizardMode))
        perror("os::init_2 getrlimit failed");
    } else {
      nbr_files.rlim_cur = nbr_files.rlim_max;
      status = setrlimit(RLIMIT_NOFILE, &nbr_files);
      if (status != 0) {
        if (PrintMiscellaneous && (Verbose || WizardMode))
          perror("os::init_2 setrlimit failed");
      }
    }
  }

  // Initialize HPI.
  jint hpi_result = hpi::initialize();
  if (hpi_result != JNI_OK) {
    tty->print_cr("There was an error trying to initialize the HPI library.");
    tty->print_cr("Please check your installation, HotSpot does not work correctly");
    tty->print_cr("when installed in the JDK 1.2 Solaris Production Release, or");
    tty->print_cr("with any JDK 1.1.x release.");
    return hpi_result;
  }

  // Calculate theoretical max. size of Threads to guard gainst
  // artifical out-of-memory situations, where all available address-
  // space has been reserved by thread stacks. Default stack size is 1Mb.
  size_t pre_thread_stack_size = (JavaThread::stack_size_at_create()) ?
    JavaThread::stack_size_at_create() : (1*K*K);
  assert(pre_thread_stack_size != 0, "Must have a stack");
  // Solaris has a maximum of 4Gb of user programs. Calculate the thread limit when
  // we should start doing Virtual Memory banging. Currently when the threads will
  // have used all but 200Mb of space.
  size_t max_address_space = ((unsigned int)4 * K * K * K) - (200 * K * K);
  Solaris::_os_thread_limit = max_address_space / pre_thread_stack_size;

  // at-exit methods are called in the reverse order of their registration.
  // In Solaris 7 and earlier, atexit functions are called on return from
  // main or as a result of a call to exit(3C). There can be only 32 of
  // these functions registered and atexit() does not set errno. In Solaris
  // 8 and later, there is no limit to the number of functions registered
  // and atexit() sets errno. In addition, in Solaris 8 and later, atexit
  // functions are called upon dlclose(3DL) in addition to return from main
  // and exit(3C).

  if (PerfAllowAtExitRegistration) {
    // only register atexit functions if PerfAllowAtExitRegistration is set.
    // atexit functions can be delayed until process exit time, which
    // can be problematic for embedded VM situations. Embedded VMs should
    // call DestroyJavaVM() to assure that VM resources are released.

    // note: perfMemory_exit_helper atexit function may be removed in
    // the future if the appropriate cleanup code can be added to the
    // VM_Exit VMOperation's doit method.
    if (atexit(perfMemory_exit_helper) != 0) {
      warning("os::init2 atexit(perfMemory_exit_helper) failed");
    }
  }

  return JNI_OK;
}


// Mark the polling page as unreadable
void os::make_polling_page_unreadable(void) {
  if( !SafepointPolling )
    return;

  if( mprotect((char *)_polling_page, page_size, PROT_NONE) != 0 )
    fatal("Could not disable polling page");
};

// Mark the polling page as readable
void os::make_polling_page_readable(void) {
  if( !SafepointPolling )
    return;

  if( mprotect((char *)_polling_page, page_size, PROT_READ) != 0 )
    fatal("Could not enable polling page");
};

// OS interface.

int os::stat(const char *path, struct stat *sbuf) {
  char pathbuf[MAX_PATH];
  if (strlen(path) > MAX_PATH - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  hpi::native_path(strcpy(pathbuf, path));
  return ::stat(pathbuf, sbuf);
}


bool os::check_heap(bool force) { return true; }

typedef int (*vsnprintf_t)(char* buf, size_t count, const char* fmt, va_list argptr);
static vsnprintf_t sol_vsnprintf = NULL;

int local_vsnprintf(char* buf, size_t count, const char* fmt, va_list argptr) {
  if (!sol_vsnprintf) {
    //search  for the named symbol in the objects that were loaded after libjvm
    void* where = RTLD_NEXT;
    if ((sol_vsnprintf = CAST_TO_FN_PTR(vsnprintf_t, dlsym(where, "__vsnprintf"))) == NULL)
	sol_vsnprintf = CAST_TO_FN_PTR(vsnprintf_t, dlsym(where, "vsnprintf"));
    if (!sol_vsnprintf){
      //search  for the named symbol in the objects that were loaded before libjvm
      where = RTLD_DEFAULT;
      if ((sol_vsnprintf = CAST_TO_FN_PTR(vsnprintf_t, dlsym(where, "__vsnprintf"))) == NULL)
	sol_vsnprintf = CAST_TO_FN_PTR(vsnprintf_t, dlsym(where, "vsnprintf"));
      assert(sol_vsnprintf != NULL, "vsnprintf not found");
    }
  }
  return (*sol_vsnprintf)(buf, count, fmt, argptr);
}


// Is a (classpath) directory empty?
bool os::dir_is_empty(const char* path) {
  DIR *dir = NULL;
  struct dirent *ptr;

  dir = opendir(path);
  if (dir == NULL) return true;

  /* Scan the directory */
  bool result = true;
  char buf[sizeof(struct dirent) + MAX_PATH];
  struct dirent *dbuf = (struct dirent *) buf;
  while (result && (ptr = readdir(dir, dbuf)) != NULL) {
    if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0) {
      result = false;
    }
  }
  closedir(dir);
  return result;
}


// Map a block of memory.
char* os::map_memory(int fd, const char* file_name, size_t file_offset,
                     char *addr, size_t bytes, bool read_only,
                     bool allow_exec) {
  int prot;
  int flags;

  if (read_only) {
    prot = PROT_READ;
    flags = MAP_SHARED;
  } else {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_PRIVATE;
  }

  if (allow_exec) {
    prot |= PROT_EXEC;
  }

  if (addr != NULL) {
    flags |= MAP_FIXED;
  }

  char* mapped_address = (char*)mmap(addr, (size_t)bytes, prot, flags,
                                     fd, file_offset);
  if (mapped_address == MAP_FAILED) {
    return NULL;
  }
  return mapped_address;
}


// Unmap a block of memory.
bool os::unmap_memory(char* addr, size_t bytes) {
  return munmap(addr, bytes) == 0;
}


#ifndef PRODUCT
#ifdef INTERPOSE_ON_SYSTEM_SYNCH_FUNCTIONS
// Turn this on if you need to trace synch operations.
// Set RECORD_SYNCH_LIMIT to a large-enough value,
// and call record_synch_enable and record_synch_disable
// around the computation of interest.

void record_synch(char* name, bool returning);  // defined below

class RecordSynch {
  char* _name;
 public:
  RecordSynch(char* name) :_name(name)
                 { record_synch(_name, false); }
  ~RecordSynch() { record_synch(_name,   true);  }
};

#define CHECK_SYNCH_OP(ret, name, params, args, inner)		\
extern "C" ret name params {					\
  typedef ret name##_t params;					\
  static name##_t* implem = NULL;				\
  static int callcount = 0;					\
  if (implem == NULL) {						\
    implem = (name##_t*) dlsym(RTLD_NEXT, #name);		\
    if (implem == NULL)  fatal(dlerror());			\
  }								\
  ++callcount;							\
  RecordSynch _rs(#name);					\
  inner;							\
  return implem args;						\
}
// in dbx, examine callcounts this way:
// for n in $(eval whereis callcount | awk '{print $2}'); do print $n; done

#define CHECK_POINTER_OK(p) \
  (Universe::perm_gen() == NULL || !Universe::is_reserved_heap((oop)(p)))
#define CHECK_MU \
  if (!CHECK_POINTER_OK(mu)) fatal("Mutex must be in C heap only.");
#define CHECK_CV \
  if (!CHECK_POINTER_OK(cv)) fatal("Condvar must be in C heap only.");
#define CHECK_P(p) \
  if (!CHECK_POINTER_OK(p))  fatal(false,  "Pointer must be in C heap only.");

#define CHECK_MUTEX(mutex_op) \
CHECK_SYNCH_OP(int, mutex_op, (mutex_t *mu), (mu), CHECK_MU);

CHECK_MUTEX(   mutex_lock)
CHECK_MUTEX(  _mutex_lock)
CHECK_MUTEX( mutex_unlock)
CHECK_MUTEX(_mutex_unlock)
CHECK_MUTEX( mutex_trylock)
CHECK_MUTEX(_mutex_trylock)

#define CHECK_COND(cond_op) \
CHECK_SYNCH_OP(int, cond_op, (cond_t *cv, mutex_t *mu), (cv, mu), CHECK_MU;CHECK_CV);

CHECK_COND( cond_wait);
CHECK_COND(_cond_wait);
CHECK_COND(_cond_wait_cancel);

#define CHECK_COND2(cond_op) \
CHECK_SYNCH_OP(int, cond_op, (cond_t *cv, mutex_t *mu, timestruc_t* ts), (cv, mu, ts), CHECK_MU;CHECK_CV);

CHECK_COND2( cond_timedwait);
CHECK_COND2(_cond_timedwait);
CHECK_COND2(_cond_timedwait_cancel);

// do the _lwp_* versions too
#define mutex_t lwp_mutex_t
#define cond_t  lwp_cond_t
CHECK_MUTEX(  _lwp_mutex_lock)
CHECK_MUTEX(  _lwp_mutex_unlock)
CHECK_MUTEX(  _lwp_mutex_trylock)
CHECK_MUTEX( __lwp_mutex_lock)
CHECK_MUTEX( __lwp_mutex_unlock)
CHECK_MUTEX( __lwp_mutex_trylock)
CHECK_MUTEX(___lwp_mutex_lock)
CHECK_MUTEX(___lwp_mutex_unlock)

CHECK_COND(  _lwp_cond_wait);
CHECK_COND( __lwp_cond_wait);
CHECK_COND(___lwp_cond_wait);

CHECK_COND2(  _lwp_cond_timedwait);
CHECK_COND2( __lwp_cond_timedwait);
#undef mutex_t
#undef cond_t

CHECK_SYNCH_OP(int, _lwp_suspend2,       (int lwp, int *n), (lwp, n), 0);
CHECK_SYNCH_OP(int,__lwp_suspend2,       (int lwp, int *n), (lwp, n), 0);
CHECK_SYNCH_OP(int, _lwp_kill,           (int lwp, int n),  (lwp, n), 0);
CHECK_SYNCH_OP(int,__lwp_kill,           (int lwp, int n),  (lwp, n), 0);
CHECK_SYNCH_OP(int, _lwp_sema_wait,      (lwp_sema_t* p),   (p),  CHECK_P(p));
CHECK_SYNCH_OP(int,__lwp_sema_wait,      (lwp_sema_t* p),   (p),  CHECK_P(p));
CHECK_SYNCH_OP(int, _lwp_cond_broadcast, (lwp_cond_t* cv),  (cv), CHECK_CV);
CHECK_SYNCH_OP(int,__lwp_cond_broadcast, (lwp_cond_t* cv),  (cv), CHECK_CV);


// recording machinery:

enum { RECORD_SYNCH_LIMIT = 200 };
char* record_synch_name[RECORD_SYNCH_LIMIT];
void* record_synch_arg0ptr[RECORD_SYNCH_LIMIT];
bool record_synch_returning[RECORD_SYNCH_LIMIT];
thread_t record_synch_thread[RECORD_SYNCH_LIMIT];
int record_synch_count = 0;
bool record_synch_enabled = false;

// in dbx, examine recorded data this way:
// for n in name arg0ptr returning thread; do print record_synch_$n[0..record_synch_count-1]; done

void record_synch(char* name, bool returning) {
  if (record_synch_enabled) {
    if (record_synch_count < RECORD_SYNCH_LIMIT) {
      record_synch_name[record_synch_count] = name;
      record_synch_returning[record_synch_count] = returning;
      record_synch_thread[record_synch_count] = thr_self();
      record_synch_arg0ptr[record_synch_count] = &name;
      record_synch_count++;
    }
    // put more checking code here:
    // ...
  }
}

void record_synch_enable() {
  // start collecting trace data, if not already doing so
  if (!record_synch_enabled)  record_synch_count = 0;
  record_synch_enabled = true;
}

void record_synch_disable() {
  // stop collecting trace data
  record_synch_enabled = false;
}

#endif // INTERPOSE_ON_SYSTEM_SYNCH_FUNCTIONS
#endif // PRODUCT

const intptr_t thr_time_off  = (intptr_t)(&((prusage_t *)(NULL))->pr_utime);
const intptr_t thr_time_size = (intptr_t)(&((prusage_t *)(NULL))->pr_ttime) -
                               (intptr_t)(&((prusage_t *)(NULL))->pr_utime);


// JVMTI & JVM monitoring and management support
// The thread_cpu_time() and current_thread_cpu_time() are only
// supported if is_thread_cpu_time_supported() returns true.
// They are not supported on Solaris T1.

// current_thread_cpu_time(bool) and thread_cpu_time(Thread*, bool)
// are used by JVM M&M and JVMTI to get user+sys or user CPU time
// of a thread.
//
// current_thread_cpu_time() and thread_cpu_time(Thread *)
// returns the fast estimate available on the platform.

// hrtime_t gethrvtime() return value includes
// user time but does not include system time
jlong os::current_thread_cpu_time() {
  return (jlong) gethrvtime();
}

jlong os::thread_cpu_time(Thread *thread) {
  // return user level CPU time only to be consistent with 
  // what current_thread_cpu_time returns.
  // thread_cpu_time_info() must be changed if this changes
  return os::thread_cpu_time(thread, false /* user time only */);
}

jlong os::current_thread_cpu_time(bool user_sys_cpu_time) {
  if (user_sys_cpu_time) {
    return os::thread_cpu_time(Thread::current(), user_sys_cpu_time);
  } else {
    return os::current_thread_cpu_time();
  }
}

jlong os::thread_cpu_time(Thread *thread, bool user_sys_cpu_time) {
  char proc_name[64];
  int count;
  prusage_t prusage;
  jlong lwp_time;
  int fd;

  sprintf(proc_name, "/proc/%d/lwp/%d/lwpusage", 
                     getpid(),
                     thread->osthread()->lwp_id());
  fd = open(proc_name, O_RDONLY);
  if ( fd == -1 ) return -1;

  do {
    count = pread(fd, 
                  (void *)&prusage.pr_utime, 
                  thr_time_size, 
                  thr_time_off);
  } while (count < 0 && errno == EINTR);
  close(fd);
  if ( count < 0 ) return -1;
  
  if (user_sys_cpu_time) {
    // user + system CPU time
    lwp_time = (((jlong)prusage.pr_stime.tv_sec + 
                 (jlong)prusage.pr_utime.tv_sec) * (jlong)1000000000) + 
                 (jlong)prusage.pr_stime.tv_nsec + 
                 (jlong)prusage.pr_utime.tv_nsec;
  } else {
    // user level CPU time only
    lwp_time = ((jlong)prusage.pr_utime.tv_sec * (jlong)1000000000) + 
                (jlong)prusage.pr_utime.tv_nsec;
  }

  return(lwp_time);
}

void os::current_thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = ALL_64_BITS;      // will not wrap in less than 64 bits
  info_ptr->may_skip_backward = false;    // elapsed time not wall time
  info_ptr->may_skip_forward = false;     // elapsed time not wall time
  info_ptr->kind = JVMTI_TIMER_USER_CPU;  // only user time is returned
}

void os::thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = ALL_64_BITS;      // will not wrap in less than 64 bits
  info_ptr->may_skip_backward = false;    // elapsed time not wall time
  info_ptr->may_skip_forward = false;     // elapsed time not wall time
  info_ptr->kind = JVMTI_TIMER_USER_CPU;  // only user time is returned
}

bool os::is_thread_cpu_time_supported() {
  if ( os::Solaris::T2_libthread() || UseBoundThreads ) {
    return true;
  } else {
    return false;
  }
}

//---------------------------------------------------------------------------------
#ifndef PRODUCT

static address same_page(address x, address y) {
  intptr_t page_bits = -os::vm_page_size();
  if ((intptr_t(x) & page_bits) == (intptr_t(y) & page_bits))
    return x;
  else if (x > y)
    return (address)(intptr_t(y) | ~page_bits) + 1;
  else
    return (address)(intptr_t(y) & page_bits);
}

bool os::find(address addr) {
  Dl_info dlinfo;
  memset(&dlinfo, 0, sizeof(dlinfo));
  if (dladdr(addr, &dlinfo)) {
#ifdef _LP64
    tty->print("0x%016lx: ", addr);
#else
    tty->print("0x%08x: ", addr);
#endif
    if (dlinfo.dli_sname != NULL)
      tty->print("%s+%#lx", dlinfo.dli_sname, addr-(intptr_t)dlinfo.dli_saddr);
    else if (dlinfo.dli_fname)
      tty->print("<offset %#lx>", addr-(intptr_t)dlinfo.dli_fbase);
    else
      tty->print("<absolute address>");
    if (dlinfo.dli_fname)  tty->print(" in %s", dlinfo.dli_fname);
#ifdef _LP64
    if (dlinfo.dli_fbase)  tty->print(" at 0x%016lx", dlinfo.dli_fbase);
#else
    if (dlinfo.dli_fbase)  tty->print(" at 0x%08x", dlinfo.dli_fbase);
#endif
    tty->cr();

    if (Verbose) {
      // decode some bytes around the PC
      address begin = same_page(addr-40, addr);
      address end   = same_page(addr+40, addr);
      address       lowest = (address) dlinfo.dli_sname;
      if (!lowest)  lowest = (address) dlinfo.dli_fbase;
      if (begin < lowest)  begin = lowest;
      Dl_info dlinfo2;
      if (dladdr(end, &dlinfo2) && dlinfo2.dli_saddr != dlinfo.dli_saddr
	  && end > dlinfo2.dli_saddr && dlinfo2.dli_saddr > begin)
        end = (address) dlinfo2.dli_saddr;
      Disassembler::decode(begin, end);
    }
    return true;
  }
  return false;
}
  
#endif


// Following function has been added to support HotSparc's libjvm.so running 
// under Solaris production JDK 1.2.2 / 1.3.0.  These came from 
// src/solaris/hpi/native_threads in the EVM codebase.
//
// NOTE: This is no longer needed in the 1.3.1 and 1.4 production release
// libraries and should thus be removed. We will leave it behind for a while
// until we no longer want to able to run on top of 1.3.0 Solaris production
// JDK. See 4341971.

#define STACK_SLACK 0x800

extern "C" {
  intptr_t sysThreadAvailableStackWithSlack(){
    stack_t st;
    intptr_t retval, stack_top;
    retval = thr_stksegment(&st);
    assert(retval == 0, "incorrect return value from thr_stksegment");
    assert((address)&st < (address)st.ss_sp, "Invalid stack base returned");
    assert((address)&st > (address)st.ss_sp-st.ss_size, "Invalid stack size returned");
    stack_top=(intptr_t)st.ss_sp-st.ss_size; 
    return ((intptr_t)&stack_top - stack_top - STACK_SLACK);
  }
}

