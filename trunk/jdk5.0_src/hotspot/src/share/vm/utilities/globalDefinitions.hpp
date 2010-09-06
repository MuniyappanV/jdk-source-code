#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)globalDefinitions.hpp	1.191 04/03/25 15:43:26 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// This file holds all globally used constants & types, class (forward)
// declarations and a few frequently used utility functions.

#ifdef SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------------------------------------
// Constants

const int LogBytesPerShort   = 1;
const int LogBytesPerInt     = 2;
#ifdef _LP64
const int LogBytesPerWord    = 3;
#else
const int LogBytesPerWord    = 2;
#endif
const int LogBytesPerLong    = 3;

const int BytesPerShort      = 1 << LogBytesPerShort;
const int BytesPerInt        = 1 << LogBytesPerInt;
const int BytesPerWord       = 1 << LogBytesPerWord;
const int BytesPerLong       = 1 << LogBytesPerLong;

const int LogBitsPerByte     = 3;
const int LogBitsPerShort    = LogBitsPerByte + LogBytesPerShort;
const int LogBitsPerInt      = LogBitsPerByte + LogBytesPerInt;
const int LogBitsPerWord     = LogBitsPerByte + LogBytesPerWord;
const int LogBitsPerLong     = LogBitsPerByte + LogBytesPerLong;

const int BitsPerByte        = 1 << LogBitsPerByte;
const int BitsPerShort       = 1 << LogBitsPerShort;
const int BitsPerInt         = 1 << LogBitsPerInt;
const int BitsPerWord        = 1 << LogBitsPerWord;
const int BitsPerLong        = 1 << LogBitsPerLong;

const int WordAlignmentMask  = (1 << LogBytesPerWord) - 1;
const int LongAlignmentMask  = (1 << LogBytesPerLong) - 1;

const int WordsPerLong       = 2;	// Number of stack entries for longs

const int oopSize            = sizeof(char*);
const int wordSize           = sizeof(char*); 
const int longSize           = sizeof(jlong);
const int jintSize           = sizeof(jint);
const int size_tSize         = sizeof(size_t);

// Size of a char[] needed to represent a jint as a string in decimal.
const int jintAsStringSize = 12;

const int LogBytesPerOop     = LogBytesPerWord;
const int LogBitsPerOop      = LogBitsPerWord;
const int BytesPerOop        = 1 << LogBytesPerOop;
const int BitsPerOop         = 1 << LogBitsPerOop;

const int BitsPerJavaInteger = 32;
const int BitsPerSize_t      = size_tSize * BitsPerByte;

// An opaque struct of heap-word width, so that HeapWord* can be a generic
// pointer into the heap.  We require that object sizes be measured in
// units of heap words, so that that 
//   HeapWord* hw;
//   hw += oop(hw)->foo();
// works, where foo is a method (like size or scavenge) that returns the
// object size.
class HeapWord {
  friend class VMStructs;
private:
  char* i;
};

// HeapWordSize must be 2^LogHeapWordSize.
const int HeapWordSize     = sizeof(HeapWord);
#ifdef _LP64
const int LogHeapWordSize  = 3;
#else
const int LogHeapWordSize  = 2;
#endif
const int HeapWordsPerOop  = oopSize      / HeapWordSize;
const int HeapWordsPerLong = BytesPerLong / HeapWordSize;

// The larger HeapWordSize for 64bit requires larger heaps
// for the same application running in 64bit.  See bug 4967770.
// The minimum alignment to a heap word size is done.  Other
// parts of the memory system may required additional alignment
// and are responsible for those alignments.
#ifdef _LP64
#define ScaleForWordSize(x) align_size_down_((x) * 13 / 10, HeapWordSize)
#else
#define ScaleForWordSize(x) (x)
#endif

// The minimum number of native machine words necessary to contain "byte_size"
// bytes.
inline size_t heap_word_size(size_t byte_size) {
  return (byte_size + (HeapWordSize-1)) >> LogHeapWordSize;
}


const int K                  = 1024;
const int M                  = K*K;
const int G                  = M*K;
const int HWperKB            = K / sizeof(HeapWord);

const jint min_jint = (jint)1 << (sizeof(jint)*BitsPerByte-1); // 0x80000000 == smallest jint
const jint max_jint = (juint)min_jint - 1;                     // 0x7FFFFFFF == largest jint

// Constants for converting from a base unit to milli-base units.  For
// example from seconds to milliseconds and microseconds

const int MILLIUNITS	= 1000;		// milli units per base unit
const int MICROUNITS	= 1000000;	// micro units per base unit

//----------------------------------------------------------------------------------------------------
// VM type definitions

// intx and uintx are the 'extended' int and 'extended' unsigned int types;
// they are 32bit wide on a 32-bit platform, and 64bit wide on a 64bit platform.

typedef intptr_t  intx;
typedef uintptr_t uintx;

const intx  min_intx  = (intx)1 << (sizeof(intx)*BitsPerByte-1);
const intx  max_intx  = (uintx)min_intx - 1;
const uintx max_uintx = (uintx)-1;

// Table of values:
// 	sizeof intx	    4		    8
// min_intx		0x80000000	0x8000000000000000
// max_intx		0x7FFFFFFF	0x7FFFFFFFFFFFFFFF
// max_uintx		0xFFFFFFFF	0xFFFFFFFFFFFFFFFF

typedef unsigned int uint;   NEEDS_CLEANUP


//----------------------------------------------------------------------------------------------------
// Java type definitions

// All kinds of 'plain' byte addresses
typedef   signed char s_char;
typedef unsigned char u_char;
typedef u_char*       address;
typedef uintptr_t     address_word; // unsigned integer which will hold a pointer
				    // except for some implementations of a C++
				    // linkage pointer to function. Should never
				    // need one of those to be placed in this
				    // type anyway.

//  Utility functions to "portably" (?) bit twiddle pointers
//  Where portable means keep ANSI C++ compilers quiet

inline address       set_address_bits(address x, int m)       { return address(intptr_t(x) | m); }
inline address       clear_address_bits(address x, int m)     { return address(intptr_t(x) & ~m); }

//  Utility functions to "portably" make cast to/from function pointers.

inline address_word  mask_address_bits(address x, int m)      { return address_word(x) & m; }
inline address_word  castable_address(address x)              { return address_word(x) ; }
inline address_word  castable_address(void* x)                { return address_word(x) ; }

// Pointer subtraction.
// The idea here is to avoid ptrdiff_t, which is signed and so doesn't have 
// the range we might need to find differences from one end of the heap 
// to the other.
// A typical use might be:
//     if (pointer_delta(end(), top()) >= size) {
//       // enough room for an object of size
//       ...
// and then additions like 
//       ... top() + size ...
// are safe because we know that top() is at least size below end().
inline size_t pointer_delta(const void* left,
			    const void* right,
			    size_t element_size) {
  return (((uintptr_t) left) - ((uintptr_t) right)) / element_size;
}
// A version specialized for HeapWord*'s.
inline size_t pointer_delta(const HeapWord* left, const HeapWord* right) {
  return pointer_delta(left, right, sizeof(HeapWord));
}

//
// ANSI C++ does not allow casting from one pointer type to a function pointer
// directly without at best a warning. This macro accomplishes it silently
// In every case that is present at this point the value be cast is a pointer
// to a C linkage function. In somecase the type used for the cast reflects
// that linkage and a picky compiler would not complain. In other cases because
// there is no convenient place to place a typedef with extern C linkage (i.e
// a platform dependent header file) it doesn't. At this point no compiler seems
// picky enough to catch these instances (which are few). It is possible that
// using templates could fix these for all cases. This use of templates is likely
// so far from the middle of the road that it is likely to be problematic in
// many C++ compilers.
//
#define CAST_TO_FN_PTR(func_type, value) ((func_type)(castable_address(value)))
#define CAST_FROM_FN_PTR(new_type, func_ptr) ((new_type)((address_word)(func_ptr)))

// Unsigned byte types for os and stream.hpp

// Unsigned one, two, four and eigth byte quantities used for describing
// the .class file format. See JVM book chapter 4.

typedef jubyte  u1;
typedef jushort u2;
typedef juint   u4;
typedef julong  u8;

const jubyte  max_jubyte  = (jubyte)-1;  // 0xFF       largest jubyte
const jushort max_jushort = (jushort)-1; // 0xFFFF     largest jushort
const juint   max_juint   = (juint)-1;   // 0xFFFFFFFF largest juint
const julong  max_julong  = (julong)-1;  // 0xFF....FF largest julong


// Booleans
#define bool    jint

#define true    v_true
#define false   v_false

const bool true        = 1;
const bool false       = 0;

const jboolean g_true  = 1;
const jboolean g_false = 0;


//----------------------------------------------------------------------------------------------------
// JVM spec restrictions

const int max_method_code_size = 64*K - 1;  // JVM spec, 2nd ed. section 4.8.1 (p.134)


//----------------------------------------------------------------------------------------------------
// HotSwap - for JVMTI   aka Class File Replacement and PopFrame
//
// Determines whether on-the-fly class replacement and frame popping are enabled.

#define HOTSWAP

//----------------------------------------------------------------------------------------------------
// Object alignment, in units of HeapWords.
//
// Minimum is max(BytesPerLong, BytesPerDouble, BytesPerOop) / HeapWordSize, so jlong, jdouble and
// reference fields can be naturally aligned.

