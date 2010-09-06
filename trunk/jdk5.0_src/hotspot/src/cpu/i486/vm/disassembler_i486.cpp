#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)disassembler_i486.cpp	1.35 03/12/23 16:36:14 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

# include "incls/_precompiled.incl"
# include "incls/_disassembler_i486.cpp.incl"

#ifndef PRODUCT

void*    Disassembler::_library            = NULL;
Disassembler::decode_func Disassembler::_decode_instruction = NULL;

bool Disassembler::load_library() {
  if (_library == NULL) {
    char buf[1024];
    char ebuf[1024];
    sprintf(buf, "disassembler%s", os::dll_file_extension());
    _library = hpi::dll_load(buf, ebuf, sizeof ebuf);
    if (_library != NULL) {
      tty->print_cr("Loaded disassembler");
      _decode_instruction = CAST_TO_FN_PTR(Disassembler::decode_func, hpi::dll_lookup(_library, "decode_instruction"));
    }
  }
  return (_library != NULL) && (_decode_instruction != NULL);
}

class i486_env : public DisassemblerEnv {
 private:
  nmethod*      code;
  outputStream* output;
 public:
  i486_env(nmethod* rcode, outputStream* routput) {
    code   = rcode;
    output = routput;
  }
  void print_label(intptr_t value);
  void print_raw(char* str) { output->print_raw(str); }
  void print(char* format, ...);
  char* string_for_offset(intptr_t value);
  char* string_for_constant(unsigned char* pc, intptr_t value, int is_decimal);
};


void i486_env::print_label(intptr_t value) {
  address adr = (address) value;
  if (StubRoutines::contains(adr)) {
    StubCodeDesc* desc = StubCodeDesc::desc_for(adr);
    const char * desc_name = "unknown stub";
    if (desc != NULL) {
      desc_name = desc->name();
    }
    output->print("Stub::%s", desc_name);
    if (WizardMode) output->print(" " INTPTR_FORMAT, value);
  } else {
    output->print(INTPTR_FORMAT, value); 
  }
}

void i486_env::print(char* format, ...) {
  va_list ap;
  va_start(ap, format);
  output->vprint(format, ap);
  va_end(ap);
}

char* i486_env::string_for_offset(intptr_t value) {
  stringStream st;
  BarrierSet* bs = Universe::heap()->barrier_set();
  BarrierSet::Name bsn = bs->kind();
    if (bs->kind() == BarrierSet::CardTableModRef &&
	(jbyte*) value == ((CardTableModRefBS*)(bs))->byte_map_base) {
    st.print("word_map_base");
  } else {
    st.print("%d", value);
  }
  return st.as_string();
}

char* i486_env::string_for_constant(unsigned char* pc, intptr_t value, int is_decimal) {
  stringStream st;
  oop obj = NULL;
#ifndef CORE
  if (code && (obj = code->embeddedOop_at(pc))) {
    obj->print_value_on(&st);
  } else 
#endif
  {
    if (is_decimal == 1) {
      st.print("%d", value);
    } else {
      st.print("0x%lx", value);
    }
  }
  return st.as_string();
}



address Disassembler::decode_instruction(address start, DisassemblerEnv* env) {
  return ((decode_func) _decode_instruction)(start, env);
}


void Disassembler::decode(CodeBlob* cb, outputStream* st) {
#ifndef CORE
  st = st ? st : tty;
  st->print_cr("Decoding CodeBlob " INTPTR_FORMAT, cb);
  decode(cb->instructions_begin(), cb->instructions_end(), st);
#endif
}


void Disassembler::decode(u_char* begin, u_char* end, outputStream* st) {
  st = st ? st : tty;

  const int show_bytes = false; // for disassembler debugging

  if (!load_library()) {
    st->print_cr("Could not load disassembler");
    return;
  }

  i486_env env(NULL, st);
  unsigned char*  p = (unsigned char*) begin;
  while (p < (unsigned char*) end) {
    unsigned char* p0 = p;
    st->print(INTPTR_FORMAT ": ", p);
    p = decode_instruction(p, &env);
    if (show_bytes) {
      st->print("\t\t\t");
      while (p0 < p) st->print("%x ", *p0++);
    }
    st->cr();
  }
}


void Disassembler::decode(nmethod* nm, outputStream* st) {
#ifndef CORE
  st = st ? st : tty;

  st->print_cr("Decoding compiled method " INTPTR_FORMAT ":", nm);
  st->print("Code:");
  st->cr();
  
  if (!load_library()) {
    st->print_cr("Could not load disassembler");
    return;
  }
  i486_env env(nm, st);
#ifdef COMPILER1
  unsigned char* p = nm->code_begin();
#else
  unsigned char* p = nm->instructions_begin();
#endif
  unsigned char* end = nm->instructions_end();
  while (p < end) {
    if (p == nm->entry_point())             st->print_cr("[Entry Point]");
    if (p == nm->verified_entry_point())    st->print_cr("[Verified Entry Point]");
    if (p == nm->exception_begin())         st->print_cr("[Exception Handler]");
    if (p == nm->stub_begin())              st->print_cr("[Stub Code]");
    unsigned char* p0 = p;
    st->print("  " INTPTR_FORMAT ": ", p);
    p = decode_instruction(p, &env);
    nm->print_code_comment_on(st, 40, p0, p);
    st->cr();
    // Output pc bucket ticks if we have any
    address bucket_pc = FlatProfiler::bucket_start_for(p);
    if (bucket_pc != NULL && bucket_pc > p0 && bucket_pc <= p) {
      int bucket_count = FlatProfiler::bucket_count_for(bucket_pc);
      tty->print_cr("[%d]", bucket_count);
    } 
  }
#endif
}

#endif // PRODUCT

