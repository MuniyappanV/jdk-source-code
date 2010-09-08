/*
 * @(#)Mac.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */
  
/*
 * NOTE:
 * Because of various external restrictions (i.e. US export
 * regulations, etc.), the actual source code can not be provided
 * at this time. This file represents the skeleton of the source
 * file, so that javadocs of the API can be created.
 */

package javax.crypto;

import java.util.*;
import java.security.*;
import sun.security.jca.*;

import java.security.Provider.Service;
import java.security.spec.AlgorithmParameterSpec;
import java.nio.ByteBuffer;
import sun.security.util.Debug;
import sun.security.jca.GetInstance.Instance;

/** 
 * This class provides the functionality of a "Message Authentication Code"
 * (MAC) algorithm.
 *
 * <p> A MAC provides a way to check
 * the integrity of information transmitted over or stored in an unreliable
 * medium, based on a secret key. Typically, message
 * authentication codes are used between two parties that share a secret
 * key in order to validate information transmitted between these
 * parties.
 *
 * <p> A MAC mechanism that is based on cryptographic hash functions is
 * referred to as HMAC. HMAC can be used with any cryptographic hash function,
 * e.g., MD5 or SHA-1, in combination with a secret shared key. HMAC is
 * specified in RFC 2104.
 *
 * @author Jan Luehe
 *
 * @version 1.25, 04/24/06
 * @since 1.4
 */
public class Mac implements Cloneable
{

    /** 
     * Creates a MAC object.
     *
     * @param macSpi the delegate
     * @param provider the provider
     * @param algorithm the algorithm
     */
    protected Mac(MacSpi macSpi, Provider provider, String algorithm) { }

    /** 
     * Returns the algorithm name of this <code>Mac</code> object.
     *
     * <p>This is the same name that was specified in one of the
     * <code>getInstance</code> calls that created this
     * <code>Mac</code> object.
     *
     * @return the algorithm name of this <code>Mac</code> object.
     */
    public final String getAlgorithm() {
        return null;
    }

    /** 
     * Returns a <code>Mac</code> object that implements the
     * specified MAC algorithm.
     *
     * <p> This method traverses the list of registered security Providers,
     * starting with the most preferred Provider.
     * A new Mac object encapsulating the
     * MacSpi implementation from the first
     * Provider that supports the specified algorithm is returned.
     *
     * <p> Note that the list of registered providers may be retrieved via
     * the {@link Security#getProviders() Security.getProviders()} method.
     *
     * @param algorithm the standard name of the requested MAC algorithm.
     * See Appendix A in the <a href=
     *   "{@docRoot}/../technotes/guides/security/crypto/CryptoSpec.html#AppA">
     * Java Cryptography Architecture Reference Guide</a>
     * for information about standard algorithm names.
     *
     * @return the new <code>Mac</code> object.
     *
     * @exception NoSuchAlgorithmException if no Provider supports a
     *		MacSpi implementation for the
     *		specified algorithm.
     *
     * @see java.security.Provider
     */
    public static final Mac getInstance(String algorithm)
        throws NoSuchAlgorithmException
    {
        return null;
    }

    /** 
     * Returns a <code>Mac</code> object that implements the
     * specified MAC algorithm.
     *
     * <p> A new Mac object encapsulating the
     * MacSpi implementation from the specified provider
     * is returned.  The specified provider must be registered
     * in the security provider list.
     *
     * <p> Note that the list of registered providers may be retrieved via
     * the {@link Security#getProviders() Security.getProviders()} method.
     *
     * @param algorithm the standard name of the requested MAC algorithm.
     * See Appendix A in the <a href=
     *   "{@docRoot}/../technotes/guides/security/crypto/CryptoSpec.html#AppA">
     * Java Cryptography Architecture Reference Guide</a>
     * for information about standard algorithm names.
     *
     * @param provider the name of the provider.
     *
     * @return the new <code>Mac</code> object.
     *
     * @exception NoSuchAlgorithmException if a MacSpi
     *		implementation for the specified algorithm is not
     *		available from the specified provider.
     *
     * @exception NoSuchProviderException if the specified provider is not
     *		registered in the security provider list.
     *
     * @exception IllegalArgumentException if the <code>provider</code>
     *		is null or empty.
     *
     * @see java.security.Provider
     */
    public static final Mac getInstance(String algorithm, String provider)
        throws NoSuchAlgorithmException, NoSuchProviderException
    {
        return null;
    }

    /** 
     * Returns a <code>Mac</code> object that implements the
     * specified MAC algorithm.
     *
     * <p> A new Mac object encapsulating the
     * MacSpi implementation from the specified Provider
     * object is returned.  Note that the specified Provider object
     * does not have to be registered in the provider list.
     *
     * @param algorithm the standard name of the requested MAC algorithm.
     * See Appendix A in the <a href=
     *   "{@docRoot}/../technotes/guides/security/crypto/CryptoSpec.html#AppA">
     * Java Cryptography Architecture Reference Guide</a>
     * for information about standard algorithm names.
     *
     * @param provider the provider.
     *
     * @return the new <code>Mac</code> object.
     *
     * @exception NoSuchAlgorithmException if a MacSpi
     *		implementation for the specified algorithm is not available
     *		from the specified Provider object.
     *
     * @exception IllegalArgumentException if the <code>provider</code>
     *		is null.
     *
     * @see java.security.Provider
     */
    public static final Mac getInstance(String algorithm, Provider provider)
        throws NoSuchAlgorithmException
    {
        return null;
    }