const int MinObjAlignment            = HeapWordsPerLong;
const int MinObjAlignmentInBytes     = MinObjAlignment * HeapWordSize;
const int MinObjAlignmentInBytesMask = MinObjAlignmentInBytes - 1;

// Machine dependent stuff

#include "incls/_globalDefinitions_pd.hpp.incl"

// The byte alignment to be used by Arena::Amalloc.  See bugid 4169348.
// Note: this value must be a power of 2

#define ARENA_AMALLOC_ALIGNMENT (2*BytesPerWord)

// Signed variants of alignment helpers.  There are two versions of each, a macro
// for use in places like enum definitions that require compile-time constant
// expressions and a function for all other places so as to get type checking.

#define align_size_up_(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))

inline intptr_t align_size_up(intptr_t size, intptr_t alignment) { 
  return align_size_up_(size, alignment);
}

#define align_size_down_(size, alignment) ((size) & ~((alignment) - 1))

inline intptr_t align_size_down(intptr_t size, intptr_t alignment) { 
  return align_size_down_(size, alignment);
}

// Align objects by rounding up their size, in HeapWord units.

#define align_object_size_(size) align_size_up_(size, MinObjAlignment)

inline intptr_t align_object_size(intptr_t size) { 
  return align_size_up(size, MinObjAlignment);
}

// Pad out certain offsets to jlong alignment, in HeapWord units.

#define align_object_offset_(offset) align_size_up_(offset, HeapWordsPerLong)

inline intptr_t align_object_offset(intptr_t offset) {
  return align_size_up(offset, HeapWordsPerLong);
}

inline bool is_object_aligned(intptr_t offset) {
  return offset == align_object_offset(offset);
}


//----------------------------------------------------------------------------------------------------
// Utility macros for compilers
// used to silence compiler warnings

#define Unused_Variable(var) var


//----------------------------------------------------------------------------------------------------
// Miscellaneous

inline jint low (jlong value)                    { return jint(value); }
inline jint high(jlong value)                    { return jint(value >> 32); }

// the fancy casts are a hopefully portable way 
// to do unsigned 32 to 64 bit type conversion
inline void set_low (jlong* value, jint low )    { *value &= (jlong)0xffffffff << 32;
                                                   *value |= (jlong)(julong)(juint)low; }

inline void set_high(jlong* value, jint high)    { *value &= (jlong)(julong)(juint)0xffffffff;
                                                   *value |= (jlong)high       << 32; }

inline jlong jlong_from(jint h, jint l) {
  jlong result = 0; // initialization to avoid warning
  set_high(&result, h);
  set_low(&result,  l);
  return result;
}

void check_basic_types(); // cannot define here; uses assert


// NOTE: replicated in SA in vm/agent/sun/jvm/hotspot/runtime/BasicType.java
enum BasicType {
  T_BOOLEAN  =  4,
  T_CHAR     =  5,
  T_FLOAT    =  6,
  T_DOUBLE   =  7,
  T_BYTE     =  8,
  T_SHORT    =  9,
  T_INT      = 10,
  T_LONG     = 11,
  T_OBJECT   = 12,
  T_ARRAY    = 13,
  T_VOID     = 14,
  T_ADDRESS  = 15,
  T_CONFLICT = 16, // for stack value type with conflicting contents
  T_ILLEGAL  = 99
};

// Convert a char from a classfile signature to a BasicType
inline BasicType char2type(char c) {
  switch( c ) {
  case 'B': return T_BYTE;    
  case 'C': return T_CHAR;    
  case 'D': return T_DOUBLE;  
  case 'F': return T_FLOAT;   
  case 'I': return T_INT;     
  case 'J': return T_LONG;    
  case 'S': return T_SHORT;   
  case 'Z': return T_BOOLEAN; 
  case 'V': return T_VOID;    
  case 'L': return T_OBJECT;  
  case '[': return T_ARRAY;   
  }
  return T_ILLEGAL;
}

extern char type2char_tab[T_CONFLICT+1];     // Map a BasicType to a jchar
inline char type2char(BasicType t) { return (uint)t < T_CONFLICT+1 ? type2char_tab[t] : 0; }
extern int type2size[T_CONFLICT+1];	    // Map BasicType to result stack elements
extern const char* type2name_tab[T_CONFLICT+1];     // Map a BasicType to a jchar
inline const char* type2name(BasicType t) { return (uint)t < T_CONFLICT+1 ? type2name_tab[t] : NULL; }
extern BasicType name2type(const char* name);


// NOTE: replicated in SA in vm/agent/sun/jvm/hotspot/runtime/BasicType.java
enum BasicTypeSize {
  T_BOOLEAN_size = 1,
  T_CHAR_size    = 1,
  T_FLOAT_size   = 1,
  T_DOUBLE_size  = 2,
  T_BYTE_size    = 1,
  T_SHORT_size   = 1,
  T_INT_size     = 1,
  T_LONG_size    = 2,
  T_OBJECT_size  = 1,
  T_ARRAY_size   = 1,
  T_VOID_size    = 0
};


