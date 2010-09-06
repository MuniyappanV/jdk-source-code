/*
 * @(#)ThreadInfo.java	1.24 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Sun grants you ("Licensee") a non-exclusive, royalty free, license to use,
 * modify and redistribute this software in source and binary code form,
 * provided that i) this copyright notice and license appear on all copies of
 * the software; and ii) Licensee does not utilize the software in a manner
 * which is disparaging to Sun.
 * 
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE
 * LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THE SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS
 * LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT,
 * INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 * 
 * This software is not designed or intended for use in on-line control of
 * aircraft, air traffic, aircraft navigation or aircraft communications; or in
 * the design, construction, operation or maintenance of any nuclear
 * facility. Licensee represents and warrants that it will not use or
 * redistribute the Software for such purposes.
 */

package com.sun.tools.example.debug.tty;

import com.sun.jdi.ThreadReference;
import com.sun.jdi.ThreadGroupReference;
import com.sun.jdi.IncompatibleThreadStateException;
import com.sun.jdi.StackFrame;
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.io.*;

class ThreadInfo {
    // This is a list of all known ThreadInfo objects. It survives 
    // ThreadInfo.invalidateAll, unlike the other static fields below. 
    private static List threads = Collections.synchronizedList(new ArrayList());
    private static boolean gotInitialThreads = false;

    private static ThreadInfo current = null;
    private static ThreadGroupReference group = null;

    private final ThreadReference thread;
    private int currentFrameIndex = 0;

    private ThreadInfo(ThreadReference thread) {
        this.thread = thread;
        if (thread == null) {
            MessageOutput.fatalError("Internal error: null ThreadInfo created");
        }
    }

    private static void initThreads() {
        if (!gotInitialThreads) {
            Iterator iter = Env.vm().allThreads().iterator();
            while (iter.hasNext()) {
                ThreadReference thread = (ThreadReference)iter.next();
                threads.add(new ThreadInfo(thread));
            }
            gotInitialThreads = true;
        }
    }

    static void addThread(ThreadReference thread) {
        synchronized (threads) {
            initThreads();
            ThreadInfo ti = new ThreadInfo(thread);
            // Guard against duplicates. Duplicates can happen during 
            // initialization when a particular thread might be added both
            // by a thread start event and by the initial call to threads()
            if (getThreadInfo(thread) == null) {
                threads.add(ti);
            }
        }
    }

    static void removeThread(ThreadReference thread) {
        if (thread.equals(ThreadInfo.current)) {
            // Current thread has died.

            // Be careful getting the thread name. If its death happens
            // as part of VM termination, it may be too late to get the 
            // information, and an exception will be thrown.
            String currentThreadName;
            try {
               currentThreadName = "\"" + thread.name() + "\"";
            } catch (Exception e) {
               currentThreadName = "";
            }
                 
            setCurrentThread(null);

            MessageOutput.println();
            MessageOutput.println("Current thread died. Execution continuing...",
                                  currentThreadName);
        }
        threads.remove(getThreadInfo(thread));
    }

    static List threads() {
        synchronized(threads) {
            initThreads();
            // Make a copy to allow iteration without synchronization
            return new ArrayList(threads);
        }
    }

    static void invalidateAll() {
        current = null;
        group = null;
        synchronized (threads) {
            Iterator iter = threads().iterator();
            while (iter.hasNext()) {
                ThreadInfo ti = (ThreadInfo)iter.next();
                ti.invalidate();
            }
        }
    }

    static void setThreadGroup(ThreadGroupReference tg) {	
        group = tg;
    }
    
    static void setCurrentThread(ThreadReference tr) {
        if (tr == null) {
            setCurrentThreadInfo(null);
        } else {
            ThreadInfo tinfo = getThreadInfo(tr);
            setCurrentThreadInfo(tinfo);
        }
    }

    static void setCurrentThreadInfo(ThreadInfo tinfo) { 
        current = tinfo;
        if (current != null) {
            current.invalidate();
        }
    }

    /**
     * Get the current ThreadInfo object.
     *
     * @return the ThreadInfo for the current thread.
     */
    static ThreadInfo getCurrentThreadInfo() {
        return current;
    }
    
