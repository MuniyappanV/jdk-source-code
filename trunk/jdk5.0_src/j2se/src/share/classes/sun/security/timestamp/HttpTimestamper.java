/*
 * @(#)HttpTimestamper.java	1.2 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.timestamp;

import java.io.BufferedInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.URL;
import java.net.HttpURLConnection;
import java.util.Iterator;
import java.util.Set;

import sun.security.pkcs.*;

/**
 * A timestamper that communicates with a Timestamping Authority (TSA) 
 * over HTTP.
 * It supports the Time-Stamp Protocol defined in:
 * <a href="http://www.ietf.org/rfc/rfc3161.txt">RFC 3161</a>.
 * 
 * @since 1.5
 * @version 1.2, 12/19/03
 * @author Vincent Ryan
 */

public class HttpTimestamper implements Timestamper {

    private static final int CONNECT_TIMEOUT = 15000; // 15 seconds

    // The MIME type for a timestamp query
    private static final String TS_QUERY_MIME_TYPE =
	"application/timestamp-query";

    // The MIME type for a timestamp reply
    private static final String TS_REPLY_MIME_TYPE =
	"application/timestamp-reply";

    private static final boolean DEBUG = false;

    /*
     * HTTP URL identifying the location of the TSA
     */
    private String tsaUrl = null;

    /**
     * Creates a timestamper that connects to the specified TSA.
     *
     * @param tsa The location of the TSA. It must be an HTTP URL.
     */
    public HttpTimestamper(String tsaUrl) {
	this.tsaUrl = tsaUrl;
    }

    /**
     * Connects to the TSA and requests a timestamp.
     *
     * @param tsQuery The timestamp query.
     * @return The result of the timestamp query.
     * @throws IOException The exception is thrown if a problem occurs while
     *         communicating with the TSA.
     */
    public TSResponse generateTimestamp(TSRequest tsQuery) throws IOException {

	HttpURLConnection connection =
	    (HttpURLConnection) new URL(tsaUrl).openConnection();
	connection.setDoOutput(true);
	connection.setUseCaches(false); // ignore cache
	connection.setRequestProperty("Content-Type", TS_QUERY_MIME_TYPE);
        connection.setRequestMethod("POST");
	// Avoids the "hang" when a proxy is required but none has been set.
        connection.setConnectTimeout(CONNECT_TIMEOUT);

	if (DEBUG) {
	    Set headers = connection.getRequestProperties().entrySet();
            System.out.println(connection.getRequestMethod() + " " + tsaUrl +
		" HTTP/1.1");
	    for (Iterator i = headers.iterator(); i.hasNext(); ) {
                System.out.println("  " + i.next());
            }
            System.out.println();
	}
	connection.connect(); // No HTTP authentication is performed

	// Send the request
	DataOutputStream output =
	    new DataOutputStream(connection.getOutputStream());
        byte[] request = tsQuery.encode();
        output.write(request, 0, request.length);
	output.flush();
	output.close();
	if (DEBUG) {
	    System.out.println("sent timestamp query (length=" +
		request.length + ")");
	}

	// Receive the reply
        BufferedInputStream input =
		new BufferedInputStream(connection.getInputStream());
	if (DEBUG) {
	    String header = connection.getHeaderField(0);
            System.out.println(header);
            int i = 1;
            while ((header = connection.getHeaderField(i)) != null) {
                String key = connection.getHeaderFieldKey(i);
                System.out.println("  " + ((key==null) ? "" : key + ": ") +
		    header);
                i++;
            }
            System.out.println();
	}
	int contentLength = connection.getContentLength();
	if (contentLength == -1) {
	    contentLength = Integer.MAX_VALUE;
	}
	verifyMimeType(connection.getContentType());

	byte[] replyBuffer = new byte[contentLength];
	int total = 0;
	int count = 0;
	while (count != -1 && total < contentLength) {
	    count = input.read(replyBuffer, total, replyBuffer.length - total);
	    total += count;
	}
	if (DEBUG) {
	    System.out.println("received timestamp response (length=" +
		replyBuffer.length + ")");
	}

	input.close();
	return new TSResponse(replyBuffer);
    }

    /*
     * Checks that the MIME content type is a timestamp reply.
     *
     * @param contentType The MIME content type to be checked.
     * @throws IOException The exception is thrown if a mismatch occurs.
     */
    private static void verifyMimeType(String contentType) throws IOException {
	if (! TS_REPLY_MIME_TYPE.equalsIgnoreCase(contentType)) {
	    throw new IOException("MIME Content-Type is not " +
		TS_REPLY_MIME_TYPE);
	}
    }
}