// maps a BasicType to its instance field storage type:
// all sub-word integral types are widened to T_INT
extern BasicType type2field[T_CONFLICT+1];
extern BasicType type2wfield[T_CONFLICT+1];


// size in bytes
enum ArrayElementSize {
  T_BOOLEAN_aelem_bytes = 1,
  T_CHAR_aelem_bytes    = 2,
  T_FLOAT_aelem_bytes   = 4,
  T_DOUBLE_aelem_bytes  = 8,
  T_BYTE_aelem_bytes    = 1,
  T_SHORT_aelem_bytes   = 2,
  T_INT_aelem_bytes     = 4,
  T_LONG_aelem_bytes    = 8,
#ifdef _LP64
  T_OBJECT_aelem_bytes  = 8,
  T_ARRAY_aelem_bytes   = 8,
#else
  T_OBJECT_aelem_bytes  = 4,
  T_ARRAY_aelem_bytes   = 4,
#endif
  T_VOID_aelem_bytes    = 0
};

extern int type2aelembytes[T_CONFLICT+1]; // maps a BasicType to nof bytes used by its array element


// JavaValue serves as a container for arbitrary Java values.

class JavaValue {

 public:
  typedef union JavaCallValue {
    jfloat   f;
    jdouble  d;
    jint     i;
    jlong    l;
    jobject  h;
  } JavaCallValue;
 
 private:
  BasicType _type;
  JavaCallValue _value;
 
 public:
  JavaValue(BasicType t = T_ILLEGAL) { _type = t; }

  JavaValue(jfloat value) {
    _type    = T_FLOAT;
    _value.f = value;
  }

  JavaValue(jdouble value) {
    _type    = T_DOUBLE;
    _value.d = value;
  }

 jfloat get_jfloat() const { return _value.f; }
 jdouble get_jdouble() const { return _value.d; }
 jint get_jint() const { return _value.i; }
 jlong get_jlong() const { return _value.l; }
 jobject get_jobject() const { return _value.h; }
 JavaCallValue* get_value_addr() { return &_value; }
 BasicType get_type() const { return _type; }
 
 void set_jfloat(jfloat f) { _value.f = f;}
 void set_jdouble(jdouble d) { _value.d = d;}
 void set_jint(jint i) { _value.i = i;}
 void set_jlong(jlong l) { _value.l = l;}
 void set_jobject(jobject h) { _value.h = h;}
 void set_type(BasicType t) { _type = t; }
 
 jboolean get_jboolean() const { return (jboolean) (_value.i);}
 jbyte get_jbyte() const { return (jbyte) (_value.i);}
 jchar get_jchar() const { return (jchar) (_value.i);}
 jshort get_jshort() const { return (jshort) (_value.i);}

};


#define STACK_BIAS	0
// V9 Sparc CPU's running in 64 Bit mode use a stack bias of 7ff
// in order to extend the reach of the stack pointer.
#if defined(SPARC) && defined(_LP64)
#undef STACK_BIAS
#define STACK_BIAS	0x7ff
#endif


// TosState describes the top-of-stack state before and after the execution of
// a bytecode or method. The top-of-stack value may be cached in one or more CPU
// registers. The TosState corresponds to the 'machine represention' of this cached
// value. There's 4 states corresponding to the JAVA types int, long, float & double
// as well as a 5th state in case the top-of-stack value is actually on the top
// of stack (in memory) and thus not cached. The atos state corresponds to the itos
// state when it comes to machine representation but is used separately for (oop)
// type specific operations (e.g. verification code).

enum TosState {         // describes the tos cache contents
  btos = 0, 		// byte, bool tos cached
  ctos = 1,		// short, char tos cached
  stos = 2,		// short, char tos cached
  itos = 3,             // int tos cached
  ltos = 4,             // long tos cached
  ftos = 5,             // float tos cached
  dtos = 6,             // double tos cached
  atos = 7, 		// object cached
  vtos = 8,             // tos not cached
  number_of_states,
  ilgl                  // illegal state: should not occur
};


inline TosState as_TosState(BasicType type) {
  switch (type) {
    case T_BYTE   : return btos;
    case T_BOOLEAN: return btos;
    case T_CHAR   : return ctos;
    case T_SHORT  : return stos;
    case T_INT    : return itos;
    case T_LONG   : return ltos;
    case T_FLOAT  : return ftos;
    case T_DOUBLE : return dtos;
    case T_VOID   : return vtos;
    case T_ARRAY  : // fall through
    case T_OBJECT : return atos;
  }
  return ilgl;
}


