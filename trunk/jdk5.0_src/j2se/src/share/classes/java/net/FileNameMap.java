/*
 * @(#)FileNameMap.java	1.14 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.net;

/**
 * A simple interface which provides a mechanism to map
 * between a file name and a MIME type string.
 *
 * @version 	1.14, 12/19/03
 * @author  Steven B. Byrne
 * @since   JDK1.1
 */
public interface FileNameMap {

    /**
     * Gets the MIME type for the specified file name.
     * @param fileName the specified file name
     * @return a <code>String</code> indicating the MIME
     * type for the specified file name.
     */
    public String getContentTypeFor(String fileName);
}
