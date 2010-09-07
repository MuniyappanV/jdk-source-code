@echo off
REM
REM Copyright (c) 1997, 2008, Oracle and/or its affiliates. All rights reserved.
REM ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM
REM  
REM


REM
REM Since we don't have uname and we could be cross-compiling,
REM Use the compiler to determine which ARCH we are building
REM 
cl 2>&1 | grep "IA-64" >NUL
if %errorlevel% == 0 goto isia64
cl 2>&1 | grep "AMD64" >NUL
if %errorlevel% == 0 goto amd64
set ARCH=x86
set BUILDARCH=i486
set Platform_arch=x86
set Platform_arch_model=x86_32
goto end
:amd64
set LP64=1
set ARCH=x86
set BUILDARCH=amd64
set Platform_arch=x86
set Platform_arch_model=x86_64
goto end
:isia64
set LP64=1
set ARCH=ia64
set Platform_arch=ia64
set Platform_arch_model=ia64
:end

if "%4" == ""          goto usage
if not "%7" == ""      goto usage

if "%1" == "product"   goto test1
if "%1" == "debug"     goto test1
if "%1" == "fastdebug" goto test1
goto usage

:test1
if "%2" == "core"      goto test2
if "%2" == "kernel"   goto test2
if "%2" == "compiler1" goto test2
if "%2" == "compiler2" goto test2
if "%2" == "tiered"    goto test2
if "%2" == "adlc"      goto build_adlc

goto usage

:test2
REM check_j2se_version
REM jvmti.make requires J2SE 1.4.x or newer.
REM If not found then fail fast.
%4\bin\javap javax.xml.transform.TransformerFactory >NUL
if %errorlevel% == 0 goto build
echo.
echo J2SE version found at %4\bin\java:
%4\bin\java -version
echo.
echo An XSLT processor (J2SE 1.4.x or newer) is required to
echo bootstrap this build
echo.

goto usage

:build
nmake -f %3/make/windows/build.make Variant=%2 WorkSpace=%3 BootStrapDir=%4 BuildUser="%USERNAME%" HOTSPOT_BUILD_VERSION="%5" %1
goto end

:build_adlc
nmake -f %3/make/windows/build.make Variant=compiler2 WorkSpace=%3 BootStrapDir=%4 BuildUser="%USERNAME%" HOTSPOT_BUILD_VERSION=%5 ADLC_ONLY=1 %1
goto end

:usage
echo Usage: build flavor version workspace bootstrap_dir [build_id] [windbg_home]
echo.
echo where:
echo flavor is "product", "debug" or "fastdebug",
echo version is "core", "kernel", "compiler1", "compiler2", or "tiered",
echo workspace is source directory without trailing slash, 
echo bootstrap_dir is a full path to echo a JDK in which bin/java 
echo   and bin/javac are present and working, and echo build_id is an 
echo   optional build identifier displayed by java -version

:end
