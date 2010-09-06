#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)jhelper.d	1.18 04/07/29 16:36:13"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

/* This file is auto-generated */
#include "JvmOffsetsIndex.h"

#define DEBUG

#ifdef DEBUG
#define MARK_LINE this->line = __LINE__
#else
#define MARK_LINE 
#endif

#ifdef _LP64
#define STACK_BIAS 0x7ff
#define pointer uint64_t
#else
#define STACK_BIAS 0
#define pointer uint32_t
#endif

extern pointer __JvmOffsets;

extern pointer __1cJCodeCacheF_heap_;
extern pointer __1cIUniverseP_methodKlassObj_;
extern pointer __1cIUniverseO_collectedHeap_;

extern pointer __1cHnmethodG__vtbl_;
extern pointer __1cKBufferBlobG__vtbl_;

#define copyin_ptr(ADDR)    *(pointer*)  copyin((pointer) (ADDR), sizeof(pointer))
#define copyin_uchar(ADDR)  *(uchar_t*)  copyin((pointer) (ADDR), sizeof(uchar_t))
#define copyin_uint16(ADDR) *(uint16_t*) copyin((pointer) (ADDR), sizeof(uint16_t))
#define copyin_uint32(ADDR) *(uint32_t*) copyin((pointer) (ADDR), sizeof(uint32_t))

#define SAME(x) x
#define copyin_offset(JVM_CONST)  JVM_CONST = \
	copyin_uint32(JvmOffsetsPtr + SAME(IDX_)JVM_CONST * sizeof(uint32_t))

int init_done;

dtrace:helper:ustack:
{
        MARK_LINE;
        this->done = 0;
        /*
         * TBD:
         * Here we initialize init_done, otherwise jhelper does not work.
         * Therefore, copyin_offset() statements work multiple times now.
         * There is a hope we could avoid it in the future, and so,
         * this initialization can be removed.
         */
        init_done  = 0;
        this->error = (char *) NULL;
        this->result = (char *) NULL;
        this->methodOop = 0;
        this->codecache = 0;
        this->klass = (pointer) NULL;
        this->vtbl  = (pointer) NULL;
        this->suffix = '\0';
}

dtrace:helper:ustack:
{
        MARK_LINE;
	/* Initialization of JvmOffsets constants */
        JvmOffsetsPtr = (pointer) &``__JvmOffsets;
	copyin_offset(JVMOFFS_CSUM);
}

/*
 * GENOFFS_VER is macro definition passed by make file at compilation time.
 * jhelper's MY_JVMOFFS_CSUM !MUST MATCH! JVMOFFS_CSUM from libjvm.so.
 */

dtrace:helper:ustack:
/!init_done && MY_JVMOFFS_CSUM != JVMOFFS_CSUM/
{
        MARK_LINE;
	this->done  = 1;
        this->error = "<JVMOFFS_CSUM of jhelper and JVM don't match>";
}

