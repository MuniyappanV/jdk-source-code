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

# The common definitions for hotspot solaris builds.
# Include the top level defs.make under make directory instead of this one.
# This file is included into make/defs.make.

# Need PLATFORM (os-arch combo names) for jdk and hotspot, plus libarch name
SLASH_JAVA ?= /java
ARCH:=$(shell uname -p)
PATH_SEP = :
ifeq ($(LP64), 1)
  ARCH_DATA_MODEL=64
else
  ARCH_DATA_MODEL=32
endif

ifeq ($(ARCH),sparc)
  ifeq ($(ARCH_DATA_MODEL), 64)
    MAKE_ARGS += LP64=1
    PLATFORM=solaris-sparcv9
    VM_PLATFORM=solaris_sparcv9
  else
    PLATFORM=solaris-sparc
    VM_PLATFORM=solaris_sparc
  endif
  HS_ARCH=sparc
else
  ifeq ($(ARCH_DATA_MODEL), 64)
    MAKE_ARGS += LP64=1
    PLATFORM=solaris-amd64
    VM_PLATFORM=solaris_amd64
    HS_ARCH=x86
  else
    PLATFORM=solaris-i586
    VM_PLATFORM=solaris_i486
    HS_ARCH=x86
  endif
endif

JDK_INCLUDE_SUBDIR=solaris

# FIXUP: The subdirectory for a debug build is NOT the same on all platforms
VM_DEBUG=jvmg

EXPORT_LIST += $(EXPORT_DOCS_DIR)/platform/jvmti/jvmti.html

# client and server subdirectories have symbolic links to ../libjsig.so
EXPORT_LIST += $(EXPORT_JRE_LIB_ARCH_DIR)/libjsig.so

EXPORT_SERVER_DIR = $(EXPORT_JRE_LIB_ARCH_DIR)/server
EXPORT_LIST += $(EXPORT_SERVER_DIR)/Xusage.txt
EXPORT_LIST += $(EXPORT_SERVER_DIR)/libjvm.so
EXPORT_LIST += $(EXPORT_SERVER_DIR)/libjvm_db.so
EXPORT_LIST += $(EXPORT_SERVER_DIR)/libjvm_dtrace.so
ifeq ($(ARCH_DATA_MODEL), 32)
  EXPORT_CLIENT_DIR = $(EXPORT_JRE_LIB_ARCH_DIR)/client
  EXPORT_LIST += $(EXPORT_CLIENT_DIR)/Xusage.txt
  EXPORT_LIST += $(EXPORT_CLIENT_DIR)/libjvm.so 
  EXPORT_LIST += $(EXPORT_CLIENT_DIR)/libjvm_db.so 
  EXPORT_LIST += $(EXPORT_CLIENT_DIR)/libjvm_dtrace.so 
  ifeq ($(ARCH),sparc)
    EXPORT_LIST += $(EXPORT_SERVER_DIR)/64/libjvm_db.so
    EXPORT_LIST += $(EXPORT_CLIENT_DIR)/64/libjvm_db.so
    EXPORT_LIST += $(EXPORT_SERVER_DIR)/64/libjvm_dtrace.so
    EXPORT_LIST += $(EXPORT_CLIENT_DIR)/64/libjvm_dtrace.so
  endif
endif

EXPORT_LIST += $(EXPORT_JRE_LIB_ARCH_DIR)/libsaproc.so
EXPORT_LIST += $(EXPORT_LIB_DIR)/sa-jdi.jar 
