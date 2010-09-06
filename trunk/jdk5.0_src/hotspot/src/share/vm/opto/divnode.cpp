#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)divnode.cpp	1.75 03/12/23 16:42:26 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style

#include "incls/_precompiled.incl"
#include "incls/_divnode.cpp.incl"
#include <math.h>

// Implement the integer constant divide -> long multiply transform found in
//   "Division by Invariant Integers using Multiplication"
//     by Granlund and Montgomery
static Node *transform_int_divide_to_long_multiply( PhaseGVN *phase, Node *dividend, int divisor ) {

  // Check for invalid divisors
  assert( divisor != 0 && divisor != min_jint && divisor != 1,
    "bad divisor for transforming to long multiply" );

  // Compute l = ceiling(log2(d))
  //   presumes d is more likely small
  bool d_pos = divisor >= 0;
  int d = d_pos ? divisor : -divisor;
  unsigned ud = (unsigned)d;
  const int N = 32;
  int l = log2_intptr(d-1)+1;
  int sh_post = l;

  const uint64_t U1 = (uint64_t)1;

  // Cliff pointed out how to prevent overflow (from the paper)
  uint64_t m_low  =  (((U1 << l) - ud) << N)                  / ud + (U1 << N);
  uint64_t m_high = ((((U1 << l) - ud) << N) + (U1 << (l+1))) / ud + (U1 << N);

  // Reduce to lowest terms
  for ( ; sh_post > 0; sh_post-- ) {
    uint64_t m_low_1  = m_low  >> 1;
    uint64_t m_high_1 = m_high >> 1;
    if ( m_low_1 >= m_high_1 )
      break;
    m_low  = m_low_1;
    m_high = m_high_1;
  }

  // Result
  Node *q;

  // division by +/- 1
  if (d == 1) {
    // Filtered out as identity above
    if (d_pos)
      return NULL;

    // Just negate the value
    else {
      q = new (3) SubINode(phase->intcon(0), dividend);
    }
  }

  // division by +/- a power of 2
  else if ( is_power_of_2(d) ) {

    // See if we can simply do a shift without rounding
    bool needs_rounding = true;
    const Type *dt = phase->type(dividend);
    const TypeInt *dti = dt->isa_int();
  
    // we don't need to round a positive dividend
    if (dti && dti->_lo >= 0)
      needs_rounding = false;

    // An AND mask of sufficient size clears the low bits and
    // I can avoid rounding.
    else if( dividend->Opcode() == Op_AndI ) {
      const TypeInt *andconi = phase->type( dividend->in(2) )->isa_int();
      if( andconi && andconi->is_con(-d) ) {
        dividend = dividend->in(1);
        needs_rounding = false;
      }
    }

    // Add rounding to the shift to handle the sign bit
    if( needs_rounding ) {
      Node *t1 = phase->transform(new (3) RShiftINode(dividend, phase->intcon(l - 1)));
      Node *t2 = phase->transform(new (3) URShiftINode(t1, phase->intcon(N - l)));
      dividend = phase->transform(new (3) AddINode(dividend, t2));
    }

    q = new (3) RShiftINode(dividend, phase->intcon(l));

    if (!d_pos)
      q = new (3) SubINode(phase->intcon(0), phase->transform(q));
  }

  // division by something else
  else if (m_high < (U1 << (N-1))) {
    Node *t1 = phase->transform(new (2) ConvI2LNode(dividend));
    Node *t2 = phase->transform(new (3) MulLNode(t1, phase->longcon(m_high)));
    Node *t3 = phase->transform(new (3) RShiftLNode(t2, phase->intcon(sh_post+N)));
    Node *t4 = phase->transform(new (2) ConvL2INode(t3));
    Node *t5 = phase->transform(new (3) RShiftINode(dividend, phase->intcon(N-1)));

    q = new (3) SubINode(d_pos ? t4 : t5, d_pos ? t5 : t4);
  }

  // This handles that case where m_high is >= 2**(N-1). In that case,
  // we subtract out 2**N from the multiply and add it in later as
  // "dividend" in the equation (t5). This case computes the same result
  // as the immediately preceeding case, save that rounding and overflow
  // are accounted for.
  else {
    Node *t1 = phase->transform(new (2) ConvI2LNode(dividend));
    Node *t2 = phase->transform(new (3) MulLNode(t1, phase->longcon(m_high - (U1 << N))));
    Node *t3 = phase->transform(new (3) RShiftLNode(t2, phase->intcon(N)));
    Node *t4 = phase->transform(new (2) ConvL2INode(t3));
    Node *t5 = phase->transform(new (3) AddINode(dividend, t4));
    Node *t6 = phase->transform(new (3) RShiftINode(t5, phase->intcon(sh_post)));
    Node *t7 = phase->transform(new (3) RShiftINode(dividend, phase->intcon(N-1)));

    q = new (3) SubINode(d_pos ? t6 : t7, d_pos ? t7 : t6);
  }

  return (q);
}

