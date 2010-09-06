#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)type.hpp	1.137 04/06/04 09:07:31 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style


// This class defines a Type lattice.  The lattice is used in the constant
// propagation algorithms, and for some type-checking of the iloc code.
// Basic types include RSD's (lower bound, upper bound, stride for integers),
// float & double precision constants, sets of data-labels and code-labels.
// The complete lattice is described below.  Subtypes have no relationship to
// up or down in the lattice; that is entirely determined by the behavior of 
// the MEET/JOIN functions.

class Dict;
class Type;
class   TypeD;
class   TypeF;
class   TypeInt;
class   TypeLong;
class   TypeAry;
class   TypeTuple;
class   TypePtr;
class     TypeRawPtr;
class     TypeOopPtr;
class       TypeInstPtr;
class       TypeAryPtr;
class       TypeKlassPtr;

//------------------------------Type-------------------------------------------
// Basic Type object, represents a set of primitive Values.
// Types are hash-cons'd into a private class dictionary, so only one of each
// different kind of Type exists.  Types are never modified after creation, so
// all their interesting fields are constant.
class Type {
public:
  enum TYPES {
    Bad=0,                      // Type check
    Control,                    // Control of code (not in lattice)
    Top,                        // Top of the lattice
    Int,                        // Integer range (lo-hi)
    Long,                       // Long integer range (lo-hi)
    Half,                       // Placeholder half of doubleword

    Tuple,                      // Method signature or object layout
    Array,                      // Array types

    AnyPtr,                     // Any old raw, klass, inst, or array pointer
    RawPtr,                     // Raw (non-oop) pointers
    OopPtr,                     // Any and all Java heap entities
    InstPtr,                    // Instance pointers (non-array objects)
    AryPtr,                     // Array pointers
    KlassPtr,                   // Klass pointers
    // (Ptr order matters:  See is_ptr, isa_ptr, is_oopptr, isa_oopptr.)

    Function,                   // Function signature
    Abio,                       // Abstract I/O
    Return_Address,             // Subroutine return address
    Memory,                     // Abstract store
    FloatTop,                   // No float value
    FloatCon,                   // Floating point constant
    FloatBot,                   // Any float value
    DoubleTop,                  // No double value
    DoubleCon,                  // Double precision constant
    DoubleBot,                  // Any double value
    Bottom,                     // Bottom of lattice
    lastype                     // Bogus ending type (not in lattice)
  };

  // Signal values for offsets from a base pointer
  enum OFFSET_SIGNALS {
    OffsetTop = -2000000000,    // undefined offset
    OffsetBot = -2000000001     // any possible offset
  };

private:
  // Dictionary of types shared among compilations.
  static Dict* _shared_type_dict;

  static int uhash( const Type *const t );
  // Structural equality check.  Assumes that cmp() has already compared
  // the _base types and thus knows it can cast 't' appropriately.
  virtual bool eq( const Type *t ) const;

  // Top-level hash-table of types
  static Dict *type_dict();

  // DUAL operation: reflect around lattice centerline.  Used instead of
  // join to ensure my lattice is symmetric up and down.  Dual is computed
  // lazily, on demand, and cached in _dual.
  const Type *_dual;            // Cached dual value
  // Table for efficient dualing of base types
  static const TYPES dual_type[lastype];

protected:
  // Each class of type is also identified by its base.
  const TYPES _base;            // Enum of Types type

  Type( TYPES t ) : _base(t), _dual(NULL) {} // Simple types
  // ~Type();                   // Use fast deallocation
  const Type *hashcons();       // Hash-cons the type

public:

  inline void*  operator new( size_t x, Compile* C) {
    C->set_type_last_size(x);
    void* temp = C->type_arena()->Amalloc_4(x);
    C->set_type_hwm(temp);
    return temp;
  }
  inline void operator delete( void* ptr, Compile* C) {
      C->type_arena()->Afree(ptr, C->type_last_size());
  }

  // The code is duplicated here because there is a speed
  // penalty if we call the "new" new from here.  These
  // operators will be deleted when the node work is done
  inline void* operator new( size_t x ) {
    Compile* compile = Compile::current();
    compile->set_type_last_size(x);
    void *temp = compile->type_arena()->Amalloc_4(x);
    compile->set_type_hwm(temp);
    return temp;
  }
  inline void operator delete( void* ptr ) {    
    Compile* compile = Compile::current();
    compile->type_arena()->Afree(ptr,compile->type_last_size());
  }  