// Helper function to convert BasicType info into TosState
// Note: Cannot define here as it uses global constant at the time being.
TosState as_TosState(BasicType type);


// ReferenceType is used to distinguish between java/lang/ref/Reference subclasses

enum ReferenceType {
 REF_NONE,      // Regular class
 REF_OTHER,     // Subclass of java/lang/ref/Reference, but not subclass of one of the classes below
 REF_SOFT,      // Subclass of java/lang/ref/SoftReference
 REF_WEAK,      // Subclass of java/lang/ref/WeakReference
 REF_FINAL,     // Subclass of java/lang/ref/FinalReference
 REF_PHANTOM    // Subclass of java/lang/ref/PhantomReference
};


// JavaThreadState keeps track of which part of the code a thread is executing in. This
// information is needed by the safepoint code.
//
// There are 4 essential states:
//
//  _thread_new         : Just started, but not executed init. code yet (most likely still in OS init code)
//  _thread_in_native   : In native code. This is a safepoint region, since all oops will be in jobject handles
//  _thread_in_vm       : Executing in the vm
//  _thread_in_Java     : Executing either interpreted or compiled Java code (or could be in a stub)
//
// Each state has an associated xxxx_trans state, which is an intermediate state used when a thread is in
// a transition from one state to another. These extra states makes it possible for the safepoint code to
// handle certain thread_states without having to suspend the thread - making the safepoint code faster.
//
// Given a state, the xxx_trans state can always be found by adding 1.
//
enum JavaThreadState {
  _thread_uninitialized     =  0, // should never happen (missing initialization)
  _thread_new               =  2, // just starting up, i.e., in process of being initialized
  _thread_new_trans         =  3, // corresponding transition state (not used, included for completness)
  _thread_in_native         =  4, // running in native code
  _thread_in_native_trans   =  5, // corresponding transition state
  _thread_in_vm             =  6, // running in VM
  _thread_in_vm_trans       =  7, // corresponding transition state
  _thread_in_Java           =  8, // running in Java or in stub code
  _thread_in_Java_trans     =  9, // corresponding transition state (not used, included for completness)
  _thread_blocked           = 10, // blocked in vm
  _thread_blocked_trans     = 11, // corresponding transition state  
  _thread_max_state         = 12  // maximum thread state+1 - used for statistics allocation
};


// Handy constants for deciding which compiler mode to use.
enum MethodCompilation {
  InvocationEntryBci = -1,     // i.e., not a on-stack replacement compilation
  InvalidOSREntryBci = -2
};

// Enumeration to distinguish tiers of compilation
enum CompLevel {
  CompLevel_none              = 0,
  CompLevel_fast_compile      = 1,
  CompLevel_full_optimization = 2,

  CompLevel_highest_tier      = CompLevel_full_optimization
};

inline bool is_tier1_compile(int comp_level) {
  return comp_level == CompLevel_fast_compile;
}
inline bool is_tier2_compile(int comp_level) {
  return comp_level == CompLevel_full_optimization;
}
inline bool is_highest_tier_compile(int comp_level) {
  return comp_level == CompLevel_highest_tier;
}

//----------------------------------------------------------------------------------------------------
// 'Forward' declarations of frequently used classes
// (in order to reduce interface dependencies & reduce
// number of unnecessary compilations after changes)

class symbolTable;
class ClassFileStream;

class Event;

class Thread;
class  VMThread;
class  JavaThread;
class Threads;

class VM_Operation;
class VMOperationQueue;

class CodeBlob;
class  nmethod;
class  OSRAdapter;
class  I2CAdapter;
class  C2IAdapter;
class CompiledIC;
class relocInfo;
class ScopeDesc;
class PcDesc;

class Recompiler;
class Recompilee;
class RecompilationPolicy;
class RFrame;
class  CompiledRFrame;
class  InterpretedRFrame;

class frame;

class vframe;
class   javaVFrame;
class     interpretedVFrame;
class     compiledVFrame;
class     deoptimizedVFrame;
class   externalVFrame;
class     entryVFrame;

class RegisterMap;

class Mutex;
class Monitor;
class BasicLock;
class BasicObjectLock;

class PeriodicTask;

class JavaCallWrapper;

class   oopDesc;
typedef class oopDesc* oop;

class NativeCall;

class zone;

class StubQueue;

class outputStream;

class ResourceArea;

class DebugInformationRecorder;
class ScopeValue;
class CompressedStream;
class   DebugInfoReadStream;
class   DebugInfoWriteStream;
class LocationValue;
class ConstantValue;
class IllegalValue;

class PrivilegedElement;
class MonitorArray;

class MonitorInfo;

class OffsetClosure;
class OopMapCache;
class InterpreterOopMap;
class OopMapCacheEntry;
class OSThread;

