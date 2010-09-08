/*
 * @(#)NTLMAuthentication.java	1.11 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.net.www.protocol.http;

import java.util.Arrays;
import java.util.StringTokenizer;
import java.util.Random;

import sun.net.www.HeaderParser;

import java.io.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import java.security.*;
import java.net.*;

/**
 * NTLMAuthentication: 
 *
 * @author Michael McMahon
 */

class NTLMAuthentication extends AuthenticationInfo {
 
    private static final long serialVersionUID = 100L;

    static final char NTLM_AUTH = 'N';
    private String hostname;
    private static String defaultDomain; /* Domain to use if not specified by user */

    static {
	
        defaultDomain = (String) java.security.AccessController.doPrivileged( 
	    new java.security.PrivilegedAction() {
	    public Object run() {
		String s = System.getProperty ("http.auth.ntlm.domain");
		if (s == null)
		    return "domain";
		return s;
	    }
        });
    };

    private void init0() {

        hostname = (String) java.security.AccessController.doPrivileged( 
	    new java.security.PrivilegedAction() {
	    public Object run() {
	        String localhost;
	        try {
		    localhost = InetAddress.getLocalHost().getHostName().toUpperCase(); 
	        } catch (UnknownHostException e) {
		     localhost = "localhost";
	        }
	        return localhost;
	    }
        });
        int x = hostname.indexOf ('.');
        if (x != -1) {
	    hostname = hostname.substring (0, x);
        }
    }

    String username;
    String ntdomain;
    String password;

    /**
     * Create a NTLMAuthentication:
     * Username may be specified as domain<BACKSLASH>username in the application Authenticator. 
     * If this notation is not used, then the domain will be taken 
     * from a system property: "http.auth.ntlm.domain".
     */
    public NTLMAuthentication(boolean isProxy, URL url, PasswordAuthentication pw) {
	super(isProxy?PROXY_AUTHENTICATION:SERVER_AUTHENTICATION, NTLM_AUTH, url, "");
	init (pw);
    }

    private void init (PasswordAuthentication pw) {
	this.pw = pw;
	if (pw != null) {
	    String s = pw.getUserName();
	    int i = s.indexOf ('\\');
	    if (i == -1) {
	        username = s;
	        ntdomain = defaultDomain;
	    } else {
	        ntdomain = s.substring (0, i).toUpperCase();
	        username = s.substring (i+1);
	    }
	    password = new String (pw.getPassword());
	} else {
	    /* credentials will be acquired from OS */
	    username = null;
	    ntdomain = null;
	    password = null;
	}
	init0();
    }

   /** 
    * Constructor used for proxy entries
    */
    public NTLMAuthentication(boolean isProxy, String host, int port, 
				PasswordAuthentication pw) {
	super(isProxy?PROXY_AUTHENTICATION:SERVER_AUTHENTICATION, NTLM_AUTH,host, port, "");
	init (pw);
    }

    /**
     * @return true if this authentication supports preemptive authorization
     */
    boolean supportsPreemptiveAuthorization() {
	return false;
    }

    /**
     * @return true if NTLM supported transparently (no password needed, SSO)
     */
    static boolean supportsTransparentAuth() {
	return true;
    }

    /**
     * @return the name of the HTTP header this authentication wants set
     */
    String getHeaderName() {
	if (type == SERVER_AUTHENTICATION) {
	    return "Authorization";
	} else {
	    return "Proxy-authorization";
	}
    }

    /**
     * Not supported. Must use the setHeaders() method
     */
    String getHeaderValue(URL url, String method) {
	throw new RuntimeException ("getHeaderValue not supported");
    }

    /**
     * Check if the header indicates that the current auth. parameters are stale.
     * If so, then replace the relevant field with the new value
     * and return true. Otherwise return false.
     * returning true means the request can be retried with the same userid/password
     * returning false means we have to go back to the user to ask for a new
     * username password.
     */
    boolean isAuthorizationStale (String header) {
	return false; /* should not be called for ntlm */
    }
	
    /**
     * Set header(s) on the given connection.
     * @param conn The connection to apply the header(s) to
     * @param p A source of header values for this connection, not used because
     * 		HeaderParser converts the fields to lower case, use raw instead
     * @param raw The raw header field.
     * @return true if all goes well, false if no headers were set.
     */
    synchronized boolean setHeaders(HttpURLConnection conn, HeaderParser p, String raw) {

	try {
	    NTLMAuthSequence seq = (NTLMAuthSequence)conn.authObj;
	    if (seq == null) {
		seq = new NTLMAuthSequence (username, password, ntdomain);
		conn.authObj = seq;
	    }
	    String response = "NTLM " + seq.getAuthHeader (raw.length()>6?raw.substring(5):null);
    	    conn.setAuthenticationProperty(getHeaderName(), response);
    	    return true;
	} catch (IOException e) {
	    return false;
	} 
    }

    /* This is a no-op for NTLM, because there is no authentication information
     * provided by the server to the client
     */
    public void checkResponse (String header, String method, URL url) throws IOException { 
    }
}
