/*
 * @(#)FileSystemImpl.java	1.5 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.management;

import java.io.File;
import java.io.IOException;

/*
 * Windows implementation of sun.management.FileSystem
 */
public class FileSystemImpl extends FileSystem {

    public boolean supportsFileSecurity(File f) throws IOException {
	return isSecuritySupported0(f.getAbsolutePath());
    }

    public boolean isAccessUserOnly(File f) throws IOException {
	String path = f.getAbsolutePath();
	if (!isSecuritySupported0(path)) {
	    throw new UnsupportedOperationException("File system does not support file security");
	}
	return isAccessUserOnly0(path);
    }

    // Native methods

    static native void init0();

    static native boolean isSecuritySupported0(String path) throws IOException;

    static native boolean isAccessUserOnly0(String path) throws IOException;

    // Initialization

    static {
	java.security.AccessController
            .doPrivileged(new sun.security.action.LoadLibraryAction("management"));
	init0();
    }
}

