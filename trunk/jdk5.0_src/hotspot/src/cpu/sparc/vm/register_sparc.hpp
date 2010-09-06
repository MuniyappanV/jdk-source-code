#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)register_sparc.hpp	1.16 03/12/23 16:37:20 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// forward declaration
class Address;


// Use Register as shortcut
class RegisterImpl;
typedef RegisterImpl* Register;


// The implementation of integer registers for the SPARC architecture

class RegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    log_set_size        = 3,                          // the number of bits to encode the set register number
    number_of_sets      = 4,                          // the number of registers sets (in, local, out, global)
    number_of_registers = number_of_sets << log_set_size,

    iset_no = 3,  ibase = iset_no << log_set_size,    // the in     register set
    lset_no = 2,  lbase = lset_no << log_set_size,    // the local  register set
    oset_no = 1,  obase = oset_no << log_set_size,    // the output register set
    gset_no = 0,  gbase = gset_no << log_set_size     // the global register set
  };

  // general construction
  friend Register as_Register(int encoding)           { return (Register)encoding; }

  // set specific construction
  friend Register as_iRegister(int number)            { return as_Register(ibase + number); }
  friend Register as_lRegister(int number)            { return as_Register(lbase + number); }
  friend Register as_oRegister(int number)            { return as_Register(obase + number); }
  friend Register as_gRegister(int number)            { return as_Register(gbase + number); }

  // accessors
  int   encoding() const                              { assert(is_valid(), "invalid register"); return value(); }
  const char* name() const;

  // testers
  bool is_valid() const                               { return (0 <= (value()&0x7F) && (value()&0x7F) < number_of_registers); }
  bool is_even() const                                { return (encoding() & 1) == 0; }
  bool is_in() const                                  { return (encoding() >> log_set_size) == iset_no; }
  bool is_local() const                               { return (encoding() >> log_set_size) == lset_no; }
  bool is_out() const                                 { return (encoding() >> log_set_size) == oset_no; }
  bool is_global() const                              { return (encoding() >> log_set_size) == gset_no; }

  // derived registers, offsets, and addresses
  Register successor() const                          { return as_Register(encoding() + 1); }

  Register after_save() const {
    assert(is_out() || is_global(), "register not visible after save");
    return is_out() ? as_Register(encoding() + (ibase - obase)) : (const Register)this;
  }

  Register after_restore() const {
    assert(is_in() || is_global(), "register not visible after restore");
    return is_in() ? as_Register(encoding() + (obase - ibase)) : (const Register)this;
  }

  int sp_offset_in_saved_window() const {
    assert(is_in() || is_local(), "only i and l registers are saved in frame");
    return encoding() - lbase;
  }

  inline Address address_in_saved_window() const;     // implemented in assembler_sparc.hpp
};


// The integer registers of the SPARC architecture

CONSTANT_REGISTER_DECLARATION(Register, noreg , (-1));

CONSTANT_REGISTER_DECLARATION(Register, G0    , (RegisterImpl::gbase + 0));
CONSTANT_REGISTER_DECLARATION(Register, G1    , (RegisterImpl::gbase + 1));
CONSTANT_REGISTER_DECLARATION(Register, G2    , (RegisterImpl::gbase + 2));
CONSTANT_REGISTER_DECLARATION(Register, G3    , (RegisterImpl::gbase + 3));
CONSTANT_REGISTER_DECLARATION(Register, G4    , (RegisterImpl::gbase + 4));
CONSTANT_REGISTER_DECLARATION(Register, G5    , (RegisterImpl::gbase + 5));
CONSTANT_REGISTER_DECLARATION(Register, G6    , (RegisterImpl::gbase + 6));
CONSTANT_REGISTER_DECLARATION(Register, G7    , (RegisterImpl::gbase + 7));
                                                                          
CONSTANT_REGISTER_DECLARATION(Register, O0    , (RegisterImpl::obase + 0));
CONSTANT_REGISTER_DECLARATION(Register, O1    , (RegisterImpl::obase + 1));
CONSTANT_REGISTER_DECLARATION(Register, O2    , (RegisterImpl::obase + 2));
CONSTANT_REGISTER_DECLARATION(Register, O3    , (RegisterImpl::obase + 3));
CONSTANT_REGISTER_DECLARATION(Register, O4    , (RegisterImpl::obase + 4));
CONSTANT_REGISTER_DECLARATION(Register, O5    , (RegisterImpl::obase + 5));
CONSTANT_REGISTER_DECLARATION(Register, O6    , (RegisterImpl::obase + 6));
CONSTANT_REGISTER_DECLARATION(Register, O7    , (RegisterImpl::obase + 7));
                                                                          
