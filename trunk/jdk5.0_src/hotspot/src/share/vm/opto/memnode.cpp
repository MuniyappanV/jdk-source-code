#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)memnode.cpp	1.212 04/03/17 09:56:31 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style

#include "incls/_precompiled.incl"
#include "incls/_memnode.cpp.incl"

//=============================================================================
uint MemNode::size_of() const { return sizeof(*this); }

const TypePtr *MemNode::adr_type() const {
  Node* adr = in(Address);
  const TypePtr* cross_check = NULL;
  debug_only(cross_check = _adr_type);
  return calculate_adr_type(adr->bottom_type(), cross_check);
}

#ifndef PRODUCT
void MemNode::dump_spec() const {
#ifndef ASSERT
  // fake the missing field
  const TypePtr* _adr_type = NULL;
  if (in(Address) != NULL)
    _adr_type = in(Address)->bottom_type()->isa_ptr();
#endif
  dump_adr_type(this, _adr_type);

  Compile* C = Compile::current();
  if( C->alias_type(adr_type())->is_volatile() )
    tty->print(" Volatile!");
}

void MemNode::dump_adr_type(const Node* mem, const TypePtr* adr_type) {
  tty->print(" @");
  if (adr_type == NULL) {
    tty->print("NULL");
  } else {
    adr_type->dump();
    Compile* C = Compile::current();
    Compile::AliasType* atp = NULL;
    if (C->have_alias_type(adr_type))  atp = C->alias_type(adr_type);
    if (atp == NULL)
      tty->print(", idx=?\?;");
    else if (atp->index() == Compile::AliasIdxBot)
      tty->print(", idx=Bot;");
    else if (atp->index() == Compile::AliasIdxTop)
      tty->print(", idx=Top;");
    else if (atp->index() == Compile::AliasIdxRaw)
      tty->print(", idx=Raw;");
    else
      tty->print(", idx=%d;", atp->index());
  }
}

extern void print_alias_types();

#endif

//--------------------------Ideal_common---------------------------------------
Node *MemNode::Ideal_common(PhaseGVN *phase, bool can_reshape) {
  // If our control input is a dead region, kill all below the region
  Node *ctl = in(MemNode::Control);
  if (ctl && remove_dead_region(phase, can_reshape)) 
    return this;

  // Ignore if memory is dead, or self-loop
  Node *mem = in(MemNode::Memory);
  if( phase->type( mem ) == Type::TOP ) return NodeSentinel; // caller will return NULL
  if( mem == this )                     return NodeSentinel; // caller will return NULL

  Node *address = in(MemNode::Address);
  const Type *t_adr = phase->type( address );
  if( t_adr == Type::TOP )              return NodeSentinel; // caller will return NULL

  // Avoid independent memory operations
  MergeMemNode* mmem = mem->is_MergeMem();
  if( mmem ) {
    const TypePtr *tp = t_adr->is_ptr(); 
    uint alias_idx = phase->C->get_alias_index(tp);
#ifdef ASSERT
    {
      // Check that current type is consistent with the alias index used during graph construction
      assert(alias_idx >= Compile::AliasIdxRaw, "must not be a bad alias_idx");
      const TypePtr *adr_t =  adr_type();
      bool consistent =  adr_t == NULL || adr_t->empty() || phase->C->must_alias(adr_t, alias_idx );
      if( !consistent ) {
        tty->print("alias_idx==%d, adr_type()==", alias_idx); if( adr_t == NULL ) { tty->print("NULL"); } else { adr_t->dump(); }
        tty->cr();
        print_alias_types();
        assert(consistent, "adr_type must match alias idx");
      }
    }
#endif
    // TypeInstPtr::NOTNULL+any is an OOP with unknown offset - generally
    // means an array I have not precisely typed yet.  Do not do any
    // alias stuff with it any time soon.
    const TypeInstPtr *tinst = tp->isa_instptr();
    if( tp->base() != Type::AnyPtr &&
        !(tinst &&
          tinst->klass()->is_java_lang_Object() &&
          tinst->offset() == Type::OffsetBot) ) {
      // compress paths and change unreachable cycles to TOP
      // If not, we can update the input infinitely along a MergeMem cycle
      // Equivalent code in PhiNode::Ideal
      Node         *m  = phase->transform(mmem);
      MergeMemNode *mm = m->is_MergeMem();
      // If tranformed to a MergeMem, get the desired slice
      // Otherwise the returned node represents memory for every slice
      Node *new_mem = (mm != NULL)? mm->memory_at(alias_idx) : m;
      // Update input if it is progress over what we have now
      if (new_mem != in(MemNode::Memory)) {
        set_req(MemNode::Memory, new_mem);
        return this;
      }
    }
  }

  // let the subclass continue analyzing...
  return NULL;
}

//----------------------calculate_adr_type-------------------------------------
// Helper function.  Notices when the given type of address hits top or bottom.
// Also, asserts a cross-check of the type against the expected address type.
const TypePtr* MemNode::calculate_adr_type(const Type* t, const TypePtr* cross_check) {
  if (t == Type::TOP)  return NULL; // does not touch memory any more?
  #ifdef PRODUCT
  cross_check = NULL;
  #else
  if (!VerifyAliases || is_error_reported() || Node::in_dump())  cross_check = NULL;
  #endif
  const TypePtr* tp = t->isa_ptr();
  if (tp == NULL) {
    assert(cross_check == NULL || cross_check == TypePtr::BOTTOM, "expected memory type must be wide");
    return TypePtr::BOTTOM;           // touches lots of memory
  } else {
    #ifdef ASSERT
    // %%%% [phh] We don't check the alias index if cross_check is
    //            TypeRawPtr::BOTTOM.  Needs to be investigated.
    if (cross_check != NULL &&
        cross_check != TypePtr::BOTTOM &&
        cross_check != TypeRawPtr::BOTTOM) {
      // Recheck the alias index, to see if it has changed (due to a bug).
      Compile* C = Compile::current();
      assert(C->get_alias_index(cross_check) == C->get_alias_index(tp),
             "must stay in the original alias category");
      // The type of the address must be contained in the adr_type,
      // disregarding "null"-ness.
      // (We make an exception for TypeRawPtr::BOTTOM, which is a bit bucket.)
      const TypePtr* tp_notnull = tp->join(TypePtr::NOTNULL)->is_ptr();
      assert(cross_check->meet(tp_notnull) == cross_check,
             "real address must not escape from expected memory type");
    }
    #endif
    return tp;
  }
}

//---------------------------cast_is_loop_invariant----------------------------
// A helper function for Ideal_DU_postCCP to check if a CastPP in a counted 
// loop is loop invariant. Make a quick traversal of Phi and CastPP nodes
// looking to see if they are a closed group within the loop.
static bool cast_is_loop_invariant(Node* cast, Node* adr) {
  // The idea is that the phi-nest must boil down to only CastPP nodes
  // with the same data input as the original cast. This implies that any path 
  // into the loop already includes such a CastPP, and so the original cast,
  // whatever its input, must be covered by an equivalent cast, with an earlier 
  // control input.
  ResourceMark rm;
  Unique_Node_List closure;
  closure.push(cast);
  closure.push(adr);
  
  Unique_Node_List worklist;
  worklist.push(adr->in(LoopNode::LoopBackControl));

  int op = cast->Opcode();
  Node* input = cast->in(1);

  // Begin recursive walk of phi nodes.
  while( worklist.size() ){
    // Take a node off the worklist
    Node *n = worklist.pop();
    if( !closure.member(n) ){
      // Add it to the closure.
      closure.push(n);
      // Make a sanity check to ensure we don't waste too much time here.
      if( closure.size() > 10) return false;
      // This node is OK if:
      //  - it is a cast identical (based on opcode and input) to the original one
      //  - or it is a phi node (then we add its inputs to the worklist)
      // Otherwise, the node is not OK, and we presume the cast is not invariant
      if( n->Opcode() == op ) {
        if( n->in(1) != input ){
          return false;
        }
      } else if( n->Opcode() == Op_Phi ) {
        for( uint i = 1; i < n->req(); i++ ) {
          worklist.push(n->in(i));
        }
      } else {
        return false;
      }
    }
  }

  // Quit when the worklist is empty, and we've found no offending nodes.
  return true;
}

//------------------------------Ideal_DU_postCCP-------------------------------
// Find any cast-away of null-ness and keep its control.  Null cast-aways are
// going away in this pass and we need to make this memory op depend on the
// gating null check.

