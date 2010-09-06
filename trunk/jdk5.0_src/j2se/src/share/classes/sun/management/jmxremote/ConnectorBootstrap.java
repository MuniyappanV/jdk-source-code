/*
 * @(#)file      ConnectorBootstrap.java
 * @(#)author    Sun Microsystems, Inc.
 * @(#)version   1.27
 * @(#)lastedit  04/06/17
 * 
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.management.jmxremote;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import java.net.MalformedURLException;

import java.rmi.NoSuchObjectException;
import java.rmi.Remote;
import java.rmi.RemoteException;
import java.rmi.registry.Registry;
import java.rmi.server.RMIClientSocketFactory;
import java.rmi.server.RMIServerSocketFactory;
import java.rmi.server.UnicastRemoteObject;

import java.security.Principal;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.StringTokenizer;

import java.lang.management.ManagementFactory;

import javax.management.MBeanServer;
import javax.management.remote.JMXAuthenticator;
import javax.management.remote.JMXConnectorServer;
import javax.management.remote.JMXConnectorServerFactory;
import javax.management.remote.JMXServiceURL;
import javax.management.remote.rmi.RMIConnectorServer;

import javax.rmi.ssl.SslRMIClientSocketFactory;
import javax.rmi.ssl.SslRMIServerSocketFactory;

import javax.security.auth.Subject;

import sun.rmi.server.UnicastServerRef;
import sun.rmi.server.UnicastServerRef2;

import sun.management.Agent;
import sun.management.AgentConfigurationError;
import static sun.management.AgentConfigurationError.*;
import sun.management.FileSystem;
import sun.management.snmp.util.MibLogger;

import com.sun.jmx.remote.internal.RMIExporter;
import com.sun.jmx.remote.security.JMXPluggableAuthenticator;

/**
 * This class initializes and starts the RMIConnectorServer for JSR 163 
 * JMX Monitoring.
 **/
public final class ConnectorBootstrap {
    
    /**
     * Default values for JMX configuration properties.
     **/
    public static interface DefaultValues {
        public static final String PORT="0";
        public static final String CONFIG_FILE_NAME="management.properties";
        public static final String USE_SSL="true";
        public static final String USE_AUTHENTICATION="true";
        public static final String PASSWORD_FILE_NAME="jmxremote.password";
        public static final String ACCESS_FILE_NAME="jmxremote.access";
        public static final String SSL_NEED_CLIENT_AUTH="false";
    }

    /**
     * Names of JMX configuration properties.
     **/
    public static interface PropertyNames {
        public static final String PORT =
            "com.sun.management.jmxremote.port";
        public static final String CONFIG_FILE_NAME =
            "com.sun.management.config.file";
        public static final String USE_SSL =
            "com.sun.management.jmxremote.ssl";
        public static final String USE_AUTHENTICATION =
            "com.sun.management.jmxremote.authenticate";
        public static final String PASSWORD_FILE_NAME =
            "com.sun.management.jmxremote.password.file";
        public static final String ACCESS_FILE_NAME =
            "com.sun.management.jmxremote.access.file";
        public static final String LOGIN_CONFIG_NAME =
            "com.sun.management.jmxremote.login.config";
        public static final String SSL_ENABLED_CIPHER_SUITES =
            "com.sun.management.jmxremote.ssl.enabled.cipher.suites";
        public static final String SSL_ENABLED_PROTOCOLS =
            "com.sun.management.jmxremote.ssl.enabled.protocols";
        public static final String SSL_NEED_CLIENT_AUTH =
            "com.sun.management.jmxremote.ssl.need.client.auth";
    }

