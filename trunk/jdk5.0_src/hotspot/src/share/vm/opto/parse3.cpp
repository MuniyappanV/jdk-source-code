#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)parse3.cpp	1.254 04/03/02 02:08:25 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_parse3.cpp.incl"

//=============================================================================
// Helper methods for _get* and _put* bytecodes
//=============================================================================
bool Parse::static_field_ok_in_clinit(ciField *field, ciMethod *method) {
  // Could be the field_holder's <clinit> method, or <clinit> for a subklass.
  // Better to check now than to Deoptimize as soon as we execute
  assert( field->is_static(), "Only check if field is static");
  // is_being_initialized() is too generous.  It allows access to statics 
  // by threads that are not running the <clinit> before the <clinit> finishes.
  // return field->holder()->is_being_initialized();

  // The following restriction is correct but conservative.
  // It is also desirable to allow compilation of methods called from <clinit>
  // but this generated code will need to be made safe for execution by 
  // other threads, or the transition from interpreted to compiled code would
  // need to be guarded.
  ciInstanceKlass *field_holder = field->holder();

  bool access_OK = false;
  if (method->holder()->is_subclass_of(field_holder)) {
    if (method->is_static()) {
      if (method->name() == ciSymbol::class_initializer_name()) {
        // OK to access static fields inside initializer
        access_OK = true;
      }
    } else {
      if (method->name() == ciSymbol::object_initializer_name()) {
        // It's also OK to access static fields inside a constructor,
        // because any thread calling the constructor must first have
        // synchronized on the class by executing a '_new' bytecode.
        access_OK = true;
      }
    }
  }

  return access_OK;

}


void Parse::do_field_access(bool is_get, bool is_field) {
  bool will_link;
  ciField* field = iter().get_field(will_link);
  assert(will_link, "getfield: typeflow responsibility");

  ciInstanceKlass* field_holder = field->holder();

  if (is_field == field->is_static()) {
    // Interpreter will throw java_lang_IncompatibleClassChangeError
    // Check this before allowing <clinit> methods to access static fields
    uncommon_trap(Deoptimization::Reason_unhandled,
                  Deoptimization::Action_none);
    return;
  }

  if (!is_field && !field_holder->is_initialized()) {
    if (!static_field_ok_in_clinit(field, method())) {
      uncommon_trap(Deoptimization::Reason_uninitialized,
                    Deoptimization::Action_reinterpret,
                    NULL, "!static_field_ok_in_clinit");
      return;
    }
  }

  assert(field->will_link(method()->holder(), bc()), "getfield: typeflow responsibility");

  // Note:  We do not check for an unloaded field type here any more.

  // Generate code for the object pointer.
  Node* obj;
  if (is_field) {
    int obj_depth = is_get ? 0 : field->type()->size();
    obj = do_null_check(peek(obj_depth), T_OBJECT);
    // Compile-time detect of null-exception?
    if (stopped())  return;

    const TypeInstPtr *tjp = TypeInstPtr::make(TypePtr::NotNull, iter().get_declared_field_holder());
    assert(_gvn.type(obj)->higher_equal(tjp), "cast_up is no longer needed");

    if (is_get) {
      --_sp;  // pop receiver before getting
      do_get_xxx(tjp, obj, field, is_field);
    } else {
      do_put_xxx(tjp, obj, field, is_field);
      --_sp;  // pop receiver after putting
    }
  } else {
    const TypeKlassPtr* tkp = TypeKlassPtr::make(field_holder);
    obj = _gvn.makecon(tkp);
    if (is_get) {
      do_get_xxx(tkp, obj, field, is_field);
    } else {
      do_put_xxx(tkp, obj, field, is_field);
    }
  }
}


