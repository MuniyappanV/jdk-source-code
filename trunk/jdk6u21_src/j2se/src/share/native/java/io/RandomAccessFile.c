/*
 * @(#)RandomAccessFile.c	1.32 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "jvm.h"

#include "io_util.h"
#include "io_util_md.h"
#include "java_io_RandomAccessFile.h"

#include <fcntl.h>

/*
 * static method to store field ID's in initializers
 */

jfieldID raf_fd; /* id for jobject 'fd' in java.io.RandomAccessFile */

JNIEXPORT void JNICALL 
Java_java_io_RandomAccessFile_initIDs(JNIEnv *env, jclass fdClass) {
    raf_fd = (*env)->GetFieldID(env, fdClass, "fd", "Ljava/io/FileDescriptor;");
}


JNIEXPORT void JNICALL
Java_java_io_RandomAccessFile_open(JNIEnv *env,
				   jobject this, jstring path, jint mode)
{
    int flags = 0;
    if (mode & java_io_RandomAccessFile_O_RDONLY)
	flags = O_RDONLY;
    else if (mode & java_io_RandomAccessFile_O_RDWR) {
	flags = O_RDWR | O_CREAT;
	if (mode & java_io_RandomAccessFile_O_SYNC)
	    flags |= O_SYNC;
	else if (mode & java_io_RandomAccessFile_O_DSYNC)
	    flags |= O_DSYNC;
    }
    fileOpen(env, this, path, raf_fd, flags);
}

JNIEXPORT jint JNICALL
Java_java_io_RandomAccessFile_read(JNIEnv *env, jobject this) {
    return readSingle(env, this, raf_fd);
}

JNIEXPORT jint JNICALL
Java_java_io_RandomAccessFile_readBytes(JNIEnv *env,
    jobject this, jbyteArray bytes, jint off, jint len) {
    return readBytes(env, this, bytes, off, len, raf_fd); 
}

JNIEXPORT void JNICALL
Java_java_io_RandomAccessFile_write(JNIEnv *env, jobject this, jint byte) {
    writeSingle(env, this, byte, raf_fd);
}

JNIEXPORT void JNICALL
Java_java_io_RandomAccessFile_writeBytes(JNIEnv *env,
    jobject this, jbyteArray bytes, jint off, jint len) {
    writeBytes(env, this, bytes, off, len, raf_fd); 
}

JNIEXPORT jlong JNICALL
Java_java_io_RandomAccessFile_getFilePointer(JNIEnv *env, jobject this) {
    FD fd;
    jlong ret;

    fd = GET_FD(this, raf_fd);
    if ((ret = IO_Lseek(fd, 0L, SEEK_CUR)) == -1) {
      	JNU_ThrowIOExceptionWithLastError(env, "Seek failed");
    }
    return ret;
}

JNIEXPORT jlong JNICALL
Java_java_io_RandomAccessFile_length(JNIEnv *env, jobject this) {
    FD fd;
    jlong cur = jlong_zero;
    jlong end = jlong_zero;

    fd = GET_FD(this, raf_fd);

    if ((cur = IO_Lseek(fd, 0L, SEEK_CUR)) == -1) {
      	JNU_ThrowIOExceptionWithLastError(env, "Seek failed");
    } else if ((end = IO_Lseek(fd, 0L, SEEK_END)) == -1) {
      	JNU_ThrowIOExceptionWithLastError(env, "Seek failed");
    } else if (IO_Lseek(fd, cur, SEEK_SET) == -1) {
      	JNU_ThrowIOExceptionWithLastError(env, "Seek failed");
    }
    return end;
}

JNIEXPORT void JNICALL
Java_java_io_RandomAccessFile_seek(JNIEnv *env,
		    jobject this, jlong pos) {

    FD fd;

    fd = GET_FD(this, raf_fd);
    if (pos < jlong_zero) {
        JNU_ThrowIOException(env, "Negative seek offset");
    } else if (IO_Lseek(fd, pos, SEEK_SET) == -1) {
      	JNU_ThrowIOExceptionWithLastError(env, "Seek failed");
    }
}

JNIEXPORT void JNICALL
Java_java_io_RandomAccessFile_setLength(JNIEnv *env, jobject this,
					jlong newLength)
{
    FD fd;
    jlong cur;

    fd = GET_FD(this, raf_fd);
    if ((cur = IO_Lseek(fd, 0L, SEEK_CUR)) == -1) goto fail;
    if (IO_SetLength(fd, newLength) == -1) goto fail;
    if (cur > newLength) {
	if (IO_Lseek(fd, 0L, SEEK_END) == -1) goto fail;
    } else {
	if (IO_Lseek(fd, cur, SEEK_SET) == -1) goto fail;
    }
    return;

 fail:
    JNU_ThrowIOExceptionWithLastError(env, "setLength failed");
}