//=============================================================================
//------------------------------Identity---------------------------------------
// If the divisor is 1, we are an identity on the dividend.
Node *DivINode::Identity( PhaseTransform *phase ) {
  return (phase->type( in(2) )->higher_equal(TypeInt::ONE)) ? in(1) : this;
}

//------------------------------Idealize---------------------------------------
// Divides can be changed to multiplies and/or shifts
Node *DivINode::Ideal(PhaseGVN *phase, bool can_reshape) {
  if (in(0) && remove_dead_region(phase, can_reshape))  return this;

  const Type *t = phase->type( in(2) );
  if( t == TypeInt::ONE )       // Identity?
    return NULL;                // Skip it

  const TypeInt *ti = t->isa_int();
  if( !ti ) return NULL;
  if( !ti->is_con() ) return NULL;
  int i = ti->get_con();        // Get divisor

  if (i == 0) return NULL;      // Dividing by zero constant does not idealize

  set_req(0,NULL);              // Dividing by a not-zero constant; no faulting

  // Dividing by MININT does not optimize as a power-of-2 shift.
  if( i == min_jint ) return NULL;

  return transform_int_divide_to_long_multiply( phase, in(1), i );
}

//------------------------------Value------------------------------------------
// A DivINode divides its inputs.  The third input is a Control input, used to
// prevent hoisting the divide above an unsafe test.
const Type *DivINode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // x/x == 1,except for 0
  if( phase->eqv( in(1), in(2) ) && t1 != TypeInt::ZERO )
    return TypeInt::ONE;

  // Divide the two numbers.  We approximate.
  // If divisor is a constant and not zero
  const TypeInt *i1 = t1->is_int();
  const TypeInt *i2 = t2->is_int();
  if( i2->is_con() && i2->get_con() ) {
    int32 d = i2->get_con(); // Divisor
    jint lo, hi;
    if( d >= 0 ) {
      lo = i1->_lo/d;
      hi = i1->_hi/d;
    } else {
      if( d == -1 && i1->_lo == min_jint ) {
        // 'min_jint/-1' throws arithmetic exception during compilation 
        lo = min_jint;
        // do not support holes, 'hi' must go to either min_jint or max_jint:
        // [min_jint, -10]/[-1,-1] ==> [min_jint] UNION [10,max_jint]
        hi = i1->_hi == min_jint ? min_jint : max_jint;
      } else {
        lo = i1->_hi/d;
        hi = i1->_lo/d;
      }
    }
    return TypeInt::make(lo, hi);
  }  

  // If the dividend is a constant
  if( i1->is_con() ) {
    int32 d = i1->get_con();
    if( d < 0 ) {
      if( d == min_jint ) {
        //  (-min_jint) == min_jint == (min_jint / -1)
        return TypeInt::make(min_jint, max_jint/2 + 1);
      } else {
        return TypeInt::make(d, -d);
      }
    }
    return TypeInt::make(-d,d);
  }

  // Otherwise we give up all hope
  return TypeInt::INT;
}


//=============================================================================
//------------------------------Identity---------------------------------------
// If the divisor is 1, we are an identity on the dividend.
Node *DivLNode::Identity( PhaseTransform *phase ) {
  return (phase->type( in(2) )->higher_equal(TypeLong::ONE)) ? in(1) : this;
}