    /**
     * <p>Prevents our RMI server objects from keeping the JVM alive.</p>
     *
     * <p>We use a private interface in Sun's JMX Remote API implementation
     * that allows us to specify how to export RMI objects.  We do so using
     * UnicastServerRef, a class in Sun's RMI implementation.  This is all
     * non-portable, of course, so this is only valid because we are inside
     * Sun's JRE.</p>
     *
     * <p>Objects are exported using {@link
     * UnicastServerRef#exportObject(Remote, Object, boolean)}.  The
     * boolean parameter is called <code>permanent</code> and means
     * both that the object is not eligible for Distributed Garbage
     * Collection, and that its continued existence will not prevent
     * the JVM from exiting.  It is the latter semantics we want (we
     * already have the former because of the way the JMX Remote API
     * works).  Hence the somewhat misleading name of this class.</p>
     */
    private static class PermanentExporter implements RMIExporter {
        public Remote exportObject(Remote obj,
                                   int port,
                                   RMIClientSocketFactory csf,
                                   RMIServerSocketFactory ssf)
                throws RemoteException {

            synchronized (this) {
                if (firstExported == null)
                    firstExported = obj;
            }

            final UnicastServerRef ref;
            if (csf == null && ssf == null)
                ref = new UnicastServerRef(port);
            else
                ref = new UnicastServerRef2(port, csf, ssf);
            return ref.exportObject(obj, null, true);
        }

        // Nothing special to be done for this case
        public boolean unexportObject(Remote obj, boolean force)
                throws NoSuchObjectException {
            return UnicastRemoteObject.unexportObject(obj, force);
        }

        Remote firstExported;
    }

    /**
     * This JMXAuthenticator wraps the JMXPluggableAuthenticator and verifies
     * that at least one of the principal names contained in the authenticated
     * Subject is present in the access file.
     */
    private static class AccessFileCheckerAuthenticator
        implements JMXAuthenticator {

        public AccessFileCheckerAuthenticator(Map env) throws IOException {
            environment = env;
            accessFile = (String) env.get("jmx.remote.x.access.file");
            properties = propertiesFromFile(accessFile);
        }

        public Subject authenticate(Object credentials) {
            final JMXAuthenticator authenticator =
                new JMXPluggableAuthenticator(environment);
            final Subject subject = authenticator.authenticate(credentials);
            checkAccessFileEntries(subject);
            return subject;
        }

        private void checkAccessFileEntries(Subject subject) {
            if (subject == null)
                throw new SecurityException(
                      "Access denied! No matching entries found in " +
                      "the access file [" + accessFile + "] as the " +
                      "authenticated Subject is null");
            final Set principals = subject.getPrincipals();
            for (Iterator i = principals.iterator(); i.hasNext(); ) {
                final Principal p = (Principal) i.next();
		if (properties.containsKey(p.getName()))
		    return;
            }
            final Set principalsStr = new HashSet();
            for (Iterator i = principals.iterator(); i.hasNext(); ) {
                final Principal p = (Principal) i.next();
                principalsStr.add(p.getName());
            }
            throw new SecurityException(
                  "Access denied! No entries found in the access file [" +
                  accessFile + "] for any of the authenticated identities " +
                  principalsStr);
        }

        private static Properties propertiesFromFile(String fname)
            throws IOException {
            Properties p = new Properties();
	    if (fname == null)
		return p;
            FileInputStream fin = new FileInputStream(fname);
            p.load(fin);
            fin.close();
            return p;
        }

        private final Map environment;
        private final Properties properties;
        private final String accessFile;
    }

    /**
     * Initializes and starts the JMX Connector Server.
     * If the com.sun.management.jmxremote.port property is not defined,
     * simply return. Otherwise, attempts to load the config file, and
     * then calls {@link #initialize(java.lang.String, java.util.Properties)}.
     *
     **/
    public static synchronized JMXConnectorServer initialize() {

        // Load a new management properties
        final Properties props = Agent.loadManagementProperties();
        if (props == null) return null;

        final String portStr = props.getProperty(PropertyNames.PORT);


        // System.out.println("initializing: {port=" + portStr + ", 
        //                     properties="+props+"}");
        return initialize(portStr,props);
    }

