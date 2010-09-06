#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)generateJvmOffsets.cpp	1.16 04/03/17 12:08:19"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

/*
 * This is to provide sanity check in jhelper.d which compares SCCS
 * versions of generateJvmOffsets.cpp used to create and extract
 * contents of __JvmOffsets[] table.
 * The __JvmOffsets[] table is located in generated JvmOffsets.cpp.
 *
 * GENOFFS_SCCS_VER 16
 */

#include "generateJvmOffsets.h"

/* A workaround for private and protected fields */
#define private   public
#define protected public

#include <proc_service.h>
#include "incls/_precompiled.incl"
#include "incls/_vmStructs.cpp.incl"

#ifdef COMPILER1
#if defined(DEBUG) || defined(FASTDEBUG)

/*
 * To avoid the most part of potential link errors
 * we link this program with -z nodefs .
 *
 * But for 'debug1' and 'fastdebug1' we still have to provide
 * a particular workaround for the following symbols bellow.
 * It will be good to find out a generic way in the future.
 */

#pragma weak tty
#pragma weak CMSExpAvgFactor

#if defined(i386) || defined(__i386)
#pragma weak noreg
#endif /* i386 */

LIR_Opr LIR_OprFact::illegalOpr = (LIR_Opr) 0;

#endif /* defined(DEBUG) || defined(FASTDEBUG) */
#endif /* COMPILER1 */