typedef int (*OSThreadStartFunc)(void*);

class Space;
class CarSpace;
class Train;

class JavaValue;
class methodHandle;
class JavaCallArguments;

// Basic support for errors (general debug facilities not defined at this point fo the include phase)

extern void basic_fatal(const char* msg);


//----------------------------------------------------------------------------------------------------
// Special constants for debugging
// Note: (long) cast is required to make 64bit constants 32bit long on 32bit machines.

const jint    badInt           = -3;                       // generic "bad int" value
const long    badAddressVal    = -2;                       // generic "bad address" value
const long    badOopVal        = -1;                       // generic "bad oop" value
const long    badHeapOopVal    = (long) CONST64(0x2BAD4B0BBAADBABE); // value used to zap heap after GC
const int     badHandleValue   = 0xBC;                     // value used to zap vm handle area
const int     badResourceValue = 0xAB;                     // value used to zap resource area
const long    badJNIHandleVal  = (long) CONST64(0xFEFEFEFEFEFEFEFE); // value used to zap jni handle area
const juint   badHeapWordVal   = 0xBAADBABE;               // value used to zap heap after GC
const int     badCodeHeapNewVal= 0xCC;                     // value used to zap Code heap at allocation
const int     badCodeHeapFreeVal = 0xDD;                   // value used to zap Code heap at deallocation


// (These must be implemented as #defines because C++ compilers are
// not obligated to inline non-integral constants!)
#define       badAddress        ((address)::badAddressVal)
#define       badOop            ((oop)::badOopVal)
#define       badHeapWord       (::badHeapWordVal)
#define       badJNIHandle      ((oop)::badJNIHandleVal)


//----------------------------------------------------------------------------------------------------
// Utility functions for bitfield manipulations

const intptr_t AllBits    = ~0; // all bits set in a word
const intptr_t NoBits     =  0; // no bits set in a word
const jlong    NoLongBits =  0; // no bits set in a long
const intptr_t OneBit     =  1; // only right_most bit set in a word

// get a word with the n.th or the right-most or left-most n bits set
// (note: #define used only so that they can be used in enum constant definitions)
#define nth_bit(n)        (n >= BitsPerWord ? 0 : OneBit << (n))
#define right_n_bits(n)   (nth_bit(n) - 1)
#define left_n_bits(n)    (right_n_bits(n) << (n >= BitsPerWord ? 0 : (BitsPerWord - n)))

// bit-operations using a mask m
inline void   set_bits    (intptr_t& x, intptr_t m) { x |= m; }
inline void clear_bits    (intptr_t& x, intptr_t m) { x &= ~m; }
inline intptr_t mask_bits      (intptr_t  x, intptr_t m) { return x & m; }
inline jlong    mask_long_bits (jlong     x, jlong    m) { return x & m; }
inline bool mask_bits_are_true (intptr_t flags, intptr_t mask) { return (flags & mask) == mask; }

// bit-operations using the n.th bit
inline void    set_nth_bit(intptr_t& x, int n) { set_bits  (x, nth_bit(n)); }
inline void  clear_nth_bit(intptr_t& x, int n) { clear_bits(x, nth_bit(n)); }
inline bool is_set_nth_bit(intptr_t  x, int n) { return mask_bits (x, nth_bit(n)) != NoBits; }

// returns the bitfield of x starting at start_bit_no with length field_length (no sign-extension!)
inline intptr_t bitfield(intptr_t x, int start_bit_no, int field_length) {
  return mask_bits(x >> start_bit_no, right_n_bits(field_length));
}


//----------------------------------------------------------------------------------------------------
// Utility functions for integers

// Avoid use of global min/max macros which may cause unwanted double
// evaluation of arguments.
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#define max(a,b) Do_not_use_max_use_MAX2_instead
#define min(a,b) Do_not_use_min_use_MIN2_instead

// It is necessary to use templates here. Having normal overloaded
// functions does not work because it is necessary to provide both 32-
// and 64-bit overloaded functions, which does not work, and having
// explicitly-typed versions of these routines (i.e., MAX2I, MAX2L)
// will be even more error-prone than macros.
template<class T> inline T MAX2(T a, T b)           { return (a > b) ? a : b; }
template<class T> inline T MIN2(T a, T b)           { return (a < b) ? a : b; }
template<class T> inline T MAX3(T a, T b, T c)      { return MAX2(MAX2(a, b), c); }
template<class T> inline T MIN3(T a, T b, T c)      { return MIN2(MIN2(a, b), c); }
template<class T> inline T MAX4(T a, T b, T c, T d) { return MAX2(MAX3(a, b, c), d); }
template<class T> inline T MIN4(T a, T b, T c, T d) { return MIN2(MIN3(a, b, c), d); }

