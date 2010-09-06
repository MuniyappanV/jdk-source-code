#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)type.cpp	1.231 04/03/12 18:47:42 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style

#include "incls/_precompiled.incl"
#include "incls/_type.cpp.incl"

// Dictionary of types shared among compilations.
Dict* Type::_shared_type_dict = NULL;

// Array which maps compiler types to Basic Types
const BasicType Type::_basic_type[Type::lastype] = {
  T_ILLEGAL,    // Bad
  T_ILLEGAL,    // Control
  T_VOID,       // Top
  T_INT,        // Int
  T_LONG,       // Long
  T_VOID,       // Half

  T_ILLEGAL,    // Tuple
  T_ARRAY,      // Array

  T_ADDRESS,    // AnyPtr   // shows up in factory methods for NULL_PTR
  T_ADDRESS,    // RawPtr
  T_OBJECT,     // OopPtr
  T_OBJECT,     // InstPtr
  T_OBJECT,     // AryPtr
  T_OBJECT,     // KlassPtr

  T_OBJECT,     // Function
  T_ILLEGAL,    // Abio
  T_ADDRESS,    // Return_Address
  T_ILLEGAL,    // Memory
  T_FLOAT,      // FloatTop
  T_FLOAT,      // FloatCon
  T_FLOAT,      // FloatBot
  T_DOUBLE,     // DoubleTop
  T_DOUBLE,     // DoubleCon
  T_DOUBLE,     // DoubleBot
  T_ILLEGAL,    // Bottom
};

// Map ideal registers (machine types) to ideal types
const Type *Type::mreg2type[_last_machine_leaf];

// Map basic types to canonical Type* pointers.
const Type* Type::     _const_basic_type[T_CONFLICT+1];

// Map basic types to constant-zero Types.
const Type* Type::            _zero_type[T_CONFLICT+1];

// Map basic types to array-body alias types.
const TypeAryPtr* TypeAryPtr::_array_body_type[T_CONFLICT+1];

//=============================================================================
// Convenience common pre-built types.
const Type *Type::ABIO;         // State-of-machine only
const Type *Type::BOTTOM;       // All values
const Type *Type::CONTROL;      // Control only
const Type *Type::DOUBLE;       // All doubles
const Type *Type::DOUBLE_TOP;   // All doubles
const Type *Type::FLOAT;        // All floats
const Type *Type::FLOAT_TOP;    // All floats
const Type *Type::HALF;         // Placeholder half of doublewide type
const Type *Type::MEMORY;       // Abstract store only
const Type *Type::RETURN_ADDRESS;
const Type *Type::TOP;          // No values in set

//------------------------------get_const_type---------------------------
const Type* Type::get_const_type(ciType* type) {
  if (type == NULL) {
    return NULL;
  } else if (type->is_primitive_type()) {
    return get_const_basic_type(type->basic_type());
  } else {
    return TypeOopPtr::make_from_klass(type->as_klass());
  }
}


//---------------------------get_typeflow_type---------------------------------
// Import a type produced by ciTypeFlow.
const Type* Type::get_typeflow_type(ciType* type) {
  switch (type->basic_type()) {

  case ciTypeFlow::StateVector::T_BOTTOM:
    assert(type == ciTypeFlow::StateVector::bottom_type(), "");
    return Type::BOTTOM;

  case ciTypeFlow::StateVector::T_TOP:
    assert(type == ciTypeFlow::StateVector::top_type(), "");
    return Type::TOP;

  case ciTypeFlow::StateVector::T_NULL:
    assert(type == ciTypeFlow::StateVector::null_type(), "");
    return TypePtr::NULL_PTR;

  case ciTypeFlow::StateVector::T_LONG2:
    // The ciTypeFlow pass pushes a long, then the half.
    // We do the same.
    assert(type == ciTypeFlow::StateVector::long2_type(), "");
    return TypeInt::TOP;

  case ciTypeFlow::StateVector::T_DOUBLE2:
    // The ciTypeFlow pass pushes double, then the half.
    // Our convention is the same.
    assert(type == ciTypeFlow::StateVector::double2_type(), "");
    return Type::TOP;

  case T_ADDRESS:
    assert(type->is_return_address(), "");
    return TypeRawPtr::make((address) type->as_return_address()->bci());

  default:
    // make sure we did not mix up the cases:
    assert(type != ciTypeFlow::StateVector::bottom_type(), "");
    assert(type != ciTypeFlow::StateVector::top_type(), "");
    assert(type != ciTypeFlow::StateVector::null_type(), "");
    assert(type != ciTypeFlow::StateVector::long2_type(), "");
    assert(type != ciTypeFlow::StateVector::double2_type(), "");
    assert(!type->is_return_address(), "");

    return Type::get_const_type(type);
  }
}


//------------------------------make-------------------------------------------
// Create a simple Type, with default empty symbol sets.  Then hashcons it
// and look for an existing copy in the type dictionary.
const Type *Type::make( enum TYPES t ) {
  return (new Type(t))->hashcons();
}

//------------------------------cmp--------------------------------------------
int Type::cmp( const Type *const t1, const Type *const t2 ) {
  if( t1->_base != t2->_base ) 
    return 1;                   // Missed badly
  assert(t1 != t2 || t1->eq(t2), "eq must be reflexive");
  return !t1->eq(t2);           // Return ZERO if equal
}

//------------------------------hash-------------------------------------------
int Type::uhash( const Type *const t ) {
  return t->hash();
}

//--------------------------Initialize_shared----------------------------------
void Type::Initialize_shared(Compile* current) {
  // This method does not need to be locked because the first system
  // compilations (stub compilations) occur serially.  If they are
  // changed to proceed in parallel, then this section will need
  // locking.
  
  Arena* save = current->type_arena();
  Arena* shared_type_arena = new Arena();

  current->set_type_arena(shared_type_arena);
  _shared_type_dict =
    new (shared_type_arena) Dict( (CmpKey)Type::cmp, (Hash)Type::uhash,
                                  shared_type_arena, 128 );
  current->set_type_dict(_shared_type_dict);

  // Make shared pre-built types.
  CONTROL = make(Control);      // Control only
  TOP     = make(Top);          // No values in set
  MEMORY  = make(Memory);       // Abstract store only
  ABIO    = make(Abio);         // State-of-machine only
  RETURN_ADDRESS=make(Return_Address);
  FLOAT   = make(FloatBot);     // All floats
  FLOAT_TOP=make(FloatTop);     // All floats
  DOUBLE  = make(DoubleBot);    // All doubles                          
  DOUBLE_TOP=make(DoubleTop);   // All doubles                          
  BOTTOM  = make(Bottom);       // Everything
  HALF    = make(Half);         // Placeholder half of doublewide type

  TypeF::ZERO = TypeF::make(0.0); // Float 0 (positive zero)
  TypeF::ONE  = TypeF::make(1.0); // Float 1

  TypeD::ZERO = TypeD::make(0.0); // Double 0 (positive zero)
  TypeD::ONE  = TypeD::make(1.0); // Double 1

  TypeInt::MINUS_1 = TypeInt::make(-1);  // -1
  TypeInt::ZERO    = TypeInt::make( 0);  //  0
  TypeInt::ONE     = TypeInt::make( 1);  //  1
  TypeInt::BOOL    = TypeInt::make(0,1); // 0 or 1, FALSE or TRUE.
  TypeInt::CC      = TypeInt::make(-1, 1);  // -1, 0 or 1, condition codes
  TypeInt::CC_LT   = TypeInt::make(-1,-1);  // == TypeInt::MINUS_1
  TypeInt::CC_GT   = TypeInt::make( 1, 1);  // == TypeInt::ONE
  TypeInt::CC_EQ   = TypeInt::make( 0, 0);  // == TypeInt::ZERO
  TypeInt::CC_LE   = TypeInt::make(-1, 0);
  TypeInt::CC_GE   = TypeInt::make( 0, 1);  // == TypeInt::BOOL
  TypeInt::BYTE    = TypeInt::make(-128,127); // Bytes
  TypeInt::CHAR    = TypeInt::make(0,65535); // Java chars
  TypeInt::SHORT   = TypeInt::make(-32768,32767); // Java shorts
  TypeInt::POS     = TypeInt::make(0,max_jint); // Positive values
  TypeInt::INT     = TypeInt::make(min_jint,max_jint,3); // 32-bit integers
  // CmpL is overloaded both as the bytecode computation returning
  // a trinary (-1,0,+1) integer result AND as an efficient long
  // compare returning optimizer ideal-type flags.
  assert( TypeInt::CC_LT == TypeInt::MINUS_1, "types must match for CmpL to work" );
  assert( TypeInt::CC_GT == TypeInt::ONE,     "types must match for CmpL to work" );
  assert( TypeInt::CC_EQ == TypeInt::ZERO,    "types must match for CmpL to work" );
  assert( TypeInt::CC_LE == TypeInt::make(-1,0), "types must match for CmpL to work" );
  assert( TypeInt::CC_GE == TypeInt::BOOL,    "types must match for CmpL to work" );

  TypeLong::MINUS_1 = TypeLong::make(-1);        // -1
  TypeLong::ZERO    = TypeLong::make( 0);        //  0
  TypeLong::ONE     = TypeLong::make( 1);        //  1
  TypeLong::LONG    = TypeLong::make(min_jlong,max_jlong,3); // 64-bit integers
  TypeLong::INT     = TypeLong::make((jlong)min_jint,(jlong)max_jint,3); // 32-bit subrange

  const Type **fboth =(const Type**)shared_type_arena->Amalloc_4(2*sizeof(Type*));
  fboth[0] = Type::CONTROL;
  fboth[1] = Type::CONTROL;
  TypeTuple::IFBOTH = TypeTuple::make( 2, fboth );

  const Type **ffalse =(const Type**)shared_type_arena->Amalloc_4(2*sizeof(Type*));
  ffalse[0] = Type::CONTROL;
  ffalse[1] = Type::TOP;
  TypeTuple::IFFALSE = TypeTuple::make( 2, ffalse );

  const Type **fneither =(const Type**)shared_type_arena->Amalloc_4(2*sizeof(Type*));
  fneither[0] = Type::TOP;
  fneither[1] = Type::TOP;
  TypeTuple::IFNEITHER = TypeTuple::make( 2, fneither );

  const Type **ftrue =(const Type**)shared_type_arena->Amalloc_4(2*sizeof(Type*));
  ftrue[0] = Type::TOP;
  ftrue[1] = Type::CONTROL;
  TypeTuple::IFTRUE = TypeTuple::make( 2, ftrue );

  const Type **floop =(const Type**)shared_type_arena->Amalloc_4(2*sizeof(Type*));
  floop[0] = Type::CONTROL;
  floop[1] = TypeInt::INT;
  TypeTuple::LOOPBODY = TypeTuple::make( 2, floop );

  TypePtr::NULL_PTR= TypePtr::make( AnyPtr, TypePtr::Null, 0 );
  TypePtr::NOTNULL = TypePtr::make( AnyPtr, TypePtr::NotNull, OffsetBot );
  TypePtr::BOTTOM  = TypePtr::make( AnyPtr, TypePtr::BotPTR, OffsetBot );

  TypeRawPtr::BOTTOM = TypeRawPtr::make( TypePtr::BotPTR );
  TypeRawPtr::NOTNULL= TypeRawPtr::make( TypePtr::NotNull );

  mreg2type[Op_Node] = Type::BOTTOM;
  mreg2type[Op_Set ] = 0;
  mreg2type[Op_RegI] = TypeInt::INT;
  mreg2type[Op_RegP] = TypePtr::BOTTOM;
  mreg2type[Op_RegF] = Type::FLOAT;
  mreg2type[Op_RegD] = Type::DOUBLE;
  mreg2type[Op_RegL] = TypeLong::LONG;
  mreg2type[Op_RegFlags] = TypeInt::CC;

  const Type **fmembar = TypeTuple::fields(0);
  TypeTuple::MEMBAR = TypeTuple::make(TypeFunc::Parms+0, fmembar);

  const Type **fsc = (const Type **)(Compile::current()->type_arena()->Amalloc_4((2)*sizeof(Type*) ));
  fsc[0] = TypeInt::CC;
  fsc[1] = Type::MEMORY;
  TypeTuple::STORECONDITIONAL = TypeTuple::make(2, fsc);

  TypeInstPtr::NOTNULL = TypeInstPtr::make(TypePtr::NotNull, current->env()->Object_klass());
  TypeInstPtr::BOTTOM  = TypeInstPtr::make(TypePtr::BotPTR,  current->env()->Object_klass());
  TypeInstPtr::MARK    = TypeInstPtr::make(TypePtr::BotPTR,  current->env()->Object_klass(),
                                           false, 0, oopDesc::mark_offset_in_bytes());
  TypeInstPtr::KLASS   = TypeInstPtr::make(TypePtr::BotPTR,  current->env()->Object_klass(),
                                           false, 0, oopDesc::klass_offset_in_bytes());
  TypeOopPtr::BOTTOM  = TypeOopPtr::make(TypePtr::BotPTR, OffsetBot);

  TypeAryPtr::RANGE   = TypeAryPtr::make( TypePtr::BotPTR, TypeAry::make(Type::BOTTOM,TypeInt::POS), current->env()->Object_klass(), false, arrayOopDesc::length_offset_in_bytes());
  // There is no shared klass for Object[].  See note in TypeAryPtr::klass().
  TypeAryPtr::OOPS    = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeInstPtr::BOTTOM,TypeInt::POS), NULL /*ciArrayKlass::make(o)*/,  false,  Type::OffsetBot);
  TypeAryPtr::BYTES   = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeInt::BYTE      ,TypeInt::POS), ciTypeArrayKlass::make(T_BYTE),   true,  Type::OffsetBot);
  TypeAryPtr::SHORTS  = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeInt::SHORT     ,TypeInt::POS), ciTypeArrayKlass::make(T_SHORT),  true,  Type::OffsetBot);
  TypeAryPtr::CHARS   = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeInt::CHAR      ,TypeInt::POS), ciTypeArrayKlass::make(T_CHAR),   true,  Type::OffsetBot);
  TypeAryPtr::INTS    = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeInt::INT       ,TypeInt::POS), ciTypeArrayKlass::make(T_INT),    true,  Type::OffsetBot);
  TypeAryPtr::LONGS   = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(TypeLong::LONG     ,TypeInt::POS), ciTypeArrayKlass::make(T_LONG),   true,  Type::OffsetBot);
  TypeAryPtr::FLOATS  = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(Type::FLOAT        ,TypeInt::POS), ciTypeArrayKlass::make(T_FLOAT),  true,  Type::OffsetBot);
  TypeAryPtr::DOUBLES = TypeAryPtr::make(TypePtr::BotPTR, TypeAry::make(Type::DOUBLE       ,TypeInt::POS), ciTypeArrayKlass::make(T_DOUBLE), true,  Type::OffsetBot);

  TypeAryPtr::_array_body_type[T_OBJECT]  = TypeAryPtr::OOPS;
  TypeAryPtr::_array_body_type[T_ARRAY]   = TypeAryPtr::OOPS;   // arrays are stored in oop arrays
  TypeAryPtr::_array_body_type[T_BYTE]    = TypeAryPtr::BYTES;
  TypeAryPtr::_array_body_type[T_BOOLEAN] = TypeAryPtr::BYTES;  // boolean[] is a byte array
  TypeAryPtr::_array_body_type[T_SHORT]   = TypeAryPtr::SHORTS;
  TypeAryPtr::_array_body_type[T_CHAR]    = TypeAryPtr::CHARS;
  TypeAryPtr::_array_body_type[T_INT]     = TypeAryPtr::INTS;
  TypeAryPtr::_array_body_type[T_LONG]    = TypeAryPtr::LONGS;
  TypeAryPtr::_array_body_type[T_FLOAT]   = TypeAryPtr::FLOATS;
  TypeAryPtr::_array_body_type[T_DOUBLE]  = TypeAryPtr::DOUBLES;

  TypeKlassPtr::OBJECT = TypeKlassPtr::make( TypePtr::NotNull, current->env()->Object_klass(), 0 );

  const Type **fi2c = TypeTuple::fields(2);
  fi2c[TypeFunc::Parms+0] = TypeInstPtr::BOTTOM; // methodOop
  fi2c[TypeFunc::Parms+1] = TypeRawPtr::BOTTOM; // argument pointer
  TypeTuple::START_I2C = TypeTuple::make(TypeFunc::Parms+2, fi2c);

  _const_basic_type[T_BOOLEAN] = TypeInt::BOOL;
  _const_basic_type[T_CHAR]    = TypeInt::CHAR;
  _const_basic_type[T_BYTE]    = TypeInt::BYTE;
  _const_basic_type[T_SHORT]   = TypeInt::SHORT;
  _const_basic_type[T_INT]     = TypeInt::INT;
  _const_basic_type[T_LONG]    = TypeLong::LONG;
  _const_basic_type[T_FLOAT]   = Type::FLOAT; 
  _const_basic_type[T_DOUBLE]  = Type::DOUBLE; 
  _const_basic_type[T_OBJECT]  = TypeInstPtr::BOTTOM;
  _const_basic_type[T_ARRAY]   = TypeInstPtr::BOTTOM; // there is no separate bottom for arrays
  _const_basic_type[T_VOID]    = TypePtr::NULL_PTR;   // reflection represents void this way
  _const_basic_type[T_ADDRESS] = TypeRawPtr::BOTTOM;  // both interpreter return addresses & random raw ptrs
  _const_basic_type[T_CONFLICT]= Type::BOTTOM;        // why not?

  _zero_type[T_BOOLEAN] = TypeInt::ZERO;     // false == 0
  _zero_type[T_CHAR]    = TypeInt::ZERO;     // '\0' == 0
  _zero_type[T_BYTE]    = TypeInt::ZERO;     // 0x00 == 0
  _zero_type[T_SHORT]   = TypeInt::ZERO;     // 0x0000 == 0
  _zero_type[T_INT]     = TypeInt::ZERO;
  _zero_type[T_LONG]    = TypeLong::ZERO;
  _zero_type[T_FLOAT]   = TypeF::ZERO; 
  _zero_type[T_DOUBLE]  = TypeD::ZERO; 
  _zero_type[T_OBJECT]  = TypePtr::NULL_PTR;
  _zero_type[T_ARRAY]   = TypePtr::NULL_PTR; // null array is null oop
  _zero_type[T_VOID]    = Type::TOP;         // the only void value is no value at all

  // get_zero_type() should not happen for either T_ADDRESS or T_CONFLICT
  _zero_type[T_ADDRESS] = NULL;
  _zero_type[T_CONFLICT]= NULL;

  // Restore working type arena.
  current->set_type_arena(save);
  current->set_type_dict(NULL);
}

