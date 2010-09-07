#
# Copyright (c) 1999, 2008, Oracle and/or its affiliates. All rights reserved.
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

# Sets make macros for making optimized version of Gamma VM
# (This is the "product", not the "release" version.)

# Compiler specific OPT_CFLAGS are passed in from gcc.make, sparcWorks.make
OPT_CFLAGS/DEFAULT= $(OPT_CFLAGS)
OPT_CFLAGS/BYFILE = $(OPT_CFLAGS/$@)$(OPT_CFLAGS/DEFAULT$(OPT_CFLAGS/$@))

# (OPT_CFLAGS/SLOWER is also available, to alter compilation of buggy files)

# If you set HOTSPARC_GENERIC=yes, you disable all OPT_CFLAGS settings
CFLAGS$(HOTSPARC_GENERIC) += $(OPT_CFLAGS/BYFILE)

# Set the environment variable HOTSPARC_GENERIC to "true"
# to inhibit the effect of the previous line on CFLAGS.

# Linker mapfile
MAPFILE = $(GAMMADIR)/make/linux/makefiles/mapfile-vers-product

G_SUFFIX =
SYSDEFS += -DPRODUCT
VERSION = optimized

# use -g to strip library as -x will discard its symbol table; -x is fine for
# executables.
STRIP = strip
STRIP_LIBJVM = $(STRIP) -g $@ || exit 1;
STRIP_AOUT   = $(STRIP) -x $@ || exit 1;

# Don't strip in VM build; JDK build will strip libraries later
# LINK_LIB.CC/POST_HOOK += $(STRIP_$(LINK_INTO))