void Parse::do_get_xxx(const TypePtr* obj_type, Node* obj, ciField* field, bool is_field) {
  // Does this field have a constant value?  If so, just push the value.
  if (field->is_constant() && push_constant(field->constant_value()))  return;

  ciType* field_klass = field->type();

  // Compute address and memory type.
  int offset = field->offset_in_bytes();
  const TypePtr* adr_type = C->alias_type(field)->adr_type();
  Node *adr = basic_plus_adr(obj, obj, offset);
  BasicType bt = field->layout_type();

  // Build the resultant type of the load
  const Type *type;

  bool must_assert_null = false;

  if( bt == T_OBJECT ) {
    if (!field->type()->is_loaded()) {
      type = TypeInstPtr::BOTTOM;
      must_assert_null = true;
    } else if (field->is_constant()) {
      // This can happen if the constant oop is non-perm.
      ciObject* con = field->constant_value().as_object();
      // Do not "join" in the previous type; it doesn't add value,
      // and may yield a vacuous result if the field is of interface type.
      type = TypeOopPtr::make_from_constant(con)->isa_oopptr();
      assert(type != NULL, "field singleton type must be consistent");
    } else {
      type = TypeOopPtr::make_from_klass(field_klass->as_klass());
    }
  } else {
    type = Type::get_const_basic_type(bt);
  }
  // Build the load.  
  Node *ld = make_load( NULL, adr, type, bt, adr_type );

  // Adjust Java stack
  if( type2size[bt] == 1 ) 
    push(ld);
  else 
    push_pair(ld);

  if (must_assert_null) {
    // Do not take a trap here.  It's possible that the program
    // will never load the field's class, and will happily see
    // null values in this field forever.  Don't stumble into a
    // trap for such a program, or we might get a long series
    // of useless recompilations.  (Or, we might load a class
    // which should not be loaded.)  If we ever see a non-null
    // value, we will then trap and recompile.  (The trap will
    // not need to mention the class index, since the class will
    // already have been loaded if we ever see a non-null value.)
    // uncommon_trap(iter().get_field_signature_index());
    if (PrintOpto && (Verbose || WizardMode)) {
      method()->print_name(); tty->print_cr(" asserting nullness of field at bci: %d", bci());
    }
    if (C->log() != NULL) {
      C->log()->elem("assert_null reason='field' klass='%d'",
                     C->log()->identify(field->type()));
    }
    // If there is going to be a trap, put it at the next bytecode:
    set_bci(iter().next_bci());
    do_null_assert(peek(), T_OBJECT);
    set_bci(iter().cur_bci()); // put it back
  }

  // If reference is volatile, prevent following memory ops from
  // floating up past the volatile read.  Also prevents commoning
  // another volatile read.
  if( field->is_volatile() ) {
    MemBarNode *mb = new MemBarAcquireNode();
    mb->add_req(ld);            // Bogus read of value to force load BEFORE membar
    insert_mem_bar(mb);
  }
}

void Parse::do_put_xxx(const TypePtr* obj_type, Node* obj, ciField* field, bool is_field) {
  bool is_vol = field->is_volatile();
  // If reference is volatile, prevent following memory ops from
  // floating down past the volatile write.  Also prevents commoning
  // another volatile read.
  if( is_vol ) 
    insert_mem_bar(new MemBarReleaseNode());

  // Compute address and memory type.
  int offset = field->offset_in_bytes();
  const TypePtr* adr_type = C->alias_type(field)->adr_type();
  Node *adr = basic_plus_adr(obj, obj, offset);
  BasicType bt = field->layout_type();
  // Value to be stored
  Node *val = type2size[bt] == 1 ? pop() : pop_pair();
  // Round doubles before storing
  if( bt == T_DOUBLE ) val = dstore_rounding(val);

  // Store the value.  
  Node *store = store_to_memory( control(), adr, val, bt, adr_type );

  // Object-writes need a store-barrier
  if( bt == T_OBJECT ) store_barrier(store, obj, val);

  // If reference is volatile, prevent following volatiles ops from
  // floating up before the volatile write.
  if( is_vol ) {
    // First place the specific membar for THIS volatile index. This first
    // membar is dependent on the store, keeping any other membars generated
    // below from floating up past the store.
    int adr_idx = C->get_alias_index(adr_type);
    insert_mem_bar_volatile(new MemBarVolatileNode(), adr_idx);

    // Now place a membar for AliasIdxBot for the unknown yet-to-be-parsed 
    // volatile alias indices. Skip this if the membar is redundant.
    if (adr_idx != Compile::AliasIdxBot) {
      insert_mem_bar_volatile(new MemBarVolatileNode(), Compile::AliasIdxBot);
    }
      
    // Finally, place alias-index-specific membars for each volatile index
    // that isn't the adr_idx membar. Typically there's only 1 or 2.
    for( int i = Compile::AliasIdxRaw; i < C->num_alias_types(); i++ ) {
      if( i != adr_idx && C->alias_type(i)->is_volatile() ) {
        insert_mem_bar_volatile(new MemBarVolatileNode(), i);
      }
    }
  }

  // If the field is final, the rules of Java say we are in <init> or <clinit>.
  // Note the presence of writes to final non-static fields, so that we
  // can insert a memory barrier later on to keep the writes from floating
  // out of the constructor.
  if (is_field && field->is_final()) {
    set_wrote_final(true);
  }
}