// I tried to leave the CastPP's in.  This makes the graph more accurate in
// some sense; we get to keep around the knowledge that an oop is not-null
// after some test.  Alas, the CastPP's interfere with GVN (some values are
// the regular oop, some are the CastPP of the oop, all merge at Phi's which
// cannot collapse, etc).  This cost us 10% on SpecJVM, even when I removed
// some of the more trivial cases in the optimizer.  Removing more useless
// Phi's started allowing Loads to illegally float above null checks.  I gave
// up on this approach.  CNC 10/20/2000
Node *MemNode::Ideal_DU_postCCP( PhaseCCP *ccp ) {
  Node *ctr = in(MemNode::Control);
  Node *mem = in(MemNode::Memory);
  Node *adr = in(MemNode::Address);
  Node *skipped_cast = NULL;
  // Need a null check?  Regular static accesses do not because they are 
  // from constant addresses.  Array ops are gated by the range check (which
  // always includes a NULL check).  Just check field ops.
  if( !ctr ) {
    // Scan upwards for the highest location we can place this memory op.
    while( true ) {
      switch( adr->Opcode() ) {

      case Op_AddP:             // No change to NULL-ness, so peek thru AddP's
        adr = adr->in(AddPNode::Base);
        continue;

      case Op_CastPP:
        // If the CastPP is useless, just peek on through it.
        if( ccp->type(adr) == ccp->type(adr->in(1)) ) {
          // Remember the cast that we've peeked though. If we peek
          // through more than one, then we end up remembering the highest
          // one, that is, if in a loop, the one closest to the top.
          skipped_cast = adr;
          adr = adr->in(1);
          continue;
        }
        // CastPP is going away in this pass!  We need this memory op to be
        // control-dependent on the test that is guarding the CastPP.
        ccp->hash_delete(this);
        set_req(MemNode::Control, adr->in(0));
        ccp->hash_insert(this);
        return this;
        
      case Op_Phi:
        // Attempt to float above a Phi to some dominating point.
        if( adr->in(0)->is_CountedLoop() ) {
          // If we've already peeked through a Cast (which could have set the
          // control), we can't float above a Phi, because the skipped Cast
          // may not be loop invariant.
          if( skipped_cast == NULL || 
              cast_is_loop_invariant(skipped_cast, adr)) {
            adr = adr->in(1);
            continue;
          }
        }

        // Intentional fallthrough!

        // No obvious dominating point.  The mem op is pinned below the Phi
        // by the Phi itself.  If the Phi goes away (no true value is merged)
        // then the mem op can float, but not indefinitely.  It must be pinned
        // behind the controls leading to the Phi.
      case Op_CheckCastPP:
        // These usually stick around to change address type, however a
        // useless one can be elided and we still need to pick up a control edge
        if (adr->in(0) == NULL) {
          // This CheckCastPP node has NO control and is likely useless. But we 
          // need check further up the ancestor chain for a control input to keep 
          // the node in place. 4959717.
          skipped_cast = adr;
          adr = adr->in(1);
          continue;
        }
        ccp->hash_delete(this);
        set_req(MemNode::Control, adr->in(0));
        ccp->hash_insert(this);
        return this;
        
        // List of "safe" opcodes; those that implicitly block the memory
        // op below any null check.
      case Op_CastL2P:          // no null checks on native pointers
      case Op_Parm:             // 'this' pointer is not null
      case Op_LoadP:            // Loading from within a klass
      case Op_LoadKlass:        // Loading from within a klass
      case Op_ConP:             // Loading from a klass
      case Op_CreateEx:         // Sucking up the guts of an exception oop
      case Op_Con:              // Reading from TLS
      case Op_CMoveP:           // CMoveP is pinned
        break;                  // No progress

      case Op_Proj:             // Direct call to an allocation routine
      case Op_SCMemProj:        // Memory state from store conditional ops
#ifdef ASSERT
        {
          ProjNode* p = adr->is_Proj();
          assert(p->_con == TypeFunc::Parms, "must be return value");
          const CallNode* call = adr->in(0)->is_Call();
          if (call->is_CallStaticJava()) {
            const CallStaticJavaNode* call_java = call->is_CallStaticJava();
            assert(call_java && call_java->method() == NULL, "must be runtime call");
            // We further presume that this is one of new_slow_Java,
            // new_Java, new_objArray_Java, new_typeArray_Java, or
            // the like, but do not assert for this.
          } else if (!call->is_CallLeaf()) {
            // Projections from fetch_oop (OSR) are allowed as well.
            ShouldNotReachHere();
          }
        }
#endif
        break;
      default:
        ShouldNotReachHere();
      }
      break;
    }
  }

  return  NULL;               // No progress
}


//=============================================================================
uint LoadNode::size_of() const { return sizeof(*this); }
uint LoadNode::cmp( const Node &n ) const
{ return !Type::cmp( _type, ((LoadNode&)n)._type ); }
const Type *LoadNode::bottom_type() const { return _type; }
uint LoadNode::ideal_reg() const { 
  return Matcher::base2reg[_type->base()];
}

// Following two methods are copied from TypeNode:
void LoadNode::raise_bottom_type(const Type* new_type) {
  if (VerifyAliases) {
    assert(new_type->higher_equal(_type), "new type must refine old type");
  }
  set_type(new_type);
}

#ifndef PRODUCT
void LoadNode::dump_spec() const { 
  MemNode::dump_spec();
  if( !Verbose && !WizardMode ) {
    // standard dump does this in Verbose and WizardMode
    tty->print(" #"); _type->dump();
  }
}
#endif


//----------------------------LoadNode::make-----------------------------------
// Polymorphic factory method:
LoadNode *LoadNode::make( Node *ctl, Node *mem, Node *adr, const TypePtr* adr_type, const Type *rt, BasicType bt ) {
  // sanity check the alias category against the created node type
  assert(!(adr_type->isa_oopptr() &&
           adr_type->offset() == oopDesc::klass_offset_in_bytes()),
         "use LoadKlassNode instead");
  assert(!(adr_type->isa_aryptr() &&
           adr_type->offset() == arrayOopDesc::length_offset_in_bytes()),
         "use LoadRangeNode instead");
  switch (bt) {
  case T_BOOLEAN:
  case T_BYTE:    return new (3) LoadBNode(ctl, mem, adr, adr_type, rt->is_int()    );
  case T_INT:     return new (3) LoadINode(ctl, mem, adr, adr_type, rt->is_int()    );
  case T_CHAR:    return new (3) LoadCNode(ctl, mem, adr, adr_type, rt->is_int()    );
  case T_SHORT:   return new (3) LoadSNode(ctl, mem, adr, adr_type, rt->is_int()    );
  case T_LONG:    return new (3) LoadLNode(ctl, mem, adr, adr_type, rt->is_long()   );
  case T_FLOAT:   return new (3) LoadFNode(ctl, mem, adr, adr_type, rt              );
  case T_DOUBLE:  return new (3) LoadDNode(ctl, mem, adr, adr_type, rt              );
  case T_ADDRESS: return new (3) LoadPNode(ctl, mem, adr, adr_type, rt->is_ptr()    );
  case T_OBJECT:  return new (3) LoadPNode(ctl, mem, adr, adr_type, rt->is_oopptr());
  }
  ShouldNotReachHere();
  return (LoadNode*)NULL;
}



//------------------------------hash-------------------------------------------
uint LoadNode::hash() const {
  // unroll addition of interesting fields
  return (uintptr_t)in(Control) + (uintptr_t)in(Memory) + (uintptr_t)in(Address);
}

//------------------------------load_can_see_stored_value----------------------
// This routine exists to make sure this set of tests is done the same
// everywhere.  We need to make a coordinated change: first LoadNode::Ideal
// will change the graph shape in a way which makes memory alive twice at the
// same time (uses the Oracle model of aliasing), then some
// LoadXNode::Identity will fold things back to the equivalence-class model
// of aliasing.
static Node* load_can_see_stored_value(LoadNode* ld, Node* mem, PhaseTransform* phase) {
  const StoreNode* st = mem->is_Store();
  if (st == NULL)  return NULL;
  if (!phase->eqv(st->in(MemNode::Address), ld->in(MemNode::Address)))
    return NULL;
  if (ld->store_Opcode() != st->Opcode())
    return NULL;
  return st->in(MemNode::ValueIn);
}

//------------------------------Identity---------------------------------------
// Loads are identity if previous store is to same address
Node *LoadNode::Identity( PhaseTransform *phase ) {
  // If the previous store-maker is the right kind of Store, and the store is
  // to the same address, then we are equal to the value stored.
  Node *mem = in(MemNode::Memory);
  Node *value = load_can_see_stored_value( this, mem, phase );
  if( value ) {
    // byte, short & char stores truncate naturally.
    // A load has to load the truncated value which requires
    // some sort of masking operation and that requires an
    // Ideal call instead of an Identity call.
    int op = Opcode();
    if( op == Op_LoadS || op == Op_LoadC || op == Op_LoadB ) {
      // If the input to the store does not fit with the load's result type,
      // it must be truncated via an Ideal call.
      if (!phase->type(mem->in(MemNode::ValueIn))->higher_equal(phase->type(this)))
        return this;
    }
    return mem->in(MemNode::ValueIn);
  }
  return this;
}