//------------------------------Idealize---------------------------------------
// Dividing by a power of 2 is a shift.
Node *DivLNode::Ideal( PhaseGVN *phase, bool can_reshape) {
  if (in(0) && remove_dead_region(phase, can_reshape))  return this;

  const Type *t = phase->type( in(2) );
  if( t == TypeLong::ONE )       // Identity?
    return NULL;                // Skip it

  const TypeLong *ti = t->isa_long();
  if( !ti ) return NULL;
  if( !ti->is_con() ) return NULL;
  jlong i = ti->get_con();      // Get divisor
  if( i ) set_req(0, NULL);     // Dividing by a not-zero constant; no faulting

  // Dividing by MININT does not optimize as a power-of-2 shift.
  if( i == min_jlong ) return NULL;

  // Check for negative power of 2 divisor, if so, negate it and set a flag
  // to indicate result needs to be negated.  Note that negating the dividend
  // here does not work when it has the value MININT
  Node *dividend = in(1);
  bool negate_res = false;
  if( (-i & ~(-i-1)) == -i ) {
    i = -i;                     // Flip divisor
    negate_res = true;
  }

  // Check for power of 2
  if( (i & ~(i-1)) != i )       // Is divisor a power of 2?
    return NULL;                // Not a power of 2

  // Compute number of bits to shift
  int log_i = log2_long(i);

  // See if we can simply do a shift without rounding
  bool needs_rounding = true;
  const Type *dt = phase->type(dividend);
  const TypeLong *dtl = dt->isa_long();

  if (dtl && dtl->_lo > 0) {
    // we don't need to round a positive dividend
    needs_rounding = false;
  } else if( dividend->Opcode() == Op_AndL ) {
    // An AND mask of sufficient size clears the low bits and
    // I can avoid rounding.
    const TypeLong *andconi = phase->type( dividend->in(2) )->isa_long();
    if( andconi &&
        andconi->is_con() &&
        andconi->get_con() == -i ) {
      dividend = dividend->in(1);
      needs_rounding = false;
    }
  }

  if (!needs_rounding) {
    Node *result = new (3) RShiftLNode(dividend, phase->intcon(log_i));
    if (negate_res) {
      result = phase->transform(result);
      result = new (3) SubLNode(phase->makecon(TypeLong::ZERO), result);
    }
    return result;
  }

  // Divide-by-power-of-2 can be made into a shift, but you have to do
  // more math for the rounding.  You need to add 0 for positive
  // numbers, and "i-1" for negative numbers.  Example: i=4, so the
  // shift is by 2.  You need to add 3 to negative dividends and 0 to
  // positive ones.  So (-7+3)>>2 becomes -1, (-4+3)>>2 becomes -1,
  // (-2+3)>>2 becomes 0, etc.

  // Compute 0 or -1, based on sign bit
  Node *sign = phase->transform(new (3) RShiftLNode(dividend,phase->intcon(63)));
  // Mask sign bit to the low sign bits
  Node *round = phase->transform(new (3) AndLNode(sign,phase->makecon(TypeLong::make(i-1))));
  // Round up before shifting
  Node *sum = phase->transform(new (3) AddLNode(dividend,round));
  // Shift for division
  Node *result = new (3) RShiftLNode(sum, phase->intcon(log_i));
  if (negate_res) {
    result = phase->transform(result);
    result = new (3) SubLNode(phase->makecon(TypeLong::ZERO), result);
  }

  return result;
}

//------------------------------Value------------------------------------------
// A DivLNode divides its inputs.  The third input is a Control input, used to
// prevent hoisting the divide above an unsafe test.
const Type *DivLNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // x/x == 1,except for 0
  if( phase->eqv( in(1), in(2) ) && t1 != TypeLong::ZERO )
    return TypeLong::ONE;

  // Divide the two numbers.  We approximate.
  // If divisor is a constant and not zero
  const TypeLong *i1 = t1->is_long();
  const TypeLong *i2 = t2->is_long();
  if( i2->is_con() && i2->get_con() ) {
    jlong d = i2->get_con();    // Divisor
    if( d < 0 ) return TypeLong::make(i1->_hi/d, i1->_lo/d);
    return TypeLong::make(i1->_lo/d, i1->_hi/d);
  }  

  // If the dividend is a constant
  if( i1->is_con() ) {
    jlong d = i1->get_con();
    if( d < 0 ) {
      if( d == min_jlong ) {
        //  (-min_jlong) == min_jlong == (min_jlong / -1)
        return TypeLong::make(min_jlong, max_jlong/2 + 1);
      } else {
        return TypeLong::make(d, -d);
      }
    }
    return TypeLong::make(-d,d);
  }

  // Otherwise we give up all hope
  return TypeLong::LONG;
}


