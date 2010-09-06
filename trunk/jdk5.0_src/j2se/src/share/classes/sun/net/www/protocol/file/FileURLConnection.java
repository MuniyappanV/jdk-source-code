/*
 * @(#)FileURLConnection.java	1.58 04/02/18
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

/**
 * Open an file input stream given a URL.
 * @author	James Gosling
 * @author	Steven B. Byrne
 * @version 	1.58, 02/18/04
 */

package sun.net.www.protocol.file;

import java.net.URL;
import java.net.FileNameMap;
import java.io.*;
import java.text.Collator;
import java.security.Permission;
import sun.net.*;
import sun.net.www.*;
import java.util.*;
import java.text.SimpleDateFormat;

import sun.security.action.GetPropertyAction;
import sun.security.action.GetIntegerAction;
import sun.security.action.GetBooleanAction;

public class FileURLConnection extends URLConnection {
    
    static String CONTENT_LENGTH = "content-length";
    static String CONTENT_TYPE = "content-type";
    static String TEXT_PLAIN = "text/plain";
    static String LAST_MODIFIED = "last-modified";

    String contentType;
    InputStream is;

    File file;
    String filename;
    boolean isDirectory = false;
    boolean exists = false;
    List files;

    long length = -1;
    long lastModified = 0;
    
    protected FileURLConnection(URL u, File file) {
	super(u);
	this.file = file;
    }

    /*  
     * Note: the semantics of FileURLConnection object is that the
     * results of the various URLConnection calls, such as
     * getContentType, getInputStream or getContentLength reflect
     * whatever was true when connect was called.  
     */
    public void connect() throws IOException {
	if (!connected) {
            try {
                filename = file.toString();
                isDirectory = file.isDirectory();
                if (isDirectory) {
                    files = (List) Arrays.asList(file.list());
                } else {
                
		    is = new BufferedInputStream(new FileInputStream(filename));
		    
		    // Check if URL should be metered
		    boolean meteredInput = ProgressMonitor.getDefault().shouldMeterInput(url, "GET");		    
		    if (meteredInput)	{
			ProgressSource pi = new ProgressSource(url, "GET", (int) file.length());
			is = new MeteredStream(is, pi, (int) file.length());
		    }
                }
            } catch (IOException e) {
                throw e;
            }
	    connected = true;
	}
    }

    private boolean initializedHeaders = false;

    private void initializeHeaders() {
	try {
	    connect();
	    exists = file.exists();
	} catch (IOException e) {
	}
	if (!initializedHeaders || !exists) {
	    length = file.length();
	    lastModified = file.lastModified();

	    if (!isDirectory) {
		FileNameMap map = java.net.URLConnection.getFileNameMap();
		contentType = map.getContentTypeFor(filename);
		if (contentType != null) {
		    properties.add(CONTENT_TYPE, contentType);
		}
		properties.add(CONTENT_LENGTH, String.valueOf(length));

		/*
		 * Format the last-modified field into the preferred 
		 * Internet standard - ie: fixed-length subset of that
		 * defined by RFC 1123
		 */
		if (lastModified != 0) {
		    Date date = new Date(lastModified);
		    SimpleDateFormat fo = 
			new SimpleDateFormat ("EEE, dd MMM yyyy HH:mm:ss 'GMT'", Locale.US);
		    fo.setTimeZone(TimeZone.getTimeZone("GMT"));
		    properties.add(LAST_MODIFIED, fo.format(date));
		}
	    } else {
		properties.add(CONTENT_TYPE, TEXT_PLAIN);
	    }
	    initializedHeaders = true;
	}
    }

    public String getHeaderField(String name) {
	initializeHeaders();
	return super.getHeaderField(name);
    }

    public String getHeaderField(int n) {
	initializeHeaders();
	return super.getHeaderField(n);
    }

    public int getContentLength() {
        initializeHeaders();
        return (int) length;
    }

    public String getHeaderFieldKey(int n) {
	initializeHeaders();
	return super.getHeaderFieldKey(n);
    }

    public MessageHeader getProperties() {
	initializeHeaders();
	return super.getProperties();
    }

    public long getLastModified() {
	initializeHeaders();
	return lastModified;
    }

    public synchronized InputStream getInputStream()
	throws IOException {

	int iconHeight;
	int iconWidth;

	connect();

	if (is == null) {
	    if (isDirectory) {
		FileNameMap map = java.net.URLConnection.getFileNameMap();

		StringBuffer buf = new StringBuffer();

		if (files == null) {
		    throw new FileNotFoundException(filename);
		}

                Collections.sort(files, Collator.getInstance());

		for (int i = 0 ; i < files.size() ; i++) {
                    String fileName = (String)files.get(i);
		    buf.append(fileName);
		    buf.append("\n");
		}
		// Put it into a (default) locale-specific byte-stream.
		is = new ByteArrayInputStream(buf.toString().getBytes());
	    } else {
		throw new FileNotFoundException(filename);
	    }
	}
	return is;
    }

    Permission permission;

    /* since getOutputStream isn't supported, only read permission is
     * relevant 
     */
    public Permission getPermission() throws IOException {
	if (permission == null) {
            String decodedPath = ParseUtil.decode(url.getPath());
	    if (File.separatorChar == '/') {
		permission = new FilePermission(decodedPath, "read");
	    } else {
		permission = new FilePermission(
			decodedPath.replace('/',File.separatorChar), "read");
	    }
	}
	return permission;
    }
}




