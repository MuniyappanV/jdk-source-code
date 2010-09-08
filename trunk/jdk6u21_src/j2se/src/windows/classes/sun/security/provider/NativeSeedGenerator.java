/*
 * @(#)NativeSeedGenerator.java	1.6 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.provider;

import java.io.IOException;

/**
 * Seed generator for Windows making use of MS CryptoAPI using native code.
 *
 * @version 1.6, 03/23/10
 */
class NativeSeedGenerator extends SeedGenerator {

    /**
     * Create a new CryptoAPI seed generator instances.
     *
     * @exception IOException if CryptoAPI seeds are not available
     * on this platform.
     */
    NativeSeedGenerator() throws IOException {
	super();
	// try generating two random bytes to see if CAPI is available
        if (!nativeGenerateSeed(new byte[2])) {
	    throw new IOException("Required native CryptoAPI features not "
				  + " available on this machine");
	}
    }

    /**
     * Native method to do the actual work.
     */
    private static native boolean nativeGenerateSeed(byte[] result);

    void getSeedBytes(byte[] result) {
	// fill array as a side effect
	if (nativeGenerateSeed(result) == false) {
	    // should never happen if constructor check succeeds
	    throw new InternalError
			    ("Unexpected CryptoAPI failure generating seed");
	}
    }

    byte getSeedByte() {
	byte[] b = new byte[1];
	getSeedBytes(b);
	return b[0];
    }
}
