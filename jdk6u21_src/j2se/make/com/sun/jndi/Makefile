#
# @(#)Makefile	1.12 10/03/23
#
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
#

#
# Makefile for building JNDI service providers
#

BUILDDIR = ../../..
include $(BUILDDIR)/common/Defs.gmk

SUBDIRS = toolkit cosnaming ldap rmi dns
all build clean clobber::
	$(SUBDIRS-loop)

clean clobber::
	$(RM) -r $(CLASSDESTDIR)/com/sun/jndi