//------------------------------Ideal------------------------------------------
// If the load is from Field memory and the pointer is non-null, we can
// zero out the control input.
Node *LoadNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  Node* p = MemNode::Ideal_common(phase, can_reshape);
  if (p)  return (p == NodeSentinel) ? NULL : p;

  Node *ctrl    = in(MemNode::Control);
  Node *mem     = in(MemNode::Memory);
  Node *address = in(MemNode::Address);

  // Skip up past a SafePoint control.  Cannot do this for Stores because
  // pointer stores & cardmarks must stay on the same side of a SafePoint.
  if( ctrl && ctrl->Opcode() == Op_SafePoint && 
      phase->C->get_alias_index(phase->type(address)->is_ptr()) != Compile::AliasIdxRaw ) {
    ctrl = ctrl->in(0);
    set_req(MemNode::Control,ctrl);
  }

  // Check for useless memory edge in some common special cases
  if( in(MemNode::Control) ) {
    Node *adr = address->is_AddP() ? address->in(AddPNode::Base) : address;
    ProjNode *p = adr->is_Proj();
    if( p && p->_con == TypeFunc::Parms && p->in(0)->is_Start() && 
        phase->type(p)->is_ptr()->_ptr == TypePtr::NotNull ) {
      set_req(MemNode::Control, NULL);
    }
  }

  // Check for prior array store with a different offset; make Load
  // independent.  Skip through any number of them.  Bail out if the stores
  // are in an endless dead cycle and report no progress.  This is a key
  // transform for Reflection.  However, if after skipping through the Stores
  // we can't then fold up against a prior store do NOT do the transform as
  // this amounts to using the 'Oracle' model of aliasing.  It leaves the same
  // array memory alive twice: once for the hoisted Load and again after the
  // bypassed Store.  This situation only works if EVERYBODY who does
  // anti-dependence work knows how to bypass.  I.e. we need all
  // anti-dependence checks to ask the same Oracle.  Right now, that Oracle is
  // the alias index stuff.  So instead, peek through Stores and IFF we can
  // fold up, do so.
  if( address->is_AddP() &&
      phase->type( address->in(AddPNode::Address) ) != Type::TOP &&
      address->in(AddPNode::Offset)->is_Con() ) {
    Node *base = address->in(AddPNode::Address);
    const Type *toffset = phase->type(address->in(AddPNode::Offset));
    int cnt = 50;               // Cycle limiter

    while( mem->is_Store() ) {
      Node *st_adr = mem->in(MemNode::Address);
      if( !st_adr->is_AddP() ) break;
      if( base != st_adr->in(AddPNode::Address) ) break;
      if( !st_adr->in(AddPNode::Offset)->is_Con() ) break;
      if( toffset == phase->type( st_adr->in(AddPNode::Offset) ) ) {
        // See if we can fold up on the spot, but don't fold up here.
        // Fold-up might require truncation (for LoadB/LoadS/LoadC) or
        // just return a prior value, which is done by Identity calls.
        if( mem != in(MemNode::Memory) && store_Opcode() == mem->Opcode() ) {
          set_req( MemNode::Memory, mem );
          return this;
        }
        break;                  // Else blocked by an overlapping store
      }
      mem = mem->in(Memory);    // Advance through independent store memory
      --cnt;
      if( cnt < 0 ) return NULL; // Caught in cycle!  Report no progress
    }
  }

  return NULL;                  // No further progress
}

//------------------------------Value-----------------------------------------
const Type *LoadNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(MemNode::Memory) );
  if( t1 == Type::TOP ) return Type::TOP;
  Node *adr = in(MemNode::Address);
  const Type *t2 = phase->type( adr );
  if( t2 == Type::TOP ) return Type::TOP;
  const TypePtr *tp = t2->is_ptr();
  if (TypePtr::above_centerline(tp->ptr())) return Type::TOP;
  int off = tp->offset();
  // If the offset is 'top', then the load is undefined
  if (off == Type::OffsetTop) return Type::TOP;

  // Try to guess loaded type from pointer type
  if (tp->base() == Type::AryPtr) {
    const Type *t = tp->is_aryptr()->elem();
    // Don't do this for integer types. There is only potential profit if
    // the element type t is lower than _type; that is, for int types, if _type is 
    // more restrictive than t.  This only happens here if one is short and the other
    // char (both 16 bits), and in those cases we've made an intentional decision
    // to use one kind of load over the other. See AndINode::Ideal and 4965907.
    if ((t->isa_int() == NULL) && (t->isa_long() == NULL)) {
      // t might actually be lower than _type, if _type is a unique
      // concrete subclass of abstract class t.
      // Make sure the reference is not into the header, by comparing
      // the offset against the offset of the start of the array's data.
      // Different array types begin at slightly different offsets (12 vs. 16).
      // We choose T_BYTE as an example base type that is least restrictive
      // as to alignment, which will therefore produce the smallest
      // possible base offset.
      const int min_base_off = arrayOopDesc::base_offset_in_bytes(T_BYTE);
      if ((uint)off >= (uint)min_base_off) {  // is the offset beyond the header?
        const Type* jt = t->join(_type);
        // In any case, do not allow the join, per se, to empty out the type.
        if (jt->empty() && !t->empty()) {
          // This can happen if a interface-typed array narrows to a class type.
          jt = _type;
        }
        return jt;
      }
    }
  } else if (tp->base() == Type::InstPtr) {
    assert( off != Type::OffsetBot ||
            // arrays can be cast to Objects
            tp->is_oopptr()->klass()->is_java_lang_Object() ||
            // unsafe field access may not have a constant offset
            phase->C->has_unsafe_access(),
            "Field accesses must be precise" );
    // For oop loads, we expect the _type to be precise
  } else if (tp->base() == Type::KlassPtr) {
    assert( off != Type::OffsetBot ||
            // arrays can be cast to Objects
            tp->is_klassptr()->klass()->is_java_lang_Object() ||
            // also allow array-loading from the primary supertype
            // array during subtype checks
            Opcode() == Op_LoadKlass, 
            "Field accesses must be precise" );
    // For klass/static loads, we expect the _type to be precise
  }

  const TypeKlassPtr *tkls = tp->isa_klassptr();
  if (tkls != NULL) {
    ciKlass* klass = tkls->klass();
    if (klass->is_loaded() && tkls->klass_is_exact()) {
      // We are loading a field from a Klass metaobject whose identity
      // is known at compile time (the type is "exact" or "precise").
      // Check for fields we know are maintained as constants by the VM.
      if (tkls->offset() == Klass::super_check_offset_offset_in_bytes() + (int)sizeof(oopDesc)) {
        // The field is Klass::_super_check_offset.  Return its (constant) value.
        // (Folds up type checking code.)
        assert(Opcode() == Op_LoadI, "must load an int from _super_check_offset");
        return TypeInt::make(klass->super_check_offset());
      }
      // Compute index into primary_supers array
      juint depth = (tkls->offset() - (Klass::primary_supers_offset_in_bytes() + (int)sizeof(oopDesc))) / sizeof(klassOop);
      // Check for overflowing; use unsigned compare to handle the negative case.
      if( depth < ciKlass::primary_super_limit() ) {
        // The field is an element of Klass::_primary_supers.  Return its (constant) value.
        // (Folds up type checking code.)
        assert(Opcode() == Op_LoadKlass, "must load a klass from _primary_supers");
        ciKlass *ss = klass->super_of_depth(depth);
        return ss ? TypeKlassPtr::make(ss) : TypePtr::NULL_PTR;
      }
      if (tkls->offset() == Klass::modifier_flags_offset_in_bytes() + (int)sizeof(oopDesc)) {
        // The field is Klass::_modifier_flags.  Return its (constant) value.
        // (Folds up the 2nd indirection in aClassConstant.getModifiers().)
        assert(Opcode() == Op_LoadI, "must load an int from _modifier_flags");
        return TypeInt::make(klass->modifier_flags());
      }
      if (tkls->offset() == Klass::access_flags_offset_in_bytes() + (int)sizeof(oopDesc)) {
        // The field is Klass::_access_flags.  Return its (constant) value.
        // (Folds up the 2nd indirection in Reflection.getClassAccessFlags(aClassConstant).)
        assert(Opcode() == Op_LoadI, "must load an int from _access_flags");
        return TypeInt::make(klass->access_flags());
      }
      if (tkls->offset() == Klass::java_mirror_offset_in_bytes() + (int)sizeof(oopDesc)) {
        // The field is Klass::_java_mirror.  Return its (constant) value.
        // (Folds up the 2nd indirection in anObjConstant.getClass().)
        assert(Opcode() == Op_LoadP, "must load an oop from _java_mirror");
        return TypeInstPtr::make(klass->java_mirror());
      }
    }

    // We can still check if we are loading from the primary_supers array at a
    // shallow enough depth.  Even though the klass is not exact, entries less
    // than or equal to its super depth are correct.
    if (klass->is_loaded() ) {
      ciType *inner = klass->klass();
      while( inner->is_obj_array_klass() )
        inner = inner->as_obj_array_klass()->base_element_type();
      if( inner->is_instance_klass() &&
          !inner->as_instance_klass()->flags().is_interface() ) {
        // Compute index into primary_supers array
        juint depth = (tkls->offset() - (Klass::primary_supers_offset_in_bytes() + (int)sizeof(oopDesc))) / sizeof(klassOop);
        // Check for overflowing; use unsigned compare to handle the negative case.
        if( depth < ciKlass::primary_super_limit() &&
            depth <= klass->super_depth() ) { // allow self-depth checks to handle self-check case
          // The field is an element of Klass::_primary_supers.  Return its (constant) value.
          // (Folds up type checking code.)
          assert(Opcode() == Op_LoadKlass, "must load a klass from _primary_supers");
          ciKlass *ss = klass->super_of_depth(depth);
          return ss ? TypeKlassPtr::make(ss) : TypePtr::NULL_PTR;
        }
      }
    }
  }
  return _type;
}

//------------------------------match_edge-------------------------------------
// Do we Match on this edge index or not?  Match only the address.
uint LoadNode::match_edge(uint idx) const {
  return idx == MemNode::Address;
}

//--------------------------LoadBNode::Ideal--------------------------------------
//
//  If the previous store is to the same address as this load,
//  and the value stored was larger than a byte, replace this load
//  with the value stored truncated to a byte.  If no truncation is
//  needed, the replacement is done in LoadNode::Identity().
//
Node *LoadBNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  Node *mem = in(MemNode::Memory);
  Node *value = load_can_see_stored_value(this,mem,phase);
  if( value && !phase->type(value)->higher_equal( _type ) ) {
    Node *result = phase->transform( new (3) LShiftINode(value, phase->intcon(24)) );
    return new (3) RShiftINode(result, phase->intcon(24));
  }
  // Identity call will handle the case where truncation is not needed.
  return LoadNode::Ideal(phase, can_reshape);
}

//--------------------------LoadCNode::Ideal--------------------------------------
//
//  If the previous store is to the same address as this load,
//  and the value stored was larger than a char, replace this load
//  with the value stored truncated to a char.  If no truncation is
//  needed, the replacement is done in LoadNode::Identity().
//
Node *LoadCNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  Node *mem = in(MemNode::Memory);
  Node *value = load_can_see_stored_value(this,mem,phase);
  if( value && !phase->type(value)->higher_equal( _type ) ) 
    return new (3) AndINode(value,phase->intcon(0xFFFF));
  // Identity call will handle the case where truncation is not needed.
  return LoadNode::Ideal(phase, can_reshape);
}

//--------------------------LoadSNode::Ideal--------------------------------------
//
//  If the previous store is to the same address as this load,
//  and the value stored was larger than a short, replace this load
//  with the value stored truncated to a short.  If no truncation is
//  needed, the replacement is done in LoadNode::Identity().
//
Node *LoadSNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  Node *mem = in(MemNode::Memory);
  Node *value = load_can_see_stored_value(this,mem,phase);
  if( value && !phase->type(value)->higher_equal( _type ) ) {
    Node *result = phase->transform( new (3) LShiftINode(value, phase->intcon(16)) );
    return new (3) RShiftINode(result, phase->intcon(16));
  }
  // Identity call will handle the case where truncation is not needed.
  return LoadNode::Ideal(phase, can_reshape);
}

//=============================================================================
//------------------------------Value------------------------------------------
const Type *LoadKlassNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(MemNode::Memory) );
  if (t1 == Type::TOP)  return Type::TOP;
  Node *adr = in(MemNode::Address);
  const Type *t2 = phase->type( adr );
  if (t2 == Type::TOP)  return Type::TOP;
  const TypePtr *tp = t2->is_ptr();
  if (TypePtr::above_centerline(tp->ptr()) ||
      tp->ptr() == TypePtr::Null)  return Type::TOP;

  // Return a more precise klass, if possible
  const TypeInstPtr *tinst = tp->isa_instptr();
  if (tinst != NULL) {
    ciInstanceKlass* ik = tinst->klass()->as_instance_klass();
    if (ik == phase->C->env()->Class_klass()
        && tinst->offset() == java_lang_Class::klass_offset_in_bytes()) {
      // We are loading a special hidden field from a Class mirror object,
      // the field which points to the VM's Klass metaobject.
      ciType* t = tinst->mirror_type();
      // mirror_type returns non-null for compile-time Class constants.
      if (t != NULL) {
        // constant oop => constant klass
        if (!t->is_klass()) {
          // a primitive Class (e.g., int.class) has NULL for a klass field
          return TypePtr::NULL_PTR;
        }
        // (Folds up the 1st indirection in aClassConstant.getModifiers().)
        return TypeKlassPtr::make(t->as_klass());
      }
      // non-constant mirror, so we can't tell what's going on
    }
    if( !ik->is_loaded() )
      return _type;             // Bail out if not loaded
    if (tinst->offset() == oopDesc::klass_offset_in_bytes()) {
      if (tinst->klass_is_exact()) {
        return TypeKlassPtr::make(ik);
      }
      // See if we can become precise: no subklasses and no interface
      if( !ik->is_interface() && !ik->has_subklass() ) {
        //assert(!UseExactTypes, "this code should be useless with exact types");
        // Add a dependence; if any subclass added we need to recompile
        if (!ik->flags().is_final()) {
          CompileLog* log = phase->C->log();
          if (log != NULL) {
            log->elem("cast_up reason='!has_subklass' from='%d' to='(exact)'",
                      log->identify(ik));
          }
          phase->C->recorder()->add_dependent(ik, NULL);
        }
        // Return precise klass
        return TypeKlassPtr::make(ik);
      }

      // Return root of possible klass
      return TypeKlassPtr::make(TypePtr::NotNull, ik, 0/*offset*/);
    }
  }

  // Check for loading klass from an array
  const TypeAryPtr *tary = tp->isa_aryptr();
  if( tary != NULL ) {
    ciKlass *tary_klass = tary->klass();
    if (tary_klass != NULL   // can be NULL when at BOTTOM or TOP
        && tary->offset() == oopDesc::klass_offset_in_bytes()) {
      if (tary->klass_is_exact()) {
        return TypeKlassPtr::make(tary_klass);
      }
      ciArrayKlass *ak = tary->klass()->as_array_klass();
      // If the klass is an object array, we defer the question to the
      // array component klass.
      if( ak->is_obj_array_klass() ) {
        assert( ak->is_loaded(), "" );
        ciKlass *base_k = ak->as_obj_array_klass()->base_element_klass();
        if( base_k->is_loaded() && base_k->is_instance_klass() ) {
          ciInstanceKlass* ik = base_k->as_instance_klass();
          // See if we can become precise: no subklasses and no interface
          if( !ik->is_interface() && !ik->has_subklass() ) {
            //assert(!UseExactTypes, "this code should be useless with exact types");
            // Add a dependence; if any subclass added we need to recompile
            if (!ik->flags().is_final()) {
              CompileLog* log = phase->C->log();
              if (log != NULL) {
                log->elem("cast_up reason='!has_subklass' from='%d' to='(exact)'",
                          log->identify(ik));
              }
              phase->C->recorder()->add_dependent(ik, NULL);
            }
            // Return precise array klass
            return TypeKlassPtr::make(ak);
          }
        }
        return TypeKlassPtr::make(TypePtr::NotNull, ak, 0/*offset*/);
      } else {                  // Found a type-array?
        //assert(!UseExactTypes, "this code should be useless with exact types");
        assert( ak->is_type_array_klass(), "" );
        return TypeKlassPtr::make(ak); // These are always precise
      }
    }
  }

  // Check for loading klass from an array klass
  const TypeKlassPtr *tkls = tp->isa_klassptr();
  if( tkls != NULL ) {
    ciKlass* klass = tkls->klass();
    if( !klass->is_loaded() )
      return _type;             // Bail out if not loaded
    if( klass->is_obj_array_klass() &&
        (uint)tkls->offset() == objArrayKlass::element_klass_offset_in_bytes() + sizeof(oopDesc)) {
      ciKlass* elem = klass->as_obj_array_klass()->element_klass();
      // // Always returning precise element type is incorrect, 
      // // e.g., element type could be object and array may contain strings
      // return TypeKlassPtr::make(TypePtr::Constant, elem, 0);

      // The array's TypeKlassPtr was declared 'precise' or 'not precise'
      // according to the element type's subclassing.
      return TypeKlassPtr::make(tkls->ptr(), elem, 0/*offset*/);
    }
  }

  // Bailout case
  return LoadNode::Value(phase);
}

