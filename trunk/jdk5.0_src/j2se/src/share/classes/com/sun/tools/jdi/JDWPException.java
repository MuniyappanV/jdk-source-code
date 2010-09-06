/*
 * @(#)JDWPException.java	1.13 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;
import com.sun.jdi.*;

class JDWPException extends Exception {

    short errorCode;

    JDWPException(short errorCode) {
	super();
        this.errorCode = errorCode;
    }

    short errorCode() {
        return errorCode;
    }

    RuntimeException toJDIException() {
        switch (errorCode) {
            case JDWP.Error.INVALID_OBJECT:
                return new ObjectCollectedException();
            case JDWP.Error.VM_DEAD:
                return new VMDisconnectedException();
            case JDWP.Error.OUT_OF_MEMORY:
                return new VMOutOfMemoryException();
            case JDWP.Error.CLASS_NOT_PREPARED:
                return new ClassNotPreparedException();
            case JDWP.Error.INVALID_FRAMEID:
            case JDWP.Error.NOT_CURRENT_FRAME:
                return new InvalidStackFrameException();
            case JDWP.Error.NOT_IMPLEMENTED:
                return new UnsupportedOperationException();
            case JDWP.Error.INVALID_INDEX:
            case JDWP.Error.INVALID_LENGTH: 
                return new IndexOutOfBoundsException();
            case JDWP.Error.TYPE_MISMATCH:
                return new InconsistentDebugInfoException();
            case JDWP.Error.INVALID_THREAD:
                return new IllegalThreadStateException();
            default:
                return new InternalException("Unexpected JDWP Error: " + errorCode, errorCode); 
        }
    }
}
