/*
 * @(#)Util.java	1.14 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

/*
 * Licensed Materials - Property of IBM
 * RMI-IIOP v1.0
 * Copyright IBM Corp. 1998 1999  All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

package sun.rmi.rmic.iiop;

import java.io.File;
import sun.tools.java.Identifier;

import com.sun.corba.se.impl.util.PackagePrefixChecker;

/**
 * Util provides static utility methods used by other rmic classes.
 * @author Bryan Atsatt
 */

public final class Util implements sun.rmi.rmic.Constants {


    public static String packagePrefix(){ return PackagePrefixChecker.packagePrefix();}


    /**
     * Return the directory that should be used for output for a given
     * class.
     * @param theClass The fully qualified name of the class.
     * @param rootDir The directory to use as the root of the
     * package heirarchy.  May be null, in which case the current
     * working directory is used as the root.
     */
    private static File getOutputDirectoryFor(Identifier theClass,
	                                     File rootDir,
	                                     BatchEnvironment env,
                                             boolean idl ) {
        File outputDir = null;
        String className = theClass.getFlatName().toString().replace('.', SIGC_INNERCLASS);    		
	String qualifiedClassName = className;
 	String packagePath = null;
 	String packageName = theClass.getQualifier().toString();
        //Shift package names for stubs generated for interfaces.
        /*if(type.isInterface())*/ 
        packageName =  correctPackageName(packageName, idl); 
        //Done.
	if (packageName.length() > 0) {
    	    qualifiedClassName = packageName + "." + className;
 	    packagePath = packageName.replace('.', File.separatorChar);
 	}

        // Do we have a root directory?
        
	if (rootDir != null) {
		    
            // Yes, do we have a package name?
                
            if (packagePath != null) {
            	    
    		// Yes, so use it as the root. Open the directory...
        		    
    		outputDir = new File(rootDir, packagePath);
        		    
    		// Make sure the directory exists...
        		    
                ensureDirectory(outputDir,env);
                    
            } else {
            	    
        	// Default package, so use root as output dir...
            	    
        	outputDir = rootDir;
            }		    
	} else {
		    
            // No root directory. Get the current working directory...
                    
            String workingDirPath = System.getProperty("user.dir");
            File workingDir = new File(workingDirPath);
                    
            // Do we have a package name?
                    
            if (packagePath == null) {
                        
                // No, so use working directory...
               
                outputDir = workingDir;
                        
            } else {
                        
                // Yes, so use working directory as the root...
                            
      		outputDir = new File(workingDir, packagePath);
                		    
    		// Make sure the directory exists...
                		    
    		ensureDirectory(outputDir,env);
            }
	}

	// Finally, return the directory...
	    
	return outputDir;
    }

    public static File getOutputDirectoryForIDL(Identifier theClass,
	                                     File rootDir,
	                                     BatchEnvironment env) {
        return getOutputDirectoryFor(theClass, rootDir, env, true);
    }

    public static File getOutputDirectoryForStub(Identifier theClass,
	                                     File rootDir,
	                                     BatchEnvironment env) {
        return getOutputDirectoryFor(theClass, rootDir, env, false);
    }

    private static void ensureDirectory (File dir, BatchEnvironment env) {
    	if (!dir.exists()) {
            dir.mkdirs();
            if (!dir.exists()) {
                env.error(0,"rmic.cannot.create.dir",dir.getAbsolutePath());
                throw new InternalError();
            }
    	}
    }

    public static String correctPackageName (String p, boolean idl){
        if (idl){
            return p;
        } else {
            return PackagePrefixChecker.correctPackageName(p);
        }
    }

    public static boolean isOffendingPackage(String p){
        return PackagePrefixChecker.isOffendingPackage(p);
    }

    public static boolean hasOffendingPrefix(String p){
        return PackagePrefixChecker.hasOffendingPrefix(p);
    }

}