//------------------------------Value-----------------------------------------
const Type *LoadRangeNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(MemNode::Memory) );
  if( t1 == Type::TOP ) return Type::TOP;
  Node *adr = in(MemNode::Address);
  const Type *t2 = phase->type( adr );
  if( t2 == Type::TOP ) return Type::TOP;
  const TypePtr *tp = t2->is_ptr();
  if (TypePtr::above_centerline(tp->ptr()))  return Type::TOP;
  const TypeAryPtr *tap = tp->isa_aryptr();
  if( !tap ) return _type;
  return tap->size();
}

//=============================================================================
//---------------------------StoreNode::make-----------------------------------
// Polymorphic factory method:
StoreNode* StoreNode::make(Node* ctl, Node* mem, Node* adr, const TypePtr* adr_type, Node* val, BasicType bt ) {
  switch (bt) {
  case T_BOOLEAN:
  case T_BYTE:    return new (4) StoreBNode(ctl, mem, adr, adr_type, val);
  case T_INT:     return new (4) StoreINode(ctl, mem, adr, adr_type, val);
  case T_CHAR:
  case T_SHORT:   return new (4) StoreCNode(ctl, mem, adr, adr_type, val);
  case T_LONG:    return new (4) StoreLNode(ctl, mem, adr, adr_type, val);
  case T_FLOAT:   return new (4) StoreFNode(ctl, mem, adr, adr_type, val);
  case T_DOUBLE:  return new (4) StoreDNode(ctl, mem, adr, adr_type, val);
  case T_ADDRESS:
  case T_OBJECT:  return new (4) StorePNode(ctl, mem, adr, adr_type, val);
  }
  ShouldNotReachHere();
  return (StoreNode*)NULL;
}

//--------------------------bottom_type----------------------------------------
const Type *StoreNode::bottom_type() const {
  return Type::MEMORY;
}

//------------------------------hash-------------------------------------------
uint StoreNode::hash() const {
  // unroll addition of interesting fields
  //return (uintptr_t)in(Control) + (uintptr_t)in(Memory) + (uintptr_t)in(Address) + (uintptr_t)in(ValueIn);

  // Since they are not commoned, do not hash them:
  return NO_HASH;
}

//------------------------------Ideal------------------------------------------
// If the store is from to memory and the pointer is non-null, we can
// zero out the control input.
Node *StoreNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  Node* p = MemNode::Ideal_common(phase, can_reshape);
  if (p)  return (p == NodeSentinel) ? NULL : p;

  Node *mem     = in(MemNode::Memory);
  Node *address = in(MemNode::Address);

  // Back-to-back stores to same address?  Fold em up.
  // Generally unsafe if I have intervening uses...
  const StoreNode *st = mem->is_Store();
  if (can_reshape && st && phase->eqv( st->in(MemNode::Address), address )) {
    // Looking at a dead closed cycle of memory?
    if( mem == st->in(MemNode::Memory) ) 
      return NULL;            // Do not bother!
    
    // Mis-matched stores on the same memory edge happen during initialization,
    // where everything is cleared with 32-bit wide stores.
    if( Opcode() == st->Opcode() ) {
      // Check for intervening uses
      bool has_load = false;
      for (DUIterator_Fast imax, i = st->fast_outs(imax); i < imax; i++)
        if (st->fast_out(i)->is_Load())
          { has_load = true; break; }
      if (!has_load) {          // No load uses

        // OUCH! Cannot have multiple copies of store alive at the same time.
        // Happens in _201_compress::decompress at least.  Require exactly
        // ONE user, the 'this' store, until such time as I clone 'st' for
        // each of 'st's uses (thus making the exactly-1-user-rule hold true).
        if (st->outcnt() == 1) {
          set_req_X( MemNode::Memory, st->in(MemNode::Memory), phase->is_IterGVN() );
          return this;
        }
      }
    }
  }

  return NULL;                  // No further progress
}

//------------------------------Value-----------------------------------------
const Type *StoreNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(MemNode::Memory) );
  if( t1 == Type::TOP ) return Type::TOP;
  const Type *t2 = phase->type( in(MemNode::Address) );
  if( t2 == Type::TOP ) return Type::TOP;
  const Type *t3 = phase->type( in(MemNode::ValueIn) );
  if( t3 == Type::TOP ) return Type::TOP;
  return Type::MEMORY;
}

//------------------------------Identity---------------------------------------
Node *StoreNode::Identity( PhaseTransform *phase ) {
  const LoadNode *ld = in(MemNode::ValueIn)->is_Load();
  // Load then Store?  Then the Store is useless
  if( ld && 
      phase->eqv( ld->in(MemNode::Address), in(MemNode::Address) ) &&
      phase->eqv( ld->in(MemNode::Memory ), in(MemNode::Memory ) ) )
    return in(MemNode::Memory);
  return this;
}

//------------------------------match_edge-------------------------------------
// Do we Match on this edge index or not?  Match only memory & value
uint StoreNode::match_edge(uint idx) const {
  return idx == MemNode::Address || idx == MemNode::ValueIn;
}

//------------------------------cmp--------------------------------------------
// Do not common stores up together.  They generally have to be split
// back up anyways, so do not bother.
uint StoreNode::cmp( const Node &n ) const { 
  return (&n == this);          // Always fail except on self
}

//------------------------------Ideal_masked_input-----------------------------
// Check for a useless mask before a partial-word store
// (StoreB ... (AndI valIn conIa) )
// If (conIa & mask == mask) this simplifies to   
// (StoreB ... (valIn) )
Node *StoreNode::Ideal_masked_input(PhaseGVN *phase, uint mask) {
  Node *val = in(MemNode::ValueIn);
  if( val->Opcode() == Op_AndI ) {
    const TypeInt *t = phase->type( val->in(2) )->isa_int();
    if( t && t->is_con() && (t->get_con() & mask) == mask ) {
      set_req(MemNode::ValueIn, val->in(1));
      return this;
    }
  }
  return NULL;
}


//------------------------------Ideal_sign_extended_input----------------------
// Check for useless sign-extension before a partial-word store
// (StoreB ... (RShiftI _ (LShiftI _ valIn conIL ) conIR) )
// If (conIL == conIR && conIR <= num_bits)  this simplifies to
// (StoreB ... (valIn) )
Node *StoreNode::Ideal_sign_extended_input(PhaseGVN *phase, int num_bits) {
  Node *val = in(MemNode::ValueIn);
  if( val->Opcode() == Op_RShiftI ) {
    const TypeInt *t = phase->type( val->in(2) )->isa_int();
    if( t && t->is_con() && (t->get_con() <= num_bits) ) {
      Node *shl = val->in(1);
      if( shl->Opcode() == Op_LShiftI ) {
        const TypeInt *t2 = phase->type( shl->in(2) )->isa_int();
        if( t2 && t2->is_con() && (t2->get_con() == t->get_con()) ) {
          set_req(MemNode::ValueIn, shl->in(1));
          return this;
        }
      }
    }
  }
  return NULL;
}

//=============================================================================
//------------------------------Ideal------------------------------------------
// If the store is from an AND mask that leaves the low bits untouched, then
// we can skip the AND operation.  If the store is from a sign-extension
// (a left shift, then right shift) we can skip both.
Node *StoreBNode::Ideal(PhaseGVN *phase, bool can_reshape){
  Node *progress = StoreNode::Ideal_masked_input(phase, 0xFF);
  if( progress != NULL ) return progress;

  progress = StoreNode::Ideal_sign_extended_input(phase, 24);
  if( progress != NULL ) return progress;

  // Finally check the default case
  return StoreNode::Ideal(phase, can_reshape);
}

//=============================================================================
//------------------------------Ideal------------------------------------------
// If the store is from an AND mask that leaves the low bits untouched, then
// we can skip the AND operation
Node *StoreCNode::Ideal(PhaseGVN *phase, bool can_reshape){
  Node *progress = StoreNode::Ideal_masked_input(phase, 0xFFFF);
  if( progress != NULL ) return progress;

  progress = StoreNode::Ideal_sign_extended_input(phase, 16);
  if( progress != NULL ) return progress;

  // Finally check the default case
  return StoreNode::Ideal(phase, can_reshape);
}

//=============================================================================
//------------------------------Identity---------------------------------------
Node *StoreCMNode::Identity( PhaseTransform *phase ) {
  // No identities defined at this time
  return this;
}

//------------------------------Value-----------------------------------------
const Type *StoreCMNode::Value( PhaseTransform *phase ) const {
  // If extra input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(MemNode::OopStore) );
  if( t1 == Type::TOP ) return Type::TOP;

  return StoreNode::Value( phase );
}


//=============================================================================
//----------------------------------SCMemProjNode------------------------------
const Type * SCMemProjNode::Value( PhaseTransform *phase ) const
{
  return bottom_type();
}

//=============================================================================
//-------------------------------StorePConditionalNode--------------------------
StorePConditionalNode::StorePConditionalNode( Node *c, Node *mem, Node *adr, Node *val, Node *ll ) : Node(5) {
  set_req(MemNode::Control, c  );
  set_req(MemNode::Memory , mem);
  set_req(MemNode::Address, adr);
  set_req(MemNode::ValueIn, val);
  set_req(         LL_input,ll );
}

