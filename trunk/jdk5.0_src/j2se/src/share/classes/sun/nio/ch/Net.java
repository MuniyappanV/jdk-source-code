/*
 * @(#)Net.java	1.27 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.nio.ch;

import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.channels.*;


class Net {						// package-private

    private Net() { }


    // -- Miscellaneous utilities --

    static InetSocketAddress checkAddress(SocketAddress sa) {
	if (sa == null)
	    throw new IllegalArgumentException();
	if (!(sa instanceof InetSocketAddress))
	    throw new UnsupportedAddressTypeException(); // ## needs arg
	InetSocketAddress isa = (InetSocketAddress)sa;
	if (isa.isUnresolved())
	    throw new UnresolvedAddressException(); // ## needs arg
	return isa;
    }

    static InetSocketAddress asInetSocketAddress(SocketAddress sa) {
	if (!(sa instanceof InetSocketAddress))
	    throw new UnsupportedAddressTypeException();
	return (InetSocketAddress)sa;
    }

    static void translateToSocketException(Exception x)
	throws SocketException
    {
	if (x instanceof SocketException) 
	    throw (SocketException)x;
	Exception nx = x;
	if (x instanceof ClosedChannelException)
	    nx = new SocketException("Socket is closed");
	else if (x instanceof AlreadyBoundException)
	    nx = new SocketException("Already bound");
	else if (x instanceof NotYetBoundException)
	    nx = new SocketException("Socket is not bound yet");
	else if (x instanceof UnsupportedAddressTypeException)
	    nx = new SocketException("Unsupported address type");
	else if (x instanceof UnresolvedAddressException) {
	    nx = new SocketException("Unresolved address");
	}
	if (nx != x) 
	    nx.initCause(x);

	if (nx instanceof RuntimeException)
	    throw (RuntimeException)nx;
	else
	    throw new Error("Untranslated exception", nx);
    }

    static void translateException(Exception x,
				   boolean unknownHostForUnresolved)
	throws IOException
    {
	if (x instanceof IOException)
	    throw (IOException)x;
        // Throw UnknownHostException from here since it cannot
        // be thrown as a SocketException
	if (unknownHostForUnresolved &&
            (x instanceof UnresolvedAddressException))
        {
             throw new UnknownHostException();
        }
	translateToSocketException(x);
    }

    static void translateException(Exception x)
	throws IOException
    {
	translateException(x, false);
    }


    // -- Socket operations --

    static FileDescriptor socket(boolean stream) {
	return IOUtil.newFD(socket0(stream, false));
    }

    static FileDescriptor serverSocket(boolean stream) {
	return IOUtil.newFD(socket0(stream, true));
    }

    // Due to oddities SO_REUSEADDR on windows reuse is ignored
    private static native int socket0(boolean stream, boolean reuse);

    static native void bind(FileDescriptor fd, InetAddress addr, int port)
        throws IOException;

    static native int connect(FileDescriptor fd,
			      InetAddress remote,
			      int remotePort,
			      int trafficClass)
        throws IOException;


    private static native int localPort(FileDescriptor fd)
	throws IOException;

    private static native InetAddress localInetAddress(FileDescriptor fd)
	throws IOException;

    static InetSocketAddress localAddress(FileDescriptor fd) {
	try {
	    return new InetSocketAddress(localInetAddress(fd),
					 localPort(fd));
	} catch (IOException x) {
	    throw new Error(x);		// Can't happen
	}
    }

    static int localPortNumber(FileDescriptor fd) {
	try {
	    return localPort(fd);
	} catch (IOException x) {
	    throw new Error(x);		// Can't happen
	}
    }

    private static native int getIntOption0(FileDescriptor fd, int opt)
	throws IOException;

    static int getIntOption(FileDescriptor fd, int opt)
	throws IOException
    {
	return getIntOption0(fd, opt);
    }


    private static native void setIntOption0(FileDescriptor fd,
					     int opt, int arg)
	throws IOException;

    static void setIntOption(FileDescriptor fd, int opt, int arg)
	throws IOException
    {
	setIntOption0(fd, opt, arg);
    }

    private static native void initIDs();

    static {
	Util.load();
        initIDs();
    }

}
