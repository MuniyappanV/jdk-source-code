/*
 * Copyright (c) 1997, 2004, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include "incls/_precompiled.incl"
#include "incls/_icache_sparc.cpp.incl"

#define __ _masm->

void ICacheStubGenerator::generate_icache_flush(
  ICache::flush_icache_stub_t* flush_icache_stub
) {
  StubCodeMark mark(this, "ICache", "flush_icache_stub");
  address start = __ pc();

  Label L;
  __ bind(L);
  __ flush( O0, G0 );
  __ deccc( O1 );
  __ br(Assembler::positive, false, Assembler::pn, L);
  __ delayed()->inc( O0, 8 );
  __ retl(false);
  __ delayed()->mov( O2, O0 ); // handshake with caller to make sure it happened!

  // Must be set here so StubCodeMark destructor can call the flush stub.
  *flush_icache_stub = (ICache::flush_icache_stub_t)start;
};

#undef __
