#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)assembler_sparc.inline.hpp	1.64 04/03/15 17:21:03 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Displacement

inline void Displacement::print() const {
  Assembler::print_instruction(data());
}


inline int Displacement::patched_branch(int dest_pos, int inst_pos) const {
  return Assembler::patched_branch(dest_pos, data(), inst_pos);
}


inline int Displacement::dest(int pos) const {
  return Assembler::branch_destination(data(), pos);
}


inline bool Address::is_simm13(int offset) { return Assembler::is_simm13(disp() + offset); }


// inlines for SPARC assembler -- dmu 5/97

inline void Assembler::check_delay() {
# ifdef CHECK_DELAY
  guarantee( delay_state != at_delay_slot, "must say delayed() when filling delay slot");
  delay_state = no_delay;
# endif
}

inline void Assembler::emit_long(int x) {
  check_delay();
  AbstractAssembler::emit_long(x);
}

inline void Assembler::emit_data(int x, relocInfo::relocType rtype) {
  relocate(rtype);
  emit_long(x);
}

inline void Assembler::emit_data(int x, RelocationHolder const& rspec) {
  relocate(rspec);
  emit_long(x);
}


inline void Assembler::add(    Register s1, Register s2, Register d )                             { emit_long( op(arith_op) | rd(d) | op3(add_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::add(    Register s1, int simm13a, Register d, relocInfo::relocType rtype ) { emit_data( op(arith_op) | rd(d) | op3(add_op3) | rs1(s1) | immed(true) | simm(simm13a, 13), rtype ); }
inline void Assembler::add(    Register s1, int simm13a, Register d, RelocationHolder const& rspec ) { emit_data( op(arith_op) | rd(d) | op3(add_op3) | rs1(s1) | immed(true) | simm(simm13a, 13), rspec ); }
inline void Assembler::add(    const Address& a, Register d, int offset) { add( a.base(), a.disp() + offset, d, a.rspec(offset)); }
  
inline void Assembler::bpr( RCondition c, bool a, Predict p, Register s1, address d, relocInfo::relocType rt ) { v9_only();  emit_data( op(branch_op) | annul(a) | cond(c) | op2(bpr_op2) | wdisp16(intptr_t(d), intptr_t(pc())) | predict(p) | rs1(s1), rt);  has_delay_slot(); }
inline void Assembler::bpr( RCondition c, bool a, Predict p, Register s1, Label& L) { bpr( c, a, p, s1, target(L)); }
      
inline void Assembler::fb( Condition c, bool a, address d, relocInfo::relocType rt ) { v9_dep();  emit_data( op(branch_op) | annul(a) | cond(c) | op2(fb_op2) | wdisp(intptr_t(d), intptr_t(pc()), 22), rt);  has_delay_slot(); }
inline void Assembler::fb( Condition c, bool a, Label& L ) { fb(c, a, target(L)); }
    
inline void Assembler::fbp( Condition c, bool a, CC cc, Predict p, address d, relocInfo::relocType rt ) { v9_only();  emit_data( op(branch_op) | annul(a) | cond(c) | op2(fbp_op2) | branchcc(cc) | predict(p) | wdisp(intptr_t(d), intptr_t(pc()), 19), rt);  has_delay_slot(); }
inline void Assembler::fbp( Condition c, bool a, CC cc, Predict p, Label& L ) { fbp(c, a, cc, p, target(L)); }

inline void Assembler::cb( Condition c, bool a, address d, relocInfo::relocType rt ) { v8_only();  emit_data( op(branch_op) | annul(a) | cond(c) | op2(cb_op2) | wdisp(intptr_t(d), intptr_t(pc()), 22), rt);  has_delay_slot(); }
inline void Assembler::cb( Condition c, bool a, Label& L ) { cb(c, a, target(L)); }
    
inline void Assembler::br( Condition c, bool a, address d, relocInfo::relocType rt ) { v9_dep();   emit_data( op(branch_op) | annul(a) | cond(c) | op2(br_op2) | wdisp(intptr_t(d), intptr_t(pc()), 22), rt);  has_delay_slot(); }
inline void Assembler::br( Condition c, bool a, Label& L ) { br(c, a, target(L)); }
    
inline void Assembler::bp( Condition c, bool a, CC cc, Predict p, address d, relocInfo::relocType rt ) { v9_only();  emit_data( op(branch_op) | annul(a) | cond(c) | op2(bp_op2) | branchcc(cc) | predict(p) | wdisp(intptr_t(d), intptr_t(pc()), 19), rt);  has_delay_slot(); }
inline void Assembler::bp( Condition c, bool a, CC cc, Predict p, Label& L ) { bp(c, a, cc, p, target(L)); }

inline void Assembler::call( address d,  relocInfo::relocType rt ) { emit_data( op(call_op) | wdisp(intptr_t(d), intptr_t(pc()), 30), rt);  has_delay_slot(); assert(rt != relocInfo::virtual_call_type, "must use virtual_call_Relocation::spec"); }
inline void Assembler::call( Label& L,   relocInfo::relocType rt ) { call( target(L), rt); }

inline void Assembler::flush( Register s1, Register s2) { emit_long( op(arith_op) | op3(flush_op3) | rs1(s1) | rs2(s2)); }
inline void Assembler::flush( Register s1, int simm13a) { emit_data( op(arith_op) | op3(flush_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::jmpl( Register s1, Register s2, Register d                          ) { emit_long( op(arith_op) | rd(d) | op3(jmpl_op3) | rs1(s1) | rs2(s2));  has_delay_slot(); }
inline void Assembler::jmpl( Register s1, int simm13a, Register d, RelocationHolder const& rspec ) { emit_data( op(arith_op) | rd(d) | op3(jmpl_op3) | rs1(s1) | immed(true) | simm(simm13a, 13), rspec);  has_delay_slot(); }

inline void Assembler::jmpl( Address& a, Register d, int offset) { jmpl( a.base(), a.disp() + offset, d, a.rspec(offset)); }


inline void Assembler::ldf(    FloatRegisterImpl::Width w, Register s1, Register s2, FloatRegister d) { emit_long( op(ldst_op) | fd(d, w) | alt_op3(ldf_op3, w) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldf(    FloatRegisterImpl::Width w, Register s1, int simm13a, FloatRegister d) { emit_data( op(ldst_op) | fd(d, w) | alt_op3(ldf_op3, w) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::ldf(    FloatRegisterImpl::Width w, const Address& a, FloatRegister d, int offset) { relocate(a.rspec(offset)); ldf( w, a.base(), a.disp() + offset, d); }

inline void Assembler::ldfsr(  Register s1, Register s2) { v9_dep();   emit_long( op(ldst_op) |             op3(ldfsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldfsr(  Register s1, int simm13a) { v9_dep();   emit_data( op(ldst_op) |             op3(ldfsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::ldxfsr( Register s1, Register s2) { v9_only();  emit_long( op(ldst_op) | rd(G1)    | op3(ldfsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldxfsr( Register s1, int simm13a) { v9_only();  emit_data( op(ldst_op) | rd(G1)    | op3(ldfsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
  
inline void Assembler::ldc(   Register s1, Register s2, int crd) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(ldc_op3  ) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldc(   Register s1, int simm13a, int crd) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(ldc_op3  ) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::lddc(  Register s1, Register s2, int crd) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(lddc_op3 ) | rs1(s1) | rs2(s2) ); }
inline void Assembler::lddc(  Register s1, int simm13a, int crd) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(lddc_op3 ) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::ldcsr( Register s1, Register s2, int crd) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(ldcsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldcsr( Register s1, int simm13a, int crd) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(ldcsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::ldsb(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(ldsb_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldsb(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(ldsb_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::ldsh(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(ldsh_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldsh(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(ldsh_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::ldsw(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(ldsw_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldsw(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(ldsw_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::ldub(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(ldub_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldub(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(ldub_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::lduh(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(lduh_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::lduh(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(lduh_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::lduw(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(lduw_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::lduw(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(lduw_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::ldx(   Register s1, Register s2, Register d) { v9_only();  emit_long( op(ldst_op) | rd(d) | op3(ldx_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldx(   Register s1, int simm13a, Register d) { v9_only();  emit_data( op(ldst_op) | rd(d) | op3(ldx_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::ldd(   Register s1, Register s2, Register d) { v9_dep(); assert(d->is_even(), "not even"); emit_long( op(ldst_op) | rd(d) | op3(ldd_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldd(   Register s1, int simm13a, Register d) { v9_dep(); assert(d->is_even(), "not even"); emit_data( op(ldst_op) | rd(d) | op3(ldd_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
      
#ifdef _LP64
// Make all 32 bit loads signed so 64 bit registers maintain proper sign 
inline void Assembler::ld(  Register s1, Register s2, Register d) { ldsw( s1, s2, d); }
inline void Assembler::ld(  Register s1, int simm13a, Register d) { ldsw( s1, simm13a, d); }
#else
inline void Assembler::ld(  Register s1, Register s2, Register d) { lduw( s1, s2, d); }
inline void Assembler::ld(  Register s1, int simm13a, Register d) { lduw( s1, simm13a, d); }
#endif


inline void Assembler::ld(   const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ld(   a.base(), a.disp() + offset, d ); }
inline void Assembler::ldsb( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldsb( a.base(), a.disp() + offset, d ); }
inline void Assembler::ldsh( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldsh( a.base(), a.disp() + offset, d ); }
inline void Assembler::ldsw( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldsw( a.base(), a.disp() + offset, d ); }
inline void Assembler::ldub( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldub( a.base(), a.disp() + offset, d ); }
inline void Assembler::lduh( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); lduh( a.base(), a.disp() + offset, d ); }
inline void Assembler::lduw( const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); lduw( a.base(), a.disp() + offset, d ); }
inline void Assembler::ldd(  const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldd(  a.base(), a.disp() + offset, d ); }
inline void Assembler::ldx(  const Address& a, Register d, int offset ) { relocate(a.rspec(offset)); ldx(  a.base(), a.disp() + offset, d ); }


inline void Assembler::ldstub(  Register s1, Register s2, Register d) { emit_long( op(ldst_op) | rd(d) | op3(ldstub_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::ldstub(  Register s1, int simm13a, Register d) { emit_data( op(ldst_op) | rd(d) | op3(ldstub_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }


inline void Assembler::prefetch(   Register s1, Register s2, PrefetchFcn f) { v9_only();  emit_long( op(ldst_op) | fcn(f) | op3(prefetch_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::prefetch(   Register s1, int simm13a, PrefetchFcn f) { v9_only();  emit_data( op(ldst_op) | fcn(f) | op3(prefetch_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::rett( Register s1, Register s2                         ) { emit_long( op(arith_op) | op3(rett_op3) | rs1(s1) | rs2(s2));  has_delay_slot(); }
inline void Assembler::rett( Register s1, int simm13a, relocInfo::relocType rt) { emit_data( op(arith_op) | op3(rett_op3) | rs1(s1) | immed(true) | simm(simm13a, 13), rt);  has_delay_slot(); }

inline void Assembler::sethi( int imm22a, Register d, RelocationHolder const& rspec ) { emit_data( op(branch_op) | rd(d) | op2(sethi_op2) | hi22(imm22a), rspec); }

  
  // pp 222
  
inline void Assembler::stf(    FloatRegisterImpl::Width w, FloatRegister d, Register s1, Register s2) { emit_long( op(ldst_op) | fd(d, w) | alt_op3(stf_op3, w) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stf(    FloatRegisterImpl::Width w, FloatRegister d, Register s1, int simm13a) { emit_data( op(ldst_op) | fd(d, w) | alt_op3(stf_op3, w) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::stf(    FloatRegisterImpl::Width w, FloatRegister d, const Address& a, int offset) { relocate(a.rspec(offset)); stf(w, d, a.base(), a.disp() + offset); }

inline void Assembler::stfsr(  Register s1, Register s2) { v9_dep();   emit_long( op(ldst_op) |             op3(stfsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stfsr(  Register s1, int simm13a) { v9_dep();   emit_data( op(ldst_op) |             op3(stfsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::stxfsr( Register s1, Register s2) { v9_only();  emit_long( op(ldst_op) | rd(G1)    | op3(stfsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stxfsr( Register s1, int simm13a) { v9_only();  emit_data( op(ldst_op) | rd(G1)    | op3(stfsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

  // p 226
  
inline void Assembler::stb(  Register d, Register s1, Register s2) { emit_long( op(ldst_op) | rd(d) | op3(stb_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stb(  Register d, Register s1, int simm13a) { emit_data( op(ldst_op) | rd(d) | op3(stb_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::sth(  Register d, Register s1, Register s2) { emit_long( op(ldst_op) | rd(d) | op3(sth_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::sth(  Register d, Register s1, int simm13a) { emit_data( op(ldst_op) | rd(d) | op3(sth_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::stw(  Register d, Register s1, Register s2) { emit_long( op(ldst_op) | rd(d) | op3(stw_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stw(  Register d, Register s1, int simm13a) { emit_data( op(ldst_op) | rd(d) | op3(stw_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }


inline void Assembler::stx(  Register d, Register s1, Register s2) { v9_only();  emit_long( op(ldst_op) | rd(d) | op3(stx_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stx(  Register d, Register s1, int simm13a) { v9_only();  emit_data( op(ldst_op) | rd(d) | op3(stx_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::std(  Register d, Register s1, Register s2) { v9_dep(); assert(d->is_even(), "not even"); emit_long( op(ldst_op) | rd(d) | op3(std_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::std(  Register d, Register s1, int simm13a) { v9_dep(); assert(d->is_even(), "not even"); emit_data( op(ldst_op) | rd(d) | op3(std_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::st(  Register d, Register s1, Register s2) { stw(d, s1, s2); }
inline void Assembler::st(  Register d, Register s1, int simm13a) { stw(d, s1, simm13a); }

inline void Assembler::stb( Register d, const Address& a, int offset) { relocate(a.rspec(offset)); stb( d, a.base(), a.disp() + offset); }
inline void Assembler::sth( Register d, const Address& a, int offset) { relocate(a.rspec(offset)); sth( d, a.base(), a.disp() + offset); }
inline void Assembler::stw( Register d, const Address& a, int offset) { relocate(a.rspec(offset)); stw( d, a.base(), a.disp() + offset); }
inline void Assembler::st(  Register d, const Address& a, int offset) { relocate(a.rspec(offset)); st(  d, a.base(), a.disp() + offset); }
inline void Assembler::std( Register d, const Address& a, int offset) { relocate(a.rspec(offset)); std( d, a.base(), a.disp() + offset); }
inline void Assembler::stx( Register d, const Address& a, int offset) { relocate(a.rspec(offset)); stx( d, a.base(), a.disp() + offset); }

// v8 p 99

inline void Assembler::stc(    int crd, Register s1, Register s2) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(stc_op3 ) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stc(    int crd, Register s1, int simm13a) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(stc_op3 ) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::stdc(   int crd, Register s1, Register s2) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(stdc_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stdc(   int crd, Register s1, int simm13a) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(stdc_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::stcsr(  int crd, Register s1, Register s2) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(stcsr_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stcsr(  int crd, Register s1, int simm13a) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(stcsr_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }
inline void Assembler::stdcq(  int crd, Register s1, Register s2) { v8_only();  emit_long( op(ldst_op) | fcn(crd) | op3(stdcq_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::stdcq(  int crd, Register s1, int simm13a) { v8_only();  emit_data( op(ldst_op) | fcn(crd) | op3(stdcq_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }


// pp 231
  
inline void Assembler::swap(    Register s1, Register s2, Register d) { v9_dep();  emit_long( op(ldst_op) | rd(d) | op3(swap_op3) | rs1(s1) | rs2(s2) ); }
inline void Assembler::swap(    Register s1, int simm13a, Register d) { v9_dep();  emit_data( op(ldst_op) | rd(d) | op3(swap_op3) | rs1(s1) | immed(true) | simm(simm13a, 13)); }

inline void Assembler::swap(    Address& a, Register d, int offset ) { relocate(a.rspec(offset)); swap(  a.base(), a.disp() + offset, d ); }


// Use the right loads/stores for the platform
inline void MacroAssembler::ld_ptr( Register s1, Register s2, Register d ) {
#ifdef _LP64
  Assembler::ldx( s1, s2, d); 
#else
  Assembler::ld(  s1, s2, d); 
#endif
}

inline void MacroAssembler::ld_ptr( Register s1, int simm13a, Register d ) {
#ifdef _LP64
  Assembler::ldx( s1, simm13a, d); 
#else
  Assembler::ld(  s1, simm13a, d); 
#endif
}

inline void MacroAssembler::ld_ptr( const Address& a, Register d, int offset ) { 
#ifdef _LP64
  Assembler::ldx(  a, d, offset );
#else
  Assembler::ld(   a, d, offset );
#endif
}

inline void MacroAssembler::st_ptr( Register d, Register s1, Register s2 ) {
#ifdef _LP64
  Assembler::stx( d, s1, s2); 
#else
  Assembler::st( d, s1, s2); 
#endif
}

inline void MacroAssembler::st_ptr( Register d, Register s1, int simm13a ) {
#ifdef _LP64
  Assembler::stx( d, s1, simm13a); 
#else
  Assembler::st( d, s1, simm13a); 
#endif
}

inline void MacroAssembler::st_ptr(  Register d, const Address& a, int offset) { 
#ifdef _LP64
  Assembler::stx(  d, a, offset);
#else
  Assembler::st(  d, a, offset);
#endif
}

// Use the right loads/stores for the platform
inline void MacroAssembler::ld_long( Register s1, Register s2, Register d ) { 
#ifdef _LP64
  Assembler::ldx(s1, s2, d); 
#else
  Assembler::ldd(s1, s2, d); 
#endif
}

inline void MacroAssembler::ld_long( Register s1, int simm13a, Register d ) { 
#ifdef _LP64
  Assembler::ldx(s1, simm13a, d); 
#else
  Assembler::ldd(s1, simm13a, d); 
#endif
}

inline void MacroAssembler::ld_long( const Address& a, Register d, int offset ) { 
#ifdef _LP64
  Assembler::ldx(a, d, offset );
#else
  Assembler::ldd(a, d, offset );
#endif
}

inline void MacroAssembler::st_long( Register d, Register s1, Register s2 ) {
#ifdef _LP64
  Assembler::stx(d, s1, s2); 
#else
  Assembler::std(d, s1, s2); 
#endif
}

inline void MacroAssembler::st_long( Register d, Register s1, int simm13a ) {
#ifdef _LP64
  Assembler::stx(d, s1, simm13a); 
#else
  Assembler::std(d, s1, simm13a); 
#endif
}

inline void MacroAssembler::st_long( Register d, const Address& a, int offset ) { 
#ifdef _LP64
  Assembler::stx(d, a, offset);
#else
  Assembler::std(d, a, offset);
#endif
}

// Functions for isolating 64 bit shifts for LP64

inline void MacroAssembler::sll_ptr( Register s1, Register s2, Register d ) {
#ifdef _LP64
  Assembler::sllx(s1, s2, d);
#else
  Assembler::sll(s1, s2, d);
#endif
}

inline void MacroAssembler::sll_ptr( Register s1, int imm6a,   Register d ) {
#ifdef _LP64
  Assembler::sllx(s1, imm6a, d);
#else
  Assembler::sll(s1, imm6a, d);
#endif
}

inline void MacroAssembler::srl_ptr( Register s1, Register s2, Register d ) {
#ifdef _LP64
  Assembler::srlx(s1, s2, d);
#else
  Assembler::srl(s1, s2, d);
#endif
}

inline void MacroAssembler::srl_ptr( Register s1, int imm6a,   Register d ) {
#ifdef _LP64
  Assembler::srlx(s1, imm6a, d);
#else
  Assembler::srl(s1, imm6a, d);
#endif
}

// Use the right branch for the platform

inline void MacroAssembler::br( Condition c, bool a, Predict p, address d, relocInfo::relocType rt ) {
  if (VM_Version::v9_instructions_work())             
    Assembler::bp(c, a, icc, p, d, rt);
  else          
    Assembler::br(c, a, d, rt); 
}

inline void MacroAssembler::br( Condition c, bool a, Predict p, Label& L ) { 
  br(c, a, p, target(L)); 
}


// Branch that tests either xcc or icc depending on the
// architecture compiled (LP64 or not)
inline void MacroAssembler::brx( Condition c, bool a, Predict p, address d, relocInfo::relocType rt ) {
#ifdef _LP64
    Assembler::bp(c, a, xcc, p, d, rt);
#else
    MacroAssembler::br(c, a, p, d, rt);
#endif
}

inline void MacroAssembler::brx( Condition c, bool a, Predict p, Label& L ) { 
  brx(c, a, p, target(L)); 
}

inline void MacroAssembler::ba( bool a, Label& L ) { 
  br(always, a, pt, L); 
}

// Warning: V9 only functions
inline void MacroAssembler::bp( Condition c, bool a, CC cc, Predict p, address d, relocInfo::relocType rt ) {
  Assembler::bp(c, a, cc, p, d, rt);
}

inline void MacroAssembler::bp( Condition c, bool a, CC cc, Predict p, Label& L ) {
  Assembler::bp(c, a, cc, p, L);
}

inline void MacroAssembler::fb( Condition c, bool a, Predict p, address d, relocInfo::relocType rt ) {
  if (VM_Version::v9_instructions_work())
    fbp(c, a, fcc0, p, d, rt);
  else
    Assembler::fb(c, a, d, rt); 
}

inline void MacroAssembler::fb( Condition c, bool a, Predict p, Label& L ) { 
  fb(c, a, p, target(L)); 
}

inline void MacroAssembler::fbp( Condition c, bool a, CC cc, Predict p, address d, relocInfo::relocType rt ) {
  Assembler::fbp(c, a, cc, p, d, rt);
}

inline void MacroAssembler::fbp( Condition c, bool a, CC cc, Predict p, Label& L ) {
  Assembler::fbp(c, a, cc, p, L);
}

inline void MacroAssembler::jmp( Register s1, Register s2 ) { jmpl( s1, s2, G0 ); }
inline void MacroAssembler::jmp( Register s1, int simm13a, RelocationHolder const& rspec ) { jmpl( s1, simm13a, G0, rspec); }

// Call with a check to see if we need to deal with the added
// expense of relocation and if we overflow the displacement 
// of the quick call instruction./
// Check to see if we have to deal with relocations
inline void MacroAssembler::call( address d, relocInfo::relocType rt ) { 
#ifdef _LP64
  intptr_t disp;
  // NULL is ok because it will be relocated later.
  // Must change NULL to a reachable address in order to 
  // pass asserts here and in wdisp.
  if ( d == NULL )
    d = pc();

  // Is this address within range of the call instruction?
  // If not, use the expensive instruction sequence
  disp = (intptr_t)d - (intptr_t)pc();
  if ( disp != (intptr_t)(int32_t)disp ) {
    relocate(rt);
    Address dest(O7, (address)d);
    sethi(dest, /*ForceRelocatable=*/ true);
    jmpl(dest, O7);
  }
  else {
    Assembler::call( d, rt );
  }
#else
  Assembler::call( d, rt );
#endif
}

inline void MacroAssembler::call( Label& L,   relocInfo::relocType rt ) {
  MacroAssembler::call( target(L), rt); 
}



inline void MacroAssembler::callr( Register s1, Register s2 ) { jmpl( s1, s2, O7 ); }
inline void MacroAssembler::callr( Register s1, int simm13a, RelocationHolder const& rspec ) { jmpl( s1, simm13a, O7, rspec); }

inline void MacroAssembler::trapx(Condition c, Register s1, int trapa) {
#ifdef _LP64
  trap( c, Assembler::xcc, s1, trapa );
#else
  trap( c, Assembler::icc, s1, trapa );
#endif
}

// prefetch instruction
inline void MacroAssembler::iprefetch( address d, relocInfo::relocType rt ) {
  if (VM_Version::v9_instructions_work())
    Assembler::bp( never, true, xcc, pt, d, rt );
}
inline void MacroAssembler::iprefetch( Label& L) { iprefetch( target(L) ); }


// clobbers o7 on V8!!
// returns delta from gotten pc to addr after
inline int MacroAssembler::get_pc( Register d ) {
  int x = offset();
  if (VM_Version::v9_instructions_work())
    rdpc(d);
  else {
    Label lbl;
    Assembler::call(lbl, relocInfo::none);  // No relocation as this is call to pc+0x8
    if (d == O7)  delayed()->nop();
    else          delayed()->mov(O7, d);
    bind(lbl);
  }
  return offset() - x;
}



inline void MacroAssembler::sethi( intptr_t imm22a, 
				   Register d, 
				   bool ForceRelocatable,
				   RelocationHolder const& rspec ) {
  Address adr( d, (address)imm22a, rspec );
  MacroAssembler::sethi( adr, ForceRelocatable );
}


inline void MacroAssembler::sethi( Address& a, bool ForceRelocatable ) {
  address save_pc;
  int shiftcnt;
  // if addr of local, do not need to load it
  assert(a.base() != FP  &&  a.base() != SP, "just use ld or st for locals");
#ifdef _LP64
# ifdef CHECK_DELAY
  assert_not_delayed( (char *)"cannot put two instructions in delay slot" );
# endif
  v9_dep();
//  ForceRelocatable = 1;
  save_pc = pc();
  if (a.hi32() == 0 && a.low32() >= 0) {
    Assembler::sethi(a.low32(), a.base(), a.rspec());
  }
  else if (a.hi32() == -1) {
    Assembler::sethi(~a.low32(), a.base(), a.rspec());
    xor3(a.base(), ~low10(~0), a.base());
  }
  else {
    Assembler::sethi(a.hi32(), a.base(), a.rspec() );	// 22
    if ( a.hi32() & 0x3ff )			// Any bits?
      or3( a.base(), a.hi32() & 0x3ff ,a.base() ); // High 32 bits are now in low 32 
    if ( a.low32() & 0xFFFFFC00 ) {		// done?
      if( (a.low32() >> 20) & 0xfff ) {		// Any bits set?
        sllx(a.base(), 12, a.base());		// Make room for next 12 bits  
        or3( a.base(), (a.low32() >> 20) & 0xfff,a.base() ); // Or in next 12
        shiftcnt = 0;				// We already shifted
      }
      else
	shiftcnt = 12;
      if( (a.low32() >> 10) & 0x3ff ) { 		
        sllx(a.base(), shiftcnt+10, a.base());// Make room for last 10 bits
        or3( a.base(), (a.low32() >> 10) & 0x3ff,a.base() ); // Or in next 10 
        shiftcnt = 0;
      }
      else
	shiftcnt = 10;
      sllx(a.base(), shiftcnt+10 , a.base());		// Shift leaving disp field 0'd
    }
    else 
      sllx( a.base(), 32, a.base() );
  }
  // Pad out the instruction sequence so it can be 
  // patched later.
  if ( ForceRelocatable || (a.rtype() != relocInfo::none && 
                            a.rtype() != relocInfo::runtime_call_type) ) {
    while ( pc() < (save_pc + (7 * BytesPerInstWord )) )
      nop();
  }
#else
  Assembler::sethi(a.hi(), a.base(), a.rspec());
#endif

}
    
inline int MacroAssembler::size_of_sethi( address a, bool worst_case) {
#ifdef _LP64
  if (worst_case) return 7;
  intptr_t iaddr = (intptr_t)a;
  int hi32 = (int)(iaddr >> 32);
  int lo32 = (int)(iaddr);
  int inst_count;
  if (hi32 == 0 && lo32 >= 0)
    inst_count = 1;
  else if (hi32 == -1)
    inst_count = 2;
  else {
    inst_count = 2;
    if ( hi32 & 0x3ff )
      inst_count++;
    if ( lo32 & 0xFFFFFC00 ) {
      if( (lo32 >> 20) & 0xfff ) inst_count += 2;
      if( (lo32 >> 10) & 0x3ff ) inst_count += 2;
    }
  }
  return BytesPerInstWord * inst_count;
#else
  return BytesPerInstWord;
#endif
}

inline int MacroAssembler::worst_case_size_of_set() {
  return size_of_sethi(NULL, true) + 1;
}

inline void MacroAssembler::set( intptr_t value, Register d, RelocationHolder const& rspec ) {
  Address val( d, (address)value, rspec);

  if ( rspec.type() == relocInfo::none ) {
    // can optimize
    if (-4096 <= value  &&  value <= 4095) {
      or3(G0, value, d); // setsw (this leaves upper 32 bits sign-extended)
      return;
    }
    if (inv_hi22(hi22(value)) == value) {
      sethi(val);		
      return;
    }
  }
  assert_not_delayed( (char *)"cannot put two instructions in delay slot" );
  sethi( val );
  add( d, value &  0x3ff, d, rspec);
}


inline void MacroAssembler::setsw( int value, Register d, RelocationHolder const& rspec ) {
  Address val( d, (address)value, rspec);
  if ( rspec.type() == relocInfo::none ) {
    // can optimize
    if (-4096 <= value  &&  value <= 4095) {
      or3(G0, value, d);
      return;
    }
    if (inv_hi22(hi22(value)) == value) {
      sethi( val );
#ifndef _LP64
      if ( value < 0 ) {
        assert_not_delayed();
        sra (d, G0, d);
      }
#endif
      return;
    }
  }
  assert_not_delayed();
  sethi( val );
  add( d, value &  0x3ff, d, rspec);

  // (A negative value could be loaded in 2 insns with sethi/xor,
  // but it would take a more complex relocation.)
#ifndef _LP64
  if ( value < 0)
    sra(d, G0, d);
#endif
}


inline void MacroAssembler::load_address( Address& a, int offset ) {
  assert_not_delayed();
#ifdef _LP64
  sethi(a);
  add(a, a.base(), offset);
#else
  if (a.hi() == 0 && a.rtype() == relocInfo::none) {
    set(a.disp() + offset, a.base());
  } 
  else {
    sethi(a);
    add(a, a.base(), offset);
  }
#endif
}


inline void MacroAssembler::split_disp( Address& a, Register temp ) {
  assert_not_delayed();
  a = a.split_disp();
  Assembler::sethi(a.hi(), temp, a.rspec());
  add(a.base(), temp, a.base());
}


inline void MacroAssembler::load_contents( Address& a, Register d, int offset ) {
  assert_not_delayed();
  sethi(a);
  ld(a, d, offset);
}


inline void MacroAssembler::load_ptr_contents( Address& a, Register d, int offset ) {
  assert_not_delayed();
  sethi(a);
  ld_ptr(a, d, offset);
}


inline void MacroAssembler::store_contents( Register s, Address& a, int offset ) {
  assert_not_delayed();
  sethi(a);
  st(s, a, offset);
}


inline void MacroAssembler::store_ptr_contents( Register s, Address& a, int offset ) {
  assert_not_delayed();
  sethi(a);
  st_ptr(s, a, offset);
}


// This code sequence is relocatable to any address, even on LP64.
inline void MacroAssembler::jumpl_to( Address& a, Register d, int offset ) {
  assert_not_delayed();
  // Force fixed length sethi because NativeJump and NativeFarCall don't handle
  // variable length instruction streams.
  sethi(a, /*ForceRelocatable=*/ true);
  jmpl(a, d, offset);
}


inline void MacroAssembler::jump_to( Address& a, int offset ) {
  jumpl_to( a, G0, offset );
}

 
inline Address MacroAssembler::internal_address( Register d, Label& L ) {
  assert(L.is_bound(), "must be previously defined");
  return Address(d, target(L), relocInfo::internal_word_type);
}


inline void MacroAssembler::set_oop( jobject obj, Register d ) {
  set_oop(allocate_oop_address(obj, d));
}


inline void MacroAssembler::set_oop_constant( jobject obj, Register d ) {
  set_oop(constant_oop_address(obj, d));
}


inline void MacroAssembler::set_oop( Address obj_addr ) {
  assert(obj_addr.rspec().type()==relocInfo::oop_type, "must be an oop reloc");
  load_address(obj_addr);
}


inline void MacroAssembler::load_argument( Argument& a, Register  d ) {
  if (a.is_register())
    mov(a.as_register(), d);
  else
    ld (a.as_address(),  d);
}

inline void MacroAssembler::store_argument( Register s, Argument& a ) {
  if (a.is_register())
    mov(s, a.as_register());
  else
    st_ptr (s, a.as_address());		// ABI says everything is right justified.
}
 
inline void MacroAssembler::store_ptr_argument( Register s, Argument& a ) {
  if (a.is_register())
    mov(s, a.as_register());
  else
    st_ptr (s, a.as_address());
}


#ifdef _LP64
inline void MacroAssembler::store_float_argument( FloatRegister s, Argument& a ) {
  if (a.is_float_register())
// V9 ABI has F1, F3, F5 are used to pass instead of O0, O1, O2
    fmov(FloatRegisterImpl::S, s, a.as_float_register() );
  else
    // Floats are stored in the high half of the stack entry 
    // The low half is undefined per the ABI.
    stf(FloatRegisterImpl::S, s, a.as_address(), sizeof(jfloat));
}

inline void MacroAssembler::store_double_argument( FloatRegister s, Argument& a ) {
  if (a.is_float_register())
// V9 ABI has D0, D2, D4 are used to pass instead of O0, O1, O2
    fmov(FloatRegisterImpl::D, s, a.as_double_register() );
  else
    stf(FloatRegisterImpl::D, s, a.as_address());
}
 
inline void MacroAssembler::store_long_argument( Register s, Argument& a ) {
  if (a.is_register())
    mov(s, a.as_register());
  else
    stx(s, a.as_address());
}
#endif

inline void MacroAssembler::clrb( Register s1, Register s2) {  stb( G0, s1, s2 ); }
inline void MacroAssembler::clrh( Register s1, Register s2) {  sth( G0, s1, s2 ); }
inline void MacroAssembler::clr(  Register s1, Register s2) {  stw( G0, s1, s2 ); }
inline void MacroAssembler::clrx( Register s1, Register s2) {  stx( G0, s1, s2 ); }

inline void MacroAssembler::clrb( Register s1, int simm13a) { stb( G0, s1, simm13a); }
inline void MacroAssembler::clrh( Register s1, int simm13a) { sth( G0, s1, simm13a); }
inline void MacroAssembler::clr(  Register s1, int simm13a) { stw( G0, s1, simm13a); }
inline void MacroAssembler::clrx( Register s1, int simm13a) { stx( G0, s1, simm13a); }

// returns if membar generates anything, obviously this code should mirror
// membar below.
inline bool MacroAssembler::membar_has_effect( Membar_mask_bits const7a ) {
  if( !os::is_MP() ) return false;  // Not needed on single CPU
  if( VM_Version::v9_instructions_work() ) {
    const Membar_mask_bits effective_mask =
	Membar_mask_bits(const7a & ~(LoadLoad | LoadStore | StoreStore));
    return (effective_mask != 0);
  } else {
    return true;
  }
}

inline void MacroAssembler::membar( Membar_mask_bits const7a ) {
  // Uniprocessors do not need memory barriers
  if (!os::is_MP()) return;
  // Weakened for current Sparcs and TSO.  See the v9 manual, sections 8.4.3,
  // 8.4.4.3, a.31 and a.50.
  if( VM_Version::v9_instructions_work() ) {
    // Under TSO, setting bit 3, 2, or 0 is redundant, so the only value
    // of the mmask subfield of const7a that does anything that isn't done
    // implicitly is StoreLoad.
    const Membar_mask_bits effective_mask =
	Membar_mask_bits(const7a & ~(LoadLoad | LoadStore | StoreStore));
    if ( effective_mask != 0 ) {
      Assembler::membar( effective_mask );
    }
  } else {
    // stbar is the closest there is on v8.  Equivalent to membar(StoreStore).  We
    // do not issue the stbar because to my knowledge all v8 machines implement TSO,
    // which guarantees that all stores behave as if an stbar were issued just after
    // each one of them.  On these machines, stbar ought to be a nop.  There doesn't
    // appear to be an equivalent of membar(StoreLoad) on v8: TSO doesn't require it,
    // it can't be specified by stbar, nor have I come up with a way to simulate it.
    //
    // Addendum.  Dice says that ldstub guarantees a write buffer flush to coherent
    // space.  Put one here to be on the safe side.
    Assembler::ldstub(SP, 0, G0);
  }
}
