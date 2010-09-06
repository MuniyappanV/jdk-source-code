/*
 * @(#)Timestamper.java	1.2 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.timestamp;

import java.io.IOException;

/**
 * A timestamping service which conforms to the Time-Stamp Protocol (TSP) 
 * defined in:
 * <a href="http://www.ietf.org/rfc/rfc3161.txt">RFC 3161</a>.
 * Individual timestampers may communicate with a Timestamping Authority (TSA)
 * over different transport machanisms. TSP permits at least the following 
 * transports: HTTP, Internet mail, file-based and socket-based.
 * 
 * @version 1.2, 12/19/03
 * @author Vincent Ryan
 * @see HttpTimestamper
 */
public interface Timestamper {

    /*
     * Connects to the TSA and requests a timestamp.
     *
     * @param tsQuery The timestamp query.
     * @return The result of the timestamp query.
     * @throws IOException The exception is thrown if a problem occurs while
     *         communicating with the TSA.
     */
    public TSResponse generateTimestamp(TSRequest tsQuery) throws IOException;
}