//=============================================================================
StoreLConditionalNode::StoreLConditionalNode( Node *c, Node *mem, Node *adr, Node *val, Node *ll ) : Node(5) {
  set_req(MemNode::Control, c  );
  set_req(MemNode::Memory , mem);
  set_req(MemNode::Address, adr);
  set_req(MemNode::ValueIn, val);
  set_req(         LL_input,ll );
}


CompareAndSwapNode::CompareAndSwapNode( Node *c, Node *mem, Node *adr, Node *val, Node *ex ) : Node(5) {
  set_req(MemNode::Control, c  );
  set_req(MemNode::Memory , mem);
  set_req(MemNode::Address, adr);
  set_req(MemNode::ValueIn, val);
  set_req(         ExpectedIn, ex );
}

CompareAndSwapLNode::CompareAndSwapLNode(Node *c, Node *mem, Node *adr, Node *val, Node *ex) : CompareAndSwapNode(c, mem, adr, val, ex) { }

CompareAndSwapINode::CompareAndSwapINode(Node *c, Node *mem, Node *adr, Node *val, Node *ex) : CompareAndSwapNode(c, mem, adr, val, ex) { }

CompareAndSwapPNode::CompareAndSwapPNode(Node *c, Node *mem, Node *adr, Node *val, Node *ex) : CompareAndSwapNode(c, mem, adr, val, ex) { }


//=============================================================================
//-------------------------------adr_type--------------------------------------
// Do we Match on this edge index or not?  Do not match memory
const TypePtr* ClearArrayNode::adr_type() const {
  Node *adr = in(3);
  return MemNode::calculate_adr_type(adr->bottom_type());
}

//------------------------------match_edge-------------------------------------
// Do we Match on this edge index or not?  Do not match memory
uint ClearArrayNode::match_edge(uint idx) const {
  return idx > 1;
}

//------------------------------Identity---------------------------------------
// Clearing a zero length array does nothing
Node *ClearArrayNode::Identity( PhaseTransform *phase ) {
  return phase->type(in(2))->higher_equal(TypeInt::ZERO)  ? in(1) : this;
}

//------------------------------Idealize---------------------------------------
// Clearing a short array is faster with stores
Node *ClearArrayNode::Ideal(PhaseGVN *phase, bool can_reshape){
  const TypeInt *t = phase->type(in(2))->isa_int();
  if( !t ) return NULL;
  if( !t->is_con() ) return NULL;
  int con = t->get_con();       // Length is in doublewords
  // Length too long; use fast hardware clear
  if( con > 8 ) return NULL;
  // Clearing nothing uses the Identity call.
  // Negative clears are possible on dead ClearArrays
  // (see jck test stmt114.stmt11402.val).
  if( con <= 0 ) return NULL;
  Node *mem = in(1);
  if( phase->type(mem)==Type::TOP ) return NULL;
  Node *adr = in(3);
  const Type* at = phase->type(adr);
  if( at==Type::TOP ) return NULL;
  const TypePtr* atp = at->isa_ptr();
  // adjust atp to be the correct array element address type
  if (atp == NULL)  atp = TypePtr::BOTTOM;
  else              atp = atp->add_offset(Type::OffsetBot);
  // Get base for derived pointer purposes
  if( adr->Opcode() != Op_AddP ) Unimplemented();
  Node *base = adr->in(1);

  // It's marginally nicer to write 32-bit chunks instead of 64-bit chunks,
  // because it's easier to fold up redundant initialization writes.
  Node *zero = phase->makecon(TypeInt::ZERO);
  con <<= 1;                    // Write words instead of doublewords
  Node *off  = phase->MakeConX(BytesPerInt);
  mem = new (4) StoreINode(in(0),mem,adr,atp,zero);
  con--;
  while( con-- ) {
    mem = phase->transform(mem);
    adr = phase->transform(new (4) AddPNode(base,adr,off));
    mem = new (4) StoreINode(in(0),mem,adr,atp,zero);
  }
  return mem;
}

//=============================================================================
// Do we match on this edge? No memory edges
uint StrCompNode::match_edge(uint idx) const {
  return idx == 5 || idx == 6;
}

//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out 
// control copies
Node *StrCompNode::Ideal(PhaseGVN *phase, bool can_reshape){
  return remove_dead_region(phase, can_reshape) ? this : NULL;
}


//=============================================================================
MemBarNode::MemBarNode() : MultiNode(TypeFunc::Parms) {
  Node *top = Compile::current()->top();
  set_req(TypeFunc::I_O,top);
  set_req(TypeFunc::FramePtr,top);
  set_req(TypeFunc::ReturnAdr,top);
}

//------------------------------cmp--------------------------------------------
uint MemBarNode::hash() const { return NO_HASH; }
uint MemBarNode::cmp( const Node &n ) const { 
  return (&n == this);          // Always fail except on self
}

//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out 
// control copies
Node *MemBarNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  if (remove_dead_region(phase, can_reshape))  return this;
  return NULL;
}

//------------------------------Value------------------------------------------
const Type *MemBarNode::Value( PhaseTransform *phase ) const {
  if( !in(0) ) return Type::TOP;
  if( phase->type(in(0)) == Type::TOP )
    return Type::TOP;
  return TypeTuple::MEMBAR;
}

//------------------------------match------------------------------------------
// Construct projections for memory.
Node *MemBarNode::match( const ProjNode *proj, const Matcher *m ) {
  switch (proj->_con) {
  case TypeFunc::Control:
  case TypeFunc::Memory:
    return new (1) MachProjNode(this,proj->_con,RegMask::Empty,MachProjNode::unmatched_proj);
  }
  ShouldNotReachHere();
  return NULL;
}

//=============================================================================
// 
// SEMANTICS OF MEMORY MERGES:  A MergeMem is a memory state assembled from several
// contributing store or call operations.  Each contributor provides the memory
// state for a particular "alias type" (see Compile::alias_type).  For example,
// if a MergeMem has an input X for alias category #6, then any memory reference
// to alias category #6 may use X as its memory state input, as an exact equivalent
// to using the MergeMem as a whole.
//   Load<6>( MergeMem(<6>: X, ...), p ) <==> Load<6>(X,p)
// 
// (Here, the <N> notation gives the index of the relevant adr_type.)
// 
// In one special case (and more cases in the future), alias categories overlap.
// The special alias category "Bot" (Compile::AliasIdxBot) includes all memory
// states.  Therefore, if a MergeMem has only one contributing input W for Bot,
// it is exactly equivalent to that state W:
//   MergeMem(<Bot>: W) <==> W
// 
// Usually, the merge has more than one input.  In that case, where inputs
// overlap (i.e., one is Bot), the narrower alias type determines the memory
// state for that type, and the wider alias type (Bot) fills in everywhere else:
//   Load<5>( MergeMem(<Bot>: W, <6>: X), p ) <==> Load<5>(W,p)
//   Load<6>( MergeMem(<Bot>: W, <6>: X), p ) <==> Load<6>(X,p)
// 
// A merge can take a "wide" memory state as one of its narrow inputs.
// This simply means that the merge observes out only the relevant parts of
// the wide input.  That is, wide memory states arriving at narrow merge inputs
// are implicitly "filtered" or "sliced" as necessary.  (This is rare.)
// 
// These rules imply that MergeMem nodes may cascade (via their <Bot> links),
// and that memory slices "leak through":
//   MergeMem(<Bot>: MergeMem(<Bot>: W, <7>: Y)) <==> MergeMem(<Bot>: W, <7>: Y)
// 
// But, in such a cascade, repeated memory slices can "block the leak":
//   MergeMem(<Bot>: MergeMem(<Bot>: W, <7>: Y), <7>: Y') <==> MergeMem(<Bot>: W, <7>: Y')
// 
// In the last example, Y is not part of the combined memory state of the
// outermost MergeMem.  The system must, of course, prevent unschedulable
// memory states from arising, so you can be sure that the state Y is somehow
// a precursor to state Y'.
// 
// 
// REPRESENTATION OF MEMORY MERGES: The indexes used to address the Node::in array
// of each MergeMemNode array are exactly the numerical alias indexes, including
// but not limited to AliasIdxTop, AliasIdxBot, and AliasIdxRaw.  The functions
// Compile::alias_type (and kin) produce and manage these indexes.
// 
// By convention, the value of in(AliasIdxTop) (i.e., in(1)) is always the top node.
// (Note that this provides quick access to the top node inside MergeMem methods,
// without the need to reach out via TLS to Compile::current.)
// 
// As a consequence of what was just described, a MergeMem that represents a full
// memory state has an edge in(AliasIdxBot) which is a "wide" memory state,
// containing all alias categories.
// 
// MergeMem nodes never (?) have control inputs, so in(0) is NULL.
// 
// All other edges in(N) (including in(AliasIdxRaw), which is in(3)) are either
// a memory state for the alias type <N>, or else the top node, meaning that
// there is no particular input for that alias type.  Note that the length of
// a MergeMem is variable, and may be extended at any time to accommodate new
// memory states at larger alias indexes.  When merges grow, they are of course
// filled with "top" in the unused in() positions.
//
// This use of top is named "empty_memory()", or "empty_mem" (no-memory) as a variable.
// (Top was chosen because it works smoothly with passes like GCM.)
// 
// For convenience, we hardwire the alias index for TypeRawPtr::BOTTOM.  (It is
// the type of random VM bits like TLS references.)  Since it is always the
// first non-Bot memory slice, some low-level loops use it to initialize an
// index variable:  for (i = AliasIdxRaw; i < req(); i++).
//
// 
// ACCESSORS:  There is a special accessor MergeMemNode::base_memory which returns
// the distinguished "wide" state.  The accessor MergeMemNode::memory_at(N) returns
// the memory state for alias type <N>, or (if there is no particular slice at <N>,
// it returns the base memory.  To prevent bugs, memory_at does not accept <Top>
// or <Bot> indexes.  The iterator MergeMemStream provides robust iteration over
// MergeMem nodes or pairs of such nodes, ensuring that the non-top edges are visited.
// 
// %%%% We may get rid of base_memory as a separate accessor at some point; it isn't
// really that different from the other memory inputs.  An abbreviation called
// "bot_memory()" for "memory_at(AliasIdxBot)" would keep code tidy.
// 
// 
// PARTIAL MEMORY STATES:  During optimization, MergeMem nodes may arise that represent
// partial memory states.  When a Phi splits through a MergeMem, the copy of the Phi
// that "emerges though" the base memory will be marked as excluding the alias types
// of the other (narrow-memory) copies which "emerged through" the narrow edges:
// 
//   Phi<Bot>(U, MergeMem(<Bot>: W, <8>: Y))
//     ==Ideal=>  MergeMem(<Bot>: Phi<Bot-8>(U, W), Phi<8>(U, Y))
// 
// This strange "subtraction" effect is necessary to ensure IGVN convergence.
// (It is currently unimplemented.)  As you can see, the resulting merge is
// actually a disjoint union of memory states, rather than an overlay.
// 

