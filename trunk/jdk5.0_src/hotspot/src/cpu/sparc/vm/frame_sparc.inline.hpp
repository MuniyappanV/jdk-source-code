#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)frame_sparc.inline.hpp	1.61 03/12/23 16:37:11 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// Inline functions for SPARC frames:

// Constructors

inline frame::frame() { _pc = NULL; _sp = NULL; _younger_sp = NULL; _interpreter_sp_adjustment = 0;}


// Accessors:

inline bool frame::equal(frame other) const {
  return sp() == other.sp()
      && fp() == other.fp()
      && pc() == other.pc();
}

// Return unique id for this frame. The id must have a value where we can distinguish
// identity and younger/older relationship. NULL represents an invalid (incomparable)
// frame.
inline intptr_t* frame::id(void) const { return unextended_sp(); }

// Relationals on frames based 
// Return true if the frame is younger (more recent activation) than the frame represented by id
inline bool frame::is_younger(intptr_t* id) const { assert(this->id() != NULL && id != NULL, "NULL frame id");
					            return this->id() < id ; }

// Return true if the frame is older (less recent activation) than the frame represented by id
inline bool frame::is_older(intptr_t* id) const   { assert(this->id() != NULL && id != NULL, "NULL frame id");
					            return this->id() > id ; }

inline int frame::frame_size() const { return sender_sp() - sp(); }

inline intptr_t* frame::link() const { return (intptr_t *)(fp()[FP->sp_offset_in_saved_window()] + STACK_BIAS); }

inline void frame::set_link(intptr_t* addr) { assert(link()==addr, "frame nesting is controlled by hardware"); }

inline intptr_t* frame::unextended_sp() const { return sp() + _interpreter_sp_adjustment; }

inline intptr_t* frame::entry_frame_argument_at(int offset) const {
  // Since an entry frame always calls the interpreter first,
  // the format of the call is always compatible with the interpreter.
  return &interpreter_frame_tos_at(offset);
}

#ifdef COMPILER2
inline intptr_t& frame::c2i_argument_at(int offset) const {
  // c2i adapter frames have a format compatible with the interpreter.
  return interpreter_frame_tos_at(offset);
}
#endif /* COMPILER2 */


// return address:

inline address  frame::sender_pc()        const    { return *I7_addr() + pc_return_offset; }

inline void     frame::set_sender_pc(address addr) { *I7_addr() = addr - pc_return_offset; }

inline address* frame::I7_addr() const  { return (address*) &sp()[ I7->sp_offset_in_saved_window()]; }  
inline address* frame::I0_addr() const  { return (address*) &sp()[ I0->sp_offset_in_saved_window()]; }  

inline address* frame::O7_addr() const  { return (address*) &younger_sp()[ I7->sp_offset_in_saved_window()]; }  
inline address* frame::O0_addr() const  { return (address*) &younger_sp()[ I0->sp_offset_in_saved_window()]; }  

inline intptr_t*    frame::sender_sp() const  { return fp(); }

// Used only in frame::oopmapreg_to_location
inline int frame::pd_oop_map_offset_adjustment() const {
  return _interpreter_sp_adjustment;
}

inline intptr_t** frame::interpreter_frame_locals_addr() const { 
  return (intptr_t**) sp_addr_at( Llocals->sp_offset_in_saved_window());
}

inline intptr_t* frame::interpreter_frame_bcx_addr() const {
  // %%%%% reinterpreting Lbcp as a bcx
  return (intptr_t*) sp_addr_at( Lbcp->sp_offset_in_saved_window());
}

#ifndef CORE
inline intptr_t* frame::interpreter_frame_mdx_addr() const {
  // %%%%% reinterpreting ImethodDataPtr as a mdx
  return (intptr_t*) sp_addr_at( ImethodDataPtr->sp_offset_in_saved_window());
}
#endif // !CORE

inline intptr_t& frame::interpreter_frame_local_at(int index) const {
    return  (*interpreter_frame_locals_addr()) [-index];
}


inline intptr_t* frame::interpreter_frame_expression_stack() const {
  return (intptr_t*)interpreter_frame_monitors() - 1;
}

inline intptr_t& frame::interpreter_frame_expression_stack_at(jint offset) const {
  return interpreter_frame_expression_stack()[-offset]; 
}

inline jint frame::interpreter_frame_expression_stack_direction() { return -1; }

// top of expression stack
inline intptr_t* frame::interpreter_frame_tos_address() const {
  return *interpreter_frame_esp_addr() + 1;
} 
inline void frame::interpreter_frame_set_tos_address( intptr_t* x ) { 
  *interpreter_frame_esp_addr() = x - 1; 
}

inline jint frame::interpreter_frame_expression_stack_size() const { 
  return interpreter_frame_expression_stack() - interpreter_frame_tos_address() + 1;
}

inline intptr_t& frame::interpreter_frame_tos_at(jint offset) const { 
  return interpreter_frame_tos_address()[offset];
}

// monitor elements

// in keeping with Intel side: end is lower in memory than begin;
// and beginning element is oldest element
// Also begin is one past last monitor.

inline BasicObjectLock* frame::interpreter_frame_monitor_begin()       const  { 
  int rounded_vm_local_words = round_to(frame::interpreter_frame_vm_local_words, WordsPerLong);
  return (BasicObjectLock *)fp_addr_at(-rounded_vm_local_words);
}

inline BasicObjectLock* frame::interpreter_frame_monitor_end()         const  { 
  return interpreter_frame_monitors();
}


inline void frame::interpreter_frame_set_monitor_end(BasicObjectLock* value) {
  interpreter_frame_set_monitors(value);
}