    /**
     * Initializes and starts a JMX Connector Server for remote
     * monitoring and management.
     **/
    public static synchronized 
        JMXConnectorServer initialize(String portStr, Properties props) {

        // Get port number
        final int port;
        try {
            port = Integer.parseInt(portStr);
        } catch (NumberFormatException x) {
            throw new AgentConfigurationError(INVALID_JMXREMOTE_PORT, x, portStr);
        }
        if (port < 0) {
            throw new AgentConfigurationError(INVALID_JMXREMOTE_PORT, portStr);
        }

        // Do we use authentication?
        final String  useAuthenticationStr = 
            props.getProperty(PropertyNames.USE_AUTHENTICATION,
                              DefaultValues.USE_AUTHENTICATION);
        final boolean useAuthentication = 
            Boolean.valueOf(useAuthenticationStr).booleanValue();

        // Do we use SSL?
        final String  useSslStr = 
            props.getProperty(PropertyNames.USE_SSL,
                              DefaultValues.USE_SSL);
        final boolean useSsl = 
            Boolean.valueOf(useSslStr).booleanValue();

        final String enabledCipherSuites =
            props.getProperty(PropertyNames.SSL_ENABLED_CIPHER_SUITES);
        String enabledCipherSuitesList[] = null;
        if (enabledCipherSuites != null) {
            StringTokenizer st = new StringTokenizer(enabledCipherSuites, ",");
            int tokens = st.countTokens();
            enabledCipherSuitesList = new String[tokens];
            for (int i = 0 ; i < tokens; i++) {
                enabledCipherSuitesList[i] = st.nextToken();
            }
        }

        final String enabledProtocols =
            props.getProperty(PropertyNames.SSL_ENABLED_PROTOCOLS);
        String enabledProtocolsList[] = null;
        if (enabledProtocols != null) {
            StringTokenizer st = new StringTokenizer(enabledProtocols, ",");
            int tokens = st.countTokens();
            enabledProtocolsList = new String[tokens];
            for (int i = 0 ; i < tokens; i++) {
                enabledProtocolsList[i] = st.nextToken();
            }
        }

        final String  sslNeedClientAuthStr =
            props.getProperty(PropertyNames.SSL_NEED_CLIENT_AUTH,
                              DefaultValues.SSL_NEED_CLIENT_AUTH);
        final boolean sslNeedClientAuth =
            Boolean.valueOf(sslNeedClientAuthStr).booleanValue();

        String loginConfigName = null;
        String passwordFileName = null;
        String accessFileName = null;

        // Initialize settings when authentication is active
        if (useAuthentication) {

            // Get non-default login configuration
            loginConfigName = 
                props.getProperty(PropertyNames.LOGIN_CONFIG_NAME);

            if (loginConfigName == null) {
                // Get password file
                passwordFileName =
                    props.getProperty(PropertyNames.PASSWORD_FILE_NAME,
                        getDefaultFileName(DefaultValues.PASSWORD_FILE_NAME));
                checkPasswordFile(passwordFileName);
            }

            // Get access file
            accessFileName = props.getProperty(PropertyNames.ACCESS_FILE_NAME,
                getDefaultFileName(DefaultValues.ACCESS_FILE_NAME));
            checkAccessFile(accessFileName);
        }

        if (log.isDebugOn()) {
            log.debug("initialize",
                      Agent.getText("jmxremote.ConnectorBootstrap.initialize") +
                      "\n\t" + PropertyNames.PORT + "=" + port +
                      "\n\t" + PropertyNames.USE_SSL + "=" + useSsl +
                      "\n\t" + PropertyNames.SSL_ENABLED_CIPHER_SUITES + "=" +
                      enabledCipherSuites +
                      "\n\t" + PropertyNames.SSL_ENABLED_PROTOCOLS + "=" +
                      enabledProtocols +
                      "\n\t" + PropertyNames.SSL_NEED_CLIENT_AUTH + "=" +
                      sslNeedClientAuth +
                      "\n\t" + PropertyNames.USE_AUTHENTICATION + "=" + 
                      useAuthentication +
                      (useAuthentication ?
                        (loginConfigName == null ?
                            ("\n\t" + PropertyNames.PASSWORD_FILE_NAME + "=" +
                             passwordFileName) :
                            ("\n\t" + PropertyNames.LOGIN_CONFIG_NAME + "=" +
                             loginConfigName)) : "\n\t" +
                        Agent.getText("jmxremote.ConnectorBootstrap.initialize.noAuthentication")) +
                      (useAuthentication ?
                       ("\n\t" + PropertyNames.ACCESS_FILE_NAME + "=" +
                        accessFileName) : "") +
                      "");
        }

        final MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        JMXConnectorServer cs = null;
        try {
            cs = exportMBeanServer(mbs, port, useSsl, enabledCipherSuitesList,
                                   enabledProtocolsList, sslNeedClientAuth,
                                   useAuthentication, loginConfigName,
                                   passwordFileName, accessFileName);

            final JMXServiceURL url = cs.getAddress();
            log.config("initialize",
                       Agent.getText("jmxremote.ConnectorBootstrap.initialize.ready",
                       new JMXServiceURL(url.getProtocol(),
                                         url.getHost(),
                                         url.getPort(),
                                         "/jndi/rmi://"+url.getHost()+":"+port+"/"+
                                         "jmxrmi").toString()));
        } catch (Exception e) {
            throw new AgentConfigurationError(AGENT_EXCEPTION, e, e.toString());
        }
        return cs;
    }

