#
# Copyright 2004 Sun Microsystems, Inc. All rights reserved.
# SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
#

#
# @(#)Makefile	1.38 03/12/19
#

#
# Makefile for building and packaging all of the JDK and the JRE. See
# also included files.
#
#
# Most frequently used targets:
#
#    all            -- stage the images, generate the patch, bind the 
#		       optimized and debug exe. 
#    patchgen       -- generate the patch
#    patcher	    -- build the patch installer
#    stage	    -- stages the jres that we want to patch, however, this 
#		       target activate only if ALT_BASE_IMAGE_JRE_ZIP and
#		       ALT_BASE_IMAGE_SDK_ZIP are defined, typically for 
#		       developer and special patches.
#    installer	    -- builds the installer for the platform
#    clobber        -- clobber the files generated by this Makefile 


INSTALL_BUILDDIR=.
include $(INSTALL_BUILDDIR)/common/Defs.gmk

all:: 
	@$(ECHO) $(ARCH) "Install Build started:  " $(FULL_VERSION)

# sigh. this is a real hack - JRE/JDK packaging and the Plugin
# require absolute paths - In order to get these, OUTPUTDIR must
# exist now. This forces the creation. This is done here so that
# it only happens once.
dummy := $(shell $(MKDIR) -p $(OUTPUTDIR))

SUBDIRS = pack

ifeq ($(ARCH_DATA_MODEL),64)
  ifeq ($(PLATFORM), solaris)
    SUBDIRS =
  endif
else
  ifeq ($(PLATFORM), windows)
    SUBDIRS += rebase update
  endif
endif

SUBDIRS += installer

ifndef EXTERNALSANITYCONTROL
all:: sanity
patchgen:: sanity
patcher:: sanity
installer:: sanity
endif

all optimized debug::
	@for i in $(SUBDIRS) ; do \
	    $(ECHO) ">>>Recursively making "$$i" "$@" @ `$(DATE)` ..."; \
	    $(CD) $$i; $(MAKE) \
            || exit 1; $(CD) ..; \
	    $(ECHO) "<<<Finished Recursively making "$$i" "$@" @ `$(DATE)`." ; \
	done

pack::
	$(CD) pack ; $(MAKE) all

patchgen::
	$(CD) update/patchgen ; $(MAKE) all

patcher::
	$(CD) update/patcher ; $(MAKE) all

stage::
	$(CD) update/stage ; $(MAKE) all

installer::
	$(CD) installer ; $(MAKE) all	


#
# Sanity checks.
#
include $(INSTALL_BUILDDIR)/common/Sanity.gmk

clean: clobber

clobber: install-clobber

install-clobber::
	@for i in $(SUBDIRS) ; do \
	    $(ECHO) ">>>Recursively making "$$i" "clean" @ `$(DATE)` ..."; \
	    $(CD) $$i; $(MAKE) clean RELEASE="$(RELEASE)" FULL_VERSION="$(FULL_VERSION)" \
            || exit 1; $(CD) ..; \
	    $(ECHO) "<<<Finished Recursively making "$$i" all @ `$(DATE)`." ; \
	done


# this should be the last rule in this file:
all::
	@$(ECHO) $(ARCH) "Install Build finished:  " $(FULL_VERSION)

#
# Phonies to avoid accidents.
#
.PHONY: all optimized debug clean clobber install-clobber sanity patchgen patcher stage installer



