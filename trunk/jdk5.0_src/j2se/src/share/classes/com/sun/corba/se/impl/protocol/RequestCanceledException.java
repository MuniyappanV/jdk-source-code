/*
 * @(#)RequestCanceledException.java	1.7 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.corba.se.impl.protocol;

/**
 * If this exception is caught explicitly, this need to be rethrown.
 */
public class RequestCanceledException extends RuntimeException {

    private int requestId = 0;

    public RequestCanceledException(int requestId) {
        this.requestId = requestId;
    }

    public int getRequestId() {
        return this.requestId;
    }
}
