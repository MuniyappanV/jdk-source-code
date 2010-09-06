/*
 * @(#)NTSystem.java	1.9 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.security.auth.module;

import javax.security.auth.login.LoginException;

/**
 * <p> This class implementation retrieves and makes available NT
 * security information for the current user.
 * 
 * @version 1.9, 12/19/03
 */
public class NTSystem {
    
    private native void getCurrent(boolean debug);
    
    private String userName;
    private String domain;
    private String domainSID;
    private String userSID;
    private String groupIDs[];
    private String primaryGroupID;
    private long   impersonationToken;
    
    static boolean loadedLibrary = false;

    /**
     * Instantiate an <code>NTSystem</code> and load
     * the native library to access the underlying system information.
     */
    public NTSystem() {
	this(false);
    }

    /**
     * Instantiate an <code>NTSystem</code> and load
     * the native library to access the underlying system information.
     */
    NTSystem(boolean debug) {
        if (!loadedLibrary) {
            loadNative();
            loadedLibrary = true;
        }
	getCurrent(debug);
    }
    
    /**
     * Get the username for the current NT user.
     *
     * <p>
     *
     * @return the username for the current NT user.
     */
    public String getName() {
        return userName;
    }
    
    /**
     * Get the domain for the current NT user.
     *
     * <p>
     *
     * @return the domain for the current NT user.
     */
    public String getDomain() {
        return domain;
    }
    
    /**
     * Get a printable SID for the current NT user's domain.
     *
     * <p>
     *
     * @return a printable SID for the current NT user's domain.
     */
    public String getDomainSID() {
        return domainSID;
    }
        
    /**
     * Get a printable SID for the current NT user.
     *
     * <p>
     *
     * @return a printable SID for the current NT user.
     */
    public String getUserSID() {
        return userSID;
    }
    
    /**
     * Get a printable primary group SID for the current NT user.
     *
     * <p>
     *
     * @return the primary group SID for the current NT user.
     */
    public String getPrimaryGroupID() {
        return primaryGroupID;
    }
    
    /**
     * Get the printable group SIDs for the current NT user.
     *
     * <p>
     *
     * @return the group SIDs for the current NT user.
     */
    public String[] getGroupIDs() {
        return groupIDs;
    }
    
    /**
     * Get an impersonation token for the current NT user.
     *
     * <p>
     *
     * @return an impersonation token for the current NT user.
     */
    public long getImpersonationToken() {
        return impersonationToken;
    }
    
    private void loadNative() {
	System.loadLibrary("jaas_nt");
    }
}