//------------------------------MergeMemNode-----------------------------------
Node* MergeMemNode::make_empty_memory() {
  Node* empty_memory = (Node*) Compile::current()->top();
  assert(empty_memory->is_top(), "correct sentinel identity");
  return empty_memory;
}

MergeMemNode::MergeMemNode(Node *new_base) : Node(1+Compile::AliasIdxRaw) {
  set_req(0, NULL);  // no control input

  // Initialize the edges uniformly to top, for starters.
  Node* empty_mem = make_empty_memory();
  for (uint i = Compile::AliasIdxTop; i < req(); i++) {
    set_req(i,empty_mem);
  }
  assert(empty_memory() == empty_mem, "");

  MergeMemNode* mdef = (new_base == NULL)? NULL: new_base->is_MergeMem();
  if( mdef ) {
    assert(mdef->empty_memory() == empty_mem, "consistent sentinels");
    for (MergeMemStream mms(this, mdef); mms.next_non_empty2(); ) {
      mms.set_memory(mms.memory2());
    }
    assert(base_memory() == mdef->base_memory(), "");
  } else {
    set_base_memory(new_base);
  }
}

//------------------------------cmp--------------------------------------------
uint MergeMemNode::hash() const { return NO_HASH; }
uint MergeMemNode::cmp( const Node &n ) const { 
  return (&n == this);          // Always fail except on self
}

//------------------------------Identity---------------------------------------
Node* MergeMemNode::Identity(PhaseTransform *phase) {
  // Identity if this merge point does not record any interesting memory 
  // disambiguations.
  Node* base_mem = base_memory();
  Node* empty_mem = empty_memory();
  for (uint i = Compile::AliasIdxRaw; i < req(); i++) {
    Node* mem = in(i);
    if (mem != empty_mem && mem != base_mem) {
      return this;              // Many memory splits; no change
    }
  }
  return base_mem;                   // No memory splits; ID on the one true input
}

//------------------------------Ideal------------------------------------------
// This method is invoked recursively on chains of MergeMem nodes
Node *MergeMemNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  // Remove chain'd MergeMems
  //
  // This is delicate, because the each "in(i)" (i >= Raw) is interpreted
  // relative to the "in(Bot)".  Since we are patching both at the same time,
  // we have to be careful to read each "in(i)" relative to the old "in(Bot)",
  // but rewrite each "in(i)" relative to the new "in(Bot)".
  Node *progress = NULL;


  Node* old_base = base_memory();
  MergeMemNode* old_mbase = (old_base == NULL)? NULL: old_base->is_MergeMem();
  Node* new_base = old_base;

  // simplify stacked MergeMems in base memory
  if (old_mbase)  new_base = old_mbase->base_memory();

  // the base memory might contribute new slices beyond my req()
  if (old_mbase)  grow_to_match(old_mbase);

  // Look carefully at the base node if it is a phi.
  PhiNode* phi_base = (new_base == NULL)? NULL: new_base->is_Phi();
  Node*    phi_reg = NULL;
  uint     phi_len = -1;
  if (phi_base != NULL && !phi_base->is_copy()) {
    // do not examine phi if degraded to a copy
    phi_reg = phi_base->region();
    phi_len = phi_base->req();
    // see if the phi is unfinished
    for (uint i = 1; i < phi_len; i++) {
      if (phi_base->in(i) == NULL) {
        // incomplete phi; do not look at it yet!
        phi_reg = NULL;
        phi_len = -1;
        break;
      }
    }
  }

  // Note:  We do not call verify_sparse on entry, because inputs
  // can normalize to the base_memory via subsume_node or similar
  // mechanisms.  This method repairs that damage.

  Node* empty_mem = empty_memory();
  assert(!old_mbase || old_mbase->is_empty_memory(empty_mem), "consistent sentinels");
  
  // Look at each slice.
  for (uint i = Compile::AliasIdxRaw; i < req(); i++) {
    Node* old_in = in(i);
    // calculate the old memory value
    Node* old_mem = old_in;
    if (old_mem == empty_mem)  old_mem = old_base;
    assert(old_mem == memory_at(i), "");

    // maybe update (reslice) the old memory value

    // simplify stacked MergeMems
    Node* new_mem = old_mem;
    MergeMemNode* old_mmem = old_mem == NULL ? NULL : old_mem->is_MergeMem();
    if (old_mmem == this) {
      // This can happen if loops break up and safepoints disappear.
      // A merge of BotPtr (default) with a RawPtr memory derived from a
      // safepoint can be rewritten to a merge of the same BotPtr with
      // the BotPtr phi coming into the loop.  If that phi disappears
      // also, we can end up with a self-loop of the mergemem.
      // In general, if loops degenerate and memory effects disappear,
      // a mergemem can be left looking at itself.  This simply means
      // that the mergemem's default should be used, since there is
      // no longer any apparent effect on this slice.
      // Note: If a memory slice is a MergeMem cycle, it is unreachable
      //       from start.  Update the input to TOP.
      new_mem = (new_base == this || new_base == phase->C->top())? phase->C->top() : new_base;
    }
    else if (old_mmem != NULL) {
      new_mem = old_mmem->memory_at(i);
    }
    // else preceeding memory was not a MergeMem

    // replace equivalent phis (unfortunately, they do not GVN together)
    if (new_mem != NULL && new_mem != new_base &&
        new_mem->req() == phi_len && new_mem->in(0) == phi_reg) {
      PhiNode* phi_mem = new_mem->is_Phi();
      if (phi_mem != NULL) {
        for (uint i = 1; i < phi_len; i++) {
          if (phi_base->in(i) != phi_mem->in(i)) {
            phi_mem = NULL;
            break;
          }
        }
        if (phi_mem != NULL) {
          // equivalent phi nodes; revert to the def
          new_mem = new_base;
        }
      }
    }

    // maybe store down a new value
    Node* new_in = new_mem;
    if (new_in == new_base)  new_in = empty_mem;

    if (new_in != old_in) {
      // Warning:  Do not combine this "if" with the previous "if"
      // A memory slice might have be be rewritten even if it is semantically
      // unchanged, if the base_memory value has changed.
      set_req(i, new_in);
      progress = this;          // Report progress
    }
  }

  if (new_base != old_base) {
    set_req(Compile::AliasIdxBot, new_base);
    // Don't use set_base_memory(new_base), because we need to update du.
    assert(base_memory() == new_base, "");
    progress = this;
  }
  if( base_memory() == this ) {
    // a self cycle indicates this memory path is dead
    set_req(Compile::AliasIdxBot, phase->C->top());
  }

  // Resolve external cycles by calling Ideal on a MergeMem base_memory
  // Recursion must occur after the self cycle check above
  MergeMemNode *new_mbase = base_memory()->is_MergeMem();
  if( new_mbase != NULL ) {
    assert(new_mbase->hash() == NO_HASH, "Must remove from hash table before Ideal call");
    Node *m = new_mbase->Ideal(phase, can_reshape);  // Rollup any cycles
    MergeMemNode *mm = (m == NULL) ? NULL : m->is_MergeMem();
    Node *top = phase->C->top();
    if( mm != NULL && mm->base_memory() == top ) {
      // propagate rollup of dead cycle to self
      set_req(Compile::AliasIdxBot, top);
      for (uint i = Compile::AliasIdxRaw; i < req(); i++) {
        if( in(i) != top ) { set_req(i, top); }
      }
      progress = this;
    }
  }

  assert(verify_sparse(), "please, no dups of base");
  return progress;
}

