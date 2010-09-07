/*
 * @(#)IPPPrintService.java	1.9 04/05/05
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.print;

import javax.print.attribute.*;
import javax.print.attribute.standard.*;
import javax.print.DocFlavor;
import javax.print.DocPrintJob;
import javax.print.PrintService;
import javax.print.ServiceUIFactory;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import java.util.Date;
import java.util.Arrays;
import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import javax.print.event.PrintServiceAttributeListener;

import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.HttpURLConnection;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.DataInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;

import java.util.Iterator;


public class IPPPrintService implements PrintService, SunPrinterJobService {  

    public static boolean debugPrint = false;
    private static String debugPrefix = "IPPPrintService>> ";
    protected static void debug_println(String str) {
	if (debugPrint) {
	    System.out.println(str);
	}
    }


    private String printer;
    private URI    myURI;
    private URL    myURL;
    transient private ServiceNotifier notifier = null;

    private static int MAXCOPIES = 1000;
    private static int ATTRIBUTESTR_MAX = 80;

    private CUPSPrinter cps;
    private HttpURLConnection urlConnection = null;
    private DocFlavor[] supportedDocFlavors;	
    private Class[] supportedCats;
    private MediaTray[] mediaTrays;
    private MediaSizeName[] mediaSizeNames;
    private CustomMediaSizeName[] customMediaSizeNames;
    private int defaultMediaIndex;
    private boolean isCupsPrinter;
    private boolean init;
    private HashMap getAttMap;
    private boolean pngImagesAdded = false;
    private boolean gifImagesAdded = false;
    private boolean jpgImagesAdded = false;
   

    /**
     * IPP Status Codes
     */
    private static final byte STATUSCODE_SUCCESS = 0x00;
   
    /**
     * IPP Group Tags.  Each tag is used once before the first attribute
     * of that group.
     */
    // operation attributes group
    private static final byte GRPTAG_OP_ATTRIBUTES = 0x01;
    // job attributes group
    private static final byte GRPTAG_JOB_ATTRIBUTES = 0x02;
    // printer attributes group
    private static final byte GRPTAG_PRINTER_ATTRIBUTES = 0x04;
    // used as the last tag in an IPP message.
    private static final byte GRPTAG_END_ATTRIBUTES = 0x03;

    /**
     * IPP Operation codes
     */
    // gets the attributes for a printer
    public static final String OP_GET_ATTRIBUTES = "000B"; 
    // gets the default printer
    public static final String OP_CUPS_GET_DEFAULT = "4001"; 
    // gets the list of printers
    public static final String OP_CUPS_GET_PRINTERS = "4002"; 


    /**
     * List of all PrintRequestAttributes.  This is used
     * for looping through all the IPP attribute name.
     */
    private static Object[] printReqAttribDefault = {
	Chromaticity.COLOR,
	new Copies(1),
	Fidelity.FIDELITY_FALSE,
	Finishings.NONE,
	//new JobHoldUntil(new Date()),
	//new JobImpressions(0),
	//JobImpressions,
	//JobKOctets,
	//JobMediaSheets,
	new JobName("", Locale.getDefault()),
	//JobPriority,
	JobSheets.NONE,
	(Media)MediaSizeName.NA_LETTER,
	//MediaPrintableArea.class, // not an IPP attribute
	//MultipleDocumentHandling.SINGLE_DOCUMENT,
	new NumberUp(1),
	OrientationRequested.PORTRAIT,
	new PageRanges(1),
	//PresentationDirection,
	         // CUPS does not supply printer-resolution attribute
	//new PrinterResolution(300, 300, PrinterResolution.DPI), 
	//PrintQuality.NORMAL, 
	new RequestingUserName("", Locale.getDefault()),
	//SheetCollate.UNCOLLATED, //CUPS has no sheet collate?
	Sides.ONE_SIDED,
    };


    /**
     * List of all PrintServiceAttributes.  This is used
     * for looping through all the IPP attribute name.
     */
    private static Object[][] serviceAttributes = {
	{ColorSupported.class, "color-supported"},
	{PagesPerMinute.class,	"pages-per-minute"},
	{PagesPerMinuteColor.class, "pages-per-minute-color"},
	{PDLOverrideSupported.class, "pdl-override-supported"},
	{PrinterInfo.class, "printer-info"},
	{PrinterIsAcceptingJobs.class, "printer-is-accepting-jobs"},
	{PrinterLocation.class, "printer-location"},
	{PrinterMakeAndModel.class, "printer-make-and-model"},
	{PrinterMessageFromOperator.class, "printer-message-from-operator"},
	{PrinterMoreInfo.class, "printer-more-info"},
	{PrinterMoreInfoManufacturer.class, "printer-more-info-manufacturer"},
	{PrinterName.class, "printer-name"},
	{PrinterState.class, "printer-state"},
	{PrinterStateReasons.class, "printer-state-reasons"},
	{PrinterURI.class, "printer-uri"},
	{QueuedJobCount.class, "queued-job-count"}
    };


    /**
     * List of DocFlavors, grouped based on matching mime-type.
     * NOTE: For any change in the predefined DocFlavors, it must be reflected
     * here also.
     */
    // PDF DocFlavors
    private static DocFlavor[] appPDF = {
	DocFlavor.BYTE_ARRAY.PDF,
	DocFlavor.INPUT_STREAM.PDF,
	DocFlavor.URL.PDF
    };

    // Postscript DocFlavors
    private static DocFlavor[] appPostScript = {
	DocFlavor.BYTE_ARRAY.POSTSCRIPT,
	DocFlavor.INPUT_STREAM.POSTSCRIPT,
	DocFlavor.URL.POSTSCRIPT
    };

    // Autosense DocFlavors
    private static DocFlavor[] appOctetStream = {
	DocFlavor.BYTE_ARRAY.AUTOSENSE,
	DocFlavor.INPUT_STREAM.AUTOSENSE,
	DocFlavor.URL.AUTOSENSE
    };

    // Text DocFlavors
    private static DocFlavor[] textPlain = {
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_HOST,
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_UTF_8,
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_UTF_16,
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_UTF_16BE,
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_UTF_16LE,
	DocFlavor.BYTE_ARRAY.TEXT_PLAIN_US_ASCII,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_HOST,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_UTF_8,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_UTF_16,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_UTF_16BE,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_UTF_16LE,
	DocFlavor.INPUT_STREAM.TEXT_PLAIN_US_ASCII,
	DocFlavor.URL.TEXT_PLAIN_HOST,
	DocFlavor.URL.TEXT_PLAIN_UTF_8,
	DocFlavor.URL.TEXT_PLAIN_UTF_16,
	DocFlavor.URL.TEXT_PLAIN_UTF_16BE,
	DocFlavor.URL.TEXT_PLAIN_UTF_16LE,
	DocFlavor.URL.TEXT_PLAIN_US_ASCII,
	DocFlavor.CHAR_ARRAY.TEXT_PLAIN,
	DocFlavor.STRING.TEXT_PLAIN,
	DocFlavor.READER.TEXT_PLAIN
    };
  
    // JPG DocFlavors
    private static DocFlavor[] imageJPG = {
	DocFlavor.BYTE_ARRAY.JPEG,
	DocFlavor.INPUT_STREAM.JPEG,
	DocFlavor.URL.JPEG
    };

    // GIF DocFlavors
    private static DocFlavor[] imageGIF = {
	DocFlavor.BYTE_ARRAY.GIF,
	DocFlavor.INPUT_STREAM.GIF,
	DocFlavor.URL.GIF
    };

    // PNG DocFlavors
    private static DocFlavor[] imagePNG = {
	DocFlavor.BYTE_ARRAY.PNG,
	DocFlavor.INPUT_STREAM.PNG,
	DocFlavor.URL.PNG
    };

    // HTML DocFlavors
    private  static DocFlavor[] textHtml = {
	DocFlavor.BYTE_ARRAY.TEXT_HTML_HOST,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_HOST,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_UTF_8,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_UTF_16,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_UTF_16BE,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_UTF_16LE,
	DocFlavor.BYTE_ARRAY.TEXT_HTML_US_ASCII,
	DocFlavor.INPUT_STREAM.TEXT_HTML_HOST,
	DocFlavor.INPUT_STREAM.TEXT_HTML_UTF_8,
	DocFlavor.INPUT_STREAM.TEXT_HTML_UTF_16,
	DocFlavor.INPUT_STREAM.TEXT_HTML_UTF_16BE,
	DocFlavor.INPUT_STREAM.TEXT_HTML_UTF_16LE,
	DocFlavor.INPUT_STREAM.TEXT_HTML_US_ASCII,
	DocFlavor.URL.TEXT_HTML_HOST,
	DocFlavor.URL.TEXT_HTML_UTF_8,
	DocFlavor.URL.TEXT_HTML_UTF_16,
	DocFlavor.URL.TEXT_HTML_UTF_16BE,
	DocFlavor.URL.TEXT_HTML_UTF_16LE,
	DocFlavor.URL.TEXT_HTML_US_ASCII,
	// These are not handled in UnixPrintJob so commenting these
	// for now.
	/*
	DocFlavor.CHAR_ARRAY.TEXT_HTML,
	DocFlavor.STRING.TEXT_HTML,
	DocFlavor.READER.TEXT_HTML,
	*/
    };

    
    // PCL DocFlavors
    private static DocFlavor[] appPCL = {
	DocFlavor.BYTE_ARRAY.PCL,
	DocFlavor.INPUT_STREAM.PCL,
	DocFlavor.URL.PCL
    };

    // List of all DocFlavors, used in looping
    // through all supported mime-types
    private static Object[] allDocFlavors = {
	appPDF, appPostScript, appOctetStream,
	textPlain, imageJPG, imageGIF, imagePNG,
	textHtml, appPCL, 
    };

    
    IPPPrintService(String name, URL url) {
	if ((name == null) || (url == null)){
	    throw new IllegalArgumentException("null uri or printer name");
	}
	printer = name;
	supportedDocFlavors = null;
	supportedCats = null;
	mediaSizeNames = null;
	customMediaSizeNames = null;
	mediaTrays = null;
	myURL = url;
	cps = null;
	isCupsPrinter = false;
	init = false;
	defaultMediaIndex = -1;

	String host = myURL.getHost();
	if (host!=null && host.equals(CUPSPrinter.getServer())) {
	    isCupsPrinter = true;
	    try {
		myURI =  new URI("ipp://"+host+
				 "/printers/"+printer);
		debug_println(debugPrefix+"IPPPrintService myURI : "+myURI);
	    } catch (java.net.URISyntaxException e) {
		throw new IllegalArgumentException("invalid url");
	    }	   
	} 

    } 
   

 
    /*
     * Initialize mediaSizeNames, mediaTrays and other attributes.
     * Media size/trays are initialized to non-null values, may be 0-length
     * array.
     */
    private void initAttributes() {
	if (!init) {
	    init = true;

	    // init customMediaSizeNames
	    customMediaSizeNames = new CustomMediaSizeName[0];	    

	    if ((urlConnection = getIPPConnection(myURL)) == null) {
		mediaSizeNames = new MediaSizeName[0];
		mediaTrays = new MediaTray[0];
		debug_println("NULL urlConnection ");
		return;
	    }

	    // get all supported attributes through IPP
	    opGetAttributes(); 

	    if (isCupsPrinter) {
		// note, it is possible to query media in CUPS using IPP
		// right now we always get it from PPD.
		// maybe use "&& (usePPD)" later?
		// Another reason why we use PPD is because 
		// IPP currently does not support it but PPD does.
		    
		try {
		    cps = new CUPSPrinter(printer);
		    mediaSizeNames = cps.getMediaSizeNames();
		    mediaTrays = cps.getMediaTrays();
		    customMediaSizeNames = cps.getCustomMediaSizeNames();
		    urlConnection.disconnect();
		    return;
		} catch (Exception e) {
		    IPPPrintService.debug_println(debugPrefix+
				       " error creating CUPSPrinter");
		}
	    } 
		  
	    // use IPP to get all media, 
	    Media[] allMedia = (Media[])getSupportedMedia();	
	    ArrayList sizeList = new ArrayList();
	    ArrayList trayList = new ArrayList();
	    for (int i=0; i<allMedia.length; i++) {
		if (allMedia[i] instanceof MediaSizeName) {
		    sizeList.add(allMedia[i]);
		} else if (allMedia[i] instanceof MediaTray) {
		    trayList.add(allMedia[i]);
		}
	    }

	    if (sizeList != null) {
		mediaSizeNames = new MediaSizeName[sizeList.size()];
		mediaSizeNames = (MediaSizeName[])sizeList.toArray(
						       mediaSizeNames);
	    }
	    if (trayList != null) {
		mediaTrays = new MediaTray[trayList.size()];
		mediaTrays = (MediaTray[])trayList.toArray(
							   mediaTrays);
	    }
	    urlConnection.disconnect();
	}
    }


    public DocPrintJob createPrintJob() {
	SecurityManager security = System.getSecurityManager();  
	if (security != null) {   
	    security.checkPrintJobAccess();    
	}
	// REMIND: create IPPPrintJob
	return new UnixPrintJob(this);
    } 


    public Object
	getSupportedAttributeValues(Class<? extends Attribute> category,
				    DocFlavor flavor,
				    AttributeSet attributes)
    {
	if (category == null) {
	    throw new NullPointerException("null category");
	}
	if (!Attribute.class.isAssignableFrom(category)) {
	    throw new IllegalArgumentException(category +
				 " does not implement Attribute");
	}
	if (flavor != null) {
	    if (!isDocFlavorSupported(flavor)) {
		throw new IllegalArgumentException(flavor +
					       " is an unsupported flavor");
	    } else if (isAutoSense(flavor)) {
		return null;
	    }
	    
	}

	if (!isAttributeCategorySupported(category)) {
	    return null;
	}

	/* Test if the flavor is compatible with the attributes */
	if (!isDestinationSupported(flavor, attributes)) {
	    return null;
	}

	initAttributes();

	/* Test if the flavor is compatible with the category */
	if ((category == Copies.class) || 
	    (category == CopiesSupported.class)) {
 	    CopiesSupported cs = new CopiesSupported(1, MAXCOPIES);
	    AttributeClass attribClass = (getAttMap != null) ?
		(AttributeClass)getAttMap.get(cs.getName()) : null;
	    if (attribClass != null) {
		int[] range = attribClass.getIntRangeValue();	
		cs = new CopiesSupported(range[0], range[1]);
	    }
	    return cs;
	} else 	if (category == Chromaticity.class) {
	    if (flavor == null ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.GIF) ||
		flavor.equals(DocFlavor.INPUT_STREAM.GIF) ||
		flavor.equals(DocFlavor.URL.GIF) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.JPEG) ||
		flavor.equals(DocFlavor.INPUT_STREAM.JPEG) ||
		flavor.equals(DocFlavor.URL.JPEG) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.PNG) ||
		flavor.equals(DocFlavor.INPUT_STREAM.PNG) ||
		flavor.equals(DocFlavor.URL.PNG)) {

		Chromaticity[]arr = new Chromaticity[1];
		arr[0] = Chromaticity.COLOR;
		return (arr);
	    } else {
		return null;
	    }
	} else if (category == Destination.class) {
	    if (flavor == null ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE)) {
		try {
		    return new Destination((new File("out.ps")).toURI());
		} catch (SecurityException se) {
		}
	    }
	    return null;
	} else if (category == Fidelity.class) {
	    Fidelity []arr = new Fidelity[2];
	    arr[0] = Fidelity.FIDELITY_FALSE;
	    arr[1] = Fidelity.FIDELITY_TRUE;
	    return arr;
	} else if (category == Finishings.class) {
	    AttributeClass attribClass = (getAttMap != null) ?
		(AttributeClass)getAttMap.get("finishings-supported")
		: null;
	    if (attribClass != null) {
		int[] finArray = attribClass.getArrayOfIntValues();
		if ((finArray != null) && (finArray.length > 0)) {
		    Finishings[] finSup = new Finishings[finArray.length];
		    for (int i=0; i<finArray.length; i++) {
			finSup[i] = Finishings.NONE;
			Finishings[] fAll = (Finishings[])
			    (new ExtFinishing(100)).getAll();
			for (int j=0; j<fAll.length; j++) {
			    if (finArray[i] == fAll[j].getValue()) {
				finSup[i] = fAll[j];
				break;
			    }
			}
		    }
		    return finSup;
		}
	    }	
	} else if (category == JobName.class) {
	    return new JobName("Java Printing", null);
	} else if (category == JobSheets.class) {
	    JobSheets arr[] = new JobSheets[2];
	    arr[0] = JobSheets.NONE;
	    arr[1] = JobSheets.STANDARD;
	    return arr;
	
	} else if (category == Media.class) {	   
	    Media[] allMedia = new Media[mediaSizeNames.length+
					mediaTrays.length];

	    for (int i=0; i<mediaSizeNames.length; i++) {
		allMedia[i] = mediaSizeNames[i];
	    }

	    for (int i=0; i<mediaTrays.length; i++) {
		allMedia[i+mediaSizeNames.length] = mediaTrays[i];
	    }
	    
	    return allMedia;
	} else if (category == MediaPrintableArea.class) {
	    MediaPrintableArea[] mpas = null;
	    if (cps != null) {
		mpas = cps.getMediaPrintableArea();
	    } 
	    
	    if (mpas == null) {
		return null;
	    }

	    if ((attributes == null) || (attributes.size() == 0)) {
		return mpas;
	    }
	    
	    int match = -1;
	    Media media = (Media)attributes.get(Media.class);
	    if (media != null && media instanceof MediaSizeName) {
		MediaSizeName msn = (MediaSizeName)media;
		for (int i=0; i<mediaSizeNames.length; i++) {
		    if (msn.equals(mediaSizeNames[i])) {
			match = i;
		    }
		}
	    }
	    
	    if (match == -1) {
		return null;
	    } else {
		MediaPrintableArea []arr = new MediaPrintableArea[1];
		arr[0] = mpas[match];
		return arr;
	    }
	} else if (category == NumberUp.class) {
	    AttributeClass attribClass = (getAttMap != null) ?
		(AttributeClass)getAttMap.get("number-up-supported") : null;
	    if (attribClass != null) {
		int[] values = attribClass.getArrayOfIntValues();
		if (values != null) {
		    NumberUp[] nUp = new NumberUp[values.length];
		    for (int i=0; i<values.length; i++) {
			nUp[i] = new NumberUp(values[i]);
		    }
		    return nUp;
		} else {
		    return null;
		}
	    } 
	} else if (category == OrientationRequested.class) {
	    if (flavor == null || 
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE)) {
		// Orientation is emulated in Pageable/Printable flavors
		// so we report the 3 orientations as supported.
		OrientationRequested []orientSup = new OrientationRequested[3];
		orientSup[0] = OrientationRequested.PORTRAIT;
		orientSup[1] = OrientationRequested.LANDSCAPE;
		orientSup[2] = OrientationRequested.REVERSE_LANDSCAPE;
		return orientSup;
	    }

	    AttributeClass attribClass = (getAttMap != null) ?
	      (AttributeClass)getAttMap.get("orientation-requested-supported") 
		: null;
	    if (attribClass != null) {
		int[] orientArray = attribClass.getArrayOfIntValues();
		if ((orientArray != null) && (orientArray.length > 0)) {
		    OrientationRequested[] orientSup = 
			new OrientationRequested[orientArray.length];
		    for (int i=0; i<orientArray.length; i++) {
			switch (orientArray[i]) {
			default:
			case 3 :
			    orientSup[i] = OrientationRequested.PORTRAIT;
			    break;
			case 4:
			    orientSup[i] = OrientationRequested.LANDSCAPE;
			    break;
			case 5:
			    orientSup[i] = 
				OrientationRequested.REVERSE_LANDSCAPE;
			    break;
			case 6:
			    orientSup[i] = 
				OrientationRequested.REVERSE_PORTRAIT;
			    break;
			}
		    }
		    return orientSup;
		}
	    }
	} else if (category == PageRanges.class) {
	   if (flavor == null ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE)) {	
		PageRanges []arr = new PageRanges[1];
		arr[0] = new PageRanges(1, Integer.MAX_VALUE);
		return arr;
	    } else {
		// Returning null as this is not yet supported in UnixPrintJob.
		return null;
	    }
	} else if (category == RequestingUserName.class) {
	    String userName = "";
	    try {
	      userName = System.getProperty("user.name", "");
	    } catch (SecurityException se) {
	    }
	    return new RequestingUserName(userName, null);
	} else if (category == Sides.class) {
	    // The printer takes care of Sides so if short-edge 
	    // is chosen in a job, the rotation is done by the printer.
	    // Orientation is rotated by emulation if pageable
	    // or printable so if the document is in Landscape, this may
	    // result in double rotation.
	    AttributeClass attribClass = (getAttMap != null) ?
		(AttributeClass)getAttMap.get("sides-supported")
		: null;
	    if (attribClass != null) {
		String[] sidesArray = attribClass.getArrayOfStringValues();
		if ((sidesArray != null) && (sidesArray.length > 0)) {
		    Sides[] sidesSup = new Sides[sidesArray.length];
		    for (int i=0; i<sidesArray.length; i++) {
			if (sidesArray[i].endsWith("long-edge")) {
			    sidesSup[i] = Sides.TWO_SIDED_LONG_EDGE;
			} else if (sidesArray[i].endsWith("short-edge")) {
			    sidesSup[i] = Sides.TWO_SIDED_SHORT_EDGE;
			} else {
			    sidesSup[i] = Sides.ONE_SIDED;
			} 
		    }
		    return sidesSup;
		}
	    }
	}

	return null;
    }

    //This class is for getting all pre-defined Finishings
    private class ExtFinishing extends Finishings {
	ExtFinishing(int value) {
	    super(100); // 100 to avoid any conflicts with predefined values.
	}

	EnumSyntax[] getAll() {
	    EnumSyntax[] es = super.getEnumValueTable();
	    return es;
	}
    }

    
    public AttributeSet getUnsupportedAttributes(DocFlavor flavor,
						 AttributeSet attributes) {
	if (flavor != null && !isDocFlavorSupported(flavor)) {
	    throw new IllegalArgumentException("flavor " + flavor +
					       "is not supported");
	}

	if (attributes == null) {
	    return null;
	}

	Attribute attr;
	AttributeSet unsupp = new HashAttributeSet();
	Attribute []attrs = attributes.toArray();
	for (int i=0; i<attrs.length; i++) {
	    try {
		attr = attrs[i];
		if (!isAttributeCategorySupported(attr.getCategory())) {
		    unsupp.add(attr);
		} else if (!isAttributeValueSupported(attr, flavor,
						      attributes)) {
		    unsupp.add(attr);
		}
	    } catch (ClassCastException e) {
	    }
	}
        if (unsupp.isEmpty()) {
	    return null;
	} else {
	    return unsupp;
	}
    }


    public DocFlavor[] getSupportedDocFlavors() {

	if (supportedDocFlavors != null) {
	    int len = supportedDocFlavors.length;
	    	DocFlavor[] copyflavors = new DocFlavor[len];
		System.arraycopy(supportedDocFlavors, 0, copyflavors, 0, len);
		return copyflavors;
	}
	initAttributes();

	if ((getAttMap != null) && 
	    getAttMap.containsKey("document-format-supported")) {

	    AttributeClass attribClass = 
		(AttributeClass)getAttMap.get("document-format-supported");
	    if (attribClass != null) {
		String mimeType;	
		boolean psSupported = false;
		String[] docFlavors = attribClass.getArrayOfStringValues();
		DocFlavor[] flavors;
		ArrayList docList = new ArrayList(); 
		int j;
		for (int i = 0; i < docFlavors.length; i++) {
		    for (j=0; j<allDocFlavors.length; j++) {
			flavors = (DocFlavor[])allDocFlavors[j];

			mimeType = flavors[0].getMimeType();
			if (mimeType.startsWith(docFlavors[i])) {

			    docList.addAll(Arrays.asList(flavors));

			    if (mimeType.equals("image/png")) {
				pngImagesAdded = true;
 			    } else if (mimeType.equals("image/gif")) {
				gifImagesAdded = true;
			    } else if (mimeType.equals("image/jpeg")) {
				jpgImagesAdded = true;
			    } else if (mimeType.indexOf("postscript") != -1) {
				docList.add(
				      DocFlavor.SERVICE_FORMATTED.PAGEABLE);
				docList.add(
				      DocFlavor.SERVICE_FORMATTED.PRINTABLE);

				psSupported = true;
			    }			    
			    break;
			}
		    }

		    // Not added? Create new DocFlavors
		    if (j == allDocFlavors.length) {
			//  make new DocFlavors
			docList.add(new DocFlavor.BYTE_ARRAY(docFlavors[i]));
			docList.add(new DocFlavor.INPUT_STREAM(docFlavors[i]));
			docList.add(new DocFlavor.URL(docFlavors[i]));
		    }
		}

		// check if we need to add image DocFlavors
		if (psSupported) {
		    if (!jpgImagesAdded) {
			docList.addAll(Arrays.asList(imageJPG));
		    } 
		    if (!pngImagesAdded) {
			docList.addAll(Arrays.asList(imagePNG));
		    } 
		    if (!gifImagesAdded) {
			docList.addAll(Arrays.asList(imageGIF));
		    }
		}
		supportedDocFlavors = new DocFlavor[docList.size()];
		docList.toArray(supportedDocFlavors);
		int len = supportedDocFlavors.length;
		DocFlavor[] copyflavors = new DocFlavor[len];
		System.arraycopy(supportedDocFlavors, 0, copyflavors, 0, len);
		return copyflavors;
	    }
	}
	return null;
    }
    

    public boolean isDocFlavorSupported(DocFlavor flavor) {
	if (supportedDocFlavors == null) {
	    getSupportedDocFlavors();
	}
	if (supportedDocFlavors != null) {
	    for (int f=0; f<supportedDocFlavors.length; f++) {
		if (flavor.equals(supportedDocFlavors[f])) {
		    return true;
		}
	    }
	}
	return false;
    }


    /**
     * Finds matching CustomMediaSizeName of given media.
     */
    public CustomMediaSizeName findCustomMedia(MediaSizeName media) {
	for (int i=0; i< customMediaSizeNames.length; i++) {
	    CustomMediaSizeName custom =
		(CustomMediaSizeName)customMediaSizeNames[i];
	    MediaSizeName msn = custom.getStandardMedia();		
	    if (media.equals(msn)) {
		return customMediaSizeNames[i];
	    }
	}
	return null;
    }


    /**
     * Returns the matching standard Media using string comparison of names.
     */
    private Media getIPPMedia(String mediaName) {
	CustomMediaSizeName sampleSize = new CustomMediaSizeName("sample", "",
								 0, 0);
	Media[] sizes = sampleSize.getSuperEnumTable();
	for (int i=0; i<sizes.length; i++) {
	    if (mediaName.equals(""+sizes[i])) {
		return sizes[i];
	    }
	}
	CustomMediaTray sampleTray = new CustomMediaTray("sample", "");
	Media[] trays = sampleTray.getSuperEnumTable();
	for (int i=0; i<trays.length; i++) {
	    if (mediaName.equals(""+trays[i])) {
		return trays[i];
	    }
	}
	return null;
    }

    private Media[] getSupportedMedia() {
	if ((getAttMap != null) && 
	    getAttMap.containsKey("media-supported")) {

	    AttributeClass attribClass = 
		(AttributeClass)getAttMap.get("media-supported");

	    if (attribClass != null) {
		String[] mediaVals = attribClass.getArrayOfStringValues();
		Media msn;
		Media[] mediaNames = 
		    new Media[mediaVals.length];
		for (int i=0; i<mediaVals.length; i++) {
		    msn = getIPPMedia(mediaVals[i]);
		    //REMIND: if null, create custom?
		    mediaNames[i] = msn;
		}
		return mediaNames;
	    }	    
	}
	return new Media[0];
    }
   

    public Class[] getSupportedAttributeCategories() {
	if (supportedCats != null) {
	    return supportedCats;
	}

	initAttributes();

	ArrayList catList = new ArrayList();
	Class cl;

	for (int i=0; i < printReqAttribDefault.length; i++) {
	    PrintRequestAttribute pra = 
		(PrintRequestAttribute)printReqAttribDefault[i];
	    if (getAttMap != null && 
		getAttMap.containsKey(pra.getName()+"-supported")) {
		cl = pra.getCategory();
		catList.add(cl);
	    }	   
	}	  
 
	// Some IPP printers like lexc710 do not have list of supported media
	// but CUPS can get the media from PPD, so we still report as 
	// supported category.
	if (isCupsPrinter) {
	    if (!catList.contains(Media.class)) {
		catList.add(Media.class);
	    }

	    // Always add MediaPrintable for cups, 
	    // because we can get it from PPD.
	    catList.add(MediaPrintableArea.class);
	    
	    // this is already supported in UnixPrintJob
	    catList.add(Destination.class);
	}

	// With the assumption that  Chromaticity is equivalent to
	// ColorSupported.
	if (getAttMap != null && getAttMap.containsKey("color-supported")) {
	    catList.add(Chromaticity.class);
	}
	supportedCats = new Class[catList.size()];
	catList.toArray(supportedCats);
	return supportedCats;
    }


    public boolean
	isAttributeCategorySupported(Class<? extends Attribute> category)
    {
	if (category == null) {
	    throw new NullPointerException("null category");
	}
	if (!(Attribute.class.isAssignableFrom(category))) {
	    throw new IllegalArgumentException(category +
					     " is not an Attribute");
	}

	if (supportedCats == null) {
	    getSupportedAttributeCategories();
	}
	
	for (int i=0;i<supportedCats.length;i++) {
	    if (category == supportedCats[i]) {
		return true;
	    }
	}
	
	return false;
    }


    public <T extends PrintServiceAttribute>
	T getAttribute(Class<T> category)
    {
	if (category == null) {
	    throw new NullPointerException("category");
	}
	if (!(PrintServiceAttribute.class.isAssignableFrom(category))) {
	    throw new IllegalArgumentException("Not a PrintServiceAttribute");
	}
	
	initAttributes();

	if (category == PrinterName.class) {
	    return (T)(new PrinterName(printer, null));
	} else if (category == QueuedJobCount.class) {
	    QueuedJobCount qjc = new QueuedJobCount(0);
	    AttributeClass ac = (getAttMap != null) ?
		(AttributeClass)getAttMap.get(qjc.getName())
		: null;
	    if (ac != null) {
		qjc = new QueuedJobCount(ac.getIntValue());
	    }
	    return (T)qjc;
	} else if (category == PrinterIsAcceptingJobs.class) {
	    PrinterIsAcceptingJobs accJob = 
		PrinterIsAcceptingJobs.ACCEPTING_JOBS;
	    AttributeClass ac = (getAttMap != null) ?
		(AttributeClass)getAttMap.get(accJob.getName()) 
		: null;
	    if ((ac != null) && (ac.getByteValue() == 0)) {
		accJob = PrinterIsAcceptingJobs.NOT_ACCEPTING_JOBS;
	    }
	    return (T)accJob;
	} else if (category == ColorSupported.class) {
	    ColorSupported cs = ColorSupported.SUPPORTED;
	    AttributeClass ac = (getAttMap != null) ?
		(AttributeClass)getAttMap.get(cs.getName())
		: null;
	    if ((ac != null) && (ac.getByteValue() == 0)) {
		cs = ColorSupported.NOT_SUPPORTED;
	    }
	    return (T)cs;
	} else if (category == PDLOverrideSupported.class) {

	    if (isCupsPrinter) {
		// Documented: For CUPS this will always be false
		return (T)PDLOverrideSupported.NOT_ATTEMPTED;
	    } else {
		// REMIND: check attribute values
		return (T)PDLOverrideSupported.NOT_ATTEMPTED;
	    }
	} else {
	    return null;
	}
    }
    

    public PrintServiceAttributeSet getAttributes() {
	// update getAttMap by sending again get-attributes IPP request
	init = false;
	initAttributes();

	HashPrintServiceAttributeSet attrs = 
	    new HashPrintServiceAttributeSet();

	for (int i=0; i < serviceAttributes.length; i++) {
	    String name = (String)serviceAttributes[i][1];
	    if (getAttMap != null && getAttMap.containsKey(name)) {
		Class c = (Class)serviceAttributes[i][0];		
		PrintServiceAttribute psa = getAttribute(c);
		if (psa != null) {
		    attrs.add(psa);
		}
	    }
	}
	return AttributeSetUtilities.unmodifiableView(attrs);
    }

    public boolean isIPPSupportedImages(String mimeType) {
	if (supportedDocFlavors == null) {
	    getSupportedDocFlavors();
	}
	
	if (mimeType.equals("image/png") && pngImagesAdded) {
	    return true;
	} else if (mimeType.equals("image/gif") && gifImagesAdded) {
	    return true;
	} else if (mimeType.equals("image/jpeg") && jpgImagesAdded) {
	    return true;
	}

	return false;
    }


    private boolean isSupportedCopies(Copies copies) {
	CopiesSupported cs = (CopiesSupported) 
	    getSupportedAttributeValues(Copies.class, null, null);
	int[][] members = cs.getMembers();
	int min, max;
	if ((members.length > 0) && (members[0].length > 0)) {
	    min = members[0][0];
	    max = members[0][1];
	} else {
	    min = 1;
	    max = MAXCOPIES;
	}
   
	int value = copies.getValue();
	return (value >= min && value <= max);
    }
  
    private boolean isAutoSense(DocFlavor flavor) {
	if (flavor.equals(DocFlavor.BYTE_ARRAY.AUTOSENSE) ||
	    flavor.equals(DocFlavor.INPUT_STREAM.AUTOSENSE) ||
	    flavor.equals(DocFlavor.URL.AUTOSENSE)) {
	    return true;
	}
	else {
	    return false;
	}
    }

    private boolean isSupportedMediaTray(MediaTray msn) {
	initAttributes();

	if (mediaTrays != null) {
	    for (int i=0; i<mediaTrays.length; i++) {
	       if (msn.equals(mediaTrays[i])) {
	    	    return true;
		}
	    } 
	}
	return false;
    }

    private boolean isSupportedMedia(MediaSizeName msn) {
	initAttributes();
	
	for (int i=0; i<mediaSizeNames.length; i++) {
	    debug_println("mediaSizeNames[i] "+mediaSizeNames[i]);
	    if (msn.equals(mediaSizeNames[i])) {
		return true;
	    }
	}
	
	return false;
    }

    /* Return false if flavor is not null, pageable, nor printable and
     * Destination is part of attributes.
     */
    private boolean
	isDestinationSupported(DocFlavor flavor, AttributeSet attributes) {
	    
	    if ((attributes != null) &&
		    (attributes.get(Destination.class) != null) &&
		    !(flavor == null ||
		      flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		      flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE))) {
		return false;
	    }
	    return true;
    }

 
   public boolean isAttributeValueSupported(Attribute attr,
                                             DocFlavor flavor,
                                             AttributeSet attributes) {
	if (attr == null) {
	    throw new NullPointerException("null attribute");
	}
	if (flavor != null) {
	    if (!isDocFlavorSupported(flavor)) {
		throw new IllegalArgumentException(flavor +
					       " is an unsupported flavor");
	    } else if (isAutoSense(flavor)) {
		return false;
	    }
	}
	Class category = attr.getCategory();
	if (!isAttributeCategorySupported(category)) {
	    return false;
	} 

	/* Test if the flavor is compatible with the attributes */
	if (!isDestinationSupported(flavor, attributes)) {
	    return false;
	} 

	/* Test if the flavor is compatible with the category */
	if (attr.getCategory() == Chromaticity.class) {
	    if ((flavor == null) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.GIF) ||
		flavor.equals(DocFlavor.INPUT_STREAM.GIF) ||
		flavor.equals(DocFlavor.URL.GIF) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.JPEG) ||
		flavor.equals(DocFlavor.INPUT_STREAM.JPEG) ||
		flavor.equals(DocFlavor.URL.JPEG) ||
		flavor.equals(DocFlavor.BYTE_ARRAY.PNG) ||
		flavor.equals(DocFlavor.INPUT_STREAM.PNG) ||
		flavor.equals(DocFlavor.URL.PNG)) {
		return attr == Chromaticity.COLOR;
	    } else {
		return false;
	    } 
	} else if (attr.getCategory() == Copies.class) {	   
	    return isSupportedCopies((Copies)attr);
	} else if (attr.getCategory() == Destination.class) {
	    if (flavor == null ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE)) {
		URI uri = ((Destination)attr).getURI();
		if ("file".equals(uri.getScheme()) &&
		    !(uri.getSchemeSpecificPart().equals(""))) {
		    return true;
		}
	    }
	    return false;
	} else if (attr.getCategory() == Media.class) {
	    if (attr instanceof MediaSizeName) {
		return isSupportedMedia((MediaSizeName)attr);
	    }
	    if (attr instanceof MediaTray) {
		return isSupportedMediaTray((MediaTray)attr);
	    }
	} else if (attr.getCategory() == PageRanges.class) {
	    if (flavor != null &&
		!(flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE))) {
		return false;
	    }
	} else if (attr.getCategory() == SheetCollate.class) {
	    if (flavor != null &&
		!(flavor.equals(DocFlavor.SERVICE_FORMATTED.PAGEABLE) ||
		flavor.equals(DocFlavor.SERVICE_FORMATTED.PRINTABLE))) {
		return false;
	    }
 	} else if (attr.getCategory() == Sides.class) {	   
	    Sides[] sidesArray = (Sides[])getSupportedAttributeValues(
					  Sides.class,
					  flavor,
					  attributes);

	    if (sidesArray != null) {
		for (int i=0; i<sidesArray.length; i++) {
		    if (sidesArray[i] == (Sides)attr) {
			return true;
		    }
		}
	    }
	    return false;
	} else if (attr.getCategory() == OrientationRequested.class) {	   
	    OrientationRequested[] orientArray = 
		(OrientationRequested[])getSupportedAttributeValues(
					  OrientationRequested.class,
					  flavor,
					  attributes);

	    if (orientArray != null) {
		for (int i=0; i<orientArray.length; i++) {
		    if (orientArray[i] == (OrientationRequested)attr) {
			return true;
		    }
		}
	    }
	    return false;
	}
	return true;
    }


    public Object
	getDefaultAttributeValue(Class<? extends Attribute> category)
    {
	if (category == null) {
	    throw new NullPointerException("null category");
	}
	if (!Attribute.class.isAssignableFrom(category)) {
	    throw new IllegalArgumentException(category +
					     " is not an Attribute");
	}
	if (!isAttributeCategorySupported(category)) {
	    return null;
	}

	initAttributes();

	String catName = null;
	for (int i=0; i < printReqAttribDefault.length; i++) {
	    PrintRequestAttribute pra = 
		(PrintRequestAttribute)printReqAttribDefault[i];
	    if (pra.getCategory() == category) {
		catName = pra.getName();
		break;
	    }
	}
	String attribName = catName+"-default";
	AttributeClass attribClass = (getAttMap != null) ?
		(AttributeClass)getAttMap.get(attribName) : null;

	if (category == Copies.class) {
	    if (attribClass != null) {
		return new Copies(attribClass.getIntValue());
	    } else {
		return new Copies(1);
	    }
	} else if (category == Chromaticity.class) {
	    return Chromaticity.COLOR; 
	} else if (category == Destination.class) {
	    try {
		return new Destination((new File("out.ps")).toURI());
	    } catch (SecurityException se) {
		return null;
	    }
	} else if (category == Fidelity.class) {
	    return Fidelity.FIDELITY_FALSE;
	} else if (category == Finishings.class) {
	    return Finishings.NONE;
	} else if (category == JobName.class) {
	    return new JobName("Java Printing", null);
	} else if (category == JobSheets.class) {
	    if (attribClass != null && 
		attribClass.getStringValue().equals("none")) {
		return JobSheets.NONE;
	    } else {
		return JobSheets.STANDARD;
	    }
	} else if (category == Media.class) {
	    defaultMediaIndex = 0;
	    if (mediaSizeNames.length == 0) {
		return null;
	    }

	    if (attribClass != null) {
		String name = attribClass.getStringValue(); 
		if (isCupsPrinter) {
		    for (int i=0; i< customMediaSizeNames.length; i++) {
			//REMIND:  get default from PPD. In native _getMedia,
			// move default (ppd_option_t->defchoice) to index 0. 
			// In the meantime, use indexOf because PPD name 
			// may be different from the IPP attribute name.
			if (customMediaSizeNames[i].toString().indexOf(name) 
			    != -1) {
			    defaultMediaIndex = i;
			    return mediaSizeNames[defaultMediaIndex]; 
			}
		    }
		} else {
		    for (int i=0; i< mediaSizeNames.length; i++) {
			if (mediaSizeNames[i].toString().indexOf(name) != -1) {
			    defaultMediaIndex = i;
			    return mediaSizeNames[defaultMediaIndex];
			}
		    }
		}
	    } 	   
	    return mediaSizeNames[defaultMediaIndex];	    
	   
	} else if (category == MediaPrintableArea.class) {
	    MediaPrintableArea[] mpas;
	     if ((cps != null)  && 
		 ((mpas = cps.getMediaPrintableArea()) != null)) {
		 if (defaultMediaIndex == -1) {
		     // initializes value of defaultMediaIndex
		     getDefaultAttributeValue(Media.class); 
		 }	
		 return mpas[defaultMediaIndex];	    	
	     }
	} else if (category == NumberUp.class) {
	    return new NumberUp(1); // for CUPS this is always 1
	} else if (category == OrientationRequested.class) {
	    if (attribClass != null) {
		switch (attribClass.getIntValue()) {
		default:
		case 3: return OrientationRequested.PORTRAIT;
		case 4: return OrientationRequested.LANDSCAPE;
		case 5: return OrientationRequested.REVERSE_LANDSCAPE;
		case 6: return OrientationRequested.REVERSE_PORTRAIT;
		}	   
	    } else {
		return OrientationRequested.PORTRAIT;
	    }
	} else if (category == PageRanges.class) {
	    if (attribClass != null) {
		int[] range = attribClass.getIntRangeValue();
		return new PageRanges(range[0], range[1]);
	    } else {
		return new PageRanges(1, Integer.MAX_VALUE);
	    }
	} else if (category == RequestingUserName.class) {
	    String userName = "";
	    try {
	      userName = System.getProperty("user.name", "");
	    } catch (SecurityException se) {
	    }
	    return new RequestingUserName(userName, null);
	} else if (category == SheetCollate.class) {
	    return SheetCollate.UNCOLLATED;
	} else if (category == Sides.class) {
	    if (attribClass != null) {
		if (attribClass.getStringValue().endsWith("long-edge")) {
		    return Sides.TWO_SIDED_LONG_EDGE;
		} else if (attribClass.getStringValue().endsWith(
							   "short-edge")) {
		    return Sides.TWO_SIDED_SHORT_EDGE;
		} 
	    }
	    return Sides.ONE_SIDED;
	}

	return null;
    }
    
    public ServiceUIFactory getServiceUIFactory() {
	return null;
    }

    public void wakeNotifier() {
	synchronized (this) {
	    if (notifier != null) {
		notifier.wake();
	    }
	}
    }

    public void addPrintServiceAttributeListener(
				 PrintServiceAttributeListener listener) {
	synchronized (this) {
	    if (listener == null) {
		return;
	    }
	    if (notifier == null) {
                notifier = new ServiceNotifier(this);
	    }
	    notifier.addListener(listener);
	}
    }

    public void removePrintServiceAttributeListener(
				  PrintServiceAttributeListener listener) {
	synchronized (this) {
	    if (listener == null || notifier == null ) {
		return;
	    }
	    notifier.removeListener(listener);
            if (notifier.isEmpty()) {
		notifier.stopNotifier();
                notifier = null;
	    }
	}
    }
    
    public String getName() {
	return printer;
    }


    public boolean usesClass(Class c) {
	return (c == sun.print.PSPrinterJob.class);
    }

   
    public static HttpURLConnection getIPPConnection(URL url) {
	HttpURLConnection connection;
	try {
	    connection = (HttpURLConnection)url.openConnection();
	} catch (java.io.IOException ioe) {
	    return null;
	}
	if (!(connection instanceof HttpURLConnection)) {
	    return null;
	}
	connection.setUseCaches(false);
	connection.setDefaultUseCaches(false);
	connection.setDoInput(true);
	connection.setDoOutput(true);
	connection.setRequestProperty("Content-type", "application/ipp");
	return connection;
    }


    private void opGetAttributes() {
	try {
	    debug_println(debugPrefix+"opGetAttributes myURI "+myURI+" myURL "+myURL);
	   
	    AttributeClass attClNoUri[] = {
		AttributeClass.ATTRIBUTES_CHARSET,
		AttributeClass.ATTRIBUTES_NATURAL_LANGUAGE};

	    AttributeClass attCl[] = {
		AttributeClass.ATTRIBUTES_CHARSET,
		AttributeClass.ATTRIBUTES_NATURAL_LANGUAGE,
		new AttributeClass("printer-uri", 
				   AttributeClass.TAG_URI, 
				   ""+myURI)};

	    OutputStream os = urlConnection.getOutputStream();
	    boolean success = (myURI == null) ? 
		writeIPPRequest(os, OP_GET_ATTRIBUTES, attClNoUri) :
		writeIPPRequest(os, OP_GET_ATTRIBUTES, attCl);
	    if (success) {
		InputStream is = null;
		if ((is = urlConnection.getInputStream())!=null) {
		    HashMap[] responseMap = readIPPResponse(is);
		   
		    if (responseMap != null && responseMap.length > 0) {
			getAttMap = responseMap[0];
		    } 
		} else {
		    debug_println(debugPrefix+"opGetAttributes - null input stream");
		}
		is.close();
	    } 
	    os.close();
	} catch (java.io.IOException e) {
	    debug_println(debugPrefix+"opGetAttributes - input/output stream: "+e);
	}
    }


    public static boolean writeIPPRequest(OutputStream os, 
					   String operCode, 
					   AttributeClass[] attCl) {
	OutputStreamWriter osw = new OutputStreamWriter(os);
	char[] opCode =  new char[2];
	opCode[0] =  (char)Byte.parseByte(operCode.substring(0,2), 16);
	opCode[1] =  (char)Byte.parseByte(operCode.substring(2,4), 16);
	char[] bytes = {0x01, 0x01, 0x00, 0x01};
	try {
	    osw.write(bytes, 0, 2); // version number
	    osw.write(opCode, 0, 2); // operation code
	    bytes[0] = 0x00; bytes[1] = 0x00; 
	    osw.write(bytes, 0, 4); // request ID #1

	    bytes[0] = 0x01; // operation-group-tag
	    osw.write(bytes[0]);
	    	    
	    String valStr;
	    char[] lenStr;	
   		      	      
	    AttributeClass ac;
	    for (int i=0; i < attCl.length; i++) {
		ac = attCl[i];	    
		osw.write(ac.getType()); // value tag
				  
		lenStr = ac.getLenChars();
		osw.write(lenStr, 0, 2); // length
		osw.write(""+ac, 0, ac.getName().length());
		
		// check if string range (0x35 -> 0x49)
		if (ac.getType() >= AttributeClass.TAG_TEXT_LANGUAGE && 
		    ac.getType() <= AttributeClass.TAG_MIME_MEDIATYPE){
		    valStr = (String)ac.getObjectValue();		    
		    bytes[0] = 0; bytes[1] = (char)valStr.length();
		    osw.write(bytes, 0, 2);
		    osw.write(valStr, 0, valStr.length());
		} // REMIND: need to support other value tags but for CUPS
		// string is all we need.
	    }

	    osw.write(GRPTAG_END_ATTRIBUTES); 
	    osw.flush();
	    osw.close();
	} catch (java.io.IOException ioe) {
	    debug_println(debugPrefix+"IPPPrintService Exception in writeIPPRequest: "+ioe);
	    return false;
	}
	return true;
    }


    public static HashMap[] readIPPResponse(InputStream inputStream) {

	if (inputStream == null) {
	    return null;
	}

	byte response[] = new byte[ATTRIBUTESTR_MAX];
	try {	    

	    DataInputStream ois = new DataInputStream(inputStream);

	    // read status and ID
	    if ((ois.read(response, 0, 8) > -1) && 
		(response[2] == STATUSCODE_SUCCESS)) {
	
		ByteArrayOutputStream outObj;
		int counter=0;	
		short len = 0;
		String attribStr = null;
		// assign default value
		byte valTagByte = AttributeClass.TAG_KEYWORD;
		ArrayList respList = new ArrayList();
		HashMap responseMap = new HashMap();
		
		response[0] = ois.readByte();
		
		// check for group tags
		while ((response[0] >= GRPTAG_OP_ATTRIBUTES) && 
		       (response[0] <= GRPTAG_PRINTER_ATTRIBUTES)
			  && (response[0] != GRPTAG_END_ATTRIBUTES)) {
		    debug_println(debugPrefix+"checking group tag,  response[0]= "+
				  response[0]);

		    outObj = new ByteArrayOutputStream();
		    //make sure counter and attribStr are re-initialized
		    counter = 0; 
		    attribStr = null; 

		    // read value tag
		    response[0] = ois.readByte();		    
		    while (response[0] >= AttributeClass.TAG_INT && 
			   response[0] <= AttributeClass.TAG_MEMBER_ATTRNAME) {
			// read name length
			len  = ois.readShort();	

			// If current value is not part of previous attribute 
			// then close stream and add it to HashMap.
			// It is part of previous attribute if name length=0.
			if ((len != 0) && (attribStr != null)) {
			    //last byte is the total # of values
			    outObj.write(counter);
			    outObj.flush();
			    outObj.close();			    
			    byte outArray[] = outObj.toByteArray();
			    
			    // if key exists, new HashMap
			    if (responseMap.containsKey(attribStr)) {
				respList.add(responseMap);
				responseMap = new HashMap();
			    }
			    AttributeClass ac = 
				new AttributeClass(attribStr, 
						   valTagByte,
						   outArray);
			   
			    responseMap.put(ac.getName(), ac);
			    
			    outObj = new ByteArrayOutputStream();
			    counter = 0; //reset counter
			}
			//check if this is new value tag
			if (counter == 0) {			    
			    valTagByte = response[0];
			}			
			// read attribute name
			if (len != 0) {
			    // read "len" characters 
			    ois.read(response, 0, len);			   
			    attribStr = new String(response, 0, len);
			}
			// read value length
			len  = ois.readShort();	
			// write name length
			outObj.write(len);		
			// read value
			ois.read(response, 0, len);
			// write value of "len" length
			outObj.write(response, 0, len);	
			counter++;
			// read next byte
			response[0] = ois.readByte();
		    }

		    if (attribStr != null) {
			outObj.write(counter);		
			outObj.flush();
			outObj.close();	
		
			// if key exists in old HashMap, new HashMap
			if ((counter != 0) && 
			    responseMap.containsKey(attribStr)) {
			    respList.add(responseMap);
			    responseMap = new HashMap();
			}	

			byte outArray[] = outObj.toByteArray();
			
			AttributeClass ac = 
			    new AttributeClass(attribStr, 
					       valTagByte,
					       outArray);
			responseMap.put(ac.getName(), ac);
		    }
		}
		ois.close();
		if ((responseMap != null) && (responseMap.size() > 0)) {
		    respList.add(responseMap);
		}
		return (HashMap[])respList.toArray(
				  new HashMap[respList.size()]);
	    } else {
		debug_println(debugPrefix+
			      "readIPPResponse client error, IPP status code-"
				   +Integer.toHexString(response[2])+" & "
				   +Integer.toHexString(response[3]));
		return null;
	    }
	    
	} catch (java.io.IOException e) {
	    debug_println(debugPrefix+"readIPPResponse: "+e);
	    return null;
	}
    }


    public String toString() {
	return "IPP Printer : " + getName();
    }

    public boolean equals(Object obj) {
	return  (obj == this ||
		 (obj instanceof IPPPrintService &&
		  ((IPPPrintService)obj).getName().equals(getName())));
    }
}