dtrace:helper:ustack:
/!init_done && !this->done/
{
        MARK_LINE;
	init_done = 1;

	copyin_offset(COMPILER);
	copyin_offset(OFFSET_CollectedHeap_reserved);
	copyin_offset(OFFSET_MemRegion_start);
	copyin_offset(OFFSET_MemRegion_word_size);
	copyin_offset(SIZE_HeapWord);

	copyin_offset(OFFSET_interpreter_frame_method);
	copyin_offset(OFFSET_Klass_name);
	copyin_offset(OFFSET_constantPoolOopDesc_pool_holder);

	copyin_offset(OFFSET_HeapBlockHeader_used);
	copyin_offset(OFFSET_oopDesc_klass);

	copyin_offset(OFFSET_symbolOopDesc_length);
	copyin_offset(OFFSET_symbolOopDesc_body);

	copyin_offset(OFFSET_methodOopDesc_constMethod);
	copyin_offset(OFFSET_methodOopDesc_constants);
	copyin_offset(OFFSET_constMethodOopDesc_name_index);

	copyin_offset(OFFSET_CodeHeap_memory);
	copyin_offset(OFFSET_CodeHeap_segmap);
	copyin_offset(OFFSET_CodeHeap_log2_segment_size);

	copyin_offset(OFFSET_VirtualSpace_low);
	copyin_offset(OFFSET_VirtualSpace_high);

	copyin_offset(OFFSET_CodeBlob_name);

	copyin_offset(OFFSET_nmethod_method);
	copyin_offset(SIZE_HeapBlockHeader);
	copyin_offset(SIZE_oopDesc);
	copyin_offset(SIZE_constantPoolOopDesc);

        /*
         * The PC to translate is in arg0.
         */
        this->pc = arg0;

        /*
         * The methodOopPtr is in %l2 on SPARC.  This can be found at
         * offset 8 from the frame pointer on 32-bit processes.
         */
#ifdef __sparc
        this->methodOopPtr = copyin_ptr(arg1 + 2 * sizeof(pointer) + STACK_BIAS);
#elif __i386
        this->methodOopPtr = copyin_ptr(arg1 + OFFSET_interpreter_frame_method);
#else
error "Don't know architecture"
#endif

        this->Universe_methodKlassOop = copyin_ptr(&``__1cIUniverseP_methodKlassObj_);
        this->CodeCache_heap_address = copyin_ptr(&``__1cJCodeCacheF_heap_);

        /* Reading volatile values */
        this->CodeCache_low = copyin_ptr(this->CodeCache_heap_address + 
            OFFSET_CodeHeap_memory + OFFSET_VirtualSpace_low);

        this->CodeCache_high = copyin_ptr(this->CodeCache_heap_address +
            OFFSET_CodeHeap_memory + OFFSET_VirtualSpace_high);

        this->CodeCache_segmap_low = copyin_ptr(this->CodeCache_heap_address +
            OFFSET_CodeHeap_segmap + OFFSET_VirtualSpace_low);

        this->CodeCache_segmap_high = copyin_ptr(this->CodeCache_heap_address +
            OFFSET_CodeHeap_segmap + OFFSET_VirtualSpace_high);

        this->CodeHeap_log2_segment_size = copyin_uint32(
           this->CodeCache_heap_address + OFFSET_CodeHeap_log2_segment_size);

        /*
         * Get Java heap bounds
         */
        this->Universe_collectedHeap = copyin_ptr(&``__1cIUniverseO_collectedHeap_);
        this->heap_start = copyin_ptr(this->Universe_collectedHeap +
                                      OFFSET_CollectedHeap_reserved +
                                      OFFSET_MemRegion_start);
        this->heap_size = SIZE_HeapWord *
	                  copyin_ptr(this->Universe_collectedHeap +
                                     OFFSET_CollectedHeap_reserved +
                                     OFFSET_MemRegion_word_size
                                    );
        this->heap_end = this->heap_start + this->heap_size;
}

dtrace:helper:ustack:
/!this->done &&
  this->CodeCache_low <= this->pc && this->pc < this->CodeCache_high/
{
        MARK_LINE;
        this->codecache = 1;

        /*
         * Find start.
         */
        this->segment = (this->pc - this->CodeCache_low) >>
                                    this->CodeHeap_log2_segment_size;
        this->block = this->CodeCache_segmap_low;
        this->tag = copyin_uchar(this->block + this->segment);
        "second";
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->tag = copyin_uchar(this->block + this->segment);
        this->segment = this->segment - this->tag;
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->tag = copyin_uchar(this->block + this->segment);
        this->segment = this->segment - this->tag;
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->tag = copyin_uchar(this->block + this->segment);
        this->segment = this->segment - this->tag;
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->tag = copyin_uchar(this->block + this->segment);
        this->segment = this->segment - this->tag;
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->tag = copyin_uchar(this->block + this->segment);
        this->segment = this->segment - this->tag;
}

dtrace:helper:ustack:
/!this->done && this->codecache && this->tag > 0/
{
        MARK_LINE;
        this->error = "<couldn't find start>";
        this->done = 1;
}

dtrace:helper:ustack:
/!this->done && this->codecache/
{
        MARK_LINE;
       this->block = this->CodeCache_low +
            (this->segment << this->CodeHeap_log2_segment_size);
        this->used = copyin_uint32(this->block + OFFSET_HeapBlockHeader_used);
}

