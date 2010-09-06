/*
 * @(#)DSAPrivateKey.java	1.57 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.provider;

import java.util.*;
import java.io.*;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.ProviderException;
import java.security.AlgorithmParameters;
import java.security.spec.DSAParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import java.security.interfaces.DSAParams;

import sun.security.x509.AlgIdDSA;
import sun.security.pkcs.PKCS8Key;
import sun.security.util.Debug;
import sun.security.util.DerValue;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;

/**
 * A PKCS#8 private key for the Digital Signature Algorithm.
 *
 * @author Benjamin Renaud
 *
 * @version 1.57, 12/19/03
 *
 * @see DSAPublicKey
 * @see AlgIdDSA
 * @see DSA
 */

public final class DSAPrivateKey extends PKCS8Key 
implements java.security.interfaces.DSAPrivateKey, Serializable {

    /** use serialVersionUID from JDK 1.1. for interoperability */
    private static final long serialVersionUID = -3244453684193605938L;

    /* the private key */
    private BigInteger x;

    /*
     * Keep this constructor for backwards compatibility with JDK1.1.
     */
    public DSAPrivateKey() {
    }

    /**
     * Make a DSA private key out of a private key and three parameters.
     */
    public DSAPrivateKey(BigInteger x, BigInteger p, 
			 BigInteger q, BigInteger g) 
    throws InvalidKeyException {
	this.x = x;
	algid = new AlgIdDSA(p, q, g);

	try {
	    key = new DerValue(DerValue.tag_Integer, 
			       x.toByteArray()).toByteArray();
	    encode();
	} catch (IOException e) {
	    InvalidKeyException ike = new InvalidKeyException(
                "could not DER encode x: " + e.getMessage());
	    ike.initCause(e);
	    throw ike;
	}
    }

    /**
     * Make a DSA private key from its DER encoding (PKCS #8).
     */
    public DSAPrivateKey(byte[] encoded) throws InvalidKeyException {
	clearOldKey();
	decode(encoded);
    }

    /**
     * Returns the DSA parameters associated with this key, or null if the
     * parameters could not be parsed.
     */
    public DSAParams getParams() {
	try {
	    if (algid instanceof DSAParams) {
		return (DSAParams)algid;
	    } else {
		DSAParameterSpec paramSpec;
		AlgorithmParameters algParams = algid.getParameters();
		if (algParams == null) {
		    return null;
		}
		paramSpec = (DSAParameterSpec)algParams.getParameterSpec
		    (DSAParameterSpec.class);
		return (DSAParams)paramSpec;
	    }
	} catch (InvalidParameterSpecException e) {
	    return null;
	}
    }

    /**
     * Get the raw private key, x, without the parameters.
     *
     * @see getParameters
     */
    public BigInteger getX() {
	return x;
    }

    private void clearOldKey() {
	int i;
	if (this.encodedKey != null) {
	    for (i = 0; i < this.encodedKey.length; i++) {
		this.encodedKey[i] = (byte)0x00;
	    }
	}
	if (this.key != null) {
	    for (i = 0; i < this.key.length; i++) {
		this.key[i] = (byte)0x00;
	    }
	}
    }

    public String toString() {
	return "Sun DSA Private Key \nparameters:" + algid + "\nx: " +
	    Debug.toHexString(x) + "\n";
    }

    protected void parseKeyBits() throws InvalidKeyException {
	try {
	    DerInputStream in = new DerInputStream(key);
	    x = in.getBigInteger();
	} catch (IOException e) {
	    InvalidKeyException ike = new InvalidKeyException(e.getMessage());
	    ike.initCause(e);
	    throw ike;
	}
    }
}