// true if x is a power of 2, false otherwise
inline bool is_power_of_2(intptr_t x) {
  return ((x != NoBits) && (mask_bits(x, x - 1) == NoBits));
}

// long version of is_power_of_2
inline bool is_power_of_2_long(jlong x) {
  return ((x != NoLongBits) && (mask_long_bits(x, x - 1) == NoLongBits));
}

//* largest i such that 2^i <= x
//  A negative value of 'x' will return '31'
inline int log2_intptr(intptr_t x) {
  int i = -1;
  uintptr_t p =  1;
  while (p != 0 && p <= (uintptr_t)x) {
    // p = 2^(i+1) && p <= x (i.e., 2^(i+1) <= x)
    i++; p *= 2;
  }
  // p = 2^(i+1) && x < p (i.e., 2^i <= x < 2^(i+1))
  // (if p = 0 then overflow occured and i = 31)
  return i;
}

//* largest i such that 2^i <= x
//  A negative value of 'x' will return '63'
inline int log2_long(jlong x) {
  int i = -1;
  julong p =  1;
  while (p != 0 && p <= (julong)x) {
    // p = 2^(i+1) && p <= x (i.e., 2^(i+1) <= x)
    i++; p *= 2;
  }
  // p = 2^(i+1) && x < p (i.e., 2^i <= x < 2^(i+1))
  // (if p = 0 then overflow occured and i = 31)
  return i;
}

//* the argument must be exactly a power of 2
inline int exact_log2(intptr_t x) {
  #ifdef ASSERT
    if (!is_power_of_2(x)) basic_fatal("x must be a power of 2");
  #endif
  return log2_intptr(x);
}


// returns integer round-up to the nearest multiple of s (s must be a power of two)
inline intptr_t round_to(intptr_t x, uintx s) {
  #ifdef ASSERT
    if (!is_power_of_2(s)) basic_fatal("s must be a power of 2");
  #endif
  const uintx m = s - 1;
  return mask_bits(x + m, ~m);
}

// returns integer round-down to the nearest multiple of s (s must be a power of two)
inline intptr_t round_down(intptr_t x, uintx s) {
  #ifdef ASSERT
    if (!is_power_of_2(s)) basic_fatal("s must be a power of 2");
  #endif
  const uintx m = s - 1;
  return mask_bits(x, ~m);
}


inline bool is_odd (intx x) { return x & 1;      }
inline bool is_even(intx x) { return !is_odd(x); }

// "to" should be greater than "from."
inline intx byte_size(void* from, void* to) {
  return (address)to - (address)from;
}

//----------------------------------------------------------------------------------------------------
// Avoid non-portable casts with these routines (DEPRECATED)

// NOTE: USE Bytes class INSTEAD WHERE POSSIBLE
//       Bytes is optimized machine-specifically and may be much faster then the portable routines below.

// Given sequence of four bytes, build into a 32-bit word
// following the conventions used in class files.
// On the 386, this could be realized with a simple address cast.
//

// This routine takes eight bytes:
inline u8 build_u8_from( u1 c1, u1 c2, u1 c3, u1 c4, u1 c5, u1 c6, u1 c7, u1 c8 ) {
  return  ( u8(c1) << 56 )  &  ( u8(0xff) << 56 )
       |  ( u8(c2) << 48 )  &  ( u8(0xff) << 48 )
       |  ( u8(c3) << 40 )  &  ( u8(0xff) << 40 )
       |  ( u8(c4) << 32 )  &  ( u8(0xff) << 32 )
       |  ( u8(c5) << 24 )  &  ( u8(0xff) << 24 )
       |  ( u8(c6) << 16 )  &  ( u8(0xff) << 16 )
       |  ( u8(c7) <<  8 )  &  ( u8(0xff) <<  8 )
       |  ( u8(c8) <<  0 )  &  ( u8(0xff) <<  0 );
}

// This routine takes four bytes:
inline u4 build_u4_from( u1 c1, u1 c2, u1 c3, u1 c4 ) {
  return  ( u4(c1) << 24 )  &  0xff000000
       |  ( u4(c2) << 16 )  &  0x00ff0000
       |  ( u4(c3) <<  8 )  &  0x0000ff00
       |  ( u4(c4) <<  0 )  &  0x000000ff;
}

// And this one works if the four bytes are contiguous in memory:
inline u4 build_u4_from( u1* p ) {
  return  build_u4_from( p[0], p[1], p[2], p[3] );
}

// Ditto for two-byte ints:
inline u2 build_u2_from( u1 c1, u1 c2 ) {
  return  u2(( u2(c1) <<  8 )  &  0xff00
          |  ( u2(c2) <<  0 )  &  0x00ff);
}

// And this one works if the two bytes are contiguous in memory:
inline u2 build_u2_from( u1* p ) {
  return  build_u2_from( p[0], p[1] );
}