  // Initialize the type system for a particular compilation.
  static void Initialize(Compile* compile);

  // Initialize the types shared by all compilations.
  static void Initialize_shared(Compile* compile);

  TYPES base() const {
    assert(_base > Bad && _base < lastype, "sanity");
    return _base;
  }

  // Create a new hash-consd type
  static const Type *make(enum TYPES);
  // Test for equivalence of types
  static int cmp( const Type *const t1, const Type *const t2 );
  // Test for higher or equal in lattice
  int higher_equal( const Type *t ) const { return !cmp(meet(t),t); }

  // MEET operation; lower in lattice.
  const Type *meet( const Type *t ) const;
  // WIDEN: 'widens' for Ints and other range types
  virtual const Type *widen( const Type *old ) const { return this; }

  // DUAL operation: reflect around lattice centerline.  Used instead of
  // join to ensure my lattice is symmetric up and down.
  const Type *dual() const { return _dual; }

  // Compute meet dependent on base type
  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  // JOIN operation; higher in lattice.  Done by finding the dual of the 
  // meet of the dual of the 2 inputs.
  const Type *join( const Type *t ) const { 
    return dual()->meet(t->dual())->dual(); }

  // Convenience access
  float getf() const;
  double getd() const;

  const TypeInt    *is_int() const;
  const TypeInt    *isa_int() const;             // Returns NULL if not an Int
  const TypeLong   *is_long() const;
  const TypeLong   *isa_long() const;            // Returns NULL if not a Long
  const TypeD      *is_double_constant() const;  // Asserts it is a DoubleCon
  const TypeD      *isa_double_constant() const; // Returns NULL if not a DoubleCon
  const TypeF      *is_float_constant() const;   // Asserts it is a FloatCon
  const TypeF      *isa_float_constant() const;  // Returns NULL if not a FloatCon
  const TypeTuple  *is_tuple() const;            // Collection of fields, NOT a pointer
  const TypeAry    *is_ary() const;              // Array, NOT array pointer
  const TypePtr    *is_ptr() const;              // Asserts it is a ptr type
  const TypePtr    *isa_ptr() const;             // Returns NULL if not ptr type
  const TypeRawPtr *is_rawptr() const;           // NOT Java oop
  const TypeOopPtr *isa_oopptr() const;          // Returns NULL if not ptr type
  const TypeKlassPtr *isa_klassptr() const; // Returns NULL if not KlassPtr
  const TypeKlassPtr *is_klassptr() const; // assert if not KlassPtr
  const TypeOopPtr  *is_oopptr() const;          // Java-style GC'd pointer
  const TypeInstPtr *isa_instptr() const;        // Returns NULL if not InstPtr
  const TypeInstPtr *is_instptr() const;         // Instance
  const TypeAryPtr *isa_aryptr() const;          // Returns NULL if not AryPtr
  const TypeAryPtr *is_aryptr() const;           // Array oop  
  virtual bool      is_finite() const;           // Has a finite value
  virtual bool      is_nan()    const;           // Is not a number (NaN)

  // Special test for register pressure heuristic
  bool is_floatingpoint() const;        // True if Float or Double base type

  // Do you have memory, directly or through a tuple?
  bool has_memory( ) const;

  // Are you a pointer type or not?
  bool isa_oop_ptr() const;
  
  // TRUE if type is a singleton
  virtual bool singleton(void) const;

  // TRUE if type is above the lattice centerline, and is therefore vacuous
  virtual bool empty(void) const;

  // Return a hash for this type.  The hash function is public so ConNode
  // (constants) can hash on their constant, which is represented by a Type.
  virtual int hash() const;

  // Map ideal registers (machine types) to ideal types
  static const Type *mreg2type[];

  // Printing, statistics
  static const char * const msg[lastype]; // Printable strings  
#ifndef PRODUCT
  void         dump() const;
  virtual void dump2( Dict &d, uint depth ) const;
  static  void dump_stats();
  static  void verify_lastype();          // Check that arrays match type enum
#endif
  void typerr(const Type *t) const; // Mixing types error

  // Create basic type
  static const Type* get_const_basic_type(BasicType type) {
    assert((uint)type <= T_CONFLICT && _const_basic_type[type] != NULL, "bad type");
    return _const_basic_type[type];
  }

  // Create standard type for a ciType:
  static const Type* get_const_type(ciType* type); 