    /*
     * Creates and starts a RMI Connector Server for "local" monitoring 
     * and management. 
     */
    public static JMXConnectorServer startLocalConnectorServer() {
        // Ensure cryptographically strong random number generater used
        // to choose the object number - see java.rmi.server.ObjID
        System.setProperty("java.rmi.server.randomIDs", "true");

        // This RMI server should not keep the VM alive
        Map env = new HashMap();
        env.put(RMIExporter.EXPORTER_ATTRIBUTE, new PermanentExporter());

        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        try {
            JMXServiceURL url = new JMXServiceURL("rmi", null, 0);
            JMXConnectorServer server =
                JMXConnectorServerFactory.newJMXConnectorServer(url, env, mbs);
            server.start();
            return server;
        } catch (Exception e) {
            throw new AgentConfigurationError(AGENT_EXCEPTION, e, e.toString());
        }
    }

    private static void checkPasswordFile(String passwordFileName) {
        if (passwordFileName == null || passwordFileName.length()==0) {
            throw new AgentConfigurationError(PASSWORD_FILE_NOT_SET);
        }
        File file = new File(passwordFileName);
        if (!file.exists()) {
            throw new AgentConfigurationError(PASSWORD_FILE_NOT_FOUND, passwordFileName);
        }

        if (!file.canRead()) {
            throw new AgentConfigurationError(PASSWORD_FILE_NOT_READABLE, passwordFileName);
        }

        FileSystem fs = FileSystem.open();
        try {
            if (fs.supportsFileSecurity(file)) {
                if (!fs.isAccessUserOnly(file)) {
                    final String msg=Agent.getText("jmxremote.ConnectorBootstrap.initialize.password.readonly",
                        passwordFileName);
                    log.config("initialize",msg);
                    throw new AgentConfigurationError(PASSWORD_FILE_ACCESS_NOT_RESTRICTED, 
                        passwordFileName);
                }
            }
        } catch (IOException e) {
            throw new AgentConfigurationError(PASSWORD_FILE_READ_FAILED,
                e, passwordFileName);
        }
    }

    private static void checkAccessFile(String accessFileName) {
        if (accessFileName == null || accessFileName.length()==0) {
            throw new AgentConfigurationError(ACCESS_FILE_NOT_SET);
        }
        File file = new File(accessFileName);
        if (!file.exists()) {
            throw new AgentConfigurationError(ACCESS_FILE_NOT_FOUND, accessFileName);
        }

        if (!file.canRead()) {
            throw new AgentConfigurationError(ACCESS_FILE_NOT_READABLE, accessFileName);
        }
    }

