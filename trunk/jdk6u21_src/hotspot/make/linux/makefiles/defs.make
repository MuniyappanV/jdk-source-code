#
# Copyright (c) 2006, 2008, Oracle and/or its affiliates. All rights reserved.
# ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
#
#
#
#
#
#
#
#
#
#
#
#
#
#
#
#
#
#
#  
#

# The common definitions for hotspot linux builds.
# Include the top level defs.make under make directory instead of this one.
# This file is included into make/defs.make.

SLASH_JAVA ?= /java

# Need PLATFORM (os-arch combo names) for jdk and hotspot, plus libarch name
ARCH:=$(shell uname -m)
PATH_SEP = :
ifeq ($(LP64), 1)
  ARCH_DATA_MODEL ?= 64
else
  ARCH_DATA_MODEL ?= 32
endif

# zero
ifeq ($(ZERO_BUILD), true)
  ifeq ($(ARCH_DATA_MODEL), 64)
    MAKE_ARGS      += LP64=1
  endif
  PLATFORM         = linux-zero
  VM_PLATFORM      = linux_$(subst i386,i486,$(ZERO_LIBARCH))
  HS_ARCH          = zero
  ARCH             = zero
endif

# ia64
ifeq ($(ARCH), ia64)
  ARCH_DATA_MODEL = 64
  MAKE_ARGS      += LP64=1
  PLATFORM        = linux-ia64
  VM_PLATFORM     = linux_ia64
  HS_ARCH         = ia64
endif

# sparc
ifeq ($(ARCH), sparc64)
  ifeq ($(ARCH_DATA_MODEL), 64)
    ARCH_DATA_MODEL  = 64
    MAKE_ARGS        += LP64=1
    PLATFORM         = linux-sparcv9
    VM_PLATFORM      = linux_sparcv9
  else
    ARCH_DATA_MODEL  = 32
    PLATFORM         = linux-sparc
    VM_PLATFORM      = linux_sparc
  endif
  HS_ARCH            = sparc
endif

# x86_64
ifeq ($(ARCH), x86_64) 
  ifeq ($(ARCH_DATA_MODEL), 64)
    ARCH_DATA_MODEL = 64
    MAKE_ARGS       += LP64=1
    PLATFORM        = linux-amd64
    VM_PLATFORM     = linux_amd64
    HS_ARCH         = x86
  else
    ARCH_DATA_MODEL = 32
    PLATFORM        = linux-i586
    VM_PLATFORM     = linux_i486
    HS_ARCH         = x86
    # We have to reset ARCH to i686 since SRCARCH relies on it
    ARCH            = i686   
  endif
endif

# i686
ifeq ($(ARCH), i686)
  ARCH_DATA_MODEL  = 32
  PLATFORM         = linux-i586
  VM_PLATFORM      = linux_i486
  HS_ARCH          = x86
endif

JDK_INCLUDE_SUBDIR=linux

# FIXUP: The subdirectory for a debug build is NOT the same on all platforms
VM_DEBUG=jvmg

EXPORT_LIST += $(EXPORT_DOCS_DIR)/platform/jvmti/jvmti.html

# client and server subdirectories have symbolic links to ../libjsig.so
EXPORT_LIST += $(EXPORT_JRE_LIB_ARCH_DIR)/libjsig.so

EXPORT_SERVER_DIR = $(EXPORT_JRE_LIB_ARCH_DIR)/server
EXPORT_LIST += $(EXPORT_SERVER_DIR)/Xusage.txt
EXPORT_LIST += $(EXPORT_SERVER_DIR)/libjvm.so
ifneq ($(ZERO_BUILD), true)
  ifeq ($(ARCH_DATA_MODEL), 32)
    EXPORT_CLIENT_DIR = $(EXPORT_JRE_LIB_ARCH_DIR)/client
    EXPORT_LIST += $(EXPORT_CLIENT_DIR)/Xusage.txt
    EXPORT_LIST += $(EXPORT_CLIENT_DIR)/libjvm.so 
    EXPORT_LIST += $(EXPORT_JRE_LIB_ARCH_DIR)/libsaproc.so
    EXPORT_LIST += $(EXPORT_LIB_DIR)/sa-jdi.jar 
  else
    ifeq ($(ARCH),ia64)
      else
        EXPORT_LIST += $(EXPORT_JRE_LIB_ARCH_DIR)/libsaproc.so
        EXPORT_LIST += $(EXPORT_LIB_DIR)/sa-jdi.jar
    endif
  endif
endif
