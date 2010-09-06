/*
 * @(#)NamingSecurityException.java	1.8 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.naming;

/**
  * This is the superclass of security-related exceptions 
  * thrown by operations in the Context and DirContext interfaces.
  * The nature of the failure is described by the name of the subclass.
  *<p>
  * If the program wants to handle this exception in particular, it
  * should catch NamingSecurityException explicitly before attempting to
  * catch NamingException. A program might want to do this, for example,
  * if it wants to treat security-related exceptions specially from
  * other sorts of naming exception.
  * <p>
  * Synchronization and serialization issues that apply to NamingException
  * apply directly here.
  *
  * @author Rosanna Lee
  * @author Scott Seligman
  * @version 1.8 03/12/19
  * @since 1.3
  */

public abstract class NamingSecurityException extends NamingException {
    /**
     * Constructs a new instance of NamingSecurityException using the
     * explanation supplied. All other fields default to null.
     *
     * @param	explanation	Possibly null additional detail about this exception.
     * @see java.lang.Throwable#getMessage
     */
    public NamingSecurityException(String explanation) {
	super(explanation);
    }

    /**
      * Constructs a new instance of NamingSecurityException.
      * All fields are initialized to null.
      */
    public NamingSecurityException() {
	super();
    }

    /**
     * Use serialVersionUID from JNDI 1.1.1 for interoperability
     */
    private static final long serialVersionUID = 5855287647294685775L;
};