    /**
     * Compute the full path name for a default file.
     * @param basename basename (with extension) of the default file.
     * @return ${JRE}/lib/management/${basename}
     **/
    private static String getDefaultFileName(String basename) {
        final String fileSeparator = File.separator;
        return System.getProperty("java.home") + fileSeparator + "lib" + 
            fileSeparator + "management" + fileSeparator +
            basename;
    }

    private static JMXConnectorServer
        exportMBeanServer(MBeanServer mbs,
                          int port,
                          boolean useSsl,
                          String[] enabledCipherSuites,
                          String[] enabledProtocols,
                          boolean sslNeedClientAuth,
                          boolean useAuthentication,
                          String loginConfigName,
                          String passwordFileName,
                          String accessFileName) 
        throws IOException, MalformedURLException {

        /* Make sure we use non-guessable RMI object IDs.  Otherwise
         * attackers could hijack open connections by guessing their
         * IDs.  */
        System.setProperty("java.rmi.server.randomIDs", "true");

        JMXServiceURL url = new JMXServiceURL("rmi", null, 0);

        Map env = new HashMap();

        PermanentExporter exporter = new PermanentExporter();
        
        env.put(RMIExporter.EXPORTER_ATTRIBUTE, exporter);

        if (useAuthentication) {
            if (loginConfigName != null) {
                env.put("jmx.remote.x.login.config", loginConfigName);
            }
            if (passwordFileName != null) {
                env.put("jmx.remote.x.password.file", passwordFileName);
            }

            env.put("jmx.remote.x.access.file", accessFileName);

            if (env.get("jmx.remote.x.password.file") != null ||
                env.get("jmx.remote.x.login.config") != null) {
                env.put(JMXConnectorServer.AUTHENTICATOR,
                        new AccessFileCheckerAuthenticator(env));
            }
        }

        final RMIClientSocketFactory csf;
        final RMIServerSocketFactory ssf;

        if (useSsl) {
            csf = new SslRMIClientSocketFactory();
            ssf = new SslRMIServerSocketFactory(enabledCipherSuites,
                                                enabledProtocols,
                                                sslNeedClientAuth);
            env.put(RMIConnectorServer.RMI_CLIENT_SOCKET_FACTORY_ATTRIBUTE,
                    csf);
            env.put(RMIConnectorServer.RMI_SERVER_SOCKET_FACTORY_ATTRIBUTE,
                    ssf);
        } else {
            csf = null;
            ssf = null;
        }
        
        JMXConnectorServer connServer = null;
        try {
            connServer =
                JMXConnectorServerFactory.newJMXConnectorServer(url, env, mbs);
            connServer.start();

        } catch (IOException e) {
            if (connServer == null) {
                throw new AgentConfigurationError(CONNECTOR_SERVER_IO_ERROR, 
                    e, url.toString());
            } else {
                throw new AgentConfigurationError(CONNECTOR_SERVER_IO_ERROR, 
                    e, connServer.getAddress().toString());
            }
        }

        final Registry registry;
        registry = new SingleEntryRegistry(port,
                                           "jmxrmi", exporter.firstExported);
        /* Our exporter remembers the first object it was asked to
           export, which will be an RMIServerImpl appropriate for
           publication in our special registry.  We could
           alternatively have constructed the RMIServerImpl explicitly
           and then constructed an RMIConnectorServer passing it as a
           parameter, but that's quite a bit more verbose and pulls in
           lots of knowledge of the RMI connector.  */

        return connServer;
    }

    /**
     * This class cannot be instantiated.
     **/
    private ConnectorBootstrap() {
    }

    // XXX Revisit: should probably clone this MibLogger....
    private static final MibLogger log = 
        new MibLogger(ConnectorBootstrap.class);

}
