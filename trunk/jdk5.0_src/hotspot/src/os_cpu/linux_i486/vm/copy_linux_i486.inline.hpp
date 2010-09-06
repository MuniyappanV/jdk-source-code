#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)copy_linux_i486.inline.hpp	1.5 04/04/30 16:15:58 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

static void pd_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  // Same as pd_aligned_conjoint_words, except includes a zero-count check.
  intx temp;
  __asm__ volatile("        testl   %6,%6         ;"
                   "        jz      7f            ;"
                   "        cmpl    %4,%5         ;"
                   "        leal    -4(%4,%6,4),%3;"
                   "        jbe     1f            ;"
                   "        cmpl    %7,%5         ;"
                   "        jbe     4f            ;"
                   "1:      cmpl    $32,%6        ;"
                   "        ja      3f            ;"
                   "        subl    %4,%1         ;"
                   "2:      movl    (%4),%3       ;"
                   "        movl    %7,(%5,%4,1)  ;"
                   "        addl    $4,%0         ;"
                   "        decl    %2            ;"
                   "        jnz     2b            ;"
                   "        jmp     7f            ;"
                   "3:      rep;    smovl         ;"
                   "        jmp     7f            ;"
                   "4:      cmpl    $32,%2        ;"
                   "        movl    %7,%0         ;"
                   "        leal    -4(%5,%6,4),%1;"
                   "        ja      6f            ;"
                   "        subl    %4,%1         ;"
                   "5:      movl    (%4),%3       ;"
                   "        movl    %7,(%5,%4,1)  ;"
                   "        subl    $4,%0         ;"
                   "        decl    %2            ;"
                   "        jnz     5b            ;"
                   "        jmp     7f            ;"
                   "6:      std                   ;"
        	   "        rep;    smovl         ;"
                   "        cld                   ;"
                   "7:      nop                    "
                   : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
                   : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
                   : "memory", "flags");
}

static void pd_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  // Same as pd_aligned_disjoint_words, except includes a zero-count check.
  intx temp;
  __asm__ volatile("        testl   %6,%6       ;"
                   "        jz      3f          ;"
                   "        cmpl    $32,%6      ;"
                   "        ja      2f          ;"
                   "        subl    %4,%1       ;"
                   "1:      movl    (%4),%3     ;"
                   "        movl    %7,(%5,%4,1);"
                   "        addl    $4,%0       ;"
                   "        decl    %2          ;"
                   "        jnz     1b          ;"
                   "        jmp     3f          ;"
                   "2:      rep;    smovl       ;"
                   "3:      nop                  "
                   : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
                   : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
                   : "memory", "cc");
}

static void pd_disjoint_words_atomic(HeapWord* from, HeapWord* to, size_t count) {
  // pd_disjoint_words is word-atomic in this implementation.
  pd_disjoint_words(from, to, count);
}

static void pd_aligned_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  // Same as pd_conjoint_words, except no zero-count check.
  intx temp;
  __asm__ volatile("        cmpl    %4,%5         ;"
                   "        leal    -4(%4,%6,4),%3;"
                   "        jbe     1f            ;"
                   "        cmpl    %7,%5         ;"
                   "        jbe     4f            ;"
                   "1:      cmpl    $32,%6        ;"
                   "        ja      3f            ;"
                   "        subl    %4,%1         ;"
                   "2:      movl    (%4),%3       ;"
                   "        movl    %7,(%5,%4,1)  ;"
                   "        addl    $4,%0         ;"
                   "        decl    %2            ;"
                   "        jnz     2b            ;"
                   "        jmp     7f            ;"
                   "3:      rep;    smovl         ;"
                   "        jmp     7f            ;"
                   "4:      cmpl    $32,%2        ;"
                   "        movl    %7,%0         ;"
                   "        leal    -4(%5,%6,4),%1;"
                   "        ja      6f            ;"
                   "        subl    %4,%1         ;"
                   "5:      movl    (%4),%3       ;"
                   "        movl    %7,(%5,%4,1)  ;"
                   "        subl    $4,%0         ;"
                   "        decl    %2            ;"
                   "        jnz     5b            ;"
                   "        jmp     7f            ;"
                   "6:      std                   ;"
        	   "        rep;    smovl         ;"
                   "        cld                   ;"
                   "7:      nop                    "
                   : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
                   : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
                   : "memory", "flags");
}

static void pd_aligned_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  // Same as pd_disjoint_words, except no zero-count check.
  intx temp;
  __asm__ volatile("        cmpl    $32,%6      ;"
                   "        ja      2f          ;"
                   "        subl    %4,%1       ;"
                   "1:      movl    (%4),%3     ;"
                   "        movl    %7,(%5,%4,1);"
                   "        addl    $4,%0       ;"
                   "        decl    %2          ;"
                   "        jnz     1b          ;"
                   "        jmp     3f          ;"
                   "2:      rep;    smovl       ;"
                   "3:      nop                  "
                   : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
                   : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
                   : "memory", "cc");
}

static void pd_conjoint_bytes(void* from, void* to, size_t count) {
  intx temp;
  __asm__ volatile("        testl   %6,%6          ;"
                   "        jz      13f            ;"
                   "        cmpl    %4,%5          ;"
                   "        leal    -1(%4,%6),%3   ;"
                   "        jbe     1f             ;"
                   "        cmpl    %7,%5          ;"
                   "        jbe     8f             ;"
                   "1:      cmpl    $3,%6          ;"
                   "        jbe     6f             ;"
                   "        movl    %6,%3          ;"
                   "        movl    $4,%2          ;"
                   "        subl    %4,%2          ;"
                   "        andl    $3,%2          ;"
        	   "        jz      2f             ;"
                   "        subl    %6,%3          ;"
                   "        rep;    smovb          ;"
                   "2:      movl    %7,%2          ;"
                   "        shrl    $2,%2          ;"
                   "        jz      5f             ;"
                   "        cmpl    $32,%2         ;"
                   "        ja      4f             ;"
                   "        subl    %4,%1          ;"
                   "3:      movl    (%4),%%edx     ;"
                   "        movl    %%edx,(%5,%4,1);"
                   "        addl    $4,%0          ;"
                   "        decl    %2             ;"
                   "        jnz     3b             ;"
                   "        addl    %4,%1          ;"
                   "        jmp     5f             ;"
                   "4:      rep;    smovl          ;"
                   "5:      movl    %7,%2          ;"
                   "        andl    $3,%2          ;"
                   "        jz      13f            ;"
                   "6:      xorl    %7,%3          ;"
                   "7:      movb    (%4,%7,1),%%dl ;"
                   "        movb    %%dl,(%5,%7,1) ;"
                   "        incl    %3             ;"
                   "        decl    %2             ;"
                   "        jnz     7b             ;"
                   "        jmp     13f            ;"
                   "8:      std                    ;"
                   "        cmpl    $12,%2         ;"
                   "        ja      9f             ;"
                   "        movl    %7,%0          ;"
                   "        leal    -1(%6,%5),%1   ;"
                   "        jmp     11f            ;"
                   "9:      xchgl   %3,%2          ;"
                   "        movl    %6,%0          ;"
                   "        incl    %2             ;"
                   "        leal    -1(%7,%5),%1   ;"
                   "        andl    $3,%2          ;"
                   "        jz      10f            ;"
                   "        subl    %6,%3          ;"
                   "        rep;    smovb          ;"
                   "10:     movl    %7,%2          ;"
                   "        subl    $3,%0          ;"
                   "        shrl    $2,%2          ;"
                   "        subl    $3,%1          ;"
                   "        rep;    smovl          ;"
                   "        andl    $3,%3          ;"
                   "        jz      12f            ;"
                   "        movl    %7,%2          ;"
                   "        addl    $3,%0          ;"
                   "        addl    $3,%1          ;"
                   "11:     rep;    smovb          ;"
                   "12:     cld                    ;"
                   "13:     nop                    ;"
                   : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
                   : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
                   : "memory", "flags", "%edx");
}

static void pd_conjoint_bytes_atomic(void* from, void* to, size_t count) {
  pd_conjoint_bytes(from, to, count);
}

static void pd_conjoint_jshorts_atomic(jshort* from, jshort* to, size_t count) {
  _Copy_conjoint_jshorts_atomic(from, to, count);
}

static void pd_conjoint_jints_atomic(jint* from, jint* to, size_t count) {
  assert(HeapWordSize == BytesPerInt, "heapwords and jints must be the same size");
  // pd_conjoint_words is word-atomic in this implementation.
  pd_conjoint_words((HeapWord*)from, (HeapWord*)to, count);
}

static void pd_conjoint_jlongs_atomic(jlong* from, jlong* to, size_t count) {
  // Guarantee use of fild/fistp or xmm regs via some asm code, because compilers won't.
  if (from > to) {
    while (count-- > 0) {
      __asm__ volatile("fildll (%0); fistpll (%1)"
                       :
                       : "r" (from), "r" (to)
                       : "memory" );
      ++from;
      ++to;
    }
  } else {
    while (count-- > 0) {
      __asm__ volatile("fildll (%0,%2,8); fistpll (%1,%2,8)"
                       :
                       : "r" (from), "r" (to), "r" (count)
                       : "memory" );
    }
  }
}

static void pd_conjoint_oops_atomic(oop* from, oop* to, size_t count) {
  assert(HeapWordSize == BytesPerOop, "heapwords and oops must be the same size");
  // pd_conjoint_words is word-atomic in this implementation.
  pd_conjoint_words((HeapWord*)from, (HeapWord*)to, count);
}

static void pd_arrayof_conjoint_bytes(HeapWord* from, HeapWord* to, size_t count) {
  _Copy_arrayof_conjoint_bytes(from, to, count);
}

static void pd_arrayof_conjoint_jshorts(HeapWord* from, HeapWord* to, size_t count) {
  _Copy_arrayof_conjoint_jshorts(from, to, count);
}

static void pd_arrayof_conjoint_jints(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_jints_atomic((jint*)from, (jint*)to, count);
}

static void pd_arrayof_conjoint_jlongs(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_jlongs_atomic((jlong*)from, (jlong*)to, count);
}

static void pd_arrayof_conjoint_oops(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_oops_atomic((oop*)from, (oop*)to, count);
}