//=============================================================================
//------------------------------Value------------------------------------------
// An DivFNode divides its inputs.  The third input is a Control input, used to
// prevent hoisting the divide above an unsafe test.
const Type *DivFNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // x/x == 1, we ignore 0/0.
  // Note: if t1 and t2 are zero then result is NaN (JVMS page 213)
  // does not work for variables because of NaN's
  if( phase->eqv( in(1), in(2) ) && t1->base() == Type::FloatCon)
    if (!g_isnan(t1->getf()) && g_isfinite(t1->getf()) && t1->getf() != 0.0) // could be negative ZERO or NaN
      return TypeF::ONE;

  if( t2 == TypeF::ONE )
    return t1;

  // If divisor is a constant and not zero, divide them numbers
  if( t1->base() == Type::FloatCon && 
      t2->base() == Type::FloatCon &&
      t2->getf() != 0.0 ) // could be negative zero
    return TypeF::make( t1->getf()/t2->getf() );

  // If the dividend is a constant zero
  // Note: if t1 and t2 are zero then result is NaN (JVMS page 213)
  // Test TypeF::ZERO is not sufficient as it could be negative zero

  if( t1 == TypeF::ZERO && !g_isnan(t2->getf()) && t2->getf() != 0.0 )
    return TypeF::ZERO;

  // Otherwise we give up all hope
  return Type::FLOAT;
}

//------------------------------isA_Copy---------------------------------------
// Dividing by self is 1.
// If the divisor is 1, we are an identity on the dividend.
Node *DivFNode::Identity( PhaseTransform *phase ) {
  return (phase->type( in(2) ) == TypeF::ONE) ? in(1) : this;
}


//------------------------------Idealize---------------------------------------
Node *DivFNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  if (in(0) && remove_dead_region(phase, can_reshape))  return this;

  const Type *t2 = phase->type( in(2) );
  if( t2 == TypeF::ONE )         // Identity?
    return NULL;                // Skip it

  const TypeF *tf = t2->isa_float_constant();
  if( !tf ) return NULL;
  if( tf->base() != Type::FloatCon ) return NULL;

  // Check for out of range values
  if( tf->is_nan() || !tf->is_finite() ) return NULL;

  // Get the value
  float f = tf->getf();
  int exp;

  // Only for special case of dividing by a power of 2
  if( frexp((double)f, &exp) != 0.5 ) return NULL;

  // Limit the range of acceptable exponents
  if( exp < -126 || exp > 126 ) return NULL;

  // Compute the reciprocal
  float reciprocal = ((float)1.0) / f;

  assert( frexp((double)reciprocal, &exp) == 0.5, "reciprocal should be power of 2" );

  // return multiplication by the reciprocal
  return (new (3) MulFNode(in(1), phase->makecon(TypeF::make(reciprocal))));
}

//=============================================================================
//------------------------------Value------------------------------------------
// An DivDNode divides its inputs.  The third input is a Control input, used to
// prvent hoisting the divide above an unsafe test.
const Type *DivDNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // x/x == 1, we ignore 0/0.
  // Note: if t1 and t2 are zero then result is NaN (JVMS page 213)
  // Does not work for variables because of NaN's
  if( phase->eqv( in(1), in(2) ) && t1->base() == Type::DoubleCon)
    if (!g_isnan(t1->getd()) && g_isfinite(t1->getd()) && t1->getd() != 0.0) // could be negative ZERO or NaN
      return TypeD::ONE;

  if( t2 == TypeD::ONE )
    return t1;

  // If divisor is a constant and not zero, divide them numbers
  if( t1->base() == Type::DoubleCon && 
      t2->base() == Type::DoubleCon &&
      t2->getd() != 0.0 ) // could be negative zero
    return TypeD::make( t1->getd()/t2->getd() );

  // If the dividend is a constant zero
  // Note: if t1 and t2 are zero then result is NaN (JVMS page 213)
  // Test TypeF::ZERO is not sufficient as it could be negative zero
  if( t1 == TypeD::ZERO && !g_isnan(t2->getd()) && t2->getd() != 0.0 )
    return TypeD::ZERO;

  // Otherwise we give up all hope
  return Type::DOUBLE;
}


