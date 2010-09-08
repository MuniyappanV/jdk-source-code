/*
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

/*
 * @(#)SourceChannelImpl.java	1.14 10/03/23
 */

package sun.nio.ch;

import java.io.IOException;
import java.io.FileDescriptor;
import java.nio.ByteBuffer;
import java.nio.channels.*;
import java.nio.channels.spi.*;

/** 
 * Pipe.SourceChannel implementation based on socket connection.
 */

class SourceChannelImpl
    extends Pipe.SourceChannel
    implements SelChImpl
{
    // The SocketChannel assoicated with this pipe
    SocketChannel sc;

    public FileDescriptor getFD() {
	return ((SocketChannelImpl) sc).getFD();
    }

    public int getFDVal() {
	return ((SocketChannelImpl) sc).getFDVal();
    }

    SourceChannelImpl(SelectorProvider sp, SocketChannel sc) {
	super(sp);
	this.sc = sc;
    }

    protected void implCloseSelectableChannel() throws IOException {
	if (!isRegistered())
	    kill();
    }

    public void kill() throws IOException {
	sc.close();
    }

    protected void implConfigureBlocking(boolean block) throws IOException {
	sc.configureBlocking(block);
    }

    public boolean translateReadyOps(int ops, int initialOps,
                                     SelectionKeyImpl sk) {
        int intOps = sk.nioInterestOps(); // Do this just once, it synchronizes
        int oldOps = sk.nioReadyOps();
        int newOps = initialOps;

        if ((ops & PollArrayWrapper.POLLNVAL) != 0)
            throw new Error("POLLNVAL detected");

        if ((ops & (PollArrayWrapper.POLLERR
                    | PollArrayWrapper.POLLHUP)) != 0) {
            newOps = intOps;
            sk.nioReadyOps(newOps);
            return (newOps & ~oldOps) != 0;
        }

        if (((ops & PollArrayWrapper.POLLIN) != 0) &&
            ((intOps & SelectionKey.OP_READ) != 0))
            newOps |= SelectionKey.OP_READ;

        sk.nioReadyOps(newOps);
        return (newOps & ~oldOps) != 0;
    }

    public boolean translateAndUpdateReadyOps(int ops, SelectionKeyImpl sk) {
        return translateReadyOps(ops, sk.nioReadyOps(), sk);
    }

    public boolean translateAndSetReadyOps(int ops, SelectionKeyImpl sk) {
        return translateReadyOps(ops, 0, sk);
    }

    public void translateAndSetInterestOps(int ops, SelectionKeyImpl sk) {
        if ((ops & SelectionKey.OP_READ) != 0)
            ops = PollArrayWrapper.POLLIN;
        sk.selector.putEventOps(sk, ops);
    }

    public int read(ByteBuffer dst) throws IOException {
	try {
	    return sc.read(dst);
	} catch (AsynchronousCloseException x) {
	    close();
	    throw x;
	}
    }

    public long read(ByteBuffer[] dsts, int offset, int length)
	throws IOException
    {
        if ((offset < 0) || (length < 0) || (offset > dsts.length - length))
           throw new IndexOutOfBoundsException();
	try {
	    return read(Util.subsequence(dsts, offset, length));
	} catch (AsynchronousCloseException x) {
	    close();
	    throw x;
	}
    }

    public long read(ByteBuffer[] dsts) throws IOException {
	try {
	    return sc.read(dsts);
	} catch (AsynchronousCloseException x) {
	    close();
	    throw x;
	}
    }

}