bool Parse::push_constant(ciConstant constant) {
  switch (constant.basic_type()) {
  case T_BOOLEAN:  push( intcon(constant.as_boolean()) ); break;
  case T_INT:      push( intcon(constant.as_int())     ); break;
  case T_CHAR:     push( intcon(constant.as_char())    ); break;
  case T_BYTE:     push( intcon(constant.as_byte())    ); break;
  case T_SHORT:    push( intcon(constant.as_short())   ); break;
  case T_FLOAT:    push( makecon(TypeF::make(constant.as_float())) );  break;
  case T_DOUBLE:   push_pair( makecon(TypeD::make(constant.as_double())) );  break;
  case T_LONG:     push_pair( makecon(TypeLong::make(constant.as_long())) ); break;
  case T_ARRAY:
  case T_OBJECT: {
    // the oop is in perm space if the ciObject "has_encoding"
    ciObject* oop_constant = constant.as_object();
    if (oop_constant->is_null_object()) {
      push( zerocon(T_OBJECT) );
      break;
    } else if (oop_constant->has_encoding()) {
      push( makecon(TypeOopPtr::make_from_constant(oop_constant)) );
      break;
    } else {
      // we cannot inline the oop, but we can use it later to narrow a type
      return false;
    }
  }
  case T_ILLEGAL: {
    // Invalid ciConstant returned due to OutOfMemoryError in the CI
    assert(C->env()->failing(), "otherwise should not see this");
    // These always occur because of object types; we are going to
    // bail out anyway, so make the stack depths match up
    push( zerocon(T_OBJECT) );
    return false;
  }
  default:
    ShouldNotReachHere();
    return false;
  }

  // success
  return true;
}



//=============================================================================
void Parse::do_anewarray() {
  bool will_link;
  ciKlass* klass = iter().get_klass(will_link);

  // Uncommon Trap when class that array contains is not loaded
  // we need the loaded class for the rest of graph; do not
  // initialize the container class (see Java spec)!!!
  assert(will_link, "anewarray: typeflow responsibility");
  
  ciObjArrayKlass* array_klass = ciObjArrayKlass::make(klass);
  // Check that array_klass object is loaded
  if (!array_klass->is_loaded()) {
    // Generate uncommon_trap for unloaded array_class
    uncommon_trap(Deoptimization::Reason_unloaded,
                  Deoptimization::Action_reinterpret,
                  array_klass);
    return;
  }
  
  kill_dead_locals();

  const Type*         element_type     = TypeOopPtr::make_from_klass_raw(klass);
  const TypeKlassPtr* array_klass_type = TypeKlassPtr::make(array_klass);
  Node* count_val = pop();
  Node* obj = new_array(count_val, T_OBJECT, element_type, array_klass_type);
  push(obj);
}


void Parse::do_newarray(BasicType elem_type) {
  kill_dead_locals();

  Node*   count_val = pop();
  const Type* etype = Type::get_const_basic_type(elem_type);
  const TypeKlassPtr* array_klass = TypeKlassPtr::make(ciTypeArrayKlass::make(elem_type));
  Node*   obj = new_array(count_val, elem_type, etype, array_klass);
  // Push resultant oop onto stack
  push(obj);
}


void Parse::do_multianewarray() {
  int ndimensions = iter().get_dimensions();

  // the m-dimensional array
  bool will_link;
  ciArrayKlass* array_klass = iter().get_klass(will_link)->as_array_klass();
  assert(will_link, "multianewarray: typeflow responsibility");

  // Note:  Array classes are always initialized; no is_initialized check.

  if (ndimensions > 5) {
    uncommon_trap(Deoptimization::Reason_unhandled,
                  Deoptimization::Action_none);
    return;
  }

  kill_dead_locals();

  // Can use _multianewarray instead of _anewarray or _newarray
  // if only one dimension
  if( ndimensions == 1 && array_klass->is_type_array_klass() ) {
    // If this is for a basic type, call code for do_newarray instead
    BasicType element_type = array_klass->as_type_array_klass()->element_type();
    do_newarray(element_type);
    return;
  }

  ciObjArrayKlass* obj_array_klass = array_klass->as_obj_array_klass();

  // find the element type (etype)
  ciKlass* element_klass = obj_array_klass->base_element_klass();
  // Base_element is either an instance-klass or a type-array but NOT
  // a basic type.  We really wanted the klass of a basic type; since that's
  // not available we have to test for type-array here.
  const Type* element_type = element_klass->is_type_array_klass()
    ? Type::get_const_basic_type(element_klass->as_type_array_klass()->element_type())
    : TypeInstPtr::make(TypePtr::BotPTR, element_klass->as_instance_klass());

  int mdimensions = obj_array_klass->dimension();

  // get the lengths from the stack (last dimension is on top)
  Node** length = NEW_RESOURCE_ARRAY(Node*, ndimensions);
  for (int j = 0; j < ndimensions; j++) length[j] = pop();

  // construct the array type
  const Type* prev_type  = element_type;
  ciKlass*    prev_array = element_klass->is_type_array_klass() ? element_klass : NULL;

  // fill the lowest dimensions with unknown sizes
  for (int index = 0; index < mdimensions - ndimensions; index++) {
    const TypeAry* arr0 = TypeAry::make(prev_type, TypeInt::POS);
    prev_type = TypeAryPtr::make(TypePtr::BotPTR, arr0, prev_array, true, 0);
    prev_array = NULL; // array klasses can be lazy, except the first
  }

  // Fill in the dimensions with known sizes (passed in the JVM stack)
  for (int i = 0; i < ndimensions; i++) {
    const Type* count_type = TypeInt::POS;
    TypePtr::PTR ptr = TypePtr::BotPTR;
    // For the outermost dimension, try to get a better type than POS for the
    // size.  We don't do this for inner dimmensions because we lack the 
    // support to invalidate the refined type when the base array is modified
    // by an aastore, or when it aliased via certain uses of an aaload.
    if (i == ndimensions - 1) {
      count_type = length[i]->bottom_type()->join(count_type);
      ptr = TypePtr::NotNull;
    } 
    assert(count_type->is_int(), "must be integer");
    const TypeAry* arr0 = TypeAry::make(prev_type, (TypeInt*)count_type);
    prev_type = TypeAryPtr::make(ptr, arr0, prev_array, true, 0);
    prev_array = NULL; // array klasses can be lazy, except the first
  }
  const TypeAryPtr* arr = (const TypeAryPtr*)prev_type;

  address fun = NULL;
  switch (ndimensions) {
   case 1: fun = OptoRuntime::multianewarray1_Java(); break;
   case 2: fun = OptoRuntime::multianewarray2_Java(); break;
   case 3: fun = OptoRuntime::multianewarray3_Java(); break;
   case 4: fun = OptoRuntime::multianewarray4_Java(); break;
   case 5: fun = OptoRuntime::multianewarray5_Java(); break;
   default: ShouldNotReachHere();
  };
  CallNode* call = new CallStaticJavaNode(OptoRuntime::multianewarray_Type( ndimensions), fun, OptoRuntime::stub_name(fun), bci());
  set_predefined_input_for_runtime_call(call);
  call->set_req(TypeFunc::Parms+0, makecon(TypeKlassPtr::make(array_klass)));
  for (int k = 0; k < ndimensions; k++) {
    call->set_req(TypeFunc::Parms + (ndimensions - k), length[k]);
  }
  
  add_safepoint_edges(call);
  Node* c = _gvn.transform(call);
  set_predefined_output_for_runtime_call(c);
  Node *res = _gvn.transform(new (1) ProjNode(c, TypeFunc::Parms));
  Node *cast = _gvn.transform( new (2) CheckCastPPNode(control(), res, arr) );
  push( cast );
}