#define GEN_OFFS(Type,Name)				\
  switch(gen_variant) {					\
  case GEN_OFFSET:					\
    printf("#define OFFSET_%-33s %d\n",		\
            #Type #Name, offsetof(Type, Name));		\
    break;						\
  case GEN_INDEX:					\
    printf("#define IDX_OFFSET_%-33s %d\n",		\
            #Type #Name, ++index);			\
    break;						\
  case GEN_TABLE:					\
    printf("\tOFFSET_%s,\n", #Type #Name);		\
    break;						\
  }

#define GEN_SIZE(Type)					\
  switch(gen_variant) {					\
  case GEN_OFFSET:					\
    printf("#define SIZE_%-35s %d\n",			\
            #Type, sizeof(Type));			\
    break;						\
  case GEN_INDEX:					\
    printf("#define IDX_SIZE_%-35s %d\n",		\
            #Type, ++index);				\
    break;						\
  case GEN_TABLE:					\
    printf("\tSIZE_%s,\n", #Type);			\
    break;						\
  }

#define GEN_VALUE(String,Value)				\
  switch(gen_variant) {					\
  case GEN_OFFSET:					\
    printf("#define %-40s %d\n", #String, Value);	\
    break;						\
  case GEN_INDEX:					\
    printf("#define IDX_%-40s %d\n", #String, ++index);\
    break;						\
  case GEN_TABLE:					\
    printf("\t" #String ",\n");				\
    break;						\
  }

void gen_prologue(GEN_variant gen_variant) {
    const char *suffix;

    switch(gen_variant) {
      case GEN_OFFSET: suffix = ".h";        break;
      case GEN_INDEX:  suffix = "Index.h";   break;
      case GEN_TABLE:  suffix = ".cpp";      break;
    }

    printf("/*\n");
    printf(" * JvmOffsets%s !!!DO NOT EDIT!!! \n", suffix);
    printf(" * The generateJvmOffsets program generates this file!\n");
    printf(" */\n\n");
    switch(gen_variant) {

      case GEN_OFFSET:
        /* printf("#define %-40s %s\n", "JVMOFFS_CSUM", "JVMOFFS_CSUM_VAL"); */
        break;

      case GEN_INDEX:
        printf("#define %-44s %d\n", "IDX_JVMOFFS_CSUM", 0);
        break;

      case GEN_TABLE:
        printf("#include \"JvmOffsets.h\"\n");
        printf("\n");
        printf("int __JvmOffsets[] = {\n");
        printf("\tJVMOFFS_CSUM,\n");
        break;
    }
}

void gen_epilogue(GEN_variant gen_variant) {
    if (gen_variant != GEN_TABLE) {
        return;
    }
    printf("};\n\n");
    printf("#ifdef COMPILER1\n\n");

    printf("/* Dtrace workaround for I2C and C2I adapter vtbl symbols\n");
    printf(" *     __1cKC2IAdapterG__vtbl_ and __1cKI2CAdapterG__vtbl_\n");
    printf(" * The problem is to have the same jhelper.d code for both\n");
    printf(" * C1 and C2 compilers. But ocurrences of C2 adapter symbols\n");
    printf(" * in jhelper.d code even for C1 specific blocks cause\n");
    printf(" * dtrace verifier to fail silently.\n");
    printf(" * Here are fake adapter classes defined to workaround the problem.\n");
    printf(" */\n");
    printf("\n");
    printf("class BasicAdapter { public: virtual void fake(void);};\n");
    printf("class I2CAdapter : public BasicAdapter { public: void fake(void);};\n");
    printf("class C2IAdapter : public BasicAdapter { public: void fake(void);};\n");
    printf("\n");
    printf("void I2CAdapter::fake(void) {};\n");
    printf("void C2IAdapter::fake(void) {};\n");
    printf("\n");
    printf("#endif /* COMPILER1 */\n");
    return;
}

int generateJvmOffsets(GEN_variant gen_variant) {
  int index = 0;	/* It is used to generate JvmOffsetsIndex.h */
  int pointer_size = sizeof(void *);
  int data_model = (pointer_size == 4) ? PR_MODEL_ILP32 : PR_MODEL_LP64;

  gen_prologue(gen_variant);

  GEN_VALUE(DATA_MODEL, data_model);
  GEN_VALUE(POINTER_SIZE, pointer_size);
#ifdef COMPILER1
  GEN_VALUE(COMPILER, 1);
#elif COMPILER2
  GEN_VALUE(COMPILER, 2);
#else // CORE
  GEN_VALUE(COMPILER, 0);
#endif // COMPILER1
  printf("\n");

  GEN_OFFS(CollectedHeap, _reserved);
  GEN_OFFS(MemRegion, _start);
  GEN_OFFS(MemRegion, _word_size);
  GEN_SIZE(HeapWord);
  printf("\n");

  GEN_OFFS(VMStructEntry, typeName);
  GEN_OFFS(VMStructEntry, fieldName);
  GEN_OFFS(VMStructEntry, address);
  GEN_SIZE(VMStructEntry);
  printf("\n");

  GEN_VALUE(MAX_METHOD_CODE_SIZE, max_method_code_size);
#if defined(sparc) || defined(__sparc)
  GEN_VALUE(OFFSET_interpreter_frame_method, 8);
  // Fake value for consistency. It is not going to be used.
  GEN_VALUE(OFFSET_interpreter_frame_bcx_offset, 0xFFFF);
#elif defined(i386) || defined(__i386)
  GEN_VALUE(OFFSET_interpreter_frame_method, -8);
#ifndef CORE
  GEN_VALUE(OFFSET_interpreter_frame_bcx_offset, -24);
#else // !CORE
  GEN_VALUE(OFFSET_interpreter_frame_bcx_offset, -20);
#endif // CORE
#endif

  GEN_OFFS(Klass, _name);
  GEN_OFFS(constantPoolOopDesc, _pool_holder);
  printf("\n");

  GEN_VALUE(OFFSET_HeapBlockHeader_used, offsetof(HeapBlock::Header, _used));
  GEN_OFFS(oopDesc, _klass);
  printf("\n");

  GEN_VALUE(AccessFlags_NATIVE, JVM_ACC_NATIVE);
  GEN_VALUE(constMethodOopDesc_has_linenumber_table, constMethodOopDesc::_has_linenumber_table);
  GEN_OFFS(AccessFlags, _flags);
  GEN_OFFS(symbolOopDesc, _length);
  GEN_OFFS(symbolOopDesc, _body);
  printf("\n");

  GEN_OFFS(methodOopDesc, _constMethod);
  GEN_OFFS(methodOopDesc, _constants);
  GEN_OFFS(methodOopDesc, _access_flags);
  printf("\n");

  GEN_OFFS(constMethodOopDesc, _flags);
  GEN_OFFS(constMethodOopDesc, _code_size);
  GEN_OFFS(constMethodOopDesc, _name_index);
  GEN_OFFS(constMethodOopDesc, _signature_index);
  printf("\n");

  GEN_OFFS(CodeHeap, _memory);
  GEN_OFFS(CodeHeap, _segmap);
  GEN_OFFS(CodeHeap, _log2_segment_size);
  printf("\n");

  GEN_OFFS(VirtualSpace, _low_boundary);
  GEN_OFFS(VirtualSpace, _high_boundary);
  GEN_OFFS(VirtualSpace, _low);
  GEN_OFFS(VirtualSpace, _high);
  printf("\n");

  GEN_OFFS(CodeBlob, _name);
  GEN_OFFS(CodeBlob, _header_size);
  GEN_OFFS(CodeBlob, _instructions_offset);
  GEN_OFFS(CodeBlob, _data_offset);
  GEN_OFFS(CodeBlob, _oops_offset);
  GEN_OFFS(CodeBlob, _oops_length);
  GEN_OFFS(CodeBlob, _frame_size);
#ifdef COMPILER2
  GEN_OFFS(CodeBlob, _link_offset);
#else
  /* A fake value for consistency */
  GEN_VALUE(OFFSET_CodeBlob_link_offset, 0);
#endif // COMPILER2
  printf("\n");

  GEN_OFFS(nmethod, _method);
  GEN_OFFS(nmethod, _scopes_data_offset);
  GEN_OFFS(nmethod, _scopes_pcs_offset);
  GEN_OFFS(nmethod, _handler_table_offset);

  GEN_OFFS(PcDesc, _pc_offset);
  GEN_OFFS(PcDesc, _scope_decode_offset);

  printf("\n");

  GEN_VALUE(SIZE_HeapBlockHeader, sizeof(HeapBlock::Header));
  GEN_SIZE(oopDesc);
  GEN_SIZE(constantPoolOopDesc);
  printf("\n");

  GEN_SIZE(PcDesc);
  GEN_SIZE(methodOopDesc);
  GEN_SIZE(constMethodOopDesc);
  GEN_SIZE(nmethod);
  GEN_SIZE(CodeBlob);
  GEN_SIZE(BufferBlob);
  GEN_SIZE(SingletonBlob);
  GEN_SIZE(RuntimeStub);
  GEN_SIZE(SafepointBlob);

  gen_epilogue(gen_variant);
  printf("\n");

  return 0;
}