  // Create standard zero value:
  static const Type* get_zero_type(BasicType type) {
    assert((uint)type <= T_CONFLICT && _zero_type[type] != NULL, "bad type");
    return _zero_type[type];
  }

  // Convenience common pre-built types.
  static const Type *ABIO;
  static const Type *BOTTOM;
  static const Type *CONTROL;
  static const Type *DOUBLE;
  static const Type *DOUBLE_TOP;
  static const Type *FLOAT;
  static const Type *FLOAT_TOP;
  static const Type *HALF;  
  static const Type *MEMORY;
  static const Type *MULTI;
  static const Type *RETURN_ADDRESS;
  static const Type *TOP;

  // Mapping from compiler type to VM BasicType
  BasicType basic_type() const { return _basic_type[_base]; }

  // Mapping from CI type system to compiler type:
  static const Type* get_typeflow_type(ciType* type);

private:
  // support arrays
  static const BasicType _basic_type[];
  static const Type*        _zero_type[T_CONFLICT+1];
  static const Type* _const_basic_type[T_CONFLICT+1];
};

//------------------------------TypeF------------------------------------------
// Class of Float-Constant Types.
class TypeF : public Type {
  TypeF( float f ) : Type(FloatCon), _f(f) {};
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
public:
  const float _f;               // Float constant

  static const TypeF *make(float f);

  virtual bool        is_finite() const;  // Has a finite value
  virtual bool        is_nan()    const;  // Is not a number (NaN)

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  // Convenience common pre-built types.
  static const TypeF *ZERO; // positive zero only
  static const TypeF *ONE;
#ifndef PRODUCT
  virtual void dump2(Dict &d, uint depth) const;
#endif
};

//------------------------------TypeD------------------------------------------
// Class of Double-Constant Types.
class TypeD : public Type {
  TypeD( double d ) : Type(DoubleCon), _d(d) {};
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
public:
  const double _d;              // Double constant

  static const TypeD *make(double d);

  virtual bool        is_finite() const;  // Has a finite value
  virtual bool        is_nan()    const;  // Is not a number (NaN)

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  // Convenience common pre-built types.
  static const TypeD *ZERO; // positive zero only
  static const TypeD *ONE;
#ifndef PRODUCT
  virtual void dump2(Dict &d, uint depth) const;
#endif
};

//------------------------------TypeInt----------------------------------------
// Class of integer ranges, the set of integers between a lower bound and an 
// upper bound, inclusive.
class TypeInt : public Type {
  TypeInt( jint lo, jint hi, int w );
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
public:
  const jint _lo, _hi;          // Lower bound, upper bound
  const short _widen;           // Limit on times we widen this sucker

  static const TypeInt *make(jint lo);
  static const TypeInt *make(jint lo, jint hi);
  static const TypeInt *make(jint lo, jint hi, int w);

  // Check for single integer
  int is_con() const { return _lo==_hi; }
  bool is_con(int i) const { return is_con() && _lo == i; }
  jint get_con() const { assert( is_con(), "" );  return _lo; }

  virtual bool        is_finite() const;  // Has a finite value

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  virtual const Type *widen( const Type *t ) const;
  // Convenience common pre-built types.
  static const TypeInt *MINUS_1;
  static const TypeInt *ZERO;
  static const TypeInt *ONE;
  static const TypeInt *BOOL;
  static const TypeInt *CC;
  static const TypeInt *CC_LT;  // [-1]  == MINUS_1
  static const TypeInt *CC_GT;  // [1]   == ONE
  static const TypeInt *CC_EQ;  // [0]   == ZERO
  static const TypeInt *CC_LE;  // [-1,0]
  static const TypeInt *CC_GE;  // [0,1] == BOOL (!)
  static const TypeInt *BYTE;
  static const TypeInt *CHAR;
  static const TypeInt *SHORT;
  static const TypeInt *POS;
  static const TypeInt *INT;
#ifndef PRODUCT
  virtual void dump2(Dict &d, uint depth) const;
#endif
};


//------------------------------TypeLong---------------------------------------
// Class of long integer ranges, the set of integers between a lower bound and 
// an upper bound, inclusive.
class TypeLong : public Type {
  TypeLong( jlong lo, jlong hi, int w );
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
public:
  const jlong _lo, _hi;         // Lower bound, upper bound
  const short _widen;           // Limit on times we widen this sucker