CONSTANT_REGISTER_DECLARATION(Register, L0    , (RegisterImpl::lbase + 0));
CONSTANT_REGISTER_DECLARATION(Register, L1    , (RegisterImpl::lbase + 1));
CONSTANT_REGISTER_DECLARATION(Register, L2    , (RegisterImpl::lbase + 2));
CONSTANT_REGISTER_DECLARATION(Register, L3    , (RegisterImpl::lbase + 3));
CONSTANT_REGISTER_DECLARATION(Register, L4    , (RegisterImpl::lbase + 4));
CONSTANT_REGISTER_DECLARATION(Register, L5    , (RegisterImpl::lbase + 5));
CONSTANT_REGISTER_DECLARATION(Register, L6    , (RegisterImpl::lbase + 6));
CONSTANT_REGISTER_DECLARATION(Register, L7    , (RegisterImpl::lbase + 7));
                                                                          
CONSTANT_REGISTER_DECLARATION(Register, I0    , (RegisterImpl::ibase + 0));
CONSTANT_REGISTER_DECLARATION(Register, I1    , (RegisterImpl::ibase + 1));
CONSTANT_REGISTER_DECLARATION(Register, I2    , (RegisterImpl::ibase + 2));
CONSTANT_REGISTER_DECLARATION(Register, I3    , (RegisterImpl::ibase + 3));
CONSTANT_REGISTER_DECLARATION(Register, I4    , (RegisterImpl::ibase + 4));
CONSTANT_REGISTER_DECLARATION(Register, I5    , (RegisterImpl::ibase + 5));
CONSTANT_REGISTER_DECLARATION(Register, I6    , (RegisterImpl::ibase + 6));
CONSTANT_REGISTER_DECLARATION(Register, I7    , (RegisterImpl::ibase + 7));
                                                                          
CONSTANT_REGISTER_DECLARATION(Register, FP    , (RegisterImpl::ibase + 6));
CONSTANT_REGISTER_DECLARATION(Register, SP    , (RegisterImpl::obase + 6));

//
// Because sparc has so many registers, #define'ing values for the is
// beneficial in code size and the cost of some of the dangers of
// defines.  We don't use them on Intel because win32 uses asm
// directives which use the same names for registers as Hotspot does,
// so #defines would screw up the inline assembly.  If a particular
// file has a problem with these defines then it's possible to turn
// them off in that file by defining DONT_USE_REGISTER_DEFINES.
// register_definition_sparc.cpp does that so that it's able to
// provide real definitions of these registers for use in debuggers
// and such.
//

#ifndef DONT_USE_REGISTER_DEFINES
#define noreg ((Register)(noreg_RegisterEnumValue))

#define G0 ((Register)(G0_RegisterEnumValue))
#define G1 ((Register)(G1_RegisterEnumValue))
#define G2 ((Register)(G2_RegisterEnumValue))
#define G3 ((Register)(G3_RegisterEnumValue))
#define G4 ((Register)(G4_RegisterEnumValue))
#define G5 ((Register)(G5_RegisterEnumValue))
#define G6 ((Register)(G6_RegisterEnumValue))
#define G7 ((Register)(G7_RegisterEnumValue))
                                       
#define O0 ((Register)(O0_RegisterEnumValue))
#define O1 ((Register)(O1_RegisterEnumValue))
#define O2 ((Register)(O2_RegisterEnumValue))
#define O3 ((Register)(O3_RegisterEnumValue))
#define O4 ((Register)(O4_RegisterEnumValue))
#define O5 ((Register)(O5_RegisterEnumValue))
#define O6 ((Register)(O6_RegisterEnumValue))
#define O7 ((Register)(O7_RegisterEnumValue))
                                       
#define L0 ((Register)(L0_RegisterEnumValue))
#define L1 ((Register)(L1_RegisterEnumValue))
#define L2 ((Register)(L2_RegisterEnumValue))
#define L3 ((Register)(L3_RegisterEnumValue))
#define L4 ((Register)(L4_RegisterEnumValue))
#define L5 ((Register)(L5_RegisterEnumValue))
#define L6 ((Register)(L6_RegisterEnumValue))
#define L7 ((Register)(L7_RegisterEnumValue))
                                       
#define I0 ((Register)(I0_RegisterEnumValue))
#define I1 ((Register)(I1_RegisterEnumValue))
#define I2 ((Register)(I2_RegisterEnumValue))
#define I3 ((Register)(I3_RegisterEnumValue))
#define I4 ((Register)(I4_RegisterEnumValue))
#define I5 ((Register)(I5_RegisterEnumValue))
#define I6 ((Register)(I6_RegisterEnumValue))
#define I7 ((Register)(I7_RegisterEnumValue))
                                       