//------------------------------Initialize-------------------------------------
void Type::Initialize(Compile* current) {
  assert(current->type_arena() != NULL, "must have created type arena");

  if (_shared_type_dict == NULL) {
    Initialize_shared(current);
  }

  Arena* type_arena = current->type_arena();

  // Create the hash-cons'ing dictionary with top-level storage allocation
  current->set_type_dict(new (type_arena) Dict( (CmpKey)Type::cmp,(Hash)Type::uhash, type_arena, 128 ));

  // Transfer the shared types.
  DictI i(_shared_type_dict);
  for( ; i.test(); ++i ) {
    Type* t = (Type*)i._value;
    type_dict()->Insert(t,t);  // New Type, insert into Type table
  }
}

//------------------------------type_dict--------------------------------------
// Top-level hash-table of types
Dict *Type::type_dict() {
  return Compile::current()->type_dict();
}

//------------------------------hashcons---------------------------------------
// Do the hash-cons trick.  If the Type already exists in the type table,
// delete the current Type and return the existing Type.  Otherwise stick the
// current Type in the Type table.
const Type *Type::hashcons(void) {
  debug_only(base());           // Check the assertion in Type::base().
  // Look up the Type in the Type dictionary
  Type* old = (Type*)(type_dict()->Insert(this, this, false));
  if( old ) {                   // Pre-existing Type?
    if( old != this )           // Yes, this guy is not the pre-existing?
      delete this;              // Yes, Nuke this guy
    assert( old->_dual, "" );
    return old;                 // Return pre-existing
  }

  // Every type has a dual (to make my lattice symmetric).
  // Since we just discovered a new Type, compute its dual right now.
  assert( !_dual, "" );         // No dual yet
  _dual = xdual();              // Compute the dual
  if( cmp(this,_dual)==0 ) {    // Handle self-symmetric
    _dual = this;
    return this; 
  }
  assert( !_dual->_dual, "" );  // No reverse dual yet
  assert( !(*type_dict())[_dual], "" ); // Dual not in type system either
  // New Type, insert into Type table
  type_dict()->Insert((void*)_dual,(void*)_dual);
  ((Type*)_dual)->_dual = this; // Finish up being symmetric
#ifdef ASSERT
  Type *dual_dual = (Type*)_dual->xdual();
  assert( eq(dual_dual), "xdual(xdual()) should be identity" );
  delete dual_dual;
#endif
  return this;                  // Return new Type
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool Type::eq( const Type * ) const {
  return true;                  // Nothing else can go wrong
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int Type::hash(void) const {
  return _base;
}

//------------------------------is_finite--------------------------------------
// Has a finite value
bool Type::is_finite() const {
  return false;
}
  
//------------------------------is_nan-----------------------------------------
// Is not a number (NaN)
bool Type::is_nan()    const {
  return false;
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  NOT virtual.  It enforces that meet is
// commutative and the lattice is symmetric.
const Type *Type::meet( const Type *t ) const {
  const Type *mt = xmeet(t);
#ifdef ASSERT
  assert( mt == t->xmeet(this), "meet not commutative" );
  const Type* dual_join = mt->_dual;
  const Type *t2t    = dual_join->xmeet(t->_dual);
  const Type *t2this = dual_join->xmeet(   _dual);
  if( t2t    != t->_dual ||
      t2this !=    _dual ) {
    tty->print_cr("=== Meet Not Symmetric ===");
    tty->print("t   =                   ");         t->dump(); tty->cr();
    tty->print("this=                   ");            dump(); tty->cr();
    tty->print("mt=(t meet this)=       ");        mt->dump(); tty->cr();

    tty->print("t_dual=                 ");  t->_dual->dump(); tty->cr();
    tty->print("this_dual=              ");     _dual->dump(); tty->cr();
    tty->print("mt_dual=                "); mt->_dual->dump(); tty->cr();

    tty->print("mt_dual meet t_dual=    "); t2t      ->dump(); tty->cr();
    tty->print("mt_dual meet this_dual= "); t2this   ->dump(); tty->cr();

    fatal("meet not symmetric" );
  }
#endif
  return mt;
}

//------------------------------xmeet------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *Type::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Meeting TOP with anything?
  if( _base == Top ) return t;

  // Meeting BOTTOM with anything?
  if( _base == Bottom ) return BOTTOM;

  // Current "this->_base" is one of: Bad, Multi, Control, Top, 
  // Abio, Abstore, Floatxxx, Doublexxx, Bottom, lastype.
  switch (t->base()) {  // Switch on original type

  // Cut in half the number of cases I must handle.  Only need cases for when
  // the given enum "t->type" is less than or equal to the local enum "type".
  case FloatCon:
  case DoubleCon:
  case Int:
  case Long:
    return t->xmeet(this);

  case OopPtr:
    return t->xmeet(this);

  case InstPtr:
    return t->xmeet(this);

  case KlassPtr:
    return t->xmeet(this);

  case AryPtr:
    return t->xmeet(this);

  case Bad:                     // Type check
  default:                      // Bogus type not in lattice
    typerr(t);
    return Type::BOTTOM;

  case Bottom:                  // Ye Olde Default
    return t;

  case FloatTop:
    if( _base == FloatTop ) return this;
  case FloatBot:                // Float
    if( _base == FloatBot || _base == FloatTop ) return FLOAT;
    if( _base == DoubleTop || _base == DoubleBot ) return Type::BOTTOM;
    typerr(t);
    return Type::BOTTOM;

  case DoubleTop:
    if( _base == DoubleTop ) return this;
  case DoubleBot:               // Double
    if( _base == DoubleBot || _base == DoubleTop ) return DOUBLE;
    if( _base == FloatTop || _base == FloatBot ) return Type::BOTTOM;
    typerr(t);
    return Type::BOTTOM;

  // These next few cases must match exactly or it is a compile-time error.
  case Control:                 // Control of code 
  case Abio:                    // State of world outside of program
  case Memory:
    if( _base == t->_base )  return this;
    typerr(t);
    return Type::BOTTOM;

  case Top:                     // Top of the lattice
    return this;
  }

  // The type is unchanged
  return this;
}

//------------------------------xdual------------------------------------------
// Compute dual right now.
const Type::TYPES Type::dual_type[Type::lastype] = {
  Bad,          // Bad
  Control,      // Control
  Bottom,       // Top
  Bad,          // Int - handled in v-call
  Bad,          // Long - handled in v-call
  Half,         // Half
  
  Bad,          // Tuple - handled in v-call
  Bad,          // Array - handled in v-call

  Bad,          // AnyPtr - handled in v-call
  Bad,          // RawPtr - handled in v-call
  Bad,          // OopPtr - handled in v-call
  Bad,          // InstPtr - handled in v-call
  Bad,          // AryPtr - handled in v-call
  Bad,          // KlassPtr - handled in v-call

  Bad,          // Function - handled in v-call
  Abio,         // Abio
  Return_Address,// Return_Address
  Memory,       // Memory
  FloatBot,     // FloatTop
  FloatCon,     // FloatCon
  FloatTop,     // FloatBot
  DoubleBot,    // DoubleTop
  DoubleCon,    // DoubleCon
  DoubleTop,    // DoubleBot
  Top           // Bottom
};

const Type *Type::xdual() const {
  // Note: the base() accessor asserts the sanity of _base.
  assert(dual_type[base()] != Bad, "implement with v-call");
  return new Type(dual_type[_base]);
}

//------------------------------has_memory-------------------------------------
bool Type::has_memory() const {
  Type::TYPES tx = base();
  if (tx == Memory) return true;
  if (tx == Tuple) {
    const TypeTuple *t = is_tuple();
    for (uint i=0; i < t->cnt(); i++) {
      tx = t->field_at(i)->base();
      if (tx == Memory)  return true;
    }
  }
  return false;
}

#ifndef PRODUCT
//------------------------------dump2------------------------------------------
void Type::dump2( Dict &d, uint depth ) const {
  tty->print(msg[_base]);
}

//------------------------------dump-------------------------------------------
void Type::dump() const {
  ResourceMark rm;
  Dict d(cmpkey,hashkey);       // Stop recursive type dumping
  dump2(d,1);
}

//------------------------------data-------------------------------------------
const char * const Type::msg[Type::lastype] = {
  "bad","control","top","int:","long:","half", 
  "tuple:", "aryptr", 
  "anyptr:", "rawptr:", "java:", "inst:", "ary:", "klass:", 
  "func", "abIO", "return_address", "memory", 
  "float_top", "ftcon:", "float",
  "double_top", "dblcon:", "double",
  "bottom"
};
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants.
bool Type::singleton(void) const {
  return _base == Top || _base == Half;
}

//------------------------------empty------------------------------------------
// TRUE if Type is a type with no values, FALSE otherwise.
bool Type::empty(void) const {
  switch (_base) {
  case DoubleTop:
  case FloatTop:
  case Top:
    return true;

  case Half:
  case Abio:
  case Return_Address:
  case Memory:
  case Bottom:
  case FloatBot:
  case DoubleBot:
    return false;  // never a singleton, therefore never empty
  }

  ShouldNotReachHere();
  return false;
}

//------------------------------dump_stats-------------------------------------
// Dump collected statistics to stderr
#ifndef PRODUCT
void Type::dump_stats() {
  tty->print("Types made: %d\n", type_dict()->Size());
}
#endif

//------------------------------typerr-----------------------------------------
void Type::typerr( const Type *t ) const {
#ifndef PRODUCT
  tty->print("\nError mixing types: ");
  dump();
  tty->print(" and ");
  t->dump();
  tty->print("\n");
#endif
  ShouldNotReachHere();
}

//------------------------------isa_oop_ptr------------------------------------
// Return true if type is an oop pointer type.  False for raw pointers.
static char isa_oop_ptr_tbl[Type::lastype] = {
  0,0,0,0,0,0,0/*tuple*/, 0/*ary*/,
  0/*anyptr*/,0/*rawptr*/,1/*OopPtr*/,1/*InstPtr*/,1/*AryPtr*/,1/*KlassPtr*/,
  0/*func*/,0,0/*return_address*/,0,
  /*floats*/0,0,0, /*doubles*/0,0,0,
  0
};
bool Type::isa_oop_ptr() const {
  return isa_oop_ptr_tbl[_base];
}

//------------------------------dump_stats-------------------------------------
// // Check that arrays match type enum
#ifndef PRODUCT
void Type::verify_lastype() {
  // Check that arrays match enumeration
  assert( Type::dual_type  [Type::lastype - 1] == Type::Top, "did not update array");
  assert( strcmp(Type::msg [Type::lastype - 1],"bottom") == 0, "did not update array");
  // assert( PhiNode::tbl     [Type::lastype - 1] == NULL,    "did not update array");
  assert( Matcher::base2reg[Type::lastype - 1] == 0,      "did not update array");
  assert( isa_oop_ptr_tbl  [Type::lastype - 1] == (char)0,  "did not update array");
}
#endif

//=============================================================================
// Convenience common pre-built types.
const TypeF *TypeF::ZERO;       // Floating point zero
const TypeF *TypeF::ONE;        // Floating point one

//------------------------------make-------------------------------------------
// Create a float constant
const TypeF *TypeF::make(float f) {
  return (TypeF*)(new TypeF(f))->hashcons();
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeF::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is FloatCon
  switch (t->base()) {          // Switch on original type
  case AnyPtr:                  // Mixing with oops happens when javac
  case RawPtr:                  // reuses local variables
  case OopPtr:
  case InstPtr:
  case KlassPtr:
  case AryPtr:
  case Int:
  case Long:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;

  case FloatBot:
    return t;

  default:                      // All else is a mistake
    typerr(t);

  case FloatCon:                // Float-constant vs Float-constant?
    if( jint_cast(_f) != jint_cast(t->getf()) )         // unequal constants?
                                // must compare bitwise as positive zero, negative zero and NaN have 
                                // all the same representation in C++
      return FLOAT;             // Return generic float
                                // Equal constants 
  case Top:
  case FloatTop:
    break;                      // Return the float constant
  }
  return this;                  // Return the float constant
}

//------------------------------xdual------------------------------------------
// Dual: symmetric
const Type *TypeF::xdual() const {
  return this;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeF::eq( const Type *t ) const {
  if( g_isnan(_f) || 
      g_isnan(t->getf()) ) {
    // One or both are NANs.  If both are NANs return true, else false.
    return (g_isnan(_f) && g_isnan(t->getf()));
  }
  if (_f == t->getf()) {
    // (NaN is impossible at this point, since it is not equal even to itself)
    if (_f == 0.0) {
      // difference between positive and negative zero
      if (jint_cast(_f) != jint_cast(t->getf()))  return false;
    }
    return true;
  }
  return false;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeF::hash(void) const {
  return *(int*)(&_f);
}

//------------------------------is_finite--------------------------------------
// Has a finite value
bool TypeF::is_finite() const {
  return g_isfinite(getf());
}
  
//------------------------------is_nan-----------------------------------------
// Is not a number (NaN)
bool TypeF::is_nan()    const {
  return g_isnan(getf());
}

//------------------------------dump2------------------------------------------
// Dump float constant Type
#ifndef PRODUCT
void TypeF::dump2( Dict &d, uint depth ) const {
  Type::dump2(d,depth);
  tty->print("%f", _f);
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants
// or a single symbol.
bool TypeF::singleton(void) const {
  return true;                  // Always a singleton
}

bool TypeF::empty(void) const {
  return false;                 // always exactly a singleton
}

//=============================================================================
// Convenience common pre-built types.
const TypeD *TypeD::ZERO;       // Floating point zero
const TypeD *TypeD::ONE;        // Floating point one

//------------------------------make-------------------------------------------
const TypeD *TypeD::make(double d) {
  return (TypeD*)(new TypeD(d))->hashcons();
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeD::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is DoubleCon
  switch (t->base()) {          // Switch on original type
  case AnyPtr:                  // Mixing with oops happens when javac
  case RawPtr:                  // reuses local variables
  case OopPtr:
  case InstPtr:
  case KlassPtr:
  case AryPtr:
  case Int:
  case Long:
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;

  case DoubleBot:
    return t;

  default:                      // All else is a mistake
    typerr(t);

  case DoubleCon:               // Double-constant vs Double-constant?
    if( jlong_cast(_d) != jlong_cast(t->getd()) )       // unequal constants? (see comment in TypeF::xmeet)
      return DOUBLE;            // Return generic double
  case Top:
  case DoubleTop:
    break;
  }
  return this;                  // Return the double constant
}

//------------------------------xdual------------------------------------------
// Dual: symmetric
const Type *TypeD::xdual() const {
  return this;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeD::eq( const Type *t ) const {
  if( g_isnan(_d) || 
      g_isnan(t->getd()) ) {
    // One or both are NANs.  If both are NANs return true, else false.
    return (g_isnan(_d) && g_isnan(t->getd()));
  }
  if (_d == t->getd()) {
    // (NaN is impossible at this point, since it is not equal even to itself)
    if (_d == 0.0) {
      // difference between positive and negative zero
      if (jlong_cast(_d) != jlong_cast(t->getd()))  return false;
    }
    return true;
  }
  return false;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeD::hash(void) const {
  return *(int*)(&_d);
}

//------------------------------is_finite--------------------------------------
// Has a finite value
bool TypeD::is_finite() const {
  return g_isfinite(getd());
}
  
//------------------------------is_nan-----------------------------------------
// Is not a number (NaN)
bool TypeD::is_nan()    const {
  return g_isnan(getd());
}

//------------------------------dump2------------------------------------------
// Dump double constant Type
#ifndef PRODUCT
void TypeD::dump2( Dict &d, uint depth ) const {
  Type::dump2(d,depth);
  tty->print("%f", _d);
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants
// or a single symbol.
bool TypeD::singleton(void) const {
  return true;                  // Always a singleton
}

bool TypeD::empty(void) const {
  return false;                 // always exactly a singleton
}

//=============================================================================
// Convience common pre-built types.
const TypeInt *TypeInt::MINUS_1;// -1
const TypeInt *TypeInt::ZERO;   // 0
const TypeInt *TypeInt::ONE;    // 1
const TypeInt *TypeInt::BOOL;   // 0 or 1, FALSE or TRUE.
const TypeInt *TypeInt::CC;     // -1,0 or 1, condition codes
const TypeInt *TypeInt::CC_LT;  // [-1]  == MINUS_1
const TypeInt *TypeInt::CC_GT;  // [1]   == ONE
const TypeInt *TypeInt::CC_EQ;  // [0]   == ZERO
const TypeInt *TypeInt::CC_LE;  // [-1,0]
const TypeInt *TypeInt::CC_GE;  // [0,1] == BOOL (!)
const TypeInt *TypeInt::BYTE;   // Bytes, -128 to 127
const TypeInt *TypeInt::CHAR;   // Java chars, 0-65535
const TypeInt *TypeInt::SHORT;  // Java shorts, -32768-32767
const TypeInt *TypeInt::POS;    // Positive 32-bit integers
const TypeInt *TypeInt::INT;    // 32-bit integers

//------------------------------TypeInt----------------------------------------
TypeInt::TypeInt( jint lo, jint hi, int w ) : Type(Int), _lo(lo), _hi(hi), _widen(w) {
}

//------------------------------make-------------------------------------------
const TypeInt *TypeInt::make( jint lo ) {
  return (TypeInt*)(new TypeInt(lo,lo,0))->hashcons();
}

const TypeInt *TypeInt::make( jint lo, jint hi ) {
  return (TypeInt*)(new TypeInt(lo,hi,0))->hashcons();
}

const TypeInt *TypeInt::make( jint lo, jint hi, int w ) {
  return (TypeInt*)(new TypeInt(lo,hi,w))->hashcons();
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type representation object
// with reference count equal to the number of Types pointing at it.
// Caller should wrap a Types around it.
const Type *TypeInt::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type?

  // Currently "this->_base" is a TypeInt
  switch (t->base()) {          // Switch on original type
  case AnyPtr:                  // Mixing with oops happens when javac
  case RawPtr:                  // reuses local variables
  case OopPtr:
  case InstPtr:
  case KlassPtr:
  case AryPtr:
  case Long:
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  default:                      // All else is a mistake
    typerr(t);
  case Top:                     // No change
    return this;
  case Int:                     // Int vs Int?
    break;
  }

  // Expand covered set
  const TypeInt *r = t->is_int();
  return make( MIN2(_lo,r->_lo), MAX2(_hi,r->_hi), MAX2(_widen,r->_widen) );
}

//------------------------------xdual------------------------------------------
// Dual: reverse hi & lo; flip widen
const Type *TypeInt::xdual() const {
  return new TypeInt(_hi,_lo,3-_widen);
}

//------------------------------widen------------------------------------------
// Only happens for optimistic top-down optimizations.
const Type *TypeInt::widen( const Type *old ) const {
  // Coming from TOP or such; no widening
  if( old->base() != Int ) return this;
  const TypeInt *ot = old->is_int();

  // If new guy is equal to old guy, no widening
  if( _lo == ot->_lo && _hi == ot->_hi ) 
    return old;

  // If new guy contains old, then we widened
  if( _lo <= ot->_lo && _hi >= ot->_hi ) {
    // New contains old
    // If new guy is already wider than old, no widening
    if( _widen > ot->_widen ) return this;
    // If new guy is tiny, do not bother
    if( _lo >= 0 && _hi <= 1 ) return this;
    // Now widen new guy.
    // Check for widening too far
    if( _widen == 3 ) return INT;
    // Returned widened new guy
    return make(_lo,_hi,_widen+1);
  }

  // If old guy contains new, then we probably widened too far & dropped to
  // bottom.  Return the wider fellow.
  if ( ot->_lo <= _lo && ot->_hi >= _hi ) 
    return old;

  //fatal("Integer value range is not subset");
  //return this;
  return TypeInt::INT;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeInt::eq( const Type *t ) const {
  const TypeInt *r = t->is_int(); // Handy access
  return r->_lo == _lo && r->_hi == _hi && r->_widen == _widen;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeInt::hash(void) const {
  return _lo+_hi+_widen+(int)Type::Int;
}

//------------------------------is_finite--------------------------------------
// Has a finite value
bool TypeInt::is_finite() const {
  return true;
}
  
//------------------------------dump2------------------------------------------
// Dump TypeInt
#ifndef PRODUCT
void TypeInt::dump2( Dict &d, uint depth ) const {
  if( this == INT )             // Common case?
    tty->print("int");
  else if( is_con() ) 
    tty->print("int:" INT32_FORMAT, get_con());
  else if( this == BOOL ) 
    tty->print("bool");
  else if( this == BYTE ) 
    tty->print("byte");
  else if( this == CHAR ) 
    tty->print("char");
  else if( this == SHORT ) 
    tty->print("short");
  else if( this == POS ) 
    tty->print("int+");
  else 
    tty->print("int:" INT32_FORMAT ".." INT32_FORMAT, _lo,_hi);
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants.
bool TypeInt::singleton(void) const {
  return _lo >= _hi;
}

bool TypeInt::empty(void) const {
  return _lo > _hi;
}

//=============================================================================
// Convenience common pre-built types.
const TypeLong *TypeLong::MINUS_1;// -1
const TypeLong *TypeLong::ZERO; // 0
const TypeLong *TypeLong::ONE;  // 1
const TypeLong *TypeLong::LONG; // 64-bit integers
const TypeLong *TypeLong::INT;  // 32-bit subrange

//------------------------------TypeLong---------------------------------------
TypeLong::TypeLong( jlong lo, jlong hi, int w ) : Type(Long), _lo(lo), _hi(hi), _widen(w) {
}

//------------------------------make-------------------------------------------
const TypeLong *TypeLong::make( jlong lo ) {
  return (TypeLong*)(new TypeLong(lo,lo,0))->hashcons();
}

const TypeLong *TypeLong::make( jlong lo, jlong hi ) {
  return (TypeLong*)(new TypeLong(lo,hi,0))->hashcons();
}

const TypeLong *TypeLong::make( jlong lo, jlong hi, int w ) {
  return (TypeLong*)(new TypeLong(lo,hi,w))->hashcons();
}


//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type representation object
// with reference count equal to the number of Types pointing at it.
// Caller should wrap a Types around it.
const Type *TypeLong::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type?

  // Currently "this->_base" is a TypeLong
  switch (t->base()) {          // Switch on original type
  case AnyPtr:                  // Mixing with oops happens when javac
  case RawPtr:                  // reuses local variables
  case OopPtr:
  case InstPtr:
  case KlassPtr:
  case AryPtr:
  case Int:
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  default:                      // All else is a mistake
    typerr(t);
  case Top:                     // No change
    return this;
  case Long:                    // Long vs Long?
    break;
  }

  // Expand covered set
  const TypeLong *r = t->is_long(); // Turn into a TypeLong
  return make( MIN2(_lo,r->_lo), MAX2(_hi,r->_hi), MAX2(_widen,r->_widen) );
}

//------------------------------xdual------------------------------------------
// Dual: reverse hi & lo; flip widen
const Type *TypeLong::xdual() const {
  return new TypeLong(_hi,_lo,3-_widen);
}

//------------------------------widen------------------------------------------
// Only happens for optimistic top-down optimizations.
const Type *TypeLong::widen( const Type *old ) const {
  // Coming from TOP or such; no widening
  if( old->base() != Long ) return this;
  const TypeLong *ot = old->is_long();

  // If new guy is equal to old guy, no widening
  if( _lo == ot->_lo && _hi == ot->_hi ) 
    return old;

  // If new guy contains old, then we widened
  if( _lo <= ot->_lo && _hi >= ot->_hi ) {
    // New contains old
    // If new guy is already wider than old, no widening
    if( _widen > ot->_widen ) return this;
    // If new guy is tiny, do not bother
    if( _lo >= 0 && _hi <= 1 ) return this;
    // Now widen new guy.
    // Check for widening too far
    if( _widen == 3 ) return LONG;
    // Returned widened new guy
    return make(_lo,_hi,_widen+1);
  }

  // If old guy contains new, then we probably widened too far & dropped to
  // bottom.  Return the wider fellow.
  if ( ot->_lo <= _lo && ot->_hi >= _hi ) 
    return old;

  //  fatal("Long value range is not subset");
  // return this;
  return TypeLong::LONG;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeLong::eq( const Type *t ) const {
  const TypeLong *r = t->is_long(); // Handy access
  return r->_lo == _lo &&  r->_hi == _hi  && r->_widen == _widen;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeLong::hash(void) const {
  return (int)(_lo+_hi+_widen+(int)Type::Long);
}

//------------------------------is_finite--------------------------------------
// Has a finite value
bool TypeLong::is_finite() const {
  return true;
}
  
//------------------------------dump2------------------------------------------
// Dump TypeLong
#ifndef PRODUCT
void TypeLong::dump2( Dict &d, uint depth ) const {
  if( this == LONG )            // Common case?
    tty->print("long");
  else if( is_con() ) 
    tty->print("long:" INT64_FORMAT, get_con());
  else {
    Type::dump2(d,depth);
    tty->print("long:" INT64_FORMAT ".." INT64_FORMAT, _lo,_hi);
  }
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants 
bool TypeLong::singleton(void) const {
  return _lo >= _hi;
}

bool TypeLong::empty(void) const {
  return _lo > _hi;
}

//=============================================================================
// Convenience common pre-built types.
const TypeTuple *TypeTuple::IFBOTH;     // Return both arms of IF as reachable
const TypeTuple *TypeTuple::IFFALSE;
const TypeTuple *TypeTuple::IFTRUE;
const TypeTuple *TypeTuple::IFNEITHER;
const TypeTuple *TypeTuple::LOOPBODY;
const TypeTuple *TypeTuple::MEMBAR;
const TypeTuple *TypeTuple::STORECONDITIONAL;
const TypeTuple *TypeTuple::START_I2C;


//------------------------------make-------------------------------------------
// Make a TypeTuple from the range of a method signature
const TypeTuple *TypeTuple::make_range(ciSignature* sig) {
  ciType* return_type = sig->return_type();
  uint total_fields = TypeFunc::Parms + return_type->size();
  const Type **field_array = fields(total_fields);
  switch (return_type->basic_type()) {
  case T_LONG:
    field_array[TypeFunc::Parms]   = TypeLong::LONG;
    field_array[TypeFunc::Parms+1] = Type::HALF;
    break;
  case T_DOUBLE:
    field_array[TypeFunc::Parms]   = Type::DOUBLE;
    field_array[TypeFunc::Parms+1] = Type::HALF;      
    break;
  case T_OBJECT:
  case T_ARRAY:
  case T_BOOLEAN:
  case T_CHAR:
  case T_FLOAT:
  case T_BYTE:
  case T_SHORT:
  case T_INT:
    field_array[TypeFunc::Parms] = get_const_type(return_type);
    break;
  case T_VOID:
    break;
  default:
    ShouldNotReachHere();
  }
  return (TypeTuple*)(new TypeTuple(total_fields,field_array))->hashcons();
}

// Make a TypeTuple from the domain of a method signature
const TypeTuple *TypeTuple::make_domain(ciInstanceKlass* recv, ciSignature* sig) {
  uint total_fields = TypeFunc::Parms + sig->size();

  uint pos = TypeFunc::Parms;
  const Type **field_array;
  if (recv != NULL) {
    total_fields++;
    field_array = fields(total_fields);
    // Use get_const_type here because it respects UseUniqueSubclasses:
    field_array[pos++] = get_const_type(recv)->join(TypePtr::NOTNULL);
  } else {
    field_array = fields(total_fields);
  }

  int i = 0;
  while (pos < total_fields) {
    ciType* type = sig->type_at(i);

    switch (type->basic_type()) {
    case T_LONG:
      field_array[pos++] = TypeLong::LONG;
      field_array[pos++] = Type::HALF;
      break;
    case T_DOUBLE:
      field_array[pos++] = Type::DOUBLE;
      field_array[pos++] = Type::HALF;      
      break;
    case T_OBJECT:
    case T_ARRAY:
    case T_BOOLEAN:
    case T_CHAR:
    case T_FLOAT:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
      field_array[pos++] = get_const_type(type);
      break;   
    default:
      ShouldNotReachHere();
    }
    i++;
  }
  return (TypeTuple*)(new TypeTuple(total_fields,field_array))->hashcons();
}

const TypeTuple *TypeTuple::make( uint cnt, const Type **fields ) {
  return (TypeTuple*)(new TypeTuple(cnt,fields))->hashcons();  
}

//------------------------------fields-----------------------------------------
// Subroutine call type with space allocated for argument types
const Type **TypeTuple::fields( uint arg_cnt ) {
  const Type **flds = (const Type **)(Compile::current()->type_arena()->Amalloc_4((TypeFunc::Parms+arg_cnt)*sizeof(Type*) ));
  flds[TypeFunc::Control  ] = Type::CONTROL;
  flds[TypeFunc::I_O      ] = Type::ABIO;
  flds[TypeFunc::Memory   ] = Type::MEMORY;
  flds[TypeFunc::FramePtr ] = TypeRawPtr::BOTTOM;
  flds[TypeFunc::ReturnAdr] = Type::RETURN_ADDRESS;

  return flds;
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeTuple::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is Tuple
  switch (t->base()) {          // switch on original type

  case Bottom:                  // Ye Olde Default
    return t;

  default:                      // All else is a mistake
    typerr(t);

  case Tuple: {                 // Meeting 2 signatures?
    const TypeTuple *x = t->is_tuple();
    assert( _cnt == x->_cnt, "" );
    const Type **fields = (const Type **)(Compile::current()->type_arena()->Amalloc_4( _cnt*sizeof(Type*) ));
    for( uint i=0; i<_cnt; i++ )
      fields[i] = field_at(i)->xmeet( x->field_at(i) );
    return TypeTuple::make(_cnt,fields);
  }           
  case Top:    
    break;
  }
  return this;                  // Return the double constant
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type *TypeTuple::xdual() const {
  const Type **fields = (const Type **)(Compile::current()->type_arena()->Amalloc_4( _cnt*sizeof(Type*) ));
  for( uint i=0; i<_cnt; i++ )
    fields[i] = _fields[i]->dual();
  return new TypeTuple(_cnt,fields);
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeTuple::eq( const Type *t ) const {
  const TypeTuple *s = (const TypeTuple *)t;
  if (_cnt != s->_cnt)  return false;  // Unequal field counts
  for (uint i = 0; i < _cnt; i++)
    if (field_at(i) != s->field_at(i)) // POINTER COMPARE!  NO RECURSION!
      return false;             // Missed
  return true;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeTuple::hash(void) const {
  intptr_t sum = _cnt;
  for( uint i=0; i<_cnt; i++ )
    sum += (intptr_t)_fields[i];     // Hash on pointers directly
  return sum;
}

//------------------------------dump2------------------------------------------
// Dump signature Type
#ifndef PRODUCT
void TypeTuple::dump2( Dict &d, uint depth ) const {
  tty->print("{");
  if( !depth || d[this] ) {     // Check for recursive print
    tty->print("...}");
    return;
  }
  d.Insert((void*)this, (void*)this);   // Stop recursion
  if( _cnt ) {
    uint i;
    for( i=0; i<_cnt-1; i++ ) {
      tty->print("%d:", i);
      _fields[i]->dump2(d, depth-1);
      tty->print(", ");
    }
    tty->print("%d:", i);
    _fields[i]->dump2(d, depth-1);
  }
  tty->print("}");
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants
// or a single symbol.
bool TypeTuple::singleton(void) const {
  return false;                 // Never a singleton
}

bool TypeTuple::empty(void) const {
  for( uint i=0; i<_cnt; i++ ) {
    if (_fields[i]->empty())  return true;
  }
  return false;
}

//=============================================================================
// Convenience common pre-built types.

//------------------------------make-------------------------------------------
const TypeAry *TypeAry::make( const Type *elem, const TypeInt *size) {
  return (TypeAry*)(new TypeAry(elem,size))->hashcons();
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeAry::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is Ary
  switch (t->base()) {          // switch on original type

  case Bottom:                  // Ye Olde Default
    return t;

  default:                      // All else is a mistake
    typerr(t);

  case Array: {                 // Meeting 2 arrays?
    const TypeAry *a = t->is_ary();
    return TypeAry::make(_elem->meet(a->_elem), 
                         _size->xmeet(a->_size)->is_int());
  }
  case Top:
    break;
  }
  return this;                  // Return the double constant
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type *TypeAry::xdual() const {
  return new TypeAry( _elem->dual(), _size->dual()->is_int() );
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeAry::eq( const Type *t ) const {
  const TypeAry *a = (const TypeAry*)t;
  return _elem == a->_elem &&
    _size == a->_size;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeAry::hash(void) const {
  return (intptr_t)_elem + (intptr_t)_size;
}

//------------------------------dump2------------------------------------------
#ifndef PRODUCT
void TypeAry::dump2( Dict &d, uint depth ) const {
  _elem->dump2(d, depth);
  tty->print("[");
  _size->dump2(d, depth);
  tty->print("]");
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants
// or a single symbol.
bool TypeAry::singleton(void) const {
  return false;                 // Never a singleton
}

bool TypeAry::empty(void) const {
  return _elem->empty() || _size->empty();
}

//--------------------------ary_must_be_exact----------------------------------
bool TypeAry::ary_must_be_exact() const {
  if (!UseExactTypes)       return false;
  // This logic looks at the element type of an array, and returns true
  // if the element type is either a primitive or a final instance class.
  // In such cases, an array built on this ary must have no subclasses.
  if (_elem == BOTTOM)      return false;  // general array not exact
  if (_elem == TOP   )      return false;  // inverted general array not exact
  const TypeOopPtr*  toop = _elem->isa_oopptr();
  if (!toop)                return true;   // a primitive type, like int
  ciKlass* tklass = toop->klass();
  if (tklass == NULL)       return false;  // unloaded class
  if (!tklass->is_loaded()) return false;  // unloaded class
  const TypeInstPtr* tinst = _elem->isa_instptr();
  if (tinst)                return tklass->as_instance_klass()->is_final();
  const TypeAryPtr*  tap = _elem->isa_aryptr();
  if (tap)                  return tap->ary()->ary_must_be_exact();
  return false;
}

//=============================================================================
// Convenience common pre-built types.
const TypePtr *TypePtr::NULL_PTR;
const TypePtr *TypePtr::NOTNULL;
const TypePtr *TypePtr::BOTTOM;

//------------------------------meet-------------------------------------------
// Meet over the PTR enum
const TypePtr::PTR TypePtr::ptr_meet[TypePtr::lastPTR][TypePtr::lastPTR] = {
  //              TopPTR,    AnyNull,   Constant, Null,   NotNull, BotPTR,
  { /* Top     */ TopPTR,    AnyNull,   Constant, Null,   NotNull, BotPTR,},
  { /* AnyNull */ AnyNull,   AnyNull,   Constant, BotPTR, NotNull, BotPTR,},
  { /* Constant*/ Constant,  Constant,  Constant, BotPTR, NotNull, BotPTR,},
  { /* Null    */ Null,      BotPTR,    BotPTR,   Null,   BotPTR,  BotPTR,},
  { /* NotNull */ NotNull,   NotNull,   NotNull,  BotPTR, NotNull, BotPTR,},
  { /* BotPTR  */ BotPTR,    BotPTR,    BotPTR,   BotPTR, BotPTR,  BotPTR,}
};

//------------------------------make-------------------------------------------
const TypePtr *TypePtr::make( TYPES t, enum PTR ptr, int offset ) {
  return (TypePtr*)(new TypePtr(t,ptr,offset))->hashcons();
}

//------------------------------cast_to_ptr_type-------------------------------
const Type *TypePtr::cast_to_ptr_type(PTR ptr) const {
  assert(_base == AnyPtr, "subclass must override cast_to_ptr_type");
  if( ptr == _ptr ) return this;
  return make(_base, ptr, _offset);
}

//------------------------------get_con----------------------------------------
intptr_t TypePtr::get_con() const {
  assert( _ptr == Null, "" );
  return _offset;
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypePtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is AnyPtr
  switch (t->base()) {          // switch on original type
  case Int:                     // Mixing ints & oops happens when javac
  case Long:                    // reuses local variables
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  case Top:
    return this;

  case AnyPtr: {                // Meeting to AnyPtrs
    const TypePtr *tp = t->is_ptr();
    return make( AnyPtr, meet_ptr(tp->ptr()), meet_offset(tp->offset()) );
  }
  case RawPtr:                  // For these, flip the call around to cut down
  case OopPtr:
  case InstPtr:                 // on the cases I have to handle.
  case KlassPtr:
  case AryPtr:
    return t->xmeet(this);      // Call in reverse direction
  default:                      // All else is a mistake
    typerr(t);

  }
  return this;                  
}

//------------------------------meet_offset------------------------------------
int TypePtr::meet_offset( int offset ) const {
  // Either is 'TOP' offset?  Return the other offset!
  if( _offset == OffsetTop ) return offset;
  if( offset == OffsetTop ) return _offset;
  // If either is different, return 'BOTTOM' offset
  if( _offset != offset ) return OffsetBot;
  return _offset;
}

//------------------------------dual_offset------------------------------------
int TypePtr::dual_offset( ) const {
  if( _offset == OffsetTop ) return OffsetBot;// Map 'TOP' into 'BOTTOM'
  if( _offset == OffsetBot ) return OffsetTop;// Map 'BOTTOM' into 'TOP'
  return _offset;               // Map everything else into self
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const TypePtr::PTR TypePtr::ptr_dual[TypePtr::lastPTR] = {
  BotPTR, NotNull, Constant, Null, AnyNull, TopPTR
};
const Type *TypePtr::xdual() const {
  return new TypePtr( AnyPtr, dual_ptr(), dual_offset() );
}

//------------------------------add_offset-------------------------------------
const TypePtr *TypePtr::add_offset( int offset ) const {
  if( offset == 0 ) return this; // No change
  if( _offset == OffsetBot ) return this;
  if(  offset == OffsetBot ) offset = OffsetBot;
  else if( _offset == OffsetTop || offset == OffsetTop ) offset = OffsetTop;
  else offset += _offset;
  return make( AnyPtr, _ptr, offset );
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypePtr::eq( const Type *t ) const {
  const TypePtr *a = (const TypePtr*)t;
  return _ptr == a->ptr() && _offset == a->offset();
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypePtr::hash(void) const {
  return _ptr + _offset;
}

//------------------------------dump2------------------------------------------
const char *const TypePtr::ptr_msg[TypePtr::lastPTR] = {
  "TopPTR","AnyNull","Constant","NULL","NotNull","BotPTR"
};

#ifndef PRODUCT
void TypePtr::dump2( Dict &d, uint depth ) const {
  if( _ptr == Null ) tty->print("NULL");
  else tty->print("%s *", ptr_msg[_ptr]);
  if( _offset == OffsetTop ) tty->print("+top");
  else if( _offset == OffsetBot ) tty->print("+bot");
  else if( _offset ) tty->print("+%d", _offset);
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants 
bool TypePtr::singleton(void) const {
  // TopPTR, Null, AnyNull, Constant are all singletons
  return !below_centerline(_ptr);
}

bool TypePtr::empty(void) const {
  return above_centerline(_ptr);
}

//=============================================================================
// Convenience common pre-built types.
const TypeRawPtr *TypeRawPtr::BOTTOM;
const TypeRawPtr *TypeRawPtr::NOTNULL;

//------------------------------make-------------------------------------------
const TypeRawPtr *TypeRawPtr::make( enum PTR ptr ) {
  assert( ptr != Constant, "what is the constant?" );
  assert( ptr != Null, "Use TypePtr for NULL" );
  return (TypeRawPtr*)(new TypeRawPtr(ptr,0))->hashcons();
}

const TypeRawPtr *TypeRawPtr::make( address bits ) {
  assert( bits, "Use TypePtr for NULL" );
  return (TypeRawPtr*)(new TypeRawPtr(Constant,bits))->hashcons();
}

//------------------------------cast_to_ptr_type-------------------------------
const Type *TypeRawPtr::cast_to_ptr_type(PTR ptr) const {
  assert( ptr != Constant, "what is the constant?" );
  assert( ptr != Null, "Use TypePtr for NULL" );
  assert( _bits==0, "Why cast a constant address?");
  if( ptr == _ptr ) return this;
  return make(ptr);
}

//------------------------------get_con----------------------------------------
intptr_t TypeRawPtr::get_con() const {
  assert( _ptr == Null || _ptr == Constant, "" );
  return (intptr_t)_bits;
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeRawPtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is RawPtr
  switch( t->base() ) {         // switch on original type
  case Bottom:                  // Ye Olde Default
    return t; 
  case Top:
    return this;
  case AnyPtr:                  // Meeting to AnyPtrs
    break;
  case RawPtr: {                // might be top, bot, any/not or constant
    enum PTR tptr = t->is_ptr()->ptr();
    enum PTR ptr = meet_ptr( tptr );
    if( ptr == Constant ) {     // Cannot be equal constants, so...
      if( tptr == Constant && _ptr != Constant)  return t; 
      if( _ptr == Constant && tptr != Constant)  return this; 
      ptr = NotNull;            // Fall down in lattice
    }
    return make( ptr );
  }

  case OopPtr:
  case InstPtr:
  case KlassPtr:
  case AryPtr:
    return TypePtr::BOTTOM;     // Oop meet raw is not well defined
  default:                      // All else is a mistake
    typerr(t);
  }

  // Found an AnyPtr type vs self-RawPtr type
  const TypePtr *tp = t->is_ptr();
  switch (tp->ptr()) {
  case TypePtr::TopPTR:  return this;
  case TypePtr::BotPTR:  return t;
  case TypePtr::Null:
    if( _ptr == TypePtr::TopPTR ) return NULL_PTR;
    return TypeRawPtr::BOTTOM;
  case TypePtr::NotNull: return TypePtr::make( AnyPtr, meet_ptr(TypePtr::NotNull), tp->meet_offset(0) );
  case TypePtr::AnyNull:
    if( _ptr == TypePtr::Constant) return this;
    return make( meet_ptr(TypePtr::AnyNull) );
  default: ShouldNotReachHere();    
  }
  return this;                  
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type *TypeRawPtr::xdual() const {
  return new TypeRawPtr( dual_ptr(), _bits );
}

//------------------------------add_offset-------------------------------------
const TypePtr *TypeRawPtr::add_offset( int offset ) const {
  if( offset == OffsetTop ) return BOTTOM; // Undefined offset-> undefined pointer
  if( offset == OffsetBot ) return BOTTOM; // Unknown offset-> unknown pointer
  if( offset == 0 ) return this; // No change
  switch (_ptr) {
  case TypePtr::TopPTR:
  case TypePtr::BotPTR:
  case TypePtr::NotNull:
    return this;
  case TypePtr::Null:
  case TypePtr::Constant:
    return make( _bits+offset );
  default:  ShouldNotReachHere();
  }
  return NULL;                  // Lint noise
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeRawPtr::eq( const Type *t ) const {
  const TypeRawPtr *a = (const TypeRawPtr*)t;
  return _bits == a->_bits && TypePtr::eq(t);
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeRawPtr::hash(void) const {
  return (intptr_t)_bits + TypePtr::hash();
}

//------------------------------dump2------------------------------------------
#ifndef PRODUCT
void TypeRawPtr::dump2( Dict &d, uint depth ) const {
  if( _ptr == Constant ) 
    tty->print(INTPTR_FORMAT, _bits);
  else
    tty->print("rawptr:%s", ptr_msg[_ptr]);
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants 
bool TypeRawPtr::singleton(void) const {
  // TopPTR, Null, AnyNull, Constant are all singletons
  return !below_centerline(_ptr);
}

bool TypeRawPtr::empty(void) const {
  return above_centerline(_ptr);
}

//=============================================================================
// Convenience common pre-built type.
const TypeOopPtr *TypeOopPtr::BOTTOM;

//------------------------------make-------------------------------------------
const TypeOopPtr *TypeOopPtr::make(PTR ptr, 
                                   int offset) {
  assert(ptr != Constant, "no constant generic pointers");
  ciKlass*  k = ciKlassKlass::make();
  bool      xk = false;
  ciObject* o = NULL;
  return (TypeOopPtr*)(new TypeOopPtr(OopPtr, ptr, k, xk, o, offset))->hashcons();
}


//------------------------------cast_to_ptr_type-------------------------------
const Type *TypeOopPtr::cast_to_ptr_type(PTR ptr) const {
  assert(_base == OopPtr, "subclass must override cast_to_ptr_type");
  if( ptr == _ptr ) return this;
  return make(ptr, _offset);
}


//-----------------------------cast_to_exactness-------------------------------
const Type *TypeOopPtr::cast_to_exactness(bool klass_is_exact) const {
  // There is no such thing as an exact general oop. 
  // Return self unchanged.
  return this;
}


//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeOopPtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is OopPtr
  switch (t->base()) {          // switch on original type

  case Int:                     // Mixing ints & oops happens when javac
  case Long:                    // reuses local variables
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  case Top:
    return this;

  default:                      // All else is a mistake
    typerr(t);

  case RawPtr:
    return TypePtr::BOTTOM;     // Oop meet raw is not well defined

  case AnyPtr: {
    // Found an AnyPtr type vs self-OopPtr type
    const TypePtr *tp = t->is_ptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case Null:
      if (ptr == Null)  return TypePtr::make(AnyPtr, ptr, offset);
      // else fall through:
    case TopPTR: 
    case AnyNull:
      return make(ptr, offset);
    case BotPTR:
    case NotNull:
      return TypePtr::make(AnyPtr, ptr, offset);
    default: typerr(t);
    }
  }
 
  case OopPtr: {                 // Meeting to other OopPtrs
    const TypeOopPtr *tp = t->is_oopptr();
    return make( meet_ptr(tp->ptr()), meet_offset(tp->offset()) );
  }

  case InstPtr:                  // For these, flip the call around to cut down
  case KlassPtr:                 // on the cases I have to handle.
  case AryPtr:
    return t->xmeet(this);      // Call in reverse direction

  } // End of switch
  return this;                  // Return the double constant
}


//------------------------------xdual------------------------------------------
// Dual of a pure heap pointer.  No relevant klass or oop information.
const Type *TypeOopPtr::xdual() const {
  assert(klass() == ciKlassKlass::make(), "no klasses here");
  assert(const_oop() == NULL,             "no constants here");
  return new TypeOopPtr(_base, dual_ptr(), klass(), klass_is_exact(), const_oop(), dual_offset() );
}

//--------------------------make_from_klass_common-----------------------------
// Computes the element-type given a klass.
const TypeOopPtr* TypeOopPtr::make_from_klass_common(ciKlass *klass, bool klass_change, bool try_for_exact) {
  assert(klass->is_java_klass(), "must be java language klass");
  if (klass->is_instance_klass()) {
    // Element is an instance
    bool klass_is_exact = false;
    if (klass->is_loaded()) {
      // Try to set klass_is_exact.
      ciInstanceKlass* ik = klass->as_instance_klass();
      klass_is_exact = ik->is_final();
      if (!klass_is_exact && klass_change && UseUniqueSubclasses) {
        ciInstanceKlass* sub = ik->unique_concrete_subklass();
        if (sub != NULL) {
          Compile* C = Compile::current();
          CompileLog* log = C->log();
          if (log != NULL) {
            log->elem("cast_up reason='unique_concrete_subklass' from='%d' to='%d'",
                      log->identify(ik), log->identify(sub));
          }
          if (C->method() != NULL && C->method()->code_size() > 0) {
            C->recorder()->add_dependent(ik, NULL);
            klass = ik = sub;
            klass_is_exact = sub->is_final();
          }
        }
      }
      if (!klass_is_exact && try_for_exact && UseExactTypes) {
        if (!ik->is_interface() && !ik->has_subklass()) {
          // Add a dependence; if any subclass added we need to recompile
          Compile* C = Compile::current();
          CompileLog* log = C->log();
          if (log != NULL) {
            log->elem("cast_up reason='!has_subklass' from='%d' to='(exact)'",
                      log->identify(ik));
          }
          if (C->method() != NULL && C->method()->code_size() > 0) {
            C->recorder()->add_dependent(ik, NULL);
            klass_is_exact = true;
          }
        }
      }
    }
    return TypeInstPtr::make(TypePtr::BotPTR, klass, klass_is_exact, NULL, 0);
  } else if (klass->is_obj_array_klass()) {
    // Element is an object array. Recursively call ourself.
    const TypeOopPtr *etype = TypeOopPtr::make_from_klass_common(klass->as_obj_array_klass()->element_klass(), false, try_for_exact);
    bool xk = etype->klass_is_exact();
    const TypeAry* arr0 = TypeAry::make(etype, TypeInt::POS);
    // We used to pass NotNull in here, asserting that the sub-arrays
    // are all not-null.  This is not true in generally, as code can 
    // slam NULLs down in the subarrays.
    const TypeAryPtr* arr = TypeAryPtr::make(TypePtr::BotPTR, arr0, klass, xk, 0);
    return arr;
  } else if (klass->is_type_array_klass()) {
    // Element is an typeArray
    const Type* etype = get_const_basic_type(klass->as_type_array_klass()->element_type());
    const TypeAry* arr0 = TypeAry::make(etype, TypeInt::POS);
    // We used to pass NotNull in here, asserting that the array pointer
    // is not-null. That was not true in general.
    const TypeAryPtr* arr = TypeAryPtr::make(TypePtr::BotPTR, arr0, klass, true, 0);
    return arr;
  } else {
    ShouldNotReachHere();
    return NULL;
  }
}

//------------------------------make_from_constant-----------------------------
// Make a java pointer from an oop constant
const TypeOopPtr* TypeOopPtr::make_from_constant(ciObject* o) {
  if (o->is_method_data() || o->is_method()) {
    // Treat much like a typeArray of bytes, like below, but fake the type...
    assert(o->has_encoding(), "must be a perm space object");
    const Type* etype = (Type*)get_const_basic_type(T_BYTE);
    const TypeAry* arr0 = TypeAry::make(etype, TypeInt::POS);
    ciKlass *klass = ciTypeArrayKlass::make((BasicType) T_BYTE);
    assert(o->has_encoding(), "method data oops should be tenured");
    const TypeAryPtr* arr = TypeAryPtr::make(TypePtr::Constant, o, arr0, klass, true, 0);
    return arr;
  } else {
    assert(o->is_java_object(), "must be java language object");
    assert(!o->is_null_object(), "null object not yet handled here.");
    ciKlass *klass = o->klass();
    if (klass->is_instance_klass()) {       
      // Element is an instance
      if (!o->has_encoding()) {  // not a perm-space constant
        // %%% remove this restriction by rewriting non-perm ConPNodes in a later phase
        return TypeInstPtr::make(TypePtr::NotNull, klass, true, NULL, 0);
      }
      return TypeInstPtr::make(o);    
    } else if (klass->is_obj_array_klass()) {
      // Element is an object array. Recursively call ourself.
      const Type *etype =
        TypeOopPtr::make_from_klass_raw(klass->as_obj_array_klass()->element_klass());
      const TypeAry* arr0 = TypeAry::make(etype, TypeInt::make(o->as_array()->length()));
      // We used to pass NotNull in here, asserting that the sub-arrays
      // are all not-null.  This is not true in generally, as code can 
      // slam NULLs down in the subarrays.  
      if (!o->has_encoding()) {  // not a perm-space constant
        // %%% remove this restriction by rewriting non-perm ConPNodes in a later phase
        return TypeAryPtr::make(TypePtr::NotNull, arr0, klass, true, 0);
      }
      const TypeAryPtr* arr = TypeAryPtr::make(TypePtr::Constant, o, arr0, klass, true, 0);
      return arr;
    } else if (klass->is_type_array_klass()) {
      // Element is an typeArray
      const Type* etype =
        (Type*)get_const_basic_type(klass->as_type_array_klass()->element_type());
      const TypeAry* arr0 = TypeAry::make(etype, TypeInt::make(o->as_array()->length()));
      // We used to pass NotNull in here, asserting that the array pointer
      // is not-null. That was not true in general.
      if (!o->has_encoding()) {  // not a perm-space constant
        // %%% remove this restriction by rewriting non-perm ConPNodes in a later phase
        return TypeAryPtr::make(TypePtr::NotNull, arr0, klass, true, 0);
      }
      const TypeAryPtr* arr = TypeAryPtr::make(TypePtr::Constant, o, arr0, klass, true, 0);
      return arr;
    }
  } 
    
  ShouldNotReachHere();
  return NULL;
}

//------------------------------get_con----------------------------------------
intptr_t TypeOopPtr::get_con() const {
  assert( _ptr == Null || _ptr == Constant, "" );
  assert( _offset >= 0, "" );
  
  if (_offset != 0) {
    // After being ported to the compiler interface, the compiler no longer
    // directly manipulates the addresses of oops.  Rather, it only has a pointer
    // to a handle at compile time.  This handle is embedded in the generated
    // code and dereferenced at the time the nmethod is made.  Until that time,
    // it is not reasonable to do arithmetic with the addresses of oops (we don't
    // have access to the addresses!).  This does not seem to currently happen,
    // but this assertion here is to help prevent its occurrance.
    tty->print_cr("Found oop constant with non-zero offset");
    ShouldNotReachHere();
  }
  
  return (intptr_t)const_oop()->encoding();
}


//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeOopPtr::eq( const Type *t ) const {
  const TypeOopPtr *a = (const TypeOopPtr*)t;
  if (_klass_is_exact != a->_klass_is_exact)  return false;
  ciObject* one = const_oop();
  ciObject* two = a->const_oop();
  if (one == NULL || two == NULL) {
    return (one == two) && TypePtr::eq(t);
  } else {
    return one->equals(two) && TypePtr::eq(t);
  }
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeOopPtr::hash(void) const {
  return 
    (const_oop() ? const_oop()->hash() : 0) +
    _klass_is_exact +
    TypePtr::hash();
}

//------------------------------dump2------------------------------------------
#ifndef PRODUCT
void TypeOopPtr::dump2( Dict &d, uint depth ) const {
  tty->print("oopptr:%s", ptr_msg[_ptr]);
  if( _klass_is_exact ) tty->print(":exact");
  if( const_oop() ) tty->print(INTPTR_FORMAT, const_oop());
  switch( _offset ) {
  case OffsetTop: tty->print("+top"); break;
  case OffsetBot: tty->print("+any"); break;
  case         0: break;
  default:        tty->print("+%d",_offset); break;
  }
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants 
bool TypeOopPtr::singleton(void) const {
  // detune optimizer to not generate constant oop + constant offset as a constant!
  // TopPTR, Null, AnyNull, Constant are all singletons
  return (_offset == 0) && !below_centerline(_ptr);
}

//------------------------------empty------------------------------------------
// TRUE if type is vacuous
bool TypeOopPtr::empty(void) const {
  return (_offset == OffsetTop) || above_centerline(_ptr);
}

//------------------------------xadd_offset------------------------------------
int TypeOopPtr::xadd_offset( int offset ) const {
  // Adding to 'TOP' offset?  Return 'TOP'!
  if( _offset == OffsetTop || offset == OffsetTop ) return OffsetTop;
  // Adding to 'BOTTOM' offset?  Return 'BOTTOM'!
  if( _offset == OffsetBot || offset == OffsetBot ) return OffsetBot;

  // assert( _offset >= 0 && _offset+offset >= 0, "" );
  // It is possible to construct a negative offset during PhaseCCP

  return _offset+offset;        // Sum valid offsets
}

//------------------------------add_offset-------------------------------------
const TypePtr *TypeOopPtr::add_offset( int offset ) const {
  return make( _ptr, xadd_offset(offset) );
}

//=============================================================================
// Convenience common pre-built types.
const TypeInstPtr *TypeInstPtr::NOTNULL;
const TypeInstPtr *TypeInstPtr::BOTTOM;
const TypeInstPtr *TypeInstPtr::MARK;
const TypeInstPtr *TypeInstPtr::KLASS;

//------------------------------TypeInstPtr-------------------------------------
TypeInstPtr::TypeInstPtr(PTR ptr, ciKlass* k, bool xk, ciObject* o, int off) 
 : TypeOopPtr(InstPtr, ptr, k, xk, o, off), _name(k->name()) {
   assert(k != NULL &&
          (k->is_loaded() || o == NULL),
          "cannot have constants with non-loaded klass");
};

//------------------------------make-------------------------------------------
const TypeInstPtr *TypeInstPtr::make(PTR ptr, 
                                     ciKlass* k,
                                     bool xk,
                                     ciObject* o,
                                     int offset) {
  assert( !k->is_loaded() || k->is_instance_klass() ||
          k->is_method_klass(), "Must be for instance or method");
  // Either const_oop() is NULL or else ptr is Constant
  assert( (!o && ptr != Constant) || (o && ptr == Constant),
          "constant pointers must have a value supplied" );
  // Ptr is never Null
  assert( ptr != Null, "NULL pointers are not typed" );

  if (!UseExactTypes)  xk = false;
  if (ptr == Constant) {
    // Note:  This case includes meta-object constants, such as methods.
    xk = true;
  } else if (k->is_loaded()) {
    ciInstanceKlass* ik = k->as_instance_klass();
    if (!xk && ik->is_final())     xk = true;   // no inexact final klass
    if (xk && ik->is_interface())  xk = false;  // no exact interface
  }

  // Now hash this baby
  TypeInstPtr *result =
    (TypeInstPtr*)(new TypeInstPtr(ptr, k, xk, o ,offset))->hashcons();

  return result;
}


//------------------------------cast_to_ptr_type-------------------------------
const Type *TypeInstPtr::cast_to_ptr_type(PTR ptr) const {
  if( ptr == _ptr ) return this;
  // Reconstruct _sig info here since not a problem with later lazy
  // construction, _sig will show up on demand.
  return make(ptr, klass(), klass_is_exact(), const_oop(), _offset);
}


//-----------------------------cast_to_exactness-------------------------------
const Type *TypeInstPtr::cast_to_exactness(bool klass_is_exact) const {
  if( klass_is_exact == _klass_is_exact ) return this;
  if (!UseExactTypes)  return this;
  if (!_klass->is_loaded())  return this;
  ciInstanceKlass* ik = _klass->as_instance_klass();
  if( (ik->is_final() || _const_oop) )  return this;  // cannot clear xk
  if( ik->is_interface() )              return this;  // cannot set xk
  return make(ptr(), klass(), klass_is_exact, const_oop(), _offset);
}


//------------------------------xmeet_unloaded---------------------------------
// Compute the MEET of two InstPtrs when at least one is unloaded.
// Assume classes are different since called after check for same name/class-loader
const TypeInstPtr *TypeInstPtr::xmeet_unloaded(const TypeInstPtr *tinst) const {
    int off = meet_offset(tinst->offset());
    PTR ptr = meet_ptr(tinst->ptr());

    const TypeInstPtr *loaded    = is_loaded() ? this  : tinst;
    const TypeInstPtr *unloaded  = is_loaded() ? tinst : this;
    if( loaded->klass()->equals(ciEnv::current()->Object_klass()) ) {
      // 
      // Meet unloaded class with java/lang/Object
      //
      // Meet
      //          |                     Unloaded Class
      //  Object  |   TOP    |   AnyNull | Constant |   NotNull |  BOTTOM   |
      //  ===================================================================
      //   TOP    | ..........................Unloaded......................|
      //  AnyNull |  U-AN    |................Unloaded......................|
      // Constant | ... O-NN .................................. |   O-BOT   |
      //  NotNull | ... O-NN .................................. |   O-BOT   |
      //  BOTTOM  | ........................Object-BOTTOM ..................|
      //
      assert(loaded->ptr() != TypePtr::Null, "insanity check");
      // 
      if(      loaded->ptr() == TypePtr::TopPTR ) { return unloaded; }
      else if (loaded->ptr() == TypePtr::AnyNull) { return TypeInstPtr::make( ptr, unloaded->klass() ); }
      else if (loaded->ptr() == TypePtr::BotPTR ) { return TypeInstPtr::BOTTOM; }
      else if (loaded->ptr() == TypePtr::Constant || loaded->ptr() == TypePtr::NotNull) {
        if (unloaded->ptr() == TypePtr::BotPTR  ) { return TypeInstPtr::BOTTOM;  }
        else                                      { return TypeInstPtr::NOTNULL; }
      }
      else if( unloaded->ptr() == TypePtr::TopPTR )  { return unloaded; }

      return unloaded->cast_to_ptr_type(TypePtr::AnyNull)->is_instptr();
    }

    // Both are unloaded, not the same class, not Object
    // Or meet unloaded with a different loaded class, not java/lang/Object
    if( ptr != TypePtr::BotPTR ) {
      return TypeInstPtr::NOTNULL;
    }
    return TypeInstPtr::BOTTOM;
}


//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeInstPtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is Pointer
  switch (t->base()) {          // switch on original type

  case Int:                     // Mixing ints & oops happens when javac
  case Long:                    // reuses local variables
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  case Top:
    return this;

  default:                      // All else is a mistake
    typerr(t);

  case RawPtr: return TypePtr::BOTTOM;

  case AryPtr: {                // All arrays inherit from Object class
    const TypeAryPtr *tp = t->is_aryptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (ptr) {
    case TopPTR: 
    case AnyNull:                // Fall 'down' to dual of object klass
      if (klass()->equals(ciEnv::current()->Object_klass())) {
        return TypeAryPtr::make(ptr, tp->ary(), tp->klass(), tp->klass_is_exact(), offset);
      } else {
        // cannot subclass, so the meet has to fall badly below the centerline
        ptr = NotNull;
        return TypeInstPtr::make( ptr, ciEnv::current()->Object_klass(), false, NULL, offset);
      }
    case Constant:
    case NotNull:
    case BotPTR:                // Fall down to object klass
      // LCA is object_klass, but if we subclass from the top we can do better
      if( above_centerline(_ptr) ) { // if( _ptr == TopPTR || _ptr == AnyNull )
        // If 'this' (InstPtr) is above the centerline and it is Object class
        // then we can subclass in the Java class heirarchy.
        if (klass()->equals(ciEnv::current()->Object_klass())) {
          // that is, tp's array type is a subtype of my klass
          return TypeAryPtr::make(ptr, tp->ary(), tp->klass(), tp->klass_is_exact(), offset);
        }
      }
      // The other case cannot happen, since I cannot be a subtype of an array.
      // The meet falls down to Object class below centerline.
      if( ptr == Constant )
         ptr = NotNull;
      return make( ptr, ciEnv::current()->Object_klass(), false, NULL, offset );
    default: typerr(t);
    }
  }

  case OopPtr: {                // Meeting to OopPtrs
    // Found a OopPtr type vs self-InstPtr type
    const TypePtr *tp = t->is_oopptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case TopPTR: 
    case AnyNull:
      return make(ptr, klass(), klass_is_exact(),
                  (ptr == Constant ? const_oop() : NULL), offset);
    case NotNull:
    case BotPTR: 
      return TypeOopPtr::make(ptr, offset);
    default: typerr(t);
    }
  }

  case AnyPtr: {                // Meeting to AnyPtrs
    // Found an AnyPtr type vs self-InstPtr type
    const TypePtr *tp = t->is_ptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case Null: 
      if( ptr == Null ) return TypePtr::make( AnyPtr, ptr, offset );
    case TopPTR: 
    case AnyNull:
      return make( ptr, klass(), klass_is_exact(),
                   (ptr == Constant ? const_oop() : NULL), offset );
    case NotNull:
    case BotPTR: 
      return TypePtr::make( AnyPtr, ptr, offset );
    default: typerr(t);
    }
  }

  /*  
                 A-top         }
               /   |   \       }  Tops
           B-top A-any C-top   }
              | /  |  \ |      }  Any-nulls
           B-any   |   C-any   }
              |    |    |
           B-con A-con C-con   } constants; not comparable across classes
              |    |    |
           B-not   |   C-not   }
              | \  |  / |      }  not-nulls
           B-bot A-not C-bot   }
               \   |   /       }  Bottoms
                 A-bot         }
  */
  
  case InstPtr: {                // Meeting 2 Oops?
    // Found an InstPtr sub-type vs self-InstPtr type
    const TypeInstPtr *tinst = t->is_instptr();
    int off = meet_offset( tinst->offset() );
    PTR ptr = meet_ptr( tinst->ptr() );

    // Check for easy case; klasses are equal (and perhaps not loaded!)
    // If we have constants, then we created oops so classes are loaded
    // and we can handle the constants further down.  This case handles
    // both-not-loaded or both-loaded classes
    if (ptr != Constant && klass()->equals(tinst->klass()) && klass_is_exact() == tinst->klass_is_exact()) {
      return make( ptr, klass(), klass_is_exact(), NULL, off );
    }
      
    // Classes require inspection in the Java klass hierarchy.  Must be loaded.
    ciKlass* tinst_klass = tinst->klass();
    ciKlass* this_klass  = this->klass();
    bool tinst_xk = tinst->klass_is_exact();
    bool this_xk  = this->klass_is_exact();
    if (!tinst_klass->is_loaded() || !this_klass->is_loaded() ) {
      // One of these classes has not been loaded
      const TypeInstPtr *unloaded_meet = xmeet_unloaded(tinst);
#ifndef PRODUCT
      if( PrintOpto && Verbose ) {
        tty->print("meet of unloaded classes resulted in: "); unloaded_meet->dump(); tty->cr();
        tty->print("  this == "); this->dump(); tty->cr();
        tty->print(" tinst == "); tinst->dump(); tty->cr();
      }
#endif
      return unloaded_meet;
    }

    // Handle mixing oops and interfaces first.
    if( this_klass->is_interface() && !tinst_klass->is_interface() ) {
      ciKlass *tmp = tinst_klass; // Swap interface around
      tinst_klass = this_klass;
      this_klass = tmp;
      bool tmp2 = tinst_xk;
      tinst_xk = this_xk;
      this_xk = tmp2;
    }
    if (tinst_klass->is_interface() &&
        !(this_klass->is_interface() ||
          // Treat java/lang/Object as an honorary interface,
          // because we need a bottom for the interface hierarchy.
          this_klass == ciEnv::current()->Object_klass())) {
      // Oop meets interface!

      // See if the oop subtypes (implements) interface.
      ciKlass *k;
      bool xk;
      if( this_klass->is_subtype_of( tinst_klass ) ) {
        // Oop indeed subtypes.  Now keep oop or interface depending
        // on whether we are both above the centerline or either is
        // below the centerline.  If we are on the centerline
        // (e.g., Constant vs. AnyNull interface), use the constant.
        k  = below_centerline(ptr) ? tinst_klass : this_klass;
        // If we are keeping this_klass, keep its exactness too.
        xk = below_centerline(ptr) ? tinst_xk    : this_xk;
      } else {                  // Does not implement, fall to Object
        // Oop does not implement interface, so mixing falls to Object
        // just like the verifier does (if both are above the
        // centerline fall to interface)
        k = above_centerline(ptr) ? tinst_klass : ciEnv::current()->Object_klass();
        xk = above_centerline(ptr) ? tinst_xk : false;
        // Watch out for Constant vs. AnyNull interface.
        if (ptr == Constant)  ptr = NotNull;   // forget it was a constant
      }
      ciObject* o = NULL;  // the Constant value, if any
      if (ptr == Constant) {
        // Find out which constant.
        o = (this_klass == klass()) ? const_oop() : tinst->const_oop();
      }
      return make( ptr, k, xk, o, off );
    }

    // Either oop vs oop or interface vs interface or interface vs Object

    // !!! Here's how the symmetry requirement breaks down into invariants:
    // If we split one up & one down AND they subtype, take the down man.
    // If we split one up & one down AND they do NOT subtype, "fall hard".
    // If both are up and they subtype, take the subtype class.
    // If both are up and they do NOT subtype, "fall hard".
    // If both are down and they subtype, take the supertype class.
    // If both are down and they do NOT subtype, "fall hard".
    // Constants treated as down.

    // Now, reorder the above list; observe that both-down+subtype is also 
    // "fall hard"; "fall hard" becomes the default case:
    // If we split one up & one down AND they subtype, take the down man.
    // If both are up and they subtype, take the subtype class.

    // If both are down and they subtype, "fall hard".
    // If both are down and they do NOT subtype, "fall hard".
    // If both are up and they do NOT subtype, "fall hard".
    // If we split one up & one down AND they do NOT subtype, "fall hard".

    // If a proper subtype is exact, and we return it, we return it exactly.
    // If a proper supertype is exact, there can be no subtyping relationship!
    // If both types are equal to the subtype, exactness is and-ed below the
    // centerline and or-ed above it.  (N.B. Constants are always exact.)
    
    // Check for subtyping:
    ciKlass *subtype = NULL;
    bool subtype_exact = false;
    if( tinst_klass->equals(this_klass) ) {
      subtype = this_klass;
      subtype_exact = below_centerline(ptr) ? (this_xk & tinst_xk) : (this_xk | tinst_xk);
    } else if( !tinst_xk && this_klass->is_subtype_of( tinst_klass ) ) {
      subtype = this_klass;     // Pick subtyping class
      subtype_exact = this_xk;
    } else if( !this_xk && tinst_klass->is_subtype_of( this_klass ) ) {
      subtype = tinst_klass;    // Pick subtyping class
      subtype_exact = tinst_xk;
    }

    if( subtype ) {
      if( above_centerline(ptr) ) { // both are up?
        this_klass = tinst_klass = subtype;
        this_xk = tinst_xk = subtype_exact;
      } else if( above_centerline(this ->_ptr) && !above_centerline(tinst->_ptr) ) {
        this_klass = tinst_klass; // tinst is down; keep down man
        this_xk = tinst_xk;
      } else if( above_centerline(tinst->_ptr) && !above_centerline(this ->_ptr) ) {
        tinst_klass = this_klass; // this is down; keep down man
        tinst_xk = this_xk;
      } else {
        this_xk = subtype_exact;  // either they are equal, or we'll do an LCA
      }
    }

    // Check for classes now being equal
    if (tinst_klass->equals(this_klass)) {
      // If the klasses are equal, the constants may still differ.  Fall to
      // NotNull if they do (neither constant is NULL; that is a special case
      // handled elsewhere).
      ciObject* o = NULL;             // Assume not constant when done
      ciObject* this_oop  = const_oop();
      ciObject* tinst_oop = tinst->const_oop();
      if( ptr == Constant ) {
        if (this_oop != NULL && tinst_oop != NULL &&
            this_oop->equals(tinst_oop) )
          o = this_oop;
        else if (above_centerline(this ->_ptr))
          o = tinst_oop;
        else if (above_centerline(tinst ->_ptr))
          o = this_oop;
        else
          ptr = NotNull;
      }
      return make( ptr, this_klass, this_xk, o, off );
    } // Else classes are not equal
               
    // Since klasses are different, we require a LCA in the Java
    // class hierarchy - which means we have to fall to at least NotNull.
    if( ptr == TopPTR || ptr == AnyNull || ptr == Constant )
      ptr = NotNull;

    // Now we find the LCA of Java classes
    ciKlass* k = this_klass->least_common_ancestor(tinst_klass);
    return make( ptr, k, false, NULL, off );
  } // End of case InstPtr

  case KlassPtr:
    return TypeInstPtr::BOTTOM;

  } // End of switch
  return this;                  // Return the double constant
}


//---------------------------mirror_type----------------------------------------
ciType* TypeInstPtr::mirror_type() const {
  // must be a singleton type
  if( const_oop() == NULL )  return NULL;

  // must be of type java.lang.Class
  if( klass() != ciEnv::current()->Class_klass() )  return NULL;

  return const_oop()->as_instance()->java_mirror_type();
}


//------------------------------xdual------------------------------------------
// Dual: do NOT dual on klasses.  This means I do NOT understand the Java
// inheritence mechanism.
const Type *TypeInstPtr::xdual() const {
  return new TypeInstPtr( dual_ptr(), klass(), klass_is_exact(), const_oop(), dual_offset() );
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeInstPtr::eq( const Type *t ) const {
  const TypeInstPtr *p = t->is_instptr();
  return 
    klass()->equals(p->klass()) &&
    TypeOopPtr::eq(p);          // Check sub-type stuff
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeInstPtr::hash(void) const {
  int hash = klass()->hash() + TypeOopPtr::hash();
  return hash;
}

//------------------------------dump2------------------------------------------
// Dump oop Type
#ifndef PRODUCT
void TypeInstPtr::dump2( Dict &d, uint depth ) const {
  // Print the name of the klass.
  klass()->print_name();

  switch( _ptr ) {
  case Constant:
    // TO DO: Make CI print the hex address of the underlying oop.
    if (WizardMode || Verbose) {
      const_oop()->print_oop();
    }
  case BotPTR:
    if (!WizardMode && !Verbose) {
      if( _klass_is_exact ) tty->print(":exact");
      break;
    }
  case TopPTR:
  case AnyNull:
  case NotNull:
    tty->print(":%s", ptr_msg[_ptr]);
    if( _klass_is_exact ) tty->print(":exact");
    break;
  }

  if( _offset ) {               // Dump offset, if any
    if( _offset == OffsetBot )      tty->print("+any");
    else if( _offset == OffsetTop ) tty->print("+unknown");
    else tty->print("+%d", _offset);
  }

  tty->print(" *");
}
#endif

//------------------------------add_offset-------------------------------------
const TypePtr *TypeInstPtr::add_offset( int offset ) const {
  return make( _ptr, klass(), klass_is_exact(), const_oop(), xadd_offset(offset) );
}

//=============================================================================
// Convenience common pre-built types.
const TypeAryPtr *TypeAryPtr::RANGE;
const TypeAryPtr *TypeAryPtr::OOPS;
const TypeAryPtr *TypeAryPtr::BYTES;
const TypeAryPtr *TypeAryPtr::SHORTS;
const TypeAryPtr *TypeAryPtr::CHARS;
const TypeAryPtr *TypeAryPtr::INTS;
const TypeAryPtr *TypeAryPtr::LONGS;
const TypeAryPtr *TypeAryPtr::FLOATS;
const TypeAryPtr *TypeAryPtr::DOUBLES;

//------------------------------make-------------------------------------------
const TypeAryPtr *TypeAryPtr::make( PTR ptr, const TypeAry *ary, ciKlass* k, bool xk, int offset ) {
  assert(!(k == NULL && ary->_elem->isa_int()),
         "integral arrays must be pre-equipped with a class");
  if (!xk)  xk = ary->ary_must_be_exact();
  if (!UseExactTypes)  xk = (ptr == Constant);
  return (TypeAryPtr*)(new TypeAryPtr(ptr, NULL, ary, k, xk, offset))->hashcons();
}

//------------------------------make-------------------------------------------
const TypeAryPtr *TypeAryPtr::make( PTR ptr, ciObject* o, const TypeAry *ary, ciKlass* k, bool xk, int offset ) {
  assert(!(k == NULL && ary->_elem->isa_int()),
         "integral arrays must be pre-equipped with a class");
  assert( (ptr==Constant && o) || (ptr!=Constant && !o), "" );
  if (!xk)  xk = (o != NULL) || ary->ary_must_be_exact();
  if (!UseExactTypes)  xk = (ptr == Constant);
  return (TypeAryPtr*)(new TypeAryPtr(ptr, o, ary, k, xk, offset))->hashcons();
}

//------------------------------cast_to_ptr_type-------------------------------
const Type *TypeAryPtr::cast_to_ptr_type(PTR ptr) const {
  if( ptr == _ptr ) return this;
  return make(ptr, const_oop(), _ary, klass(), klass_is_exact(), _offset);
}


//-----------------------------cast_to_exactness-------------------------------
const Type *TypeAryPtr::cast_to_exactness(bool klass_is_exact) const {
  if( klass_is_exact == _klass_is_exact ) return this;
  if (!UseExactTypes)  return this;
  if (_ary->ary_must_be_exact())  return this;  // cannot clear xk
  return make(ptr(), const_oop(), _ary, klass(), klass_is_exact, _offset);
}


//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeAryPtr::eq( const Type *t ) const {
  const TypeAryPtr *p = t->is_aryptr();
  return 
    _ary == p->_ary &&  // Check array
    TypeOopPtr::eq(p);  // Check sub-parts
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeAryPtr::hash(void) const {
  return (intptr_t)_ary + TypeOopPtr::hash();
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeAryPtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?
  // Current "this->_base" is Pointer
  switch (t->base()) {          // switch on original type

  // Mixing ints & oops happens when javac reuses local variables
  case Int:
  case Long:
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  case Top:
    return this;

  default:                      // All else is a mistake
    typerr(t);

  case OopPtr: {                // Meeting to OopPtrs
    // Found a OopPtr type vs self-AryPtr type
    const TypePtr *tp = t->is_oopptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case TopPTR: 
    case AnyNull:
      return make(ptr, (ptr == Constant ? const_oop() : NULL), _ary, _klass, _klass_is_exact, offset);
    case BotPTR:
    case NotNull:
      return TypeOopPtr::make(ptr, offset);
    default: ShouldNotReachHere();
    }
  }

  case AnyPtr: {                // Meeting two AnyPtrs
    // Found an AnyPtr type vs self-AryPtr type
    const TypePtr *tp = t->is_ptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case TopPTR: 
      return this;
    case BotPTR:
    case NotNull:
      return TypePtr::make(AnyPtr, ptr, offset);
    case Null:   
      if( ptr == Null ) return TypePtr::make(AnyPtr, ptr, offset);
    case AnyNull:
      return make( ptr, (ptr == Constant ? const_oop() : NULL), _ary, _klass, _klass_is_exact, offset );
    default: ShouldNotReachHere();
    }
  }

  case RawPtr: return TypePtr::BOTTOM;

  case AryPtr: {                // Meeting 2 references?
    const TypeAryPtr *tap = t->is_aryptr();
    int off = meet_offset(tap->offset());
    const TypeAry *tary = _ary->meet(tap->_ary)->is_ary();
    PTR ptr = meet_ptr(tap->ptr());
    ciKlass* lazy_klass = NULL;
    if (tary->_elem->isa_int()) {
      // Integral array element types have irrelevant lattice relations.
      // It is the klass that determines array layout, not the element type.
      if (_klass == NULL)
        lazy_klass = tap->_klass;
      else if (tap->_klass == NULL || tap->_klass == _klass) {
        lazy_klass = _klass;
      } else {  
        // Something like byte[int+] meets char[int+].
        // This must fall to bottom, not (int[-128..65535])[int+].
        tary = TypeAry::make(Type::BOTTOM, tary->_size);
      }
    }
    bool xk;
    switch (tap->ptr()) {
    case AnyNull: 
    case TopPTR:  
      // Compute new klass on demand, do not use tap->_klass
      xk = (tap->_klass_is_exact | this->_klass_is_exact);
      return make( ptr, const_oop(), tary, lazy_klass, xk, off );
    case Constant: {
      ciObject* o = const_oop();
      if( _ptr == Constant ) {
        if( tap->const_oop() != NULL && !o->equals(tap->const_oop()) ) {
          ptr = NotNull;
          o = NULL;
        }
      } else if( above_centerline(_ptr) ) {
        o = tap->const_oop();
      }
      xk = true;
      return TypeAryPtr::make( ptr, o, tary, tap->_klass, xk, off );
    }
    case NotNull: 
    case BotPTR:  
      // Compute new klass on demand, do not use tap->_klass
      if (above_centerline(this->_ptr))
            xk = tap->_klass_is_exact;
      else if (above_centerline(tap->_ptr))
            xk = this->_klass_is_exact;
      else  xk = (tap->_klass_is_exact & this->_klass_is_exact) &&
              (klass() == tap->klass()); // Only precise for identical arrays
      return TypeAryPtr::make( ptr, NULL, tary, lazy_klass, xk, off );
    default: ShouldNotReachHere();
    }
  }

  // All arrays inherit from Object class
  case InstPtr: {
    const TypeInstPtr *tp = t->is_instptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (ptr) {
    case TopPTR: 
    case AnyNull:                // Fall 'down' to dual of object klass
      if( tp->klass()->equals(ciEnv::current()->Object_klass()) ) {
        return TypeAryPtr::make( ptr, _ary, _klass, _klass_is_exact, offset );
      } else {
        // cannot subclass, so the meet has to fall badly below the centerline
        ptr = NotNull;
        return TypeInstPtr::make( ptr, ciEnv::current()->Object_klass(), false, NULL,offset);
      }
    case Constant:
    case NotNull:
    case BotPTR:                // Fall down to object klass
      // LCA is object_klass, but if we subclass from the top we can do better
      if (above_centerline(tp->ptr())) {
        // If 'tp'  is above the centerline and it is Object class
        // then we can subclass in the Java class heirarchy.
        if( tp->klass()->equals(ciEnv::current()->Object_klass()) ) {
          // that is, my array type is a subtype of 'tp' klass
          return make( ptr, _ary, _klass, _klass_is_exact, offset );
        }
      }
      // The other case cannot happen, since t cannot be a subtype of an array.
      // The meet falls down to Object class below centerline.
      if( ptr == Constant )
         ptr = NotNull;
      return TypeInstPtr::make( ptr, ciEnv::current()->Object_klass(), false, NULL,offset);
    default: typerr(t);
    }
  }

  case KlassPtr:
    return TypeInstPtr::BOTTOM;

  }
  return this;                  // Lint noise
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type *TypeAryPtr::xdual() const {
  return new TypeAryPtr( dual_ptr(), _const_oop, _ary->dual()->is_ary(),_klass, _klass_is_exact, dual_offset());
}

//------------------------------dump2------------------------------------------
#ifndef PRODUCT
void TypeAryPtr::dump2( Dict &d, uint depth ) const {
  _ary->dump2(d,depth);
  switch( _ptr ) {
  case Constant:
    const_oop()->print();
    break;
  case BotPTR:
    if (!WizardMode && !Verbose) {
      if( _klass_is_exact ) tty->print(":exact");
      break;
    }
  case TopPTR:
  case AnyNull:
  case NotNull:
    tty->print(":%s", ptr_msg[_ptr]);
    if( _klass_is_exact ) tty->print(":exact");
    break;
  }

  tty->print("*");
  if( !_offset ) return;
  if( _offset == OffsetTop )      tty->print("+undefined");
  else if( _offset == OffsetBot ) tty->print("+any");
  else if( _offset < 12 )         tty->print("+%d",_offset);
  else                            tty->print("[%d]", (_offset-12)/4 );
}
#endif

bool TypeAryPtr::empty(void) const {
  if (_ary->empty())       return true;
  return TypeOopPtr::empty();
}

//------------------------------add_offset-------------------------------------
const TypePtr *TypeAryPtr::add_offset( int offset ) const {
  return make( _ptr, _const_oop, _ary, _klass, _klass_is_exact, xadd_offset(offset) );
}


//=============================================================================
// Convenience common pre-built types.

// Not-null object klass or below
const TypeKlassPtr *TypeKlassPtr::OBJECT;

//------------------------------TypeKlasPtr------------------------------------
TypeKlassPtr::TypeKlassPtr( PTR ptr, ciKlass* klass, int offset )
  : TypeOopPtr(KlassPtr, ptr, klass, (ptr==Constant), (ptr==Constant ? klass : NULL), offset) {
}

//------------------------------make-------------------------------------------
// ptr to klass 'k', if Constant, or possibly to a sub-klass if not a Constant
const TypeKlassPtr *TypeKlassPtr::make( PTR ptr, ciKlass* k, int offset ) {
  assert( k != NULL, "Expect a non-NULL klass");
  assert(k->is_instance_klass() || k->is_array_klass() ||
         k->is_method_klass(), "Incorrect type of klass oop");
  TypeKlassPtr *r =
    (TypeKlassPtr*)(new TypeKlassPtr(ptr, k, offset))->hashcons();

  return r;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeKlassPtr::eq( const Type *t ) const {
  const TypeKlassPtr *p = t->is_klassptr();
  return 
    klass()->equals(p->klass()) && 
    TypeOopPtr::eq(p);
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeKlassPtr::hash(void) const {
  return klass()->hash() + TypeOopPtr::hash();
}


//------------------------------klass------------------------------------------
// Return the defining klass for this class
ciKlass* TypeAryPtr::klass() const {
  if( _klass ) return _klass;   // Return cached value, if possible

  // Oops, need to compute _klass and cache it
  ciKlass* k_ary = NULL;
  const TypeInstPtr *tinst;
  const TypeAryPtr *tary;
  // Get element klass
  if ((tinst = elem()->isa_instptr()) != NULL) {
    // Compute array klass from element klass
    k_ary = ciObjArrayKlass::make(tinst->klass());
  } else if ((tary = elem()->isa_aryptr()) != NULL) {
    // Compute array klass from element klass
    ciKlass* k_elem = tary->klass();
    // If element type is something like bottom[], k_elem will be null.
    if (k_elem != NULL)
      k_ary = ciObjArrayKlass::make(k_elem);
  } else if ((elem()->base() == Type::Top) || 
             (elem()->base() == Type::Bottom)) {
    // element type of Bottom occurs from meet of basic type
    // and object; Top occurs when doing join on Bottom.
    // Leave k_ary at NULL.
  } else {
    // Cannot compute array klass directly from basic type,
    // since subtypes of TypeInt all have basic type T_INT.
    assert(!elem()->isa_int(),
           "integral arrays must be pre-equipped with a class");
    // Compute array klass directly from basic type
    k_ary = ciTypeArrayKlass::make(elem()->basic_type());
  }
  
  if( this != TypeAryPtr::OOPS )
    // The _klass field acts as a cache of the underlying
    // ciKlass for this array type.  In order to set the field,
    // we need to cast away const-ness.
    //
    // IMPORTANT NOTE: we *never* set the _klass field for the
    // type TypeAryPtr::OOPS.  This Type is shared between all
    // active compilations.  However, the ciKlass which represents
    // this Type is *not* shared between compilations, so caching
    // this value would result in fetching a dangling pointer.
    //
    // Recomputing the underlying ciKlass for each request is
    // a bit less efficient than caching, but calls to
    // TypeAryPtr::OOPS->klass() are not common enough to matter.
    ((TypeAryPtr*)this)->_klass = k_ary;
  return k_ary;
}


//------------------------------add_offset-------------------------------------
// Access internals of klass object
const TypePtr *TypeKlassPtr::add_offset( int offset ) const {
  return make( _ptr, klass(), xadd_offset(offset) );
}

//------------------------------cast_to_ptr_type-------------------------------
const Type *TypeKlassPtr::cast_to_ptr_type(PTR ptr) const {
  assert(_base == OopPtr, "subclass must override cast_to_ptr_type");
  if( ptr == _ptr ) return this;
  return make(ptr, _klass, _offset);
}


//-----------------------------cast_to_exactness-------------------------------
const Type *TypeKlassPtr::cast_to_exactness(bool klass_is_exact) const {
  if( klass_is_exact == _klass_is_exact ) return this;
  if (!UseExactTypes)  return this;
  return make(klass_is_exact ? Constant : NotNull, _klass, _offset);
}


//------------------------------xmeet------------------------------------------
// Compute the MEET of two types, return a new Type object.
const Type    *TypeKlassPtr::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is Pointer
  switch (t->base()) {          // switch on original type

  case Int:                     // Mixing ints & oops happens when javac
  case Long:                    // reuses local variables
  case FloatTop:
  case FloatCon:
  case FloatBot:
  case DoubleTop:
  case DoubleCon:
  case DoubleBot:
  case Bottom:                  // Ye Olde Default
    return Type::BOTTOM;
  case Top:
    return this;

  default:                      // All else is a mistake
    typerr(t);

  case RawPtr: return TypePtr::BOTTOM;

  case OopPtr: {                // Meeting to OopPtrs
    // Found a OopPtr type vs self-KlassPtr type
    const TypePtr *tp = t->is_oopptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case TopPTR: 
    case AnyNull:
      return make(ptr, klass(), offset);
    case BotPTR:
    case NotNull:
      return TypePtr::make(AnyPtr, ptr, offset);
    default: typerr(t);
    }
  }

  case AnyPtr: {                // Meeting to AnyPtrs
    // Found an AnyPtr type vs self-KlassPtr type
    const TypePtr *tp = t->is_ptr();
    int offset = meet_offset(tp->offset());
    PTR ptr = meet_ptr(tp->ptr());
    switch (tp->ptr()) {
    case TopPTR: 
      return this;
    case Null:
      if( ptr == Null ) return TypePtr::make( AnyPtr, ptr, offset );
    case AnyNull:
      return make( ptr, klass(), offset );
    case BotPTR:
    case NotNull:
      return TypePtr::make(AnyPtr, ptr, offset);
    default: typerr(t);
    }
  }

  case AryPtr:                  // Meet with AryPtr
  case InstPtr:                 // Meet with InstPtr
    return TypeInstPtr::BOTTOM;

  //  
  //             A-top         }
  //           /   |   \       }  Tops
  //       B-top A-any C-top   }
  //          | /  |  \ |      }  Any-nulls
  //       B-any   |   C-any   }
  //          |    |    |
  //       B-con A-con C-con   } constants; not comparable across classes
  //          |    |    |
  //       B-not   |   C-not   }
  //          | \  |  / |      }  not-nulls
  //       B-bot A-not C-bot   }
  //           \   |   /       }  Bottoms
  //             A-bot         }
  //
  
  case KlassPtr: {  // Meet two KlassPtr types
    const TypeKlassPtr *tkls = t->is_klassptr();
    int  off     = meet_offset(tkls->offset());
    PTR  ptr     = meet_ptr(tkls->ptr());

    // Check for easy case; klasses are equal (and perhaps not loaded!)
    // If we have constants, then we created oops so classes are loaded
    // and we can handle the constants further down.  This case handles
    // not-loaded classes
    if( ptr != Constant && tkls->klass()->equals(klass()) ) {
      return make( ptr, klass(), off );
    }

    // Classes require inspection in the Java klass hierarchy.  Must be loaded.
    ciKlass* tkls_klass = tkls->klass();
    ciKlass* this_klass = this->klass();
    assert( tkls_klass->is_loaded(), "This class should have been loaded.");
    assert( this_klass->is_loaded(), "This class should have been loaded.");

    // If 'this' type is above the centerline and is a superclass of the
    // other, we can treat 'this' as having the same type as the other.
    if ((above_centerline(this->ptr())) &&
        tkls_klass->is_subtype_of(this_klass)) {
      this_klass = tkls_klass;
    }
    // If 'tinst' type is above the centerline and is a superclass of the
    // other, we can treat 'tinst' as having the same type as the other.
    if ((above_centerline(tkls->ptr())) &&
        this_klass->is_subtype_of(tkls_klass)) {
      tkls_klass = this_klass;
    }

    // Check for classes now being equal
    if (tkls_klass->equals(this_klass)) {
      // If the klasses are equal, the constants may still differ.  Fall to
      // NotNull if they do (neither constant is NULL; that is a special case
      // handled elsewhere).
      ciObject* o = NULL;             // Assume not constant when done
      ciObject* this_oop = const_oop();
      ciObject* tkls_oop = tkls->const_oop();
      if( ptr == Constant ) {
        if (this_oop != NULL && tkls_oop != NULL &&
            this_oop->equals(tkls_oop) )
          o = this_oop;
        else if (above_centerline(this->ptr()))
          o = tkls_oop;
        else if (above_centerline(tkls->ptr()))
          o = this_oop;
        else
          ptr = NotNull;
      }
      return make( ptr, this_klass, off );
    } // Else classes are not equal
               
    // Since klasses are different, we require the LCA in the Java
    // class hierarchy - which means we have to fall to at least NotNull.
    if( ptr == TopPTR || ptr == AnyNull || ptr == Constant )
      ptr = NotNull;
    // Now we find the LCA of Java classes
    ciKlass* k = this_klass->least_common_ancestor(tkls_klass);
    return   make( ptr, k, off );
  } // End of case KlassPtr

  } // End of switch
  return this;                  // Return the double constant
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type    *TypeKlassPtr::xdual() const {
  return new TypeKlassPtr( dual_ptr(), klass(), dual_offset() );
}

//------------------------------dump2------------------------------------------
// Dump Klass Type
#ifndef PRODUCT
void TypeKlassPtr::dump2( Dict & d, uint depth ) const {
  switch( _ptr ) {
  case Constant:
    tty->print("precise ");
  case NotNull:
    {
      const char *name = klass()->name()->as_utf8();
      if( name ) {
        tty->print("klass %s: " INTPTR_FORMAT, name, klass());
      } else {
        ShouldNotReachHere();
      }
    }
  case BotPTR:
    if( !WizardMode && !Verbose && !_klass_is_exact ) break;
  case TopPTR:
  case AnyNull:
    tty->print(":%s", ptr_msg[_ptr]);
    if( _klass_is_exact ) tty->print(":exact");
    break;
  }

  if( _offset ) {               // Dump offset, if any
    if( _offset == OffsetBot )      { tty->print("+any"); }
    else if( _offset == OffsetTop ) { tty->print("+unknown"); }
    else                            { tty->print("+%d", _offset); }
  }

  tty->print(" *");
}
#endif



//=============================================================================
// Convenience common pre-built types.

//------------------------------make-------------------------------------------
const TypeFunc *TypeFunc::make( const TypeTuple *domain, const TypeTuple *range ) {
  return (TypeFunc*)(new TypeFunc(domain,range))->hashcons();
}

//------------------------------make-------------------------------------------
const TypeFunc *TypeFunc::make(ciMethod* method) {
  Compile* C = Compile::current();
  const TypeFunc* tf = C->last_tf(method); // check cache
  if (tf != NULL)  return tf;  // The hit rate here is almost 50%.
  const TypeTuple *domain;
  if (method->flags().is_static()) {
    domain = TypeTuple::make_domain(NULL, method->signature());
  } else {
    domain = TypeTuple::make_domain(method->holder(), method->signature());
  }
  const TypeTuple *range  = TypeTuple::make_range(method->signature());
  tf = TypeFunc::make(domain, range);
  C->set_last_tf(method, tf);  // fill cache
  return tf;
}

//------------------------------meet-------------------------------------------
// Compute the MEET of two types.  It returns a new Type object.
const Type *TypeFunc::xmeet( const Type *t ) const {
  // Perform a fast test for common case; meeting the same types together.
  if( this == t ) return this;  // Meeting same type-rep?

  // Current "this->_base" is Func
  switch (t->base()) {          // switch on original type

  case Bottom:                  // Ye Olde Default
    return t;

  default:                      // All else is a mistake
    typerr(t);

  case Top:
    break;
  }
  return this;                  // Return the double constant
}

//------------------------------xdual------------------------------------------
// Dual: compute field-by-field dual
const Type *TypeFunc::xdual() const {
  return this;
}

//------------------------------eq---------------------------------------------
// Structural equality check for Type representations
bool TypeFunc::eq( const Type *t ) const {
  const TypeFunc *a = (const TypeFunc*)t;
  return _domain == a->_domain &&
    _range == a->_range;
}

//------------------------------hash-------------------------------------------
// Type-specific hashing function.
int TypeFunc::hash(void) const {
  return (intptr_t)_domain + (intptr_t)_range;
}

//------------------------------dump2------------------------------------------
// Dump Function Type
#ifndef PRODUCT
void TypeFunc::dump2( Dict &d, uint depth ) const {
  if( _range->_cnt <= Parms )
    tty->print("void");
  else {
    uint i;
    for (i = Parms; i < _range->_cnt-1; i++) {
      _range->field_at(i)->dump2(d,depth);
      tty->print("/");
    }
    _range->field_at(i)->dump2(d,depth);
  }
  tty->print(" ");
  tty->print("( ");
  if( !depth || d[this] ) {     // Check for recursive dump
    tty->print("...)");
    return;
  }
  d.Insert((void*)this,(void*)this);    // Stop recursion
  if (Parms < _domain->_cnt)
    _domain->field_at(Parms)->dump2(d,depth-1);
  for (uint i = Parms+1; i < _domain->_cnt; i++) {
    tty->print(", ");
    _domain->field_at(i)->dump2(d,depth-1);
  }
  tty->print(" )");
}

//------------------------------print_flattened--------------------------------
// Print a 'flattened' signature
static const char * const flat_type_msg[Type::lastype] = {
  "bad","control","top","int","long","_", 
  "tuple:", "array:", 
  "ptr", "rawptr", "ptr", "ptr", "ptr", "ptr", 
  "func", "abIO", "return_address", "mem", 
  "float_top", "ftcon:", "flt",
  "double_top", "dblcon:", "dbl",
  "bottom"
};

void TypeFunc::print_flattened() const {
  if( _range->_cnt <= Parms )
    tty->print("void");
  else {
    uint i;
    for (i = Parms; i < _range->_cnt-1; i++)
      tty->print("%s/",flat_type_msg[_range->field_at(i)->base()]);
    tty->print("%s",flat_type_msg[_range->field_at(i)->base()]);
  }
  tty->print(" ( ");
  if (Parms < _domain->_cnt)
    tty->print("%s",flat_type_msg[_domain->field_at(Parms)->base()]);
  for (uint i = Parms+1; i < _domain->_cnt; i++)
    tty->print(", %s",flat_type_msg[_domain->field_at(i)->base()]);
  tty->print(" )");
}
#endif

//------------------------------singleton--------------------------------------
// TRUE if Type is a singleton type, FALSE otherwise.   Singletons are simple
// constants (Ldi nodes).  Singletons are integer, float or double constants
// or a single symbol.
bool TypeFunc::singleton(void) const {
  return false;                 // Never a singleton
}

bool TypeFunc::empty(void) const {
  return false;                 // Never empty
}


bool TypeFunc::returns_long() const{
  const Type *return_val = range()->cnt() == TypeFunc::Parms ?
                        Type::TOP : range()->field_at(TypeFunc::Parms);

  return return_val != Type::TOP && return_val->base() == Type::Long;
}

