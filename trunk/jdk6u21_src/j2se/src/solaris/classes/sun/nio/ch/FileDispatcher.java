/*
 * @(#)FileDispatcher.java	1.15 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.nio.ch;

import java.io.*;

/**
 * Allows different platforms to call different native methods
 * for read and write operations.
 */

class FileDispatcher extends NativeDispatcher
{

    static {
        Util.load();
	init();
    }

    int read(FileDescriptor fd, long address, int len) throws IOException {
        return read0(fd, address, len);
    }

    int pread(FileDescriptor fd, long address, int len,
                             long position, Object lock) throws IOException {
        return pread0(fd, address, len, position);
    }

    long readv(FileDescriptor fd, long address, int len) throws IOException {
        return readv0(fd, address, len);
    }

    int write(FileDescriptor fd, long address, int len) throws IOException {
        return write0(fd, address, len);
    }

    int pwrite(FileDescriptor fd, long address, int len,
                             long position, Object lock) throws IOException
    {
        return pwrite0(fd, address, len, position);
    }

    long writev(FileDescriptor fd, long address, int len)
	throws IOException
    {
        return writev0(fd, address, len);
    }

    void close(FileDescriptor fd) throws IOException {
        close0(fd);
    }

    void preClose(FileDescriptor fd) throws IOException {
	preClose0(fd);
    }

    // -- Native methods --

    static native int read0(FileDescriptor fd, long address, int len)
	throws IOException;

    static native int pread0(FileDescriptor fd, long address, int len,
                             long position) throws IOException;

    static native long readv0(FileDescriptor fd, long address, int len)
	throws IOException;

    static native int write0(FileDescriptor fd, long address, int len)
	throws IOException;

    static native int pwrite0(FileDescriptor fd, long address, int len,
                             long position) throws IOException;

    static native long writev0(FileDescriptor fd, long address, int len)
	throws IOException;

    static native void close0(FileDescriptor fd) throws IOException;

    static native void preClose0(FileDescriptor fd) throws IOException;

    static native void closeIntFD(int fd) throws IOException;

    static native void init();

}