// Ditto for floats:
inline jfloat build_float_from( u1 c1, u1 c2, u1 c3, u1 c4 ) {
  u4 u = build_u4_from( c1, c2, c3, c4 );
  return  *(jfloat*)&u;
}

inline jfloat build_float_from( u1* p ) {
  u4 u = build_u4_from( p );
  return  *(jfloat*)&u;
}


// now (64-bit) longs

inline jlong build_long_from( u1 c1, u1 c2, u1 c3, u1 c4, u1 c5, u1 c6, u1 c7, u1 c8 ) {
  return  ( jlong(c1) << 56 )  &  ( jlong(0xff) << 56 )
       |  ( jlong(c2) << 48 )  &  ( jlong(0xff) << 48 )
       |  ( jlong(c3) << 40 )  &  ( jlong(0xff) << 40 )
       |  ( jlong(c4) << 32 )  &  ( jlong(0xff) << 32 )
       |  ( jlong(c5) << 24 )  &  ( jlong(0xff) << 24 )
       |  ( jlong(c6) << 16 )  &  ( jlong(0xff) << 16 )
       |  ( jlong(c7) <<  8 )  &  ( jlong(0xff) <<  8 )
       |  ( jlong(c8) <<  0 )  &  ( jlong(0xff) <<  0 );
}

inline jlong build_long_from( u1* p ) {
  return  build_long_from( p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7] );
}


// Doubles, too!
inline jdouble build_double_from( u1 c1, u1 c2, u1 c3, u1 c4, u1 c5, u1 c6, u1 c7, u1 c8 ) {
  jlong u = build_long_from( c1, c2, c3, c4, c5, c6, c7, c8 );
  return  *(jdouble*)&u;
}

inline jdouble build_double_from( u1* p ) {
  jlong u = build_long_from( p );
  return  *(jdouble*)&u;
}


// Portable routines to go the other way:

inline void explode_short_to( u2 x, u1& c1, u1& c2 ) {
  c1 = u1(x >> 8);
  c2 = u1(x);
}

inline void explode_short_to( u2 x, u1* p ) {
  explode_short_to( x, p[0], p[1]);
}

inline void explode_int_to( u4 x, u1& c1, u1& c2, u1& c3, u1& c4 ) {
  c1 = u1(x >> 24);
  c2 = u1(x >> 16);
  c3 = u1(x >>  8);
  c4 = u1(x);
}

inline void explode_int_to( u4 x, u1* p ) {
  explode_int_to( x, p[0], p[1], p[2], p[3]);
}


// Pack and extract shorts to/from ints:

inline int extract_low_short_from_int(jint x) {
  return x & 0xffff;
}

inline int extract_high_short_from_int(jint x) {
  return (x >> 16) & 0xffff;
}

inline int build_int_from_shorts( jushort low, jushort high ) {
  return ((int)((unsigned int)high << 16) | (unsigned int)low);
}

// Printf-style formatters for fixed- and variable-width types as pointers and
// integers.
// 
// Each compiler-specific definitions file (e.g., globalDefinitions_gcc.hpp)
// must define the macro FORMAT64_MODIFIER, which is the modifier for '%x' or
// '%d' formats to indicate a 64-bit quantity; commonly "l" (in LP64) or "ll"
// (in ILP32).

// Format 32-bit quantities.
#define INT32_FORMAT  "%d"
#define UINT32_FORMAT "%u"
#define PTR32_FORMAT  "0x%08x"

// Format 64-bit quantities.
#define INT64_FORMAT  "%" FORMAT64_MODIFIER "d"
#define UINT64_FORMAT "%" FORMAT64_MODIFIER "u"
#define PTR64_FORMAT  "0x%016" FORMAT64_MODIFIER "x"

// Format pointers and size_t (or size_t-like integer types) which change size
// between 32- and 64-bit.
#ifdef	_LP64
#define PTR_FORMAT    PTR64_FORMAT
#define UINTX_FORMAT  UINT64_FORMAT
#define INTX_FORMAT   INT64_FORMAT
#define SIZE_FORMAT   UINT64_FORMAT
#define SSIZE_FORMAT  INT64_FORMAT
#else	// !_LP64
#define PTR_FORMAT    PTR32_FORMAT
#define UINTX_FORMAT  UINT32_FORMAT
#define INTX_FORMAT   INT32_FORMAT
#define SIZE_FORMAT   UINT32_FORMAT
#define SSIZE_FORMAT  INT32_FORMAT
#endif	// _LP64

#define INTPTR_FORMAT PTR_FORMAT

// Enable zap-a-lot if in debug version.

# ifdef ASSERT
# ifdef COMPILER2
#   define ENABLE_ZAP_DEAD_LOCALS
# endif
# endif