  static const TypeLong *make(jlong lo);
  static const TypeLong *make(jlong lo, jlong hi);
  static const TypeLong *make(jlong lo, jlong hi, int w);

  // Check for single integer
  int is_con() const { return _lo==_hi; }
  jlong get_con() const { assert( is_con(), "" ); return _lo; }

  virtual bool        is_finite() const;  // Has a finite value

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  virtual const Type *widen( const Type *t ) const;
  // Convenience common pre-built types.
  static const TypeLong *MINUS_1;
  static const TypeLong *ZERO;
  static const TypeLong *ONE;
  static const TypeLong *LONG;
  static const TypeLong *INT;  // 32-bit subrange
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const;// Specialized per-Type dumping
#endif
};

//------------------------------TypeTuple--------------------------------------
// Class of Tuple Types, essentially type collections for function signatures
// and class layouts.  It happens to also be a fast cache for the HotSpot
// signature types.
class TypeTuple : public Type {
  TypeTuple( uint cnt, const Type **fields ) : Type(Tuple), _cnt(cnt), _fields(fields) { }
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous

public:
  const uint          _cnt;              // Count of fields
  const Type ** const _fields;           // Array of field types

  // Accessors:
  uint cnt() const { return _cnt; }
  const Type* field_at(uint i) const {
    assert(i < _cnt, "oob");
    return _fields[i];
  }
  void set_field_at(uint i, const Type* t) {
    assert(i < _cnt, "oob");
    _fields[i] = t;
  }

  static const TypeTuple *make( uint cnt, const Type **fields );
  static const TypeTuple *make_range(ciSignature *sig);
  static const TypeTuple *make_domain(ciInstanceKlass* recv, ciSignature *sig);

  // Subroutine call type with space allocated for argument types
  static const Type **fields( uint arg_cnt );

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  // Convenience common pre-built types.
  static const TypeTuple *IFBOTH;
  static const TypeTuple *IFFALSE;
  static const TypeTuple *IFTRUE;
  static const TypeTuple *IFNEITHER;
  static const TypeTuple *LOOPBODY;
  static const TypeTuple *MEMBAR;
  static const TypeTuple *STORECONDITIONAL;
  static const TypeTuple *START_I2C;
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
#endif
};

//------------------------------TypeAry----------------------------------------
// Class of Array Types
class TypeAry : public Type {
  TypeAry( const Type *elem, const TypeInt *size) : Type(Array),
    _elem(elem), _size(size) {}
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous

private:
  const Type *_elem;            // Element type of array
  const TypeInt *_size;         // Elements in array
  friend class TypeAryPtr;

public:
  static const TypeAry *make(  const Type *elem, const TypeInt *size);

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  bool ary_must_be_exact() const;  // true if arrays of such are never generic
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
#endif
};

//------------------------------TypePtr----------------------------------------
// Class of machine Pointer Types: raw data, instances or arrays.
// If the _base enum is AnyPtr, then this refers to all of the above.
// Otherwise the _base will indicate which subset of pointers is affected,
// and the class will be inherited from.
class TypePtr : public Type {
public:
  enum PTR { TopPTR, AnyNull, Constant, Null, NotNull, BotPTR, lastPTR };
protected:
  TypePtr( TYPES t, PTR ptr, int offset ) : Type(t), _ptr(ptr), _offset(offset) {}
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  static const PTR ptr_meet[lastPTR][lastPTR];
  static const PTR ptr_dual[lastPTR];
  static const char * const ptr_msg[lastPTR];
  
public:
  const int _offset;            // Offset into oop, with TOP & BOT
  const PTR _ptr;               // Pointer equivalence class

  const int offset() const { return _offset; }
  const PTR ptr()    const { return _ptr; }

  static const TypePtr *make( TYPES t, PTR ptr, int offset );

  // Return a 'ptr' version of this type
  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual intptr_t get_con() const;

