/*
 * @(#)DSAParameterSpec.java	1.16 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.security.spec;

import java.math.BigInteger;

/**
 * This class specifies the set of parameters used with the DSA algorithm.
 * 
 * @author Jan Luehe
 *
 * @version 1.16, 12/19/03
 *
 * @see AlgorithmParameterSpec
 *
 * @since 1.2
 */

public class DSAParameterSpec implements AlgorithmParameterSpec,
java.security.interfaces.DSAParams {

    BigInteger p;
    BigInteger q;
    BigInteger g;

    /**
     * Creates a new DSAParameterSpec with the specified parameter values.
     * 
     * @param p the prime.
     * 
     * @param q the sub-prime.
     * 
     * @param g the base.
     */
    public DSAParameterSpec(BigInteger p, BigInteger q, BigInteger g) {
	this.p = p;
	this.q = q;
	this.g = g;
    }

    /**
     * Returns the prime <code>p</code>.
     *
     * @return the prime <code>p</code>.
     */
    public BigInteger getP() {
	return this.p;
    }

    /**
     * Returns the sub-prime <code>q</code>.
     *
     * @return the sub-prime <code>q</code>.
     */
    public BigInteger getQ() {
	return this.q;
    }

    /**
     * Returns the base <code>g</code>.
     *
     * @return the base <code>g</code>.
     */
    public BigInteger getG() {
	return this.g;
    }    
}