inline int frame::interpreter_frame_monitor_size() {
  return round_to(BasicObjectLock::size(), WordsPerLong);
}

inline methodOop* frame::interpreter_frame_method_addr() const { 
  return (methodOop*)sp_addr_at( Lmethod->sp_offset_in_saved_window());
}



// Constant pool cache

inline constantPoolCacheOop* frame::interpreter_frame_cache_addr() const {
  return (constantPoolCacheOop*)sp_addr_at( LcpoolCache->sp_offset_in_saved_window());
}


inline JavaCallWrapper* frame::entry_frame_call_wrapper() const {
  // note: adjust this code if the link argument in StubGenerator::call_stub() changes!
  const Argument link = Argument(0, false);
  return (JavaCallWrapper*)sp()[link.as_in().as_register()->sp_offset_in_saved_window()];
}


inline int frame::local_offset_for_compiler(int local_index, int nof_args, int max_nof_locals, int max_nof_monitors) {
   // always allocate non-argument locals 0..5 as if they were arguments:
  int allocated_above_frame = nof_args;
  if (allocated_above_frame < callee_register_argument_save_area_words)
    allocated_above_frame = callee_register_argument_save_area_words;
  if (allocated_above_frame > max_nof_locals)
    allocated_above_frame = max_nof_locals;

  // Note: monitors (BasicLock blocks) are never allocated in argument slots
  //assert(local_index >= 0 && local_index < max_nof_locals, "bad local index");
  if (local_index < allocated_above_frame)
    return local_index + callee_register_argument_save_area_sp_offset;
  else
    return local_index - (max_nof_locals + max_nof_monitors*2) + compiler_frame_vm_locals_fp_offset;
}

inline int frame::monitor_offset_for_compiler(int local_index, int nof_args, int max_nof_locals, int max_nof_monitors) {
  assert(local_index >= max_nof_locals && ((local_index - max_nof_locals) & 1) && (local_index - max_nof_locals) < max_nof_monitors*2, "bad monitor index");

  // The compiler uses the __higher__ of two indexes allocated to the monitor.
  // Increasing local indexes are mapped to increasing memory locations,
  // so the start of the BasicLock is associated with the __lower__ index.

  int offset = (local_index-1) - (max_nof_locals + max_nof_monitors*2) + compiler_frame_vm_locals_fp_offset;

  // We allocate monitors aligned zero mod 8:
  assert((offset & 1) == 0, "monitor must be an an even address.");
  // This works because all monitors are allocated after
  // all locals, and because the highest address corresponding to any
  // monitor index is always even.
  assert((compiler_frame_vm_locals_fp_offset & 1) == 0, "end of monitors must be even address");

  return offset;
}

inline int frame::min_local_offset_for_compiler(int nof_args, int max_nof_locals, int max_nof_monitors) {
   // always allocate non-argument locals 0..5 as if they were arguments:
  int allocated_above_frame = nof_args;
  if (allocated_above_frame < callee_register_argument_save_area_words)
    allocated_above_frame = callee_register_argument_save_area_words;
  if (allocated_above_frame > max_nof_locals)
    allocated_above_frame = max_nof_locals;

  int allocated_in_frame = (max_nof_locals + max_nof_monitors*2) - allocated_above_frame;

  return compiler_frame_vm_locals_fp_offset - allocated_in_frame;
}

// On SPARC, the %lN and %iN registers are non-volatile.
inline bool frame::volatile_across_calls(Register reg) {
  // This predicate is (presently) applied only to temporary registers,
  // and so it need not recognize non-volatile globals.
  return reg->is_out() || reg->is_global();
}

#ifndef CORE
#undef O0_NAME
#ifdef COMPILER1
#define O0_NAME (VMReg::Name(O0->encoding()))
#endif /* COMPILER1 */

#ifdef COMPILER2
#define O0_NAME (VMReg::Name(R_O0_num))
#endif /* COMPILER2 */

inline oop  frame::saved_oop_result(RegisterMap* map) const      {
  return *((oop*) map->location(O0_NAME));
}

inline void frame::set_saved_oop_result(RegisterMap* map, oop obj) {
  *((oop*) map->location(O0_NAME)) = obj;
}

#ifdef COMPILER1
inline oop  frame::saved_receiver(RegisterMap* map) const      {
  return *((oop*) map->location(O0_NAME));
}

inline void frame::set_saved_receiver(RegisterMap* map, oop obj) {
  *((oop*) map->location(O0_NAME)) = obj;
}
#endif /* COMPILER1 */

#endif /* CORE */

//Reconciliation History
// 1.12 97/11/24 10:34:54 frame_i486.inline.hpp
// 1.15 98/01/26 13:03:53 frame_i486.inline.hpp
// 1.17 98/01/26 13:03:53 frame_i486.inline.hpp
// 1.19 98/03/05 05:29:45 frame_i486.inline.hpp
// 1.19 98/03/06 16:35:13 frame_i486.inline.hpp
// 1.21 98/04/30 16:37:50 frame_i486.inline.hpp
// 1.22 98/05/07 14:20:18 frame_i486.inline.hpp
// 1.22 98/05/07 14:20:18 frame_i486.inline.hpp
// 1.23 98/06/24 16:38:47 frame_i486.inline.hpp
// 1.27 98/09/29 15:07:12 frame_i486.inline.hpp
// 1.28 98/11/09 20:04:23 frame_i486.inline.hpp
// 1.32 99/07/12 11:29:47 frame_i486.inline.hpp
// 1.33 99/08/16 13:44:02 frame_i486.inline.hpp
//End

