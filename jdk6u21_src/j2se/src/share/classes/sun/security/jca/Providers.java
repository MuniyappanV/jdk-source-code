/*
 * @(#)Providers.java	1.9 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.jca;

import java.util.*;

import java.security.AccessController;
import java.security.Policy;
import java.security.PrivilegedAction;
import java.security.Provider;
import java.security.Security;

/**
 * Collection of methods to get and set provider list. Also includes
 * special code for the provider list during JAR verification.
 *
 * @author  Andreas Sterbenz
 * @version 1.9, 03/23/10
 * @since   1.5
 */
public class Providers {
    
    private static final ThreadLocal<ProviderList> threadLists =
	new InheritableThreadLocal<ProviderList>();
	
    // number of threads currently using thread-local provider lists
    // tracked to allow an optimization if == 0
    private static volatile int threadListsUsed;
    
    // current system-wide provider list
    // Note volatile immutable object, so no synchronization needed.
    private static volatile ProviderList providerList;
    
    private static volatile boolean policyInitialized;

    static {
	// set providerList to empty list first in case initialization somehow
	// triggers a getInstance() call (although that should not happen)
	providerList = ProviderList.EMPTY;
	providerList = ProviderList.fromSecurityProperties();
    }
    
    private Providers() {
	// empty
    }

    // we need special handling to resolve circularities when loading
    // signed JAR files during startup. The code below is part of that.
    
    // Basically, before we load data from a signed JAR file, we parse
    // the PKCS#7 file and verify the signature. We need a
    // CertificateFactory, Signatures, etc. to do that. We have to make
    // sure that we do not try to load the implementation from the JAR
    // file we are just verifying.
    //
    // To avoid that, we use different provider settings during JAR
    // verification.  However, we do not want those provider settings to 
    // interfere with other parts of the system. Therefore, we make them local
    // to the Thread executing the JAR verification code.
    //
    // The code here is used by sun.security.util.SignatureFileVerifier.
    // See there for details.
    
    // Hardcoded classnames of providers to use for JAR verification.
    // MUST NOT be in signed JAR files.
    private static final String[] jarClassNames = {
	"sun.security.provider.Sun",
	"sun.security.rsa.SunRsaSign",
    };
    
    /**
     * Start JAR verification. This sets a special provider list for
     * the current thread. You MUST save the return value from this
     * method and you MUST call stopJarVerification() with that object
     * once you are done.
     */
    public static Object startJarVerification() {
	ProviderList currentList = getProviderList();
	ProviderList jarList = currentList.getJarList(jarClassNames);
	// return the old thread-local provider list, usually null
	return beginThreadProviderList(jarList);
    }
    
    /**
     * Stop JAR verification. Call once you have completed JAR verification.
     */
    public static void stopJarVerification(Object obj) {
	// restore old thread-local provider list
	endThreadProviderList((ProviderList)obj);
    }
    
    /**
     * Return the current ProviderList. If the thread-local list is set,
     * it is returned. Otherwise, the system wide list is returned.
     */
    public static ProviderList getProviderList() {
	if (!policyInitialized) {
	    try {
		if (System.getSecurityManager() != null) {

		    // trigger policy initialization - see 6424631
		    AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
			    Policy.getPolicy();
			    return null;
			}       
		    });
	  	}
		policyInitialized = true;
	    } catch (Exception e) {
		if (ProviderList.debug != null) {
		    ProviderList.debug.println("policy init failed");
		    e.printStackTrace();
		}
	    }
	}

	ProviderList list = getThreadProviderList();
	if (list == null) {
	    list = getSystemProviderList();
	}
	return list;
    }
    
    /**
     * Set the current ProviderList. Affects the thread-local list if set,
     * otherwise the system wide list.
     */
    public static void setProviderList(ProviderList newList) {
	if (getThreadProviderList() == null) {
	    setSystemProviderList(newList);
	} else {
	    changeThreadProviderList(newList);
	}
    }

    /**
     * Get the full provider list with invalid providers (those that
     * could not be loaded) removed. This is the list we need to
     * present to applications.
     */
    public static synchronized ProviderList getFullProviderList() {
	ProviderList list = getThreadProviderList();
	if (list != null) {
	    ProviderList newList = list.removeInvalid();
	    if (newList != list) {
		changeThreadProviderList(newList);
		list = newList;
	    }
	    return list;
	}
	list = getSystemProviderList();
	ProviderList newList = list.removeInvalid();
	if (newList != list) {
	    setSystemProviderList(newList);
	    list = newList;
	}
	return list;
    }
    
    private static ProviderList getSystemProviderList() {
	return providerList;
    }
    
    private static void setSystemProviderList(ProviderList list) {
	providerList = list;
    }
    
    public static ProviderList getThreadProviderList() {
	// avoid accessing the threadlocal if none are currently in use
	// (first use of ThreadLocal.get() for a Thread allocates a Map)
	if (threadListsUsed == 0) {
	    return null;
	}
	return threadLists.get();
    }

    // Change the thread local provider list. Use only if the current thread
    // is already using a thread local list and you want to change it in place.
    // In other cases, use the begin/endThreadProviderList() methods.
    private static void changeThreadProviderList(ProviderList list) {
	threadLists.set(list);
    }
    
    /**
     * Methods to manipulate the thread local provider list. It is for use by 
     * JAR verification (see above) and the SunJSSE FIPS mode only.
     *
     * It should be used as follows:
     * 
     *   ProviderList list = ...;
     *   ProviderList oldList = Providers.beginThreadProviderList(list);
     *   try {
     *     // code that needs thread local provider list
     *   } finally {
     *     Providers.endThreadProviderList(oldList);
     *   }
     *
     */

    public static synchronized ProviderList beginThreadProviderList(ProviderList list) {
	if (ProviderList.debug != null) {
	    ProviderList.debug.println("ThreadLocal providers: " + list);
	}
	ProviderList oldList = threadLists.get();
	threadListsUsed++;
	threadLists.set(list);
	return oldList;
    }
    
    public static synchronized void endThreadProviderList(ProviderList list) {
	if (list == null) {
	    if (ProviderList.debug != null) {
		ProviderList.debug.println("Disabling ThreadLocal providers");
	    }
	    threadLists.remove();
	} else {
	    if (ProviderList.debug != null) {
		ProviderList.debug.println
		    ("Restoring previous ThreadLocal providers: " + list);
	    }
 	    threadLists.set(list);
	}
	threadListsUsed--;
    }

}