  virtual const TypePtr *add_offset( int offset ) const;

  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
  virtual const Type *xmeet( const Type *t ) const;
  int meet_offset( int offset ) const;
  int dual_offset( ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  // meet, dual and join over pointer equivalence sets
  PTR meet_ptr( const PTR in_ptr ) const { return ptr_meet[in_ptr][ptr()]; }
  PTR dual_ptr()                   const { return ptr_dual[ptr()];      }

  // This is textually confusing unless one recalls that
  // join(t) == dual()->meet(t->dual())->dual().
  PTR join_ptr( const PTR in_ptr ) const {
    return ptr_dual[ ptr_meet[ ptr_dual[in_ptr] ] [ dual_ptr() ] ];
  }

  // Tests for relation to centerline of type lattice:
  static bool above_centerline(PTR ptr) { return (ptr <= AnyNull); }
  static bool below_centerline(PTR ptr) { return (ptr >= NotNull); }
  // Convenience common pre-built types.
  static const TypePtr *NULL_PTR;
  static const TypePtr *NOTNULL;
  static const TypePtr *BOTTOM;
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint depth ) const;
#endif
};

//------------------------------TypeRawPtr-------------------------------------
// Class of raw pointers, pointers to things other than Oops.  Examples
// include the stack pointer, top of heap, card-marking area, handles, etc.
class TypeRawPtr : public TypePtr {
protected:
  TypeRawPtr( PTR ptr, address bits ) : TypePtr(RawPtr,ptr,0), _bits(bits){}
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;     // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous

  const address _bits;          // Constant value, if applicable

  static const TypeRawPtr *make( PTR ptr );
  static const TypeRawPtr *make( address bits );

  // Return a 'ptr' version of this type
  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual intptr_t get_con() const;

  virtual const TypePtr *add_offset( int offset ) const;

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.
  // Convenience common pre-built types.
  static const TypeRawPtr *BOTTOM;
  static const TypeRawPtr *NOTNULL;
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint depth ) const;
#endif
};

//------------------------------TypeOopPtr-------------------------------------
// Some kind of oop (Java pointer), either klass or instance or array.
class TypeOopPtr : public TypePtr {
protected:
  TypeOopPtr( TYPES t, PTR ptr, ciKlass* k, bool xk, ciObject* o, int offset ) : TypePtr(t, ptr, offset), _const_oop(o), _klass(k), _klass_is_exact(xk) { }
public:
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
protected:

  int xadd_offset( int offset ) const;
  // Oop is NULL, unless this is a constant oop.
  ciObject*     _const_oop;   // Constant oop
  // If _klass is NULL, then so is _sig.  This is an unloaded klass.
  ciKlass*      _klass;       // Klass object
  // Does the type exclude subclasses of the klass?  (Inexact == polymorphic.)
  bool          _klass_is_exact;

  static const TypeOopPtr* make_from_klass_common(ciKlass* klass, bool klass_change, bool try_for_exact);

public:
  // Creates a type given a klass. Correctly handles multi-dimensional arrays
  // Respects UseUniqueSubclasses.
  // If the klass is final, the resulting type will be exact.
  static const TypeOopPtr* make_from_klass(ciKlass* klass) {
    return make_from_klass_common(klass, true, false);
  }
  // Same as before, but will produce an exact type, even if
  // the klass is not final, as long as it has exactly one implementation.
  static const TypeOopPtr* make_from_klass_unique(ciKlass* klass) {
    return make_from_klass_common(klass, true, true);
  }
  // Same as before, but does not respects UseUniqueSubclasses.
  // Use this only for creating array element types.
  static const TypeOopPtr* make_from_klass_raw(ciKlass* klass) {
    return make_from_klass_common(klass, false, false);
  }
  // Creates a singleton type given an object.
  static const TypeOopPtr* make_from_constant(ciObject* o);

  // Make a generic (unclassed) pointer to an oop.
  static const TypeOopPtr* make(PTR ptr, int offset);

  ciObject* const_oop()    const { return _const_oop; }
  virtual ciKlass* klass() const { return _klass;     } 
  bool klass_is_exact()    const { return _klass_is_exact; }

  virtual intptr_t get_con() const;

  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual const Type *cast_to_exactness(bool klass_is_exact) const;

  virtual const TypePtr *add_offset( int offset ) const;

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  // Convenience common pre-built type.
  static const TypeOopPtr *BOTTOM;
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint depth ) const;
#endif
};

//------------------------------TypeInstPtr------------------------------------
// Class of Java object pointers, pointing either to non-array Java instances
// or to a klassOop (including array klasses).
class TypeInstPtr : public TypeOopPtr {
  TypeInstPtr( PTR ptr, ciKlass* k, bool xk, ciObject* o, int offset );
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing

  ciSymbol*  _name;        // class name

 public:
  ciSymbol* name()         const { return _name; }

  bool  is_loaded() const { return _klass->is_loaded(); }

