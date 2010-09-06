/*
 * @(#)AsynchInvoke.java	1.12 03/01/23
 *
 * Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*
 * Licensed Materials - Property of IBM
 * RMI-IIOP v1.0
 * Copyright IBM Corp. 1998 1999  All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

package com.sun.corba.se.internal.corba;


///////////////////////////////////////////////////////////////////////////
// helper class for deferred invocations

/*
 * The AsynchInvoke class allows for the support of asynchronous
 * invocations. Instances of these are created with a request object,
 * and when run, perform an invocation. The instance is also
 * responsible for waking up a client that might be waiting on the
 * 'get_response' method.
 */

class AsynchInvoke implements Runnable {

    private RequestImpl _req;
    private ORB         _orb;
    private boolean     _notifyORB;

    AsynchInvoke (ORB o, RequestImpl reqToInvokeOn, boolean n)
    {
	_orb = o;
	_req = reqToInvokeOn;
	_notifyORB = n;
    };


    /*
     * The run operation calls the invocation on the request object,
     * updates the RequestImpl state to indicate that the asynchronous
     * invocation is complete, and wakes up any client that might be
     * waiting on a 'get_response' call.
     *
     */

    public void run() 
    {

	// do the actual invocation
	_req.doInvocation();
    
	// for the asynchronous case, note that the response has been
	// received.
	synchronized (_req)
	    {
		// update local boolean indicator
		_req.gotResponse = true;

		// notify any client waiting on a 'get_response'
		_req.notify();
	    }
      
	if (_notifyORB == true) {
	    synchronized(_orb._svResponseReceived) {
		_orb.notifyResponse();
	    }
	}
    }

};

///////////////////////////////////////////////////////////////////////////