/*
 * @(#)ECPublicKeyImpl.java	1.2 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.ec;

import java.io.IOException;

import java.security.*;
import java.security.interfaces.*;
import java.security.spec.*;

import sun.security.util.*;
import sun.security.x509.*;

/**
 * Key implementation for EC public keys.
 *
 * @since   1.6
 * @version 1.2, 03/23/10
 * @author  Andreas Sterbenz
 */
public final class ECPublicKeyImpl extends X509Key implements ECPublicKey {

    private static final long serialVersionUID = -2462037275160462289L;
    
    private ECPoint w;
    private ECParameterSpec params;
    
    /**
     * Construct a key from its components. Used by the
     * ECKeyFactory and SunPKCS11.
     */
    public ECPublicKeyImpl(ECPoint w, ECParameterSpec params)
	    throws InvalidKeyException {
	this.w = w;
	this.params = params;
	// generate the encoding
	algid = new AlgorithmId
	    (AlgorithmId.EC_oid, ECParameters.getAlgorithmParameters(params));
	key = ECParameters.encodePoint(w, params.getCurve());
    }
    
    /**
     * Construct a key from its encoding. Used by RSAKeyFactory.
     */
    public ECPublicKeyImpl(byte[] encoded) throws InvalidKeyException {
	decode(encoded);
    }
    
    // see JCA doc
    public String getAlgorithm() {
	return "EC";
    }
    
    // see JCA doc
    public ECPoint getW() {
	return w;
    }
    
    // see JCA doc
    public ECParameterSpec getParams() {
	return params;
    }

    // Internal API to get the encoded point. Currently used by SunPKCS11.
    // This may change/go away depending on what we do with the public API.
    public byte[] getEncodedPublicValue() {
	return key.clone();
    }
    
    /**
     * Parse the key. Called by X509Key.
     */
    protected void parseKeyBits() throws InvalidKeyException {
	try {
	    AlgorithmParameters algParams = this.algid.getParameters();
	    params = algParams.getParameterSpec(ECParameterSpec.class);
	    w = ECParameters.decodePoint(key, params.getCurve());
	} catch (IOException e) {
	    throw new InvalidKeyException("Invalid EC key", e);
	} catch (InvalidParameterSpecException e) {
	    throw new InvalidKeyException("Invalid EC key", e);
	}
    }
    
    // return a string representation of this key for debugging
    public String toString() {
	return "Sun EC public key, " + params.getCurve().getField().getFieldSize()
	    + " bits\n  public x coord: " + w.getAffineX()
	    + "\n  public y coord: " + w.getAffineY()
	    + "\n  parameters: " + params;
    }

    protected Object writeReplace() throws java.io.ObjectStreamException {
	return new KeyRep(KeyRep.Type.PUBLIC,
			getAlgorithm(),
			getFormat(),
			getEncoded());
    }
}