  // Make a pointer to a constant oop.
  static const TypeInstPtr *make(ciObject* o) {
    return make(TypePtr::Constant, o->klass(), true, o, 0);
  }

  // Make a pointer to a constant oop with offset.
  static const TypeInstPtr *make(ciObject* o, int offset) {
    return make(TypePtr::Constant, o->klass(), true, o, offset);
  }

  // Make a pointer to some value of type klass.
  static const TypeInstPtr *make(PTR ptr, ciKlass* klass) {
    return make(ptr, klass, false, NULL, 0);
  }

  // Make a pointer to some non-polymorphic value of exactly type klass.
  static const TypeInstPtr *make_exact(PTR ptr, ciKlass* klass) {
    return make(ptr, klass, true, NULL, 0);
  }

  // Make a pointer to some value of type klass with offset.
  static const TypeInstPtr *make(PTR ptr, ciKlass* klass, int offset) {
    return make(ptr, klass, false, NULL, offset);
  }

  // Make a pointer to an oop.
  static const TypeInstPtr *make(PTR ptr, ciKlass* k, bool xk, ciObject* o, int offset );

  // If this is a java.lang.Class constant, return the type for it or NULL.
  // Pass to Type::get_const_type to turn it to a type, which will usually
  // be a TypeInstPtr, but may also be a TypeInt::INT for int.class, etc.
  ciType* mirror_type() const;

  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual const Type *cast_to_exactness(bool klass_is_exact) const;

  virtual const TypePtr *add_offset( int offset ) const;

  virtual const Type *xmeet( const Type *t ) const;
  virtual const TypeInstPtr *xmeet_unloaded( const TypeInstPtr *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  // Convenience common pre-built types.
  static const TypeInstPtr *NOTNULL;
  static const TypeInstPtr *BOTTOM;
  static const TypeInstPtr *MARK;
  static const TypeInstPtr *KLASS;
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
#endif
};

//------------------------------TypeAryPtr-------------------------------------
// Class of Java array pointers
class TypeAryPtr : public TypeOopPtr {
  TypeAryPtr( PTR ptr, ciObject* o, const TypeAry *ary, ciKlass* k, bool xk, int offset ) : TypeOopPtr(AryPtr,ptr,k,xk,o,offset), _ary(ary) {};
  virtual int eq( const Type *t ) const;
  virtual int hash() const;     // Type specific hashing
  const TypeAry *_ary;          // Array we point into

public:
  // Accessors
  ciKlass* klass() const;
  const TypeAry* ary() const  { return _ary; }
  const Type*    elem() const { return _ary->_elem; }
  const TypeInt* size() const { return _ary->_size; }

  static const TypeAryPtr *make( PTR ptr, const TypeAry *ary, ciKlass* k, bool xk, int offset);
  // Constant pointer to array
  static const TypeAryPtr *make( PTR ptr, ciObject* o, const TypeAry *ary, ciKlass* k, bool xk, int offset);

  // Convenience
  static const TypeAryPtr *make(ciObject* o);

  // Return a 'ptr' version of this type
  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual const Type *cast_to_exactness(bool klass_is_exact) const;

  virtual int empty(void) const;        // TRUE if type is vacuous
  virtual const TypePtr *add_offset( int offset ) const;

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  // Convenience common pre-built types.
  static const TypeAryPtr *RANGE;
  static const TypeAryPtr *OOPS;
  static const TypeAryPtr *BYTES;
  static const TypeAryPtr *SHORTS;
  static const TypeAryPtr *CHARS;
  static const TypeAryPtr *INTS;
  static const TypeAryPtr *LONGS;
  static const TypeAryPtr *FLOATS;
  static const TypeAryPtr *DOUBLES;
  // selects one of the above:
  static const TypeAryPtr *get_array_body_type(BasicType elem) {
    assert((uint)elem <= T_CONFLICT && _array_body_type[elem] != NULL, "bad elem type");
    return _array_body_type[elem];
  }
  static const TypeAryPtr *_array_body_type[T_CONFLICT+1];
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
#endif
};

//------------------------------TypeKlassPtr-----------------------------------
// Class of Java Klass pointers
class TypeKlassPtr : public TypeOopPtr {
  TypeKlassPtr( PTR ptr, ciKlass* klass, int offset );

  virtual int eq( const Type *t ) const;
  virtual int hash() const;             // Type specific hashing

public:
  ciSymbol* name()  const { return _klass->name(); }

  // ptr to klass 'k'
  static const TypeKlassPtr *make( ciKlass* k ) { return make( TypePtr::Constant, k, 0); }
  // ptr to klass 'k' with offset
  static const TypeKlassPtr *make( ciKlass* k, int offset ) { return make( TypePtr::Constant, k, offset); }
  // ptr to klass 'k' or sub-klass
  static const TypeKlassPtr *make( PTR ptr, ciKlass* k, int offset);

  virtual const Type *cast_to_ptr_type(PTR ptr) const;

  virtual const Type *cast_to_exactness(bool klass_is_exact) const;

  virtual const TypePtr *add_offset( int offset ) const;
  virtual const Type    *xmeet( const Type *t ) const;
  virtual const Type    *xdual() const;      // Compute dual right now.

  // Convenience common pre-built types.
  static const TypeKlassPtr *OBJECT; // Not-null object klass or below
#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
#endif
};

//------------------------------TypeFunc---------------------------------------
// Class of Array Types
class TypeFunc : public Type {
  TypeFunc( const TypeTuple *domain, const TypeTuple *range ) : Type(Function),  _domain(domain), _range(range) {}
  virtual bool eq( const Type *t ) const;
  virtual int  hash() const;             // Type specific hashing
  virtual bool singleton(void) const;    // TRUE if type is a singleton
  virtual bool empty(void) const;        // TRUE if type is vacuous
public:
  // Constants are shared among ADLC and VM
  enum { Control    = AdlcVMDeps::Control, 
         I_O        = AdlcVMDeps::I_O, 
         Memory     = AdlcVMDeps::Memory, 
         FramePtr   = AdlcVMDeps::FramePtr, 
         ReturnAdr  = AdlcVMDeps::ReturnAdr, 
         Parms      = AdlcVMDeps::Parms
  };

  const TypeTuple* const _domain;     // Domain of inputs
  const TypeTuple* const _range;      // Range of results

  // Accessors:
  const TypeTuple* domain() const { return _domain; }
  const TypeTuple* range()  const { return _range; }

  static const TypeFunc *make(ciMethod* method);
  static const TypeFunc *make(ciSignature signature, const Type* extra);
  static const TypeFunc *make(const TypeTuple* domain, const TypeTuple* range);

  virtual const Type *xmeet( const Type *t ) const;
  virtual const Type *xdual() const;    // Compute dual right now.