//------------------------------isA_Copy---------------------------------------
// Dividing by self is 1.
// If the divisor is 1, we are an identity on the dividend.
Node *DivDNode::Identity( PhaseTransform *phase ) {
  return (phase->type( in(2) ) == TypeD::ONE) ? in(1) : this;
}

//------------------------------Idealize---------------------------------------
Node *DivDNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  if (in(0) && remove_dead_region(phase, can_reshape))  return this;

  const Type *t2 = phase->type( in(2) );
  if( t2 == TypeD::ONE )         // Identity?
    return NULL;                // Skip it

  const TypeD *td = t2->isa_double_constant();
  if( !td ) return NULL;
  if( td->base() != Type::DoubleCon ) return NULL;

  // Check for out of range values
  if( td->is_nan() || !td->is_finite() ) return NULL;

  // Get the value
  double d = td->getd();
  int exp;

  // Only for special case of dividing by a power of 2
  if( frexp(d, &exp) != 0.5 ) return NULL;

  // Limit the range of acceptable exponents
  if( exp < -1021 || exp > 1022 ) return NULL;

  // Compute the reciprocal
  double reciprocal = 1.0 / d;

  assert( frexp(reciprocal, &exp) == 0.5, "reciprocal should be power of 2" );

  // return multiplication by the reciprocal
  return (new (3) MulDNode(in(1), phase->makecon(TypeD::make(reciprocal))));
}

//=============================================================================
//------------------------------Idealize---------------------------------------
Node *ModINode::Ideal(PhaseGVN *phase, bool can_reshape) {
  // Check for dead control input
  if( remove_dead_region(phase, can_reshape) )  return this;

  // Get the modulus
  const Type *t = phase->type( in(2) );
  if( t == Type::TOP ) return NULL;
  const TypeInt *ti = t->is_int();

  // Check for useless control input
  // Check for excluding mod-zero case
  if( in(0) && (ti->_hi < 0 || ti->_lo > 0) ) {
    set_req(0, NULL);        // Yank control input
    return this;
  }

  // See if we are MOD'ing by 2^k or 2^k-1.
  if( !ti->is_con() ) return NULL;
  jint con = ti->get_con();

  // First, special check for modulo 2^k-1
  if( con >= 0 && con < max_jint && is_power_of_2(con+1) ) {
    int k = exact_log2(con+1);  // Extract k

    // Basic algorithm by David Detlefs.  See fastmod_int.java for gory details.
    static int unroll_factor[] = { 999, 999, 29, 14, 9, 7, 5, 4, 4, 3, 3, 2, 2, 2, 2, 2, 1 /*past here we assume 1 forever*/};
    int trip_count = 1;
    if( k < sizeof(unroll_factor)/sizeof(unroll_factor[0]) )
      trip_count = unroll_factor[k];

    // If the unroll factor is not too large, and if conditional moves are
    // ok, then use this case
    if( trip_count <= 5 && ConditionalMoveLimit != 0 ) {
      Node *x = in(1);            // Value being mod'd
      Node *divisor = in(2);      // Also is mask
      
      // Generate code to reduce X rapidly to nearly 2^k-1.
      for( int i = 0; i < trip_count; i++ ) {
          Node *xl = phase->transform( new (3) AndINode(x,divisor) );
          Node *xh = phase->transform( new (3) RShiftINode(x,phase->intcon(k)) ); // Must be signed
          x = phase->transform( new (3) AddINode(xh,xl) );
      }

      // Generate sign-fixup code.  Was original value positive?
      // int hack_res = (i >= 0) ? divisor : 1;
      Node *cmp1 = phase->transform( new (3) CmpINode( in(1), phase->intcon(0) ) );
      Node *bol1 = phase->transform( new (2) BoolNode( cmp1, BoolTest::ge ) );
      Node *cmov1= phase->transform( new CMoveINode(bol1,phase->intcon(1),divisor,TypeInt::POS) );
      // if( x >= hack_res ) x -= divisor;
      Node *sub  = phase->transform( new (3) SubINode( x, divisor ) );
      Node *cmp2 = phase->transform( new (3) CmpINode( x, cmov1 ) );
      Node *bol2 = phase->transform( new (2) BoolNode( cmp2, BoolTest::ge ) );
      // Convention is to not transform the return value of an Ideal
      // since Ideal is expected to return a modified 'this' or a new node.
      Node *cmov2= new CMoveINode(bol2,x,sub,TypeInt::INT);
      // cmov2 is now the mod
      
      return cmov2;
    }
  }

  // Fell thru, the unroll case is not appropriate. Transform the modulo
  // into a long multiply/int multiply/subtract case

  // Cannot handle mod 0, and min_jint isn't handled by the transform
  if( con == 0 || con == min_jint ) return NULL;

  // Get the absolute value of the constant; at this point, we can use this
  jint pos_con = (con >= 0) ? con : -con;

  // integer Mod 1 is always 0
  if( pos_con == 1 ) return new (1) ConINode(TypeInt::ZERO);

  int log2_con = -1;

  // If this is a power of two, they maybe we can mask it
  if( is_power_of_2(pos_con) ) {
    log2_con = log2_intptr((intptr_t)pos_con);

    const Type *dt = phase->type(in(1));
    const TypeInt *dti = dt->isa_int();

    // See if this can be masked, if the dividend is non-negative
    if( dti && dti->_lo >= 0 )
      return ( new (3) AndINode( in(1), phase->intcon( pos_con-1 ) ) );
  }

  // Save in(1) so that it cannot be changed or deleted
  Node *hook = new (2) Node( (Node*)NULL, (Node*)NULL );
  hook->set_req(1, in(1));

  // Divide using the transform from DivI to MulL
  Node *divide = phase->transform( transform_int_divide_to_long_multiply( phase, in(1), pos_con ) );

  // Re-multiply, using a shift if this is a power of two
  Node *mult = NULL;

  if( log2_con >= 0 )
    mult = phase->transform( new (3) LShiftINode( divide, phase->intcon( log2_con ) ) );
  else
    mult = phase->transform( new (3) MulINode( divide, phase->intcon( pos_con ) ) );

  // Finally, subtract the multiplied divided value from the original
  Node *result = new (3) SubINode( in(1), mult );

  // Release in(1)
  hook->set_req(1, NULL);

  // return the value
  return result;
}

//------------------------------Value------------------------------------------
const Type *ModINode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // 0 MOD X is 0
  if( t1 == TypeInt::ZERO ) return TypeInt::ZERO; 
  // X MOD X is 0
  if( phase->eqv( in(1), in(2) ) && t1 != TypeInt::ZERO) return TypeInt::ZERO;

  const TypeInt *i1 = t1->is_int();
  const TypeInt *i2 = t2->is_int();
  if( !i1->is_con() || !i2->is_con() ) {
    if( i1->_lo >= 0 && i2->_lo >= 0 )
      return TypeInt::POS;
    // If both numbers are not constants, we know little.
    return TypeInt::INT;
  }
  // Mod by zero?  Throw exception at runtime!
  if( !i2->get_con() ) return TypeInt::POS;
  
  // We must be modulo'ing 2 float constants.
  // Check for min_jint % '-1', result is defined to be '0'.
  if( i1->get_con() == min_jint && i2->get_con() == -1 ) 
    return TypeInt::ZERO;

  return TypeInt::make( i1->get_con() % i2->get_con() );
}


