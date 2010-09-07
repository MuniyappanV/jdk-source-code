/*
 * @(#)SunSDK.java	1.6 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import java.io.File;

/**
 * This class encapsulates platform-specific details of the 
 * structure of an SDK which has a JRE embedded in it. 
 */
class SunSDK {
    /**
     * Returns the home directory of a Java 2 SDK if the current
     * JRE is embedded in one. 
     */
    static String home() {
        File jreHome = new File(System.getProperty("java.home"));
        File jreParent = new File(jreHome.getParent());

        String jdwpLibName = "bin" + File.separator + 
                             System.mapLibraryName("jdwp");
        File jdwpLib = new File(jreParent, jdwpLibName);
        return jdwpLib.exists() ? jreParent.getAbsolutePath() : null;
    }
}
