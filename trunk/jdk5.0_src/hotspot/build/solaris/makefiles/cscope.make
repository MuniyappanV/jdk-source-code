# 
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.

#
# @(#)cscope.make	1.6 03/12/23 16:35:24
#
# The cscope.out file is made in the current directory and spans the entire
# source tree.
#
# Things to note:
#	1. We use relative names for cscope.
#	2. We *don't* remove the old cscope.out file, because cscope is smart
#	   enough to only build what has changed.  It can be confused, however,
#	   if files are renamed or removed, so it may be necessary to manually
#	   remove cscope.out if a lot of reorganization has occurred.
#
NAWK	= /usr/xpg4/bin/awk
RM	= rm -f
CS_TOP	= ../..

CSDIRS	= $(CS_TOP)/src $(CS_TOP)/build
CSINCS	= $(CSDIRS:%=-I%)

CSCOPE		= cscope
CSCOPE_FLAGS	= -b

# Allow .java files to be added from the environment (CSCLASSES=yes).
ifdef	CSCLASSES
ADDCLASSES=	-o -name '*.java'
endif

# Adding CClassHeaders also pushes the file count of a full workspace up about
# 200 files (these files also don't exist in a new workspace, and thus will
# cause the recreation of the database as they get created, which might seem
# a little confusing).  Thus allow these files to be added from the environment
# (CSHEADERS=yes).
ifndef	CSHEADERS
RMCCHEADERS=	-o -name CClassHeaders
endif

# Use CS_GENERATED=x to include auto-generated files in the build directories.
ifdef	CS_GENERATED
CS_ADD_GENERATED	= -o -name '*.incl'
else
CS_PRUNE_GENERATED	= -o -name '${OS}_*_core' -o -name '${OS}_*_compiler?'
endif

# OS-specific files for other systems are excluded by default.  Use CS_OS=yes
# to include platform-specific files for other platforms.
ifndef	CS_OS
CS_OS		= linux macos solaris win32
CS_PRUNE_OS	= $(patsubst %,-o -name '*%*',$(filter-out ${OS},${CS_OS}))
endif

# Processor-specific files for other processors are excluded by default.  Use
# CS_CPU=x to include platform-specific files for other platforms.
ifndef	CS_CPU
CS_CPU		= i486 sparc
CS_PRUNE_CPU	= $(patsubst %,-o -name '*%*',$(filter-out ${ARCH},${CS_CPU}))
endif

# What files should we include?  A simple rule might be just those files under
# SCCS control, however this would miss files we create like the opcodes and
# CClassHeaders.  The following attempts to find everything that is *useful*.
# (.del files are created by sccsrm, demo directories contain many .java files
# that probably aren't useful for development, and the pkgarchive may contain
# duplicates of files within the source hierarchy).

# Directories to exclude.
CS_PRUNE_STD	= -name SCCS \
		  -o -name '.del-*' \
		  -o -name '*demo' \
		  -o -name pkgarchive

CS_PRUNE	= $(CS_PRUNE_STD) \
		  $(CS_PRUNE_OS) \
		  $(CS_PRUNE_CPU) \
		  $(CS_PRUNE_GENERATED) \
		  $(RMCCHEADERS)

# File names to include.
CSFILENAMES	= -name '*.[ch]pp' \
		  -o -name '*.[Ccshlxy]' \
		  $(CS_ADD_GENERATED) \
		  -o -name '*.il' \
		  -o -name '*.cc' \
		  -o -name '*[Mm]akefile*' \
		  -o -name '*.gmk' \
		  -o -name '*.make' \
		  -o -name '*.ad' \
		  $(ADDCLASSES)

.PRECIOUS:	cscope.out
.PHONY:		cscope cscope.clean TAGS.clean nametable.clean FORCE

cscope cscope.out: cscope.files FORCE
	$(CSCOPE) $(CSCOPE_FLAGS)

# The .raw file is reordered here in an attempt to make cscope display the most
# relevant files first.
cscope.files: .cscope.files.raw
	echo "$(CSINCS)" > $@
	-egrep -v "\.java|\/build\/"	$< >> $@
	-fgrep ".java"			$< >> $@
	-fgrep "/build/"		$< >> $@

.cscope.files.raw:  .nametable.files
	-find $(CSDIRS) -type d \( $(CS_PRUNE) \) -prune -o \
	    -type f \( $(CSFILENAMES) \) -print > $@

cscope.clean:  nametable.clean
	-$(RM) cscope.out cscope.files .cscope.files.raw

TAGS:  cscope.files FORCE
	egrep -v '^-|^$$' $< | etags --members -

TAGS.clean:  nametable.clean
	-$(RM) TAGS

# .nametable.files and .nametable.files.tmp are used to determine if any files
# were added to/deleted from/renamed in the workspace.  If not, then there's
# normally no need to run find.  To force a 'find':  gmake nametable.clean.
.nametable.files:  .nametable.files.tmp
	cmp -s $@ $< || cp $< $@

.nametable.files.tmp:  $(CS_TOP)/Codemgr_wsdata/nametable
	$(NAWK) \
	'{ if (sub("( [a-z0-9]{2,8}){4}$$", "")) print $$0; }' $< > $@

nametable.clean:
	-$(RM) .nametable.files .nametable.files.tmp

FORCE:
