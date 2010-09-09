/*
 * @(#)Jstatd.java	1.4 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.tools.jstatd;

import java.rmi.*;
import java.rmi.server.*;
import java.rmi.registry.Registry;
import java.rmi.registry.LocateRegistry;
import java.net.MalformedURLException;
import sun.jvmstat.monitor.remote.*;

/**
 * Application providing remote access to the jvmstat instrumentation
 * exported by local Java Virtual Machine processes. Remote access is
 * provided through an RMI interface.
 *
 * @author Brian Doherty
 * @version 1.4, 03/23/10
 * @since 1.5
 */
public class Jstatd {

    private static Registry registry;
    private static int port = -1;
    private static boolean startRegistry = true;

    private static void printUsage() {
        System.err.println("usage: jstatd [-nr] [-p port] [-n rminame]");
    }

    static void bind(String name, RemoteHostImpl remoteHost)
                throws RemoteException, MalformedURLException, Exception {

        try {
            Naming.rebind(name, remoteHost);
        } catch (java.rmi.ConnectException e) {
            /*
             * either the registry is not running or we cannot contact it.
             * start an internal registry if requested.
             */
            if (startRegistry && registry == null) {
                int localport = (port < 0) ? Registry.REGISTRY_PORT : port;
                registry = LocateRegistry.createRegistry(localport);
                bind(name, remoteHost);
            }
            else {
                System.out.println("Could not contact registry\n"
                                   + e.getMessage());
                e.printStackTrace();
            }
        } catch (RemoteException e) {
            System.err.println("Could not bind " + name + " to RMI Registry");
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        String rminame = null;
        int argc = 0;

        for ( ; (argc < args.length) && (args[argc].startsWith("-")); argc++) {
            String arg = args[argc];

            if (arg.compareTo("-nr") == 0) {
                startRegistry = false;
            } else if (arg.startsWith("-p")) {
                if (arg.compareTo("-p") != 0) {
                    port = Integer.parseInt(arg.substring(2));
                } else {
                  argc++;
                  if (argc >= args.length) {
                      printUsage();
                      System.exit(1);
                  }
                  port = Integer.parseInt(args[argc]);
                }
            } else if (arg.startsWith("-n")) {
                if (arg.compareTo("-n") != 0) {
                    rminame = arg.substring(2);
                } else {
                    argc++;
                    if (argc >= args.length) {
                        printUsage();
                        System.exit(1);
                    }
                    rminame = args[argc];
                }
            } else {
                printUsage();
                System.exit(1);
            }
        }

        if (argc < args.length) {
            printUsage();
            System.exit(1);
        }

        if (System.getSecurityManager() == null) {
            System.setSecurityManager(new RMISecurityManager());
        }

        StringBuilder name = new StringBuilder();

        if (port >= 0) {
            name.append("//:").append(port);
        }

        if (rminame == null) {
            rminame = "JStatRemoteHost";
        }

        name.append("/").append(rminame);

        try {
            // use 1.5.0 dynamically generated subs.
            System.setProperty("java.rmi.server.ignoreSubClasses", "true");
            RemoteHostImpl remoteHost = new RemoteHostImpl();
            RemoteHost stub = (RemoteHost) UnicastRemoteObject.exportObject(
                    remoteHost, 0);
            bind(name.toString(), remoteHost);
        } catch (MalformedURLException e) {
            if (rminame != null) {
                System.out.println("Bad RMI server name: " + rminame);
            } else {
                System.out.println("Bad RMI URL: " + name + " : "
                                   + e.getMessage());
            }
            System.exit(1);
        } catch (java.rmi.ConnectException e) {
            // could not attach to or create a registry
            System.out.println("Could not contact RMI registry\n"
                               + e.getMessage());
            System.exit(1);
        } catch (Exception e) {
            System.out.println("Could not create remote object\n"
                               + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
