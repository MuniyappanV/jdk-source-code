#! /bin/sh
# 
# @(#)buildATree.sh	1.9 03/12/23 16:35:15
# 
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
# 

set -eu

# This shell script builds gamma from a workspace
#  - adopted from the buildATree script by David Ungar

# It should be invoked with three arguments:
#  the variant of gamma to build, the workspace location and the platform

case $# in
4) true;;
*)
  echo "Usage: $0 [core | compiler1 | compiler2] gamma-directory-name platform arch"
  echo " e.g.: $0 core /import/hotsparc/gamma linux sparc"
  exit 1
esac

variant=$1
gammadir=$2
os_family=$3
arch=$4

platform=${os_family}_${arch}
platform_file=$gammadir/build/${os_family}/platform_${arch}
dir_name=`pwd`/${platform}_${variant}

#
# We do two levels of exclusion in the shared directory.
# TOPLEVEL excludes are pruned, they are not recursively searched,
# but lower level directories can be named without fear of collision.
# ALWAYS excludes are excluded at any level in the directory tree.
#

always_exclude_dirs="-name SCCS -o -name CVS"
set toplevel_exclude_dirs=-none-
  
case $variant in
 compiler2)
        toplevel_exclude_dirs="${always_exclude_dirs} -o -name adlc -o -name c1 -o -name agent"
        ;;
 compiler1)
        toplevel_exclude_dirs="${always_exclude_dirs} -o -name adlc -o -name opto -o -name libadt -o -name agent"
        ;;
 core)
        toplevel_exclude_dirs="${always_exclude_dirs} -o -name adlc -o -name opto -o -name libadt -o -name agent"
 	;;
 *)
 	echo "${0}: First argument (variant) must be one of core, compiler1, or compiler2."
 	exit 1
esac

#
#set compiler
#
eval "`sed -n  '/compiler/s/ *= */=/p' < $platform_file`"

#
#set lib_arch
#
eval "`sed -n  '/lib_arch/s/ *= */=/p' < $platform_file`"

#
# Set up the build tree it not already there
#
mkdir -p $dir_name/generated/incls
mkdir -p $dir_name/generated/adfiles
mkdir -p $dir_name/generated/jvmtifiles

for i in debug release optimized fastdebug jvmg profiled product
do
  (
  mkdir -p $dir_name/$i
  cd $dir_name/$i >/dev/null

    # make 3 makefiles: 
    #  flags.make with macro settings,
    #  Makefile for "make foo"
    #  vm.make to support making "make -v vm.make" in makefiles.
    #  (The makefiles are split this way so that "make foo"
    #   will run faster by not having to read the dependency files for the vm.)

    Src_Shared_Dirs=` find $gammadir/src/share/vm/* -prune ! \( -name SCCS -o -name CVS -o -name .cvsignore \) -type d -print `

    echo "# Generated by $0"              >flags.make
    echo ""			          >>flags.make
    echo Platform_file = $platform_file   >>flags.make
    sed -n < $platform_file  \
		'/=/s/^ */Platform_/p'    >>flags.make
    echo ""			          >>flags.make
    echo GAMMADIR = $gammadir  		  >>flags.make
    echo SYSDEFS = \$\(Platform_sysdefs\) >>flags.make
    echo ""			          >>flags.make

    # collect all source directories in vm for Makefile Src_Dirs macro
    # (be careful to skip such names as SCCS)
    echo ""			          >>flags.make
    echo Src_Dirs =  \\                   >>flags.make

    find ${gammadir}/src/share/vm/* -prune \
	-type d \! \( ${toplevel_exclude_dirs} \) -exec find {} \
        -type d \! \( ${always_exclude_dirs} \) -printf "%p \\\\\\n" \; >> flags.make
    
    echo $gammadir/src/cpu/$arch/vm     \\  >> flags.make
    echo $gammadir/src/os/$os_family/vm \\  >> flags.make
    echo $gammadir/src/os_cpu/${os_family}_$arch/vm \\  >> flags.make
    echo ""			          >>flags.make
    echo include $gammadir/build/$os_family/makefiles/${variant}.make   >>flags.make
    [ ${i} = profiled ] && \
    echo include $gammadir/build/$os_family/makefiles/optimized.make    >>flags.make
    echo include $gammadir/build/$os_family/makefiles/${i}.make         >>flags.make
    echo include $gammadir/build/$os_family/makefiles/${compiler}.make  >>flags.make
 
    echo "# Generated by $0"             >Makefile
    echo ""			         >>Makefile
    echo include flags.make              >>Makefile
    echo ""			         >>Makefile
    echo include $gammadir/build/$os_family/makefiles/top.make >>Makefile
 
    echo "# Generated by $0"             >vm.make
    echo ""			         >>vm.make
    echo include flags.make              >>vm.make
    echo ""			         >>vm.make
    echo include $gammadir/build/$os_family/makefiles/vm.make  >>vm.make

    echo "# Generated by $0"             >adlc.make
    echo ""			         >>adlc.make
    echo include flags.make              >>adlc.make
    echo ""			         >>adlc.make
    echo include $gammadir/build/$os_family/makefiles/adlc.make  >>adlc.make

    echo "# Generated by $0"             >jvmti.make
    echo ""			         >>jvmti.make
    echo include flags.make              >>jvmti.make
    echo ""			         >>jvmti.make
    echo include $gammadir/build/$os_family/makefiles/jvmti.make  >>jvmti.make

    echo "# Generated by $0"             >sa.make
    echo ""			         >>sa.make
    echo include flags.make              >>sa.make
    echo ""			         >>sa.make
    echo include $gammadir/build/$os_family/makefiles/sa.make  >>sa.make


    : ${JAVA_HOME:='${JAVA_HOME}'}
    export JAVA_HOME

    case ${CLASSPATH-} in
    '')
      CLASSPATH=".:${JAVA_HOME}/jre/lib/rt.jar:${JAVA_HOME}/jre/lib/i18n.jar"
      ;;
    *)
      CLASSPATH="${CLASSPATH}:.:${JAVA_HOME}/jre/lib/rt.jar:${JAVA_HOME}/jre/lib/i18n.jar"
    esac
    export CLASSPATH

    case ${LD_LIBRARY_PATH-} in
    '')
      LD_LIBRARY_PATH="${JAVA_HOME}/jre/lib/${lib_arch}"
      ;;
    *)
      LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${JAVA_HOME}/jre/lib/${lib_arch}"
    esac
    export LD_LIBRARY_PATH

    echo "# Environment settings recorded by $0"   >env.csh
    echo setenv JAVA_HOME ${JAVA_HOME}             >>env.csh
    echo setenv LD_LIBRARY_PATH .:${LD_LIBRARY_PATH} \
      | sed s:${JAVA_HOME}:\${JAVA_HOME}:g         >>env.csh
    echo setenv CLASSPATH $CLASSPATH               \
      | sed s:${JAVA_HOME}:\${JAVA_HOME}:g         >>env.csh

    sed < env.csh > env.ksh \
	-e '/^#/{p;d;}' \
	-e 's/^setenv //;s/ /=/;s/^/export /'
  )
done