//=============================================================================
//------------------------------Idealize---------------------------------------
Node *ModLNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  // Check for dead control input
  if( remove_dead_region(phase, can_reshape) )  return this;

  // Get the modulus
  const Type *t = phase->type( in(2) );
  if( t == Type::TOP ) return NULL;
  const TypeLong *ti = t->is_long();

  // Check for useless control input
  // Check for excluding mod-zero case
  if( in(0) && (ti->_hi < 0 || ti->_lo > 0) ) {
    set_req(0, NULL);        // Yank control input
    return this;
  }

  // See if we are MOD'ing by 2^k or 2^k-1.
  if( !ti->is_con() ) return NULL;
  jlong con = ti->get_con();
  bool m1 = false;
  if( !is_power_of_2_long(con) ) {      // Not 2^k
    if( !is_power_of_2_long(con+1) ) // Not 2^k-1?
      return NULL;              // No interesting mod hacks
    m1 = true;                  // Found 2^k-1
    con++;                      // Convert to 2^k form
  }
  int k = log2_long(con);       // Extract k

  // Expand mod
  if( !m1 ) {                   // Case 2^k
  } else {                      // Case 2^k-1
    // Basic algorithm by David Detlefs.  See fastmod_long.java for gory details.
    // Used to help a popular random number generator which does a long-mod
    // of 2^31-1 and shows up in SpecJBB and SciMark.
    static int unroll_factor[] = { 999, 999, 61, 30, 20, 15, 12, 10, 8, 7, 6, 6, 5, 5, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1 /*past here we assume 1 forever*/};
    int trip_count = 1;
    if( k < sizeof(unroll_factor)/sizeof(unroll_factor[0]) )
        trip_count = unroll_factor[k];
    if( trip_count > 4 ) return NULL; // Too much unrolling
    if (ConditionalMoveLimit == 0) return NULL;  // cmov is required
    
    Node *x = in(1);            // Value being mod'd
    Node *divisor = in(2);      // Also is mask
    
    // Generate code to reduce X rapidly to nearly 2^k-1.
    for( int i = 0; i < trip_count; i++ ) {
        Node *xl = phase->transform( new (3) AndLNode(x,divisor) );
        Node *xh = phase->transform( new (3) RShiftLNode(x,phase->intcon(k)) ); // Must be signed
        x = phase->transform( new (3) AddLNode(xh,xl) );
    }
    // Generate sign-fixup code.  Was original value positive?
    // long hack_res = (i >= 0) ? divisor : CONST64(1);
    Node *cmp1 = phase->transform( new (3) CmpLNode( in(1), phase->makecon(TypeLong::ZERO) ) );
    Node *bol1 = phase->transform( new (2) BoolNode( cmp1, BoolTest::ge ) );
    Node *cmov1= phase->transform( new CMoveLNode(bol1,phase->makecon(TypeLong::ONE),divisor,TypeLong::LONG) );
    // if( x >= hack_res ) x -= divisor;
    Node *sub  = phase->transform( new (3) SubLNode( x, divisor ) );
    Node *cmp2 = phase->transform( new (3) CmpLNode( x, cmov1 ) );
    Node *bol2 = phase->transform( new (2) BoolNode( cmp2, BoolTest::ge ) );
    // Convention is to not transform the return value of an Ideal
    // since Ideal is expected to return a modified 'this' or a new node.
    Node *cmov2= new CMoveLNode(bol2,x,sub,TypeLong::LONG);
    // cmov2 is now the mod
    
    return cmov2;
  }
  return NULL;
}

//------------------------------Value------------------------------------------
const Type *ModLNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // 0 MOD X is 0
  if( t1 == TypeLong::ZERO ) return TypeLong::ZERO; 
  // X MOD X is 0
  if( phase->eqv( in(1), in(2) ) && t1 != TypeLong::ZERO) return TypeLong::ZERO;

  const TypeLong *i1 = t1->is_long();
  const TypeLong *i2 = t2->is_long();
  if( !i1->is_con() || !i2->is_con() ) {
    if( i1->_lo >= CONST64(0) && i2->_lo >= CONST64(0) )
      return TypeLong::make(CONST64(0),max_jlong);
    // If both numbers are not constants, we know little.
    return TypeLong::LONG;
  }
  // Mod by zero?  Throw exception at runtime!
  if( !i2->get_con() ) return TypeLong::make(CONST64(0),max_jlong);
  
  // We must be modulo'ing 2 float constants.
  // Check for min_jint % '-1', result is defined to be '0'.
  if( i1->get_con() == min_jlong && i2->get_con() == -1 ) 
    return TypeLong::ZERO;

  return TypeLong::make( i1->get_con() % i2->get_con() );
}


//=============================================================================
//------------------------------Value------------------------------------------
const Type *ModFNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // If either is a NaN, return an input NaN
  if( g_isnan(t1->getf()) )    return t1;
  if( g_isnan(t2->getf()) )    return t2;

  // It is not worth trying to constant fold this stuff!
  return Type::FLOAT;

  /*
  // If dividend is infinity or divisor is zero, or both, the result is NaN
  if( !g_isfinite(t1->getf()) || ((t2->getf() == 0.0) || (jint_cast(t2->getf()) == 0x80000000)) )

  // X MOD infinity = X
  if( !g_isfinite(t2->getf()) && !g_isnan(t2->getf()) ) return t1;
  // 0 MOD finite = dividend (positive or negative zero)
  // Not valid for: NaN MOD any; any MOD nan; 0 MOD 0; or for 0 MOD NaN
  // NaNs are handled previously.
  if( !(t2->getf() == 0.0) && !((int)t2->getf() == 0x80000000)) {
    if (((t1->getf() == 0.0) || ((int)t1->getf() == 0x80000000)) && g_isfinite(t2->getf()) ) {
      return t1;
    }
  }
  // X MOD X is 0
  // Does not work for variables because of NaN's
  if( phase->eqv( in(1), in(2) ) && t1->base() == Type::FloatCon) 
    if (!g_isnan(t1->getf()) && (t1->getf() != 0.0) && ((int)t1->getf() != 0x80000000)) {
      if(t1->getf() < 0.0) {
        float result = jfloat_cast(0x80000000);
        return TypeF::make( result );
      }
      else
        return TypeF::ZERO;
    }

  // If both numbers are not constants, we know nothing.
  if( (t1->base() != Type::FloatCon) || (t2->base() != Type::FloatCon) )
    return Type::FLOAT;
  
  // We must be modulo'ing 2 float constants.
  // Make sure that the sign of the fmod is equal to the sign of the dividend
  float result = (float)fmod( t1->getf(), t2->getf() );
  float dividend = t1->getf();
  if( (dividend < 0.0) || ((int)dividend == 0x80000000) ) {
    if( result > 0.0 )
      result = 0.0 - result;
    else if( result == 0.0 ) {
      result = jfloat_cast(0x80000000);
    }
  }
  return TypeF::make( result );
  */
}


//=============================================================================
//------------------------------Value------------------------------------------
const Type *ModDNode::Value( PhaseTransform *phase ) const {
  // Either input is TOP ==> the result is TOP
  const Type *t1 = phase->type( in(1) );
  const Type *t2 = phase->type( in(2) );
  if( t1 == Type::TOP ) return Type::TOP;
  if( t2 == Type::TOP ) return Type::TOP;

  // Either input is BOTTOM ==> the result is the local BOTTOM
  const Type *bot = bottom_type();
  if( (t1 == bot) || (t2 == bot) ||
      (t1 == Type::BOTTOM) || (t2 == Type::BOTTOM) ) 
    return bot;

  // If either is a NaN, return an input NaN
  if( g_isnan(t1->getd()) )    return t1;
  if( g_isnan(t2->getd()) )    return t2;
  // X MOD infinity = X
  if( !g_isfinite(t2->getd())) return t1;
  // 0 MOD finite = dividend (positive or negative zero)
  // Not valid for: NaN MOD any; any MOD nan; 0 MOD 0; or for 0 MOD NaN
  // NaNs are handled previously.
  if( !(t2->getd() == 0.0) ) {
    if( t1->getd() == 0.0 && g_isfinite(t2->getd()) ) {
      return t1;
    }
  }

  // X MOD X is 0
  // does not work for variables because of NaN's
  if( phase->eqv( in(1), in(2) ) && t1->base() == Type::DoubleCon ) 
    if (!g_isnan(t1->getd()) && t1->getd() != 0.0)
      return TypeD::ZERO;
 

  // If both numbers are not constants, we know nothing.
  if( (t1->base() != Type::DoubleCon) || (t2->base() != Type::DoubleCon) )
    return Type::DOUBLE;
  
  // We must be modulo'ing 2 double constants.
  return TypeD::make( fmod( t1->getd(), t2->getd() ) );
}