#define FP ((Register)(FP_RegisterEnumValue))
#define SP ((Register)(SP_RegisterEnumValue))
#endif // DONT_USE_REGISTER_DEFINES

// Use FloatRegister as shortcut
class FloatRegisterImpl;
typedef FloatRegisterImpl* FloatRegister;


// The implementation of float registers for the SPARC architecture

class FloatRegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers = 63
  };

  enum Width {
    S = 1,  D = 2,  Q = 3
  };

  // construction
  friend FloatRegister as_FloatRegister(int encoding) { return (FloatRegister)encoding; }

  // accessors
 private:
  int encoding() const                                { assert(is_valid(), "invalid register"); return value(); }

 public:
  int encoding(Width w) const {
    const int c = encoding();
    switch (w) {
      case S:
        assert(c < 32, "bad single float register");
        return c;
      
      case D:
        assert(c < 64  &&  (c & 1) == 0, "bad double float register");
        assert(c < 32 || VM_Version::v9_instructions_work(), "V9 float work only on V9 platform");
        return (c & 0x1e) | ((c & 0x20) >> 5);
      
      case Q:
        assert(c < 64  &&  (c & 3) == 0, "bad quad float register");
        assert(c < 32 || VM_Version::v9_instructions_work(), "V9 float work only on V9 platform");
        return (c & 0x1c) | ((c & 0x20) >> 5);
    }
    ShouldNotReachHere();
    return -1;
  }

  bool  is_valid() const                              { return 0 <= value() && value() < number_of_registers; }
  const char* name() const;

  FloatRegister successor() const                     { return as_FloatRegister(encoding() + 1); }
};


// The float registers of the SPARC architecture

CONSTANT_REGISTER_DECLARATION(FloatRegister, fnoreg , (-1));

CONSTANT_REGISTER_DECLARATION(FloatRegister, F0     , ( 0));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F1     , ( 1));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F2     , ( 2));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F3     , ( 3));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F4     , ( 4));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F5     , ( 5));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F6     , ( 6));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F7     , ( 7));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F8     , ( 8));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F9     , ( 9));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F10    , (10));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F11    , (11));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F12    , (12));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F13    , (13));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F14    , (14));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F15    , (15));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F16    , (16));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F17    , (17));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F18    , (18));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F19    , (19));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F20    , (20));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F21    , (21));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F22    , (22));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F23    , (23));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F24    , (24));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F25    , (25));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F26    , (26));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F27    , (27));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F28    , (28));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F29    , (29));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F30    , (30));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F31    , (31));

CONSTANT_REGISTER_DECLARATION(FloatRegister, F32    , (32));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F34    , (34));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F36    , (36));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F38    , (38));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F40    , (40));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F42    , (42));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F44    , (44));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F46    , (46));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F48    , (48));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F50    , (50));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F52    , (52));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F54    , (54));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F56    , (56));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F58    , (58));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F60    , (60));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F62    , (62));


