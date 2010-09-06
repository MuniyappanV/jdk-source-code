/*
 * @(#)RMIIIOPServerImpl.java	1.24 04/05/05
 * 
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.management.remote.rmi;

import java.io.IOException;
import java.rmi.Remote;
import java.util.Map;
import java.util.Collections;
import java.util.Properties;
import javax.rmi.PortableRemoteObject;
import javax.security.auth.Subject;

/**
 * <p>An {@link RMIServerImpl} that is exported through IIOP and that
 * creates client connections as RMI objects exported through IIOP.
 * User code does not usually reference this class directly.</p>
 *
 * @see RMIServerImpl
 *
 * @since 1.5
 * @since.unbundled 1.0
 */
public class RMIIIOPServerImpl extends RMIServerImpl {
    /**
     * <p>Creates a new {@link RMIServerImpl}.</p>
     *
     * @param env the environment containing attributes for the new
     * <code>RMIServerImpl</code>.  Can be null, which is equivalent
     * to an empty Map.
     *
     * @exception IOException if the RMI object cannot be created.
     */
    public RMIIIOPServerImpl(Map<String,?> env)
	    throws IOException {
	super(env);

	this.env = (env == null) ? Collections.EMPTY_MAP : env;
    }

    protected void export() throws IOException {
	PortableRemoteObject.exportObject(this);
    }

    protected String getProtocol() {
	return "iiop";
    }

    /**
     * <p>Returns an IIOP stub.</p>
     * The stub might not yet be connected to the ORB. The stub will
     * be serializable only if it is connected to the ORB.
     * @return an IIOP stub.
     * @exception IOException if the stub cannot be created - e.g the
     *            RMIIIOPServerImpl has not been exported yet.
     **/
    public Remote toStub() throws IOException {
	// javax.rmi.CORBA.Stub stub = 
	//    (javax.rmi.CORBA.Stub) PortableRemoteObject.toStub(this);
	final Remote stub = PortableRemoteObject.toStub(this);
	// java.lang.System.out.println("NON CONNECTED STUB " + stub);
	// org.omg.CORBA.ORB orb =
	//    org.omg.CORBA.ORB.init((String[])null, (Properties)null);
	// stub.connect(orb);
	// java.lang.System.out.println("CONNECTED STUB " + stub);
	return (Remote) stub;
    }

    /**
     * <p>Creates a new client connection as an RMI object exported
     * through IIOP.
     *
     * @param connectionId the ID of the new connection.  Every
     * connection opened by this connector server will have a
     * different ID.  The behavior is unspecified if this parameter is
     * null.
     *
     * @param subject the authenticated subject.  Can be null.
     *
     * @return the newly-created <code>RMIConnection</code>.
     *
     * @exception IOException if the new client object cannot be
     * created or exported.
     */
    protected RMIConnection makeClient(String connectionId, Subject subject)
	    throws IOException {

	if (connectionId == null)
	    throw new NullPointerException("Null connectionId");

	RMIConnection client =
	    new RMIConnectionImpl(this, connectionId, getDefaultClassLoader(),
				  subject, env);
	PortableRemoteObject.exportObject(client);
	return client;
    }

    protected void closeClient(RMIConnection client) throws IOException {
	PortableRemoteObject.unexportObject(client);
    }
    
    /**
     * <p>Called by {@link #close()} to close the connector server by
     * unexporting this object.  After returning from this method, the
     * connector server must not accept any new connections.</p>
     *
     * @exception IOException if the attempt to close the connector
     * server failed.
     */
    protected void closeServer() throws IOException {
	PortableRemoteObject.unexportObject(this);
    }

    private final Map env;
}
