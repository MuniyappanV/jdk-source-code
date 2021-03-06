/*
 * @(#)Win32PrintServiceLookup.java	1.10 04/05/05
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.print;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.util.ArrayList;
import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import javax.print.DocFlavor;
import javax.print.MultiDocPrintService;
import javax.print.PrintService;
import javax.print.PrintServiceLookup;
import javax.print.attribute.Attribute;
import javax.print.attribute.AttributeSet;
import javax.print.attribute.HashPrintRequestAttributeSet;
import javax.print.attribute.HashPrintServiceAttributeSet;
import javax.print.attribute.PrintRequestAttribute;
import javax.print.attribute.PrintRequestAttributeSet;
import javax.print.attribute.PrintServiceAttribute;
import javax.print.attribute.PrintServiceAttributeSet;
import javax.print.attribute.standard.PrinterName;

public class Win32PrintServiceLookup extends PrintServiceLookup {

    /* Remind: the current implementation is static, as its assumed
     * its preferable to minimise creation of PrintService instances.
     * Later we should add logic to add/remove services on the fly which
     * will take a hit of needing to regather the list of services.
     */
    private String defaultPrinter;
    private PrintService defaultPrintService;
    private String[] printers; /* excludes the default printer */
    private PrintService[] printServices; /* includes the default printer */

    static {
        java.security.AccessController.doPrivileged(
                    new sun.security.action.LoadLibraryAction("awt"));
    }


    /* Want the PrintService which is default print service to have
     * equality of reference with the equivalent in list of print services
     * This isn't required by the API and there's a risk doing this will
     * lead people to assume its guaranteed.
     */
    public PrintService[] getPrintServices() {
      SecurityManager security = System.getSecurityManager();
      if (security != null) {  
	security.checkPrintJobAccess();
      }
      if (printServices == null) {
	    PrintService defService = getDefaultPrintService();	    
	    printers = getAllPrinterNames();
	    if (printers == null) {
	      return null;
	    }
	    printServices = new PrintService[printers.length];	   
	    for (int p=0; p<printers.length; p++) {
		if (defService != null &&
		    printers[p].equals(defService.getName())) {
		    printServices[p] = defService;
		} else {
		    printServices[p] = new Win32PrintService(printers[p]);
		}
	    }
	}
	return printServices;
    }
   
    private synchronized PrintService getPrintServiceByName(String name) {

	if (name == null || name.equals("")) {
	    return null;
	} else if (printServices == null) {
	    String []allNames = getAllPrinterNames();
	    for (int i=0; i<allNames.length; i++) {
		if (allNames[i].equals(name)) {
		    return new Win32PrintService(name);
		}
	    }
	    return null;
	} else {
	    for (int i=0; i<printServices.length; i++) {
		if (printServices[i].getName().equals(name)) {
		    return printServices[i];
		}
	    }
	    return null;
	}
    }

    boolean matchingService(PrintService service,
			    PrintServiceAttributeSet serviceSet) {
	if (serviceSet != null) {
	    Attribute [] attrs =  serviceSet.toArray();
	    Attribute serviceAttr;
	    for (int i=0; i<attrs.length; i++) {
		serviceAttr
		    = service.getAttribute((Class<PrintServiceAttribute>)attrs[i].getCategory());
		if (serviceAttr == null || !serviceAttr.equals(attrs[i])) {
		    return false;
		}
	    }
	}
	return true;
    }

    public PrintService[] getPrintServices(DocFlavor flavor,
					   AttributeSet attributes) {

        SecurityManager security = System.getSecurityManager();
	if (security != null) {  
	  security.checkPrintJobAccess();
	}
	PrintRequestAttributeSet requestSet = null;
	PrintServiceAttributeSet serviceSet = null;

	if (attributes != null && !attributes.isEmpty()) {

	    requestSet = new HashPrintRequestAttributeSet();
	    serviceSet = new HashPrintServiceAttributeSet();

	    Attribute[] attrs = attributes.toArray();
	    for (int i=0; i<attrs.length; i++) {
		if (attrs[i] instanceof PrintRequestAttribute) {
		    requestSet.add(attrs[i]);
		} else if (attrs[i] instanceof PrintServiceAttribute) {
		    serviceSet.add(attrs[i]);
		}
	    }
	}
	
	/*
	 * Special case: If client is asking for a particular printer
	 * (by name) then we can save time by getting just that service
	 * to check against the rest of the specified attributes.
	 */
	PrintService[] services = null;
	if (serviceSet != null && serviceSet.get(PrinterName.class) != null) {
	    PrinterName name = (PrinterName)serviceSet.get(PrinterName.class);
	    PrintService service = getPrintServiceByName(name.getValue());
	    if (service == null || !matchingService(service, serviceSet)) {
		services = new PrintService[0];
	    } else {
		services = new PrintService[1];
		services[0] = service;
	    }
	} else {
	    services = getPrintServices();
	}

	if (services.length == 0) {
	    return services;
	} else {
	    ArrayList matchingServices = new ArrayList();
	    for (int i=0; i<services.length; i++) {
		try {
		    if (services[i].
			getUnsupportedAttributes(flavor, requestSet) == null) {
			matchingServices.add(services[i]);
		    }
		} catch (IllegalArgumentException e) {
		}
	    }
	    services = new PrintService[matchingServices.size()];
	    return (PrintService[])matchingServices.toArray(services);
	}
    }

    /*
     * return empty array as don't support multi docs
     */
    public MultiDocPrintService[] 
	getMultiDocPrintServices(DocFlavor[] flavors,
				 AttributeSet attributes) {
	SecurityManager security = System.getSecurityManager();
	if (security != null) {  
	  security.checkPrintJobAccess();
	}
	return new MultiDocPrintService[0];
    }


    public PrintService getDefaultPrintService() {
        SecurityManager security = System.getSecurityManager();
	if (security != null) {  
	  security.checkPrintJobAccess();
	}
	if (defaultPrintService == null) {
	   defaultPrinter = getDefaultPrinterName();
	   if (defaultPrinter != null) {
	       defaultPrintService = new Win32PrintService(defaultPrinter);
	   }
	}
	return defaultPrintService;
    }

    private native String getDefaultPrinterName();
   
    private native String[] getAllPrinterNames();

}