  bool returns_long() const;

#ifndef PRODUCT
  virtual void dump2( Dict &d, uint ) const; // Specialized per-Type dumping
  void print_flattened() const; // Print a 'flattened' signature
#endif
  // Convenience common pre-built types.
};

//------------------------------accessors--------------------------------------
inline float Type::getf() const { 
  assert( _base == FloatCon, "Not a FloatCon" ); 
  return ((TypeF*)this)->_f;
}

inline double Type::getd() const { 
  assert( _base == DoubleCon, "Not a DoubleCon" ); 
  return ((TypeD*)this)->_d; 
}

inline const TypeF *Type::is_float_constant() const { 
  assert( _base == FloatCon, "Not a Float" ); 
  return (TypeF*)this; 
}

inline const TypeF *Type::isa_float_constant() const {
  return ( _base == FloatCon ? (TypeF*)this : NULL);
}

inline const TypeD *Type::is_double_constant() const { 
  assert( _base == DoubleCon, "Not a Double" ); 
  return (TypeD*)this; 
}

inline const TypeD *Type::isa_double_constant() const {
  return ( _base == DoubleCon ? (TypeD*)this : NULL);
}

inline const TypeInt *Type::is_int() const { 
  assert( _base == Int, "Not an Int" ); 
  return (TypeInt*)this; 
}

inline const TypeInt *Type::isa_int() const { 
  return ( _base == Int ? (TypeInt*)this : NULL); 
}

inline const TypeLong *Type::is_long() const { 
  assert( _base == Long, "Not a Long" ); 
  return (TypeLong*)this; 
}

inline const TypeLong *Type::isa_long() const { 
  return ( _base == Long ? (TypeLong*)this : NULL);
}

inline const TypeTuple *Type::is_tuple() const { 
  assert( _base == Tuple, "Not a Tuple" ); 
  return (TypeTuple*)this; 
}

inline const TypeAry *Type::is_ary() const { 
  assert( _base == Array , "Not an Array" ); 
  return (TypeAry*)this; 
}

inline const TypePtr *Type::is_ptr() const { 
  // AnyPtr is the first Ptr and KlassPtr the last, with no non-ptrs between.
  assert(_base >= AnyPtr && _base <= KlassPtr, "Not a pointer");
  return (TypePtr*)this; 
}

inline const TypePtr *Type::isa_ptr() const { 
  // AnyPtr is the first Ptr and KlassPtr the last, with no non-ptrs between.
  return (_base >= AnyPtr && _base <= KlassPtr) ? (TypePtr*)this : NULL;
}

inline const TypeOopPtr *Type::is_oopptr() const { 
  // OopPtr is the first and KlassPtr the last, with no non-oops between.
  assert(_base >= OopPtr && _base <= KlassPtr, "Not a Java pointer" ) ;
  return (TypeOopPtr*)this;
}

inline const TypeOopPtr *Type::isa_oopptr() const { 
  // OopPtr is the first and KlassPtr the last, with no non-oops between.
  return (_base >= OopPtr && _base <= KlassPtr) ? (TypeOopPtr*)this : NULL;
}

inline const TypeRawPtr *Type::is_rawptr() const { 
  assert( _base == RawPtr, "Not a raw pointer" ); 
  return (TypeRawPtr*)this; 
}

inline const TypeInstPtr *Type::isa_instptr() const { 
  return (_base == InstPtr) ? (TypeInstPtr*)this : NULL;
}

inline const TypeInstPtr *Type::is_instptr() const { 
  assert( _base == InstPtr, "Not an object pointer" ); 
  return (TypeInstPtr*)this; 
}

inline const TypeAryPtr *Type::isa_aryptr() const { 
  return (_base == AryPtr) ? (TypeAryPtr*)this : NULL;
}

inline const TypeAryPtr *Type::is_aryptr() const { 
  assert( _base == AryPtr, "Not an array pointer" ); 
  return (TypeAryPtr*)this; 
}

inline const TypeKlassPtr *Type::isa_klassptr() const {
  return (_base == KlassPtr) ? (TypeKlassPtr*)this : NULL;
}

inline const TypeKlassPtr *Type::is_klassptr() const {
  assert( _base == KlassPtr, "Not a klass pointer" );
  return (TypeKlassPtr*)this;
}

inline bool Type::is_floatingpoint() const {
  if( (_base == FloatCon)  || (_base == FloatBot) ||
      (_base == DoubleCon) || (_base == DoubleBot) )
    return true;
  return false;
}


// ===============================================================
// Things that need to be 64-bits in the 64-bit build but
// 32-bits in the 32-bit build.  Done this way to get full
// optimization AND strong typing.
#ifdef _LP64

// For type queries and asserts
#define is_intptr_t  is_long
#define isa_intptr_t isa_long
#define TypeX        TypeLong
#define TypeX_X      TypeLong::LONG
#define TypeX_ZERO   TypeLong::ZERO
// For 'ideal_reg' machine registers 
#define Op_RegX      Op_RegL
// For phase->intcon variants
#define MakeConX     longcon
#define ConXNode     ConLNode
// For array index arithmetic
#define MulXNode     MulLNode
#define AndXNode     AndLNode
#define CmpXNode     CmpLNode
#define SubXNode     SubLNode
#define LShiftXNode  LShiftLNode
// For card marks and hashcodes
#define CastP2XNode  CastP2LNode
#define URShiftXNode URShiftLNode
// Opcodes
#define Op_LShiftX   Op_LShiftL
#define Op_AddX      Op_AddL

#else

// For type queries and asserts
#define is_intptr_t  is_int
#define isa_intptr_t isa_int
#define TypeX        TypeInt
#define TypeX_X      TypeInt::INT
#define TypeX_ZERO   TypeInt::ZERO
// For 'ideal_reg' machine registers
#define Op_RegX      Op_RegI
// For phase->intcon variants
#define MakeConX     intcon
#define ConXNode     ConINode
// For array index arithmetic
#define MulXNode     MulINode
#define AndXNode     AndINode
#define CmpXNode     CmpINode
#define SubXNode     SubINode
#define LShiftXNode  LShiftINode
// For card marks and hashcodes
#define CastP2XNode  CastP2INode
#define URShiftXNode URShiftINode
// Opcodes
#define Op_LShiftX   Op_LShiftI
#define Op_AddX      Op_AddI

#endif