#ifndef DONT_USE_REGISTER_DEFINES
#define fnoreg ((FloatRegister)(fnoreg_FloatRegisterEnumValue))
#define F0     ((FloatRegister)(    F0_FloatRegisterEnumValue))
#define F1     ((FloatRegister)(    F1_FloatRegisterEnumValue))
#define F2     ((FloatRegister)(    F2_FloatRegisterEnumValue))
#define F3     ((FloatRegister)(    F3_FloatRegisterEnumValue))
#define F4     ((FloatRegister)(    F4_FloatRegisterEnumValue))
#define F5     ((FloatRegister)(    F5_FloatRegisterEnumValue))
#define F6     ((FloatRegister)(    F6_FloatRegisterEnumValue))
#define F7     ((FloatRegister)(    F7_FloatRegisterEnumValue))
#define F8     ((FloatRegister)(    F8_FloatRegisterEnumValue))
#define F9     ((FloatRegister)(    F9_FloatRegisterEnumValue))
#define F10    ((FloatRegister)(   F10_FloatRegisterEnumValue))
#define F11    ((FloatRegister)(   F11_FloatRegisterEnumValue))
#define F12    ((FloatRegister)(   F12_FloatRegisterEnumValue))
#define F13    ((FloatRegister)(   F13_FloatRegisterEnumValue))
#define F14    ((FloatRegister)(   F14_FloatRegisterEnumValue))
#define F15    ((FloatRegister)(   F15_FloatRegisterEnumValue))
#define F16    ((FloatRegister)(   F16_FloatRegisterEnumValue))
#define F17    ((FloatRegister)(   F17_FloatRegisterEnumValue))
#define F18    ((FloatRegister)(   F18_FloatRegisterEnumValue))
#define F19    ((FloatRegister)(   F19_FloatRegisterEnumValue))
#define F20    ((FloatRegister)(   F20_FloatRegisterEnumValue))
#define F21    ((FloatRegister)(   F21_FloatRegisterEnumValue))
#define F22    ((FloatRegister)(   F22_FloatRegisterEnumValue))
#define F23    ((FloatRegister)(   F23_FloatRegisterEnumValue))
#define F24    ((FloatRegister)(   F24_FloatRegisterEnumValue))
#define F25    ((FloatRegister)(   F25_FloatRegisterEnumValue))
#define F26    ((FloatRegister)(   F26_FloatRegisterEnumValue))
#define F27    ((FloatRegister)(   F27_FloatRegisterEnumValue))
#define F28    ((FloatRegister)(   F28_FloatRegisterEnumValue))
#define F29    ((FloatRegister)(   F29_FloatRegisterEnumValue))
#define F30    ((FloatRegister)(   F30_FloatRegisterEnumValue))
#define F31    ((FloatRegister)(   F31_FloatRegisterEnumValue))
#define F32    ((FloatRegister)(   F32_FloatRegisterEnumValue))
#define F34    ((FloatRegister)(   F34_FloatRegisterEnumValue))
#define F36    ((FloatRegister)(   F36_FloatRegisterEnumValue))
#define F38    ((FloatRegister)(   F38_FloatRegisterEnumValue))
#define F40    ((FloatRegister)(   F40_FloatRegisterEnumValue))
#define F42    ((FloatRegister)(   F42_FloatRegisterEnumValue))
#define F44    ((FloatRegister)(   F44_FloatRegisterEnumValue))
#define F46    ((FloatRegister)(   F46_FloatRegisterEnumValue))
#define F48    ((FloatRegister)(   F48_FloatRegisterEnumValue))
#define F50    ((FloatRegister)(   F50_FloatRegisterEnumValue))
#define F52    ((FloatRegister)(   F52_FloatRegisterEnumValue))
#define F54    ((FloatRegister)(   F54_FloatRegisterEnumValue))
#define F56    ((FloatRegister)(   F56_FloatRegisterEnumValue))
#define F58    ((FloatRegister)(   F58_FloatRegisterEnumValue))
#define F60    ((FloatRegister)(   F60_FloatRegisterEnumValue))
#define F62    ((FloatRegister)(   F62_FloatRegisterEnumValue))
#endif // DONT_USE_REGISTER_DEFINES

// Maximum number of incoming arguments that can be passed in i registers.
const int SPARC_ARGS_IN_REGS_NUM = 6;


// Need to know the total number of registers of all sorts for SharedInfo.
// Define a class that exports it.

class ConcreteRegisterImpl : public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers = RegisterImpl::number_of_registers + FloatRegisterImpl::number_of_registers
  };
};


// Single, Double and Quad fp reg classes.  These exist to map the ADLC
// encoding for a floating point register, to the FloatRegister number
// desired by the macroassembler.  A FloatRegister is a number between
// 0 and 63 passed around as a pointer.  For ADLC, an fp register encoding
// is the actual bit encoding used by the sparc hardware.  When ADLC used
// the macroassembler to generate an instruction that references, e.g., a
// double fp reg, it passed the bit encoding to the macroassembler via
// as_FloatRegister, which, for double regs > 30, returns an illegal
// register number.
//
// Therefore we provide the following classes for use by ADLC.  Their
// sole purpose is to convert from sparc register encodings to FloatRegisters.
// At some future time, we might replace FloatRegister with these classes,
// hence the definitions of as_xxxFloatRegister as class methods rather
// than as external inline routines.

class SingleFloatRegisterImpl;
typedef SingleFloatRegisterImpl *SingleFloatRegister;

class SingleFloatRegisterImpl {
 public:
  friend FloatRegister as_SingleFloatRegister(int encoding) {
    assert(encoding < 32, "bad single float register encoding");
    return as_FloatRegister(encoding);
  }
};


class DoubleFloatRegisterImpl;
typedef DoubleFloatRegisterImpl *DoubleFloatRegister;

class DoubleFloatRegisterImpl {
 public:
  friend FloatRegister as_DoubleFloatRegister(int encoding) {
    assert(encoding < 32, "bad double float register encoding");
    return as_FloatRegister( ((encoding & 1) << 5) | (encoding & 0x1e) );
  }
};


class QuadFloatRegisterImpl;
typedef QuadFloatRegisterImpl *QuadFloatRegister;

class QuadFloatRegisterImpl {
 public:
  friend FloatRegister as_QuadFloatRegister(int encoding) {
    assert(encoding < 32 && ((encoding & 2) == 0), "bad quad float register encoding");
    return as_FloatRegister( ((encoding & 1) << 5) | (encoding & 0x1c) );
  }
};