    /**
     * Get the thread from this ThreadInfo object.
     *
     * @return the Thread wrapped by this ThreadInfo.
     */
    ThreadReference getThread() {
        return thread;
    }

    static ThreadGroupReference group() {
	if (group == null) {
            // Current thread group defaults to the first top level
            // thread group.
	    setThreadGroup((ThreadGroupReference)
                           Env.vm().topLevelThreadGroups().get(0));
	}
        return group;
    }
    
    static ThreadInfo getThreadInfo(long id) {
        ThreadInfo retInfo = null;

        synchronized (threads) {
            Iterator iter = threads().iterator();
            while (iter.hasNext()) {
                ThreadInfo ti  = (ThreadInfo)iter.next();
                if (ti.thread.uniqueID() == id) {
                   retInfo = ti;
                   break;
                }
            }
        }
        return retInfo;
    }

    static ThreadInfo getThreadInfo(ThreadReference tr) {
        return getThreadInfo(tr.uniqueID());
    }

    static ThreadInfo getThreadInfo(String idToken) {
        ThreadInfo tinfo = null;
        if (idToken.startsWith("t@")) {
            idToken = idToken.substring(2);
        }
        try {
            long threadId = Long.decode(idToken).longValue();
            tinfo = getThreadInfo(threadId);
        } catch (NumberFormatException e) {
            tinfo = null;
        }
        return tinfo;
    }

    /**
     * Get the thread stack frames.
     *
     * @return a <code>List</code> of the stack frames.
     */
    List getStack() throws IncompatibleThreadStateException {
        return thread.frames();
    }

    /**
     * Get the current stackframe.
     *
     * @return the current stackframe.
     */
    StackFrame getCurrentFrame() throws IncompatibleThreadStateException {
        if (thread.frameCount() == 0) {
            return null;
        }
        return thread.frame(currentFrameIndex);
    }

    /**
     * Invalidate the current stackframe index.
     */
    void invalidate() {
        currentFrameIndex = 0;
    }

    /* Throw IncompatibleThreadStateException if not suspended */
    private void assureSuspended() throws IncompatibleThreadStateException {
        if (!thread.isSuspended()) {
            throw new IncompatibleThreadStateException();
        }
    }

    /**
     * Get the current stackframe index.
     *
     * @return the number of the current stackframe.  Frame zero is the
     * closest to the current program counter
     */
    int getCurrentFrameIndex() {
        return currentFrameIndex;
    }

    /**
     * Set the current stackframe to a specific frame.
     *
     * @param nFrame	the number of the desired stackframe.  Frame zero is the
     * closest to the current program counter
     * @exception IllegalAccessError when the thread isn't 
     * suspended or waiting at a breakpoint
     * @exception ArrayIndexOutOfBoundsException when the 
     * requested frame is beyond the stack boundary
     */
    void setCurrentFrameIndex(int nFrame) throws IncompatibleThreadStateException {
        assureSuspended();
        if ((nFrame < 0) || (nFrame >= thread.frameCount())) {
            throw new ArrayIndexOutOfBoundsException();
        }
        currentFrameIndex = nFrame;
    }

    /**
     * Change the current stackframe to be one or more frames higher
     * (as in, away from the current program counter).
     *
     * @param nFrames	the number of stackframes
     * @exception IllegalAccessError when the thread isn't 
     * suspended or waiting at a breakpoint
     * @exception ArrayIndexOutOfBoundsException when the 
     * requested frame is beyond the stack boundary
     */
    void up(int nFrames) throws IncompatibleThreadStateException {
        setCurrentFrameIndex(currentFrameIndex + nFrames);
    }

    /**
     * Change the current stackframe to be one or more frames lower
     * (as in, toward the current program counter).     *
     * @param nFrames	the number of stackframes
     * @exception IllegalAccessError when the thread isn't 
     * suspended or waiting at a breakpoint
     * @exception ArrayIndexOutOfBoundsException when the 
     * requested frame is beyond the stack boundary
     */
    void down(int nFrames) throws IncompatibleThreadStateException {
        setCurrentFrameIndex(currentFrameIndex - nFrames);
    }

}
                            