//-------------------------set_base_memory-------------------------------------
void MergeMemNode::set_base_memory(Node *new_base) {
  Node* empty_mem = empty_memory();
  set_req(Compile::AliasIdxBot, new_base);
  assert(memory_at(req()) == new_base, "must set default memory");
  // Clear out other occurrences of new_base:
  if (new_base != empty_mem) {
    for (uint i = Compile::AliasIdxRaw; i < req(); i++) {
      if (in(i) == new_base)  set_req(i, empty_mem);
    }
  }
}

//------------------------------out_RegMask------------------------------------
const RegMask &MergeMemNode::out_RegMask() const {
  return RegMask::Empty;
}

//------------------------------dump_spec--------------------------------------
#ifndef PRODUCT
void MergeMemNode::dump_spec() const {
  tty->print(" {");
  Node* base_mem = base_memory();
  for( uint i = Compile::AliasIdxRaw; i < req(); i++ ) {
    Node* mem = memory_at(i);
    if (mem == base_mem) { tty->print(" -"); continue; }
    tty->print( " N%d:", mem->_idx );
    Compile::current()->get_adr_type(i)->dump();
  }
  tty->print(" }");
}
#endif // !PRODUCT


#ifdef ASSERT
static bool might_be_same(Node* a, Node* b) {
  if (a == b)  return true;
  PhiNode* pa = a->is_Phi();
  PhiNode* pb = b->is_Phi();
  if (!(pa || pb))  return false;
  // phis shift around during optimization
  return true;  // pretty stupid...
}

// verify a narrow slice (either incoming or outgoing)
static void verify_memory_slice(const MergeMemNode* m, int alias_idx, Node* n) {
  if (!VerifyAliases)       return;  // don't bother to verify unless requested
  if (is_error_reported())  return;  // muzzle asserts when debugging an error
  if (Node::in_dump())      return;  // muzzle asserts when printing
  assert(alias_idx >= Compile::AliasIdxRaw, "must not disturb base_memory or sentinel");
  assert(n != NULL, "");
  // Elide intervening MergeMem's
  for (MergeMemNode* mc; (mc = n->is_MergeMem()) != NULL; ) {
    n = mc->memory_at(alias_idx);
  }
  Compile* C = Compile::current();
  const TypePtr* n_adr_type = n->adr_type();
  if (n == m->empty_memory()) {
    // Implicit copy of base_memory()
  } else if (n_adr_type != TypePtr::BOTTOM) {
    assert(n_adr_type != NULL, "new memory must have a well-defined adr_type");
    assert(C->must_alias(n_adr_type, alias_idx), "new memory must match selected slice");
  } else {
    // A few places like Parse::make_slow_call "know" that VM calls are narrow,
    // and can be used to update only the VM bits stored as TypeRawPtr::BOTTOM.
    bool expected_wide_mem = false;
    if (n == m->base_memory()) {
      expected_wide_mem = true;
    } else if (alias_idx == Compile::AliasIdxRaw ||
               n == m->memory_at(Compile::AliasIdxRaw)) {
      expected_wide_mem = true;
    } else if (!C->alias_type(alias_idx)->is_rewritable()) {
      // set_all_rewritable_memory is called on a membar or call,
      // but lets some memories "leak through" on memory channels that
      // are write-once.  Allow this also.
      expected_wide_mem = true;
    }
    assert(expected_wide_mem, "expected narrow slice replacement");
  }
}
#else // !ASSERT
#define verify_memory_slice(m,i,n) (0)  // PRODUCT version is no-op
#endif


//-----------------------------memory_at---------------------------------------
Node* MergeMemNode::memory_at(uint alias_idx) const {
  assert(alias_idx >= Compile::AliasIdxRaw ||
         alias_idx == Compile::AliasIdxBot && Compile::current()->AliasLevel() == 0,
         "must avoid base_memory and AliasIdxTop");

  // Otherwise, it is a narrow slice.
  Node* n = alias_idx < req() ? in(alias_idx) : empty_memory();
  if (is_empty_memory(n)) {
    // the array is sparse; empty slots are the "top" node
    n = base_memory();
    assert(Node::in_dump()
           || n == NULL || n->bottom_type() == Type::TOP
           || n->adr_type() == TypePtr::BOTTOM
           || Compile::current()->AliasLevel() == 0,
           "must be a wide memory");
    // AliasLevel == 0 if we are organizing the memory states manually.
  } else {
    // make sure the stored slice is sane
    #ifdef ASSERT
    if (is_error_reported() || Node::in_dump()) {
    } else if (might_be_same(n, base_memory())) {
      // Give it a pass:  It is a mostly harmless repetition of the base.
      // This can arise normally from node subsumption during optimization.
    } else {
      verify_memory_slice(this, alias_idx, n);
    }
    #endif
  }
  return n;
}

//---------------------------set_memory_at-------------------------------------
void MergeMemNode::set_memory_at(uint alias_idx, Node *n) {
  verify_memory_slice(this, alias_idx, n);
  Node* empty_mem = empty_memory();
  if (n == base_memory())  n = empty_mem;  // collapse default
  uint need_req = alias_idx+1;
  if (req() < need_req) {
    if (n == empty_mem)  return;  // already the default, so do not grow me
    // grow the sparse array
    grow(alias_idx);
    while (req() < need_req)  add_req(empty_mem);
  }
  set_req( alias_idx, n );
}



//-------------------------clone_all_memory------------------------------------
MergeMemNode* MergeMemNode::clone_all_memory(Node* mem) {
  MergeMemNode* mmem = mem->is_MergeMem();
  if (mmem == NULL)
    return new MergeMemNode(mem);
  MergeMemNode* cmem = (MergeMemNode*) mmem->clone();
  return cmem;
}


//--------------------------iteration_setup------------------------------------
void MergeMemNode::iteration_setup(const MergeMemNode* other) {
  if (other != NULL) {
    grow_to_match(other);
    // invariant:  the finite support of mm2 is within mm->req()
    #ifdef ASSERT
    for (uint i = req(); i < other->req(); i++) {
      assert(other->is_empty_memory(other->in(i)), "slice left uncovered");
    }
    #endif
  }
  // Replace spurious copies of base_memory by top.
  Node* base_mem = base_memory();
  if (base_mem != NULL && !base_mem->is_top()) {
    for (uint i = Compile::AliasIdxBot+1, imax = req(); i < imax; i++) {
      if (in(i) == base_mem)
        set_req(i, empty_memory());
    }
  }
}

//---------------------------grow_to_match-------------------------------------
void MergeMemNode::grow_to_match(const MergeMemNode* other) {
  Node* empty_mem = empty_memory();
  assert(other->is_empty_memory(empty_mem), "consistent sentinels");
  // look for the finite support of the other memory
  for (uint i = other->req(); --i >= req(); ) {
    if (other->in(i) != empty_mem) {
      uint new_len = i+1;
      while (req() < new_len)  add_req(empty_mem);
      break;
    }
  }
}

//---------------------------verify_sparse-------------------------------------
#ifndef PRODUCT
bool MergeMemNode::verify_sparse() const {
  assert(is_empty_memory(make_empty_memory()), "sane sentinel");
  Node* base_mem = base_memory();
  // The following can happen in degenerate cases, since empty==top.
  if (is_empty_memory(base_mem))  return true;
  for (uint i = Compile::AliasIdxRaw; i < req(); i++) {
    assert(in(i) != NULL, "sane slice");
    if (in(i) == base_mem)  return false;  // should have been the sentinel value!
  }
  return true;
}

bool MergeMemStream::match_memory(Node* mem, const MergeMemNode* mm, int idx) {
  Node* n;
  n = mm->in(idx);
  if (mem == n)  return true;  // might be empty_memory()
  n = (idx == Compile::AliasIdxBot)? mm->base_memory(): mm->memory_at(idx);
  if (mem == n)  return true;
  PhiNode* phi = n->is_Phi();
  while (phi && (n = phi->is_copy()) != NULL) {
    if (mem == n)  return true;
    if (n == NULL)  break;
    phi = n->is_Phi();
  }
  return false;
}
#endif // !PRODUCT