dtrace:helper:ustack:
/!this->done && this->codecache && !this->used/
{
        MARK_LINE;
        this->error = "<block not in use>";
        this->done = 1;
}

dtrace:helper:ustack:
/!this->done && this->codecache/
{
        MARK_LINE;
        this->start = this->block + SIZE_HeapBlockHeader;
        this->vtbl = copyin_ptr(this->start);

        this->nmethod_vtbl            = (pointer) &``__1cHnmethodG__vtbl_;
        this->BufferBlob_vtbl         = (pointer) &``__1cKBufferBlobG__vtbl_;
}

dtrace:helper:ustack:
/!this->done && this->vtbl == this->nmethod_vtbl/
{
        MARK_LINE;
        this->methodOopPtr = copyin_ptr(this->start + OFFSET_nmethod_method);
        this->suffix = '*';
        this->methodOop = 1;
}

dtrace:helper:ustack:
/!this->done && this->vtbl == this->BufferBlob_vtbl/
{
        MARK_LINE;
        this->name = copyin_ptr(this->start + OFFSET_CodeBlob_name);
}

dtrace:helper:ustack:
/!this->done && this->vtbl == this->BufferBlob_vtbl &&
 this->methodOopPtr > this->heap_start && this->methodOopPtr < this->heap_end/
{
        MARK_LINE;
        this->klass = copyin_ptr(this->methodOopPtr + OFFSET_oopDesc_klass);
        this->methodOop = this->klass == this->Universe_methodKlassOop;
        this->done = !this->methodOop;
}

dtrace:helper:ustack:
/!this->done && !this->methodOop/
{
        MARK_LINE;
        this->name = copyin_ptr(this->start + OFFSET_CodeBlob_name);
        this->result = this->name != 0 ? copyinstr(this->name) : "<CodeBlob>";
        this->done = 1;
}

dtrace:helper:ustack:
/!this->done && this->methodOop/
{
        MARK_LINE;
        this->constMethod = copyin_ptr(this->methodOopPtr +
            OFFSET_methodOopDesc_constMethod);

        this->nameIndex = copyin_uint16(this->constMethod +
            OFFSET_constMethodOopDesc_name_index);

        this->constantPool = copyin_ptr(this->methodOopPtr +
            OFFSET_methodOopDesc_constants);

        this->nameSymbol = copyin_ptr(this->constantPool +
            this->nameIndex * sizeof (pointer) + SIZE_constantPoolOopDesc);

        this->nameSymbolLength = copyin_uint16(this->nameSymbol +
            OFFSET_symbolOopDesc_length);

        this->klassPtr = copyin_ptr(this->constantPool +
            OFFSET_constantPoolOopDesc_pool_holder);

        this->klassSymbol = copyin_ptr(this->klassPtr +
            OFFSET_Klass_name + SIZE_oopDesc);

        this->klassSymbolLength = copyin_uint16(this->klassSymbol +
            OFFSET_symbolOopDesc_length);

	/*
	 * Enough for both strings, plus the '.', plus the trailing '\0'.
	 */
	this->result = (char *) alloca(this->klassSymbolLength +
	    this->nameSymbolLength + 2 + 1);

	copyinto(this->klassSymbol + OFFSET_symbolOopDesc_body,
	    this->klassSymbolLength, this->result);

	/*
	 * Add the '.' between the class and the name.
	 */
        this->result[this->klassSymbolLength] = '.';

	copyinto(this->nameSymbol + OFFSET_symbolOopDesc_body,
	    this->nameSymbolLength,
	    this->result + this->klassSymbolLength + 1);

	/*
	 * Now we need to add a trailing '\0' and possibly a tag character.
	 */
        this->result[this->klassSymbolLength + 1 + this->nameSymbolLength] = this->suffix;
        this->result[this->klassSymbolLength + 2 + this->nameSymbolLength] = '\0';

        this->done = 1;
}

dtrace:helper:ustack:
/this->done && this->error == (char *) NULL/
{
        this->result;   
}

dtrace:helper:ustack:
/this->done && this->error != (char *) NULL/
{
        this->error;
}

dtrace:helper:ustack:
/!this->done && this->codecache/
{
        this->done = 1;
        "error";
}


dtrace:helper:ustack:
/!this->done/
{
        NULL;
}