    /** 
     * Returns the provider of this <code>Mac</code> object.
     *
     * @return the provider of this <code>Mac</code> object.
     */
    public final Provider getProvider() {
        return null;
    }

    /** 
     * Returns the length of the MAC in bytes.
     *
     * @return the MAC length in bytes.
     */
    public final int getMacLength() {
        return 0;
    }

    /** 
     * Initializes this <code>Mac</code> object with the given key.
     *
     * @param key the key.
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this MAC.
     */
    public final void init(Key key) throws InvalidKeyException { }

    /** 
     * Initializes this <code>Mac</code> object with the given key and
     * algorithm parameters.
     *
     * @param key the key.
     * @param params the algorithm parameters.
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this MAC.
     * @exception InvalidAlgorithmParameterException if the given algorithm
     * parameters are inappropriate for this MAC.
     */
    public final void init(Key key, AlgorithmParameterSpec params)
        throws InvalidKeyException, InvalidAlgorithmParameterException
    { }

    /** 
     * Processes the given byte.
     *
     * @param input the input byte to be processed.
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final void update(byte input) throws IllegalStateException { }

    /** 
     * Processes the given array of bytes.
     *
     * @param input the array of bytes to be processed.
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final void update(byte[] input) throws IllegalStateException { }

    /** 
     * Processes the first <code>len</code> bytes in <code>input</code>,
     * starting at <code>offset</code> inclusive.
     *
     * @param input the input buffer.
     * @param offset the offset in <code>input</code> where the input starts.
     * @param len the number of bytes to process.
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final void update(byte[] input, int offset, int len)
        throws IllegalStateException
    { }

    /** 
     * Processes <code>input.remaining()</code> bytes in the ByteBuffer
     * <code>input</code>, starting at <code>input.position()</code>.
     * Upon return, the buffer's position will be equal to its limit;
     * its limit will not have changed.
     *
     * @param input the ByteBuffer
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     * @since 1.5
     */
    public final void update(ByteBuffer input) { }

    /** 
     * Finishes the MAC operation.
     *
     * <p>A call to this method resets this <code>Mac</code> object to the
     * state it was in when previously initialized via a call to
     * <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     * That is, the object is reset and available to generate another MAC from
     * the same key, if desired, via new calls to <code>update</code> and
     * <code>doFinal</code>.
     * (In order to reuse this <code>Mac</code> object with a different key,
     * it must be reinitialized via a call to <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     *
     * @return the MAC result.
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final byte[] doFinal() throws IllegalStateException {
        return null;
    }

    /** 
     * Finishes the MAC operation.
     *
     * <p>A call to this method resets this <code>Mac</code> object to the
     * state it was in when previously initialized via a call to
     * <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     * That is, the object is reset and available to generate another MAC from
     * the same key, if desired, via new calls to <code>update</code> and
     * <code>doFinal</code>.
     * (In order to reuse this <code>Mac</code> object with a different key,
     * it must be reinitialized via a call to <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     *
     * <p>The MAC result is stored in <code>output</code>, starting at
     * <code>outOffset</code> inclusive.
     *
     * @param output the buffer where the MAC result is stored
     * @param outOffset the offset in <code>output</code> where the MAC is
     * stored
     *
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final void doFinal(byte[] output, int outOffset)
        throws ShortBufferException, IllegalStateException
    { }

    /** 
     * Processes the given array of bytes and finishes the MAC operation.
     *
     * <p>A call to this method resets this <code>Mac</code> object to the
     * state it was in when previously initialized via a call to
     * <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     * That is, the object is reset and available to generate another MAC from
     * the same key, if desired, via new calls to <code>update</code> and
     * <code>doFinal</code>.
     * (In order to reuse this <code>Mac</code> object with a different key,
     * it must be reinitialized via a call to <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     *
     * @param input data in bytes
     * @return the MAC result.
     *
     * @exception IllegalStateException if this <code>Mac</code> has not been
     * initialized.
     */
    public final byte[] doFinal(byte[] input) throws IllegalStateException {
        return null;
    }

    /** 
     * Resets this <code>Mac</code> object.
     *
     * <p>A call to this method resets this <code>Mac</code> object to the
     * state it was in when previously initialized via a call to
     * <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     * That is, the object is reset and available to generate another MAC from
     * the same key, if desired, via new calls to <code>update</code> and
     * <code>doFinal</code>.
     * (In order to reuse this <code>Mac</code> object with a different key,
     * it must be reinitialized via a call to <code>init(Key)</code> or
     * <code>init(Key, AlgorithmParameterSpec)</code>.
     */
    public final void reset() { }

    /** 
     * Returns a clone if the provider implementation is cloneable.
     *
     * @return a clone if the provider implementation is cloneable.
     *
     * @exception CloneNotSupportedException if this is called on a
     * delegate that does not support <code>Cloneable</code>.
     */
    public final Object clone() throws CloneNotSupportedException {
        return null;
    }
}
