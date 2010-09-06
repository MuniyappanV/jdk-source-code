/*
 * @(#)ServiceUnavailableException.java	1.8 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.naming;

/**
  * This exception is thrown when attempting to communicate with a
  * directory or naming service and that service is not available.
  * It might be unavailable for different reasons. For example,
  * the server might be too busy to service the request, or the server
  * might not be registered to service any requests, etc.
  * <p>
  * Synchronization and serialization issues that apply to NamingException
  * apply directly here.
  *
  * @author Rosanna Lee
  * @author Scott Seligman
  *
  * @version 1.8 03/12/19
  * @since 1.3
  */

public class ServiceUnavailableException extends NamingException {
    /**
     * Constructs a new instance of ServiceUnavailableException using an
     * explanation. All other fields default to null.
     *
     * @param	explanation	Possibly null additional detail about this exception.
     * @see java.lang.Throwable#getMessage
     */
    public ServiceUnavailableException(String explanation) {
	super(explanation);
    }

    /**
      * Constructs a new instance of ServiceUnavailableException.
      * All fields default to null.
      */
    public ServiceUnavailableException() {
	super();
    }

    /**
     * Use serialVersionUID from JNDI 1.1.1 for interoperability
     */
    private static final long serialVersionUID = -4996964726566773444L;
}
