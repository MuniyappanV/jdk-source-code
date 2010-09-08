/*
 * @(#)PRNG.java	1.4 10/03/23
 *
 * Copyright (c) 2006, 2008, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.mscapi;

import java.security.ProviderException;
import java.security.SecureRandomSpi;

/**
 * Native PRNG implementation for Windows using the Microsoft Crypto API.
 *
 * @since 1.6
 */

public final class PRNG extends SecureRandomSpi
    implements java.io.Serializable {

    // TODO - generate the serialVersionUID
    //private static final long serialVersionUID = XXX;

    /*
     * The CryptGenRandom function fills a buffer with cryptographically random
     * bytes.
     *
     * @exception ProviderException if MS-CAPI cannot acquire the resources
     *            needed to generate random data.
     */
    private static native byte[] generateSeed(int length, byte[] seed);

    /**
     * Creates a random number generator.
     */
    public PRNG() {
    }

    /**
     * Reseeds this random object. The given seed supplements, rather than
     * replaces, the existing seed. Thus, repeated calls are guaranteed
     * never to reduce randomness.
     *
     * @param seed the seed.
     */
    protected void engineSetSeed(byte[] seed) {
	if (seed != null) {
	    generateSeed(-1, seed);
	}
    }

    /**
     * Generates a user-specified number of random bytes.
     *
     * @param bytes the array to be filled in with random bytes.
     */
    protected void engineNextBytes(byte[] bytes) {
	if (bytes != null) {
	    if (generateSeed(0, bytes) == null) {
		throw new ProviderException("Error generating random bytes");
	    }
	}
    }

    /**
     * Returns the given number of seed bytes.  This call may be used to
     * seed other random number generators.
     *
     * @param numBytes the number of seed bytes to generate.
     *
     * @return the seed bytes.
     */
    protected byte[] engineGenerateSeed(int numBytes) {
	byte[] seed = generateSeed(numBytes, null);

	if (seed == null) {
	    throw new ProviderException("Error generating seed bytes");
	}
	return seed;
    }
}
