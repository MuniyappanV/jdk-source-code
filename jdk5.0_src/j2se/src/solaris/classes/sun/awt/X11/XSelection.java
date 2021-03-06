/*
 * @(#)XSelection.java	1.12 04/01/08
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.awt.X11;

import java.awt.datatransfer.Transferable;

import java.io.IOException;

import java.util.Hashtable;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;

import sun.awt.AppContext;
import sun.awt.SunToolkit;

import sun.awt.datatransfer.DataTransferer;

/**
 * A class which interfaces with the X11 selection service.
 */
public class XSelection {

    /* Maps atoms to XSelection instances. */
    private static final Hashtable table = new Hashtable();
    /* Prevents from parallel selection data request processing. */
    private static final Object lock = new Object();
    /* The property in which the owner should place the requested data. */
    private static final XAtom selectionPropertyAtom = XAtom.get("XAWT_SELECTION");
    /* The maximal length of the property data. */
    public static final long MAX_LENGTH = 1000000;
    /*
     * The maximum data size for ChangeProperty request.
     * 100 is for the structure prepended to the request.
     */
    public static final int MAX_PROPERTY_SIZE;
    static {
        synchronized (XToolkit.getAWTLock()) {
            MAX_PROPERTY_SIZE =
                (int)(XlibWrapper.XMaxRequestSize(XToolkit.getDisplay()) * 4 - 100);
        }
    }
    /* The selection timeout. */
    public static final long SELECTION_TIMEOUT = 10000;

    /* The PropertyNotify event handler for incremental data transfer. */
    private static final XEventDispatcher incrementalTransferHandler = 
        new IncrementalTransferHandler();
    /* The context for the current request - protected with awtLock. */
    private static WindowPropertyGetter propertyGetter = null;

    // The orders of the lock acquisition:
    //   XClipboard -> XSelection -> awtLock.
    //   lock -> awtLock.

    /* The X atom for the underlying selection. */
    private final XAtom selectionAtom;
    /*
     * XClipboard.run() is to be called when we lose ownership.
     * XClipbioard.checkChange() is to be called when tracking changes of flavors.
     */
    private final XClipboard clipboard;

    /*
     * Owner-related variables - protected with synchronized (this).
     */

    /* The contents supplied by the current owner. */
    private Transferable contents = null;
    /* The format-to-flavor map for the current owner. */
    private Map formatMap = null;
    /* The formats supported by the current owner was set. */
    private long[] formats = null;
    /* The AppContext in which the current owner was set. */
    private AppContext appContext = null;
    /* The time at which the current owner was set. */
    private long ownershipTime = 0;
    // True if we are the owner of this selection.
    private boolean isOwner;
    // The property in which the owner should place requested targets
    // when tracking changes of available data flavors (practically targets).
    private volatile XAtom targetsPropertyAtom;
    // A set of these property atoms.
    private static volatile Set targetsPropertyAtoms;
    // The flag used not to call XConvertSelection() if the previous SelectionNotify
    // has not been processed by checkChange().
    private volatile boolean isSelectionNotifyProcessed;
    // Time of calling XConvertSelection().
    private long convertSelectionTime;


    static {
        XToolkit.addEventDispatcher(XWindow.getXAWTRootWindow().getWindow(), 
                                    new SelectionEventHandler());
    }

    /*
     * Returns the XSelection object for the specified selection atom or
     * <code>null</code> if none exists.
     */
    static XSelection getSelection(XAtom atom) {
        return (XSelection)table.get(atom);
    }

    /**
     * Creates a selection object.
     *
     * @param atom   the selection atom.
     * @param clpbrd the corresponding clipoboard
     * @exception NullPointerException if atom is <code>null</code>.
     */
    public XSelection(XAtom atom, XClipboard clpbrd) {
        if (atom == null) {
            throw new NullPointerException("Null atom");
        }
        selectionAtom = atom;
        clipboard = clpbrd;
        table.put(selectionAtom, this);
    }

    public XAtom getSelectionAtom() {
        return selectionAtom;
    }

    void initializeSelectionForTrackingChanges() {
        targetsPropertyAtom = XAtom.get("XAWT_TARGETS_OF_SELECTION:" + selectionAtom.getName());
        if (targetsPropertyAtoms == null) {
            targetsPropertyAtoms = Collections.synchronizedSet(new HashSet(2));
        }
        targetsPropertyAtoms.add(new Long(targetsPropertyAtom.getAtom()));
        // for XConvertSelection() to be called for the first time in getTargetsDelayed()
        isSelectionNotifyProcessed = true;
    }

    void deinitializeSelectionForTrackingChanges() {
        if (targetsPropertyAtoms != null && targetsPropertyAtom != null) {
            targetsPropertyAtoms.remove(new Long(targetsPropertyAtom.getAtom()));
        }
        isSelectionNotifyProcessed = false;
    }

    public synchronized boolean setOwner(Transferable contents, Map formatMap,
                                         long[] formats, long time) {
        long owner = XWindow.getXAWTRootWindow().getWindow();
        long selection = selectionAtom.getAtom();

        // ICCCM prescribes that CurrentTime should not be used for SetSelectionOwner.
        if (time == XlibWrapper.CurrentTime) {
            time = XToolkit.getCurrentServerTime();
        }

        this.contents = contents;
        this.formatMap = formatMap;
        this.formats = formats;
        this.appContext = AppContext.getAppContext();
        this.ownershipTime = time;

        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.XSetSelectionOwner(XToolkit.getDisplay(), 
                                           selection, owner, time);
            if (XlibWrapper.XGetSelectionOwner(XToolkit.getDisplay(), 
                                               selection) != owner) {

                reset();
                return false;
            }
            isOwner = true;
            if (clipboard != null) {
                clipboard.checkChangeHere(contents);
            }
            return true;
        }
    }

    private static void waitForSelectionNotify(WindowPropertyGetter dataGetter) throws InterruptedException {
        long startTime = System.currentTimeMillis();
        synchronized (XToolkit.getAWTLock()) {
            do {
                DataTransferer.getInstance().processDataConversionRequests();
                XToolkit.getAWTLock().wait(250);
            } while (propertyGetter == dataGetter && System.currentTimeMillis() < startTime + SELECTION_TIMEOUT);
        }
    }

    /*
     * Returns the list of atoms that represent the targets for which an attempt
     * to convert the current selection will succeed.
     */
    public long[] getTargets(long time) {
        if (XToolkit.isToolkitThread()) {
            throw new Error("UNIMPLEMENTED");
        }

        long[] formats = null;

        synchronized (lock) {  
            WindowPropertyGetter targetsGetter = 
                new WindowPropertyGetter(XWindow.getXAWTRootWindow().getWindow(), 
                                         selectionPropertyAtom, 0, MAX_LENGTH,
                                         true, XlibWrapper.AnyPropertyType);

            try {
                synchronized (XToolkit.getAWTLock()) {
                    propertyGetter = targetsGetter;

                    XlibWrapper.XConvertSelection(XToolkit.getDisplay(), 
                                                  getSelectionAtom().getAtom(), 
                                                  XDataTransferer.TARGETS_ATOM.getAtom(),
                                                  selectionPropertyAtom.getAtom(),
                                                  XWindow.getXAWTRootWindow().getWindow(),
                                                  time);
                    
                    // If the owner doesn't respond within the
                    // SELECTION_TIMEOUT, we report conversion failure.
                    try {
                        waitForSelectionNotify(targetsGetter);
                    } catch (InterruptedException ie) {
                        return new long[0];
                    } finally {
                        propertyGetter = null;
                    }
                }
                formats = getFormats(targetsGetter);
            } finally {
                targetsGetter.dispose();
            }
        }
        return formats;
    }

    private static long[] getFormats(WindowPropertyGetter targetsGetter) {
        long[] formats = null;

        if (targetsGetter.isExecuted() && !targetsGetter.isDisposed() &&
                (targetsGetter.getActualType() == XAtom.XA_ATOM ||  
                 targetsGetter.getActualType() == XDataTransferer.TARGETS_ATOM.getAtom()) &&
                targetsGetter.getActualFormat() == 32) {

            int count = (int)targetsGetter.getNumberOfItems();
            if (count > 0) {
                long atoms = targetsGetter.getData();
                formats = new long[count];
                for (int index = 0; index < count; index++) {
                    formats[index] = 
                            Native.getLong(atoms+index*XAtom.getAtomSize());
                }
            }
        }

        return formats != null ? formats : new long[0];
    }

    // checkChange() will be called on SelectionNotify
    void getTargetsDelayed() {
        synchronized (XToolkit.getAWTLock()) {
            long curTime = System.currentTimeMillis();
            if (isSelectionNotifyProcessed || curTime >= convertSelectionTime + SELECTION_TIMEOUT) {
                convertSelectionTime = curTime;
                XlibWrapper.XConvertSelection(XToolkit.getDisplay(),
                                              getSelectionAtom().getAtom(),
                                              XDataTransferer.TARGETS_ATOM.getAtom(),
                                              targetsPropertyAtom.getAtom(),
                                              XWindow.getXAWTRootWindow().getWindow(),
                                              XlibWrapper.CurrentTime);
                isSelectionNotifyProcessed = false;
            }
        }
    }

    /*
     * Requests the selection data in the specified format and returns 
     * the data provided by the owner.
     */
    public byte[] getData(long format, long time) throws IOException {
        if (XToolkit.isToolkitThread()) {
            throw new Error("UNIMPLEMENTED");
        }

        byte[] data = null;

        synchronized (lock) {  
            WindowPropertyGetter dataGetter = 
                new WindowPropertyGetter(XWindow.getXAWTRootWindow().getWindow(), 
                                         selectionPropertyAtom, 0, MAX_LENGTH, 
                                         false, // don't delete to handle INCR properly.
                                         XlibWrapper.AnyPropertyType);
            
            try {
                synchronized (XToolkit.getAWTLock()) {
                    propertyGetter = dataGetter;

                    XlibWrapper.XConvertSelection(XToolkit.getDisplay(), 
                                                  getSelectionAtom().getAtom(), 
                                                  format,
                                                  selectionPropertyAtom.getAtom(),
                                                  XWindow.getXAWTRootWindow().getWindow(),
                                                  time);

                    // If the owner doesn't respond within the
                    // SELECTION_TIMEOUT, we report conversion failure.
                    try {
                        waitForSelectionNotify(dataGetter);
                    } catch (InterruptedException ie) {
                        return new byte[0];
                    } finally {
                        propertyGetter = null;
                    }
                }
                if (!dataGetter.isExecuted()) {
                    throw new IOException("Owner failed to convert data");
                }

                if (dataGetter.isDisposed()) {
                    throw new IOException("Owner failed to convert data");
                }

                // Handle incremental transfer.
                if (dataGetter.getActualType() ==
                    XDataTransferer.INCR_ATOM.getAtom()) {

                    if (dataGetter.getActualFormat() != 32) {
                        throw new IOException("Unsupported INCR format: " + 
                                              dataGetter.getActualFormat());
                    }

                    int count = (int)dataGetter.getNumberOfItems();

                    if (count <= 0) {
                        throw new IOException("INCR data is missed.");
                    }

                    long ptr = dataGetter.getData();

                    int len = 0;

                    {
                        // Following Xt sources use the last element.
                        long longLength = Native.getLong(ptr, count-1);

                        if (longLength <= 0) {
                            return new byte[0];
                        }

                        if (longLength > Integer.MAX_VALUE) {
                            throw new IOException("Can't handle large data block: " 
                                                  + longLength + " bytes");
                        }

                        len = (int)longLength;
                    }

                    dataGetter.dispose();

                    data = new byte[len];
                    int cur = 0;

                    while (true) {
                        WindowPropertyGetter incrDataGetter = 
                            new WindowPropertyGetter(XWindow.getXAWTRootWindow().getWindow(), 
                                                     selectionPropertyAtom, 
                                                     0, MAX_LENGTH, false,  
                                                     XlibWrapper.AnyPropertyType);

                        try {
                            XToolkit.awtLock();
                            XToolkit.addEventDispatcher(XWindow.getXAWTRootWindow().getWindow(), 
                                                        incrementalTransferHandler);

                            propertyGetter = incrDataGetter;

                            try {
                                XlibWrapper.XDeleteProperty(XToolkit.getDisplay(),
                                                            XWindow.getXAWTRootWindow().getWindow(),
                                                            selectionPropertyAtom.getAtom());
                                
                                // If the owner doesn't respond within the
                                // SELECTION_TIMEOUT, we terminate incremental
                                // transfer.
                                waitForSelectionNotify(incrDataGetter);
                            } catch (InterruptedException ie) {
                                break;
                            } finally {
                                propertyGetter = null;
                                XToolkit.removeEventDispatcher(XWindow.getXAWTRootWindow().getWindow(), 
                                                               incrementalTransferHandler);
                                XToolkit.awtUnlock();
                            }

                            // The owner didn't respond - terminate the transfer.
                            if (!incrDataGetter.isExecuted()) {
                                break;
                            }

                            if (incrDataGetter.isDisposed()) {
                                throw new IOException("Owner failed to convert data");
                            }

                            if (incrDataGetter.getActualFormat() != 8) {
                                throw new IOException("Unsupported data format: " + 
                                                      dataGetter.getActualFormat());
                            }

                            count = (int)incrDataGetter.getNumberOfItems();
                            
                            if (count == 0) {
                                break;
                            }

                            if (count > 0) {
                                ptr = incrDataGetter.getData();
                                for (int index = 0; index < count; index++) {
                                    data[cur++] = Native.getByte(ptr + index);
                                }
                            }

                        } finally {
                            incrDataGetter.dispose();
                        }
                    }
                } else {
                    synchronized (XToolkit.getAWTLock()) {
                        XlibWrapper.XDeleteProperty(XToolkit.getDisplay(),
                                                    XWindow.getXAWTRootWindow().getWindow(),
                                                    selectionPropertyAtom.getAtom());
                    }

                    if (dataGetter.getActualFormat() != 8) {
                        throw new IOException("Unsupported data format: " + 
                                              dataGetter.getActualFormat());
                    }

                    int count = (int)dataGetter.getNumberOfItems();
                    if (count > 0) {
                        data = new byte[count];
                        long ptr = dataGetter.getData();
                        for (int index = 0; index < count; index++) {
                            data[index] = Native.getByte(ptr + index);
                        }
                    }
                }
            } finally {
                dataGetter.dispose();
            }
        }

        return data != null ? data : new byte[0];
    }

    // To be MT-safe this method should be called under awtLock.
    boolean isOwner() {
        return isOwner;
    }

    public void lostOwnership() {
        isOwner = false;
        if (clipboard != null) {
            clipboard.run();
        }
    }

    public synchronized void reset() {
        contents = null;
        formatMap = null;
        formats = null;
        appContext = null;
        ownershipTime = 0;
    }
    
    // Converts the data to the 'format' and if the conversion succeeded stores
    // the data in the 'property' on the 'requestor' window.
    // Returns true if the conversion succeeded.
    private boolean convertAndStore(long requestor, long format, long property) {
        int dataFormat = 8; /* Can choose between 8,16,32. */
        byte[] byteData = null;
        long nativeDataPtr = 0;
        int count = 0;

        try {
            SunToolkit.insertTargetMapping(this, appContext);

            byteData = DataTransferer.getInstance().convertData(this,
                                                                contents, 
                                                                format, 
                                                                formatMap,
                                                                XToolkit.isToolkitThread());
        } catch (IOException ioe) {
            return false;
        }

        if (byteData == null) {
            return false;
        }
        
        count = byteData.length;

        try {
            if (count > 0) {
                if (count <= MAX_PROPERTY_SIZE) {
                    nativeDataPtr = Native.toData(byteData);
                } else {
                    // Initiate incremental data transfer.
                    new IncrementalDataProvider(requestor, property, format, 8,
                                                byteData); 

                    nativeDataPtr = 
                        XlibWrapper.unsafe.allocateMemory(XAtom.getAtomSize());

                    Native.putLong(nativeDataPtr, (long)count);

                    format = XDataTransferer.INCR_ATOM.getAtom();
                    dataFormat = 32;
                    count = 1;
                }

            }

            synchronized (XToolkit.getAWTLock()) {
                XlibWrapper.XChangeProperty(XToolkit.getDisplay(), requestor, property,
                                            format, dataFormat,
                                            XlibWrapper.PropModeReplace, 
                                            nativeDataPtr, count);
            }
        } finally {
            if (nativeDataPtr != 0) {
                XlibWrapper.unsafe.freeMemory(nativeDataPtr);
                nativeDataPtr = 0;
            }
        }            

        return true;
    }

    private void handleSelectionRequest(XSelectionRequestEvent xsre) {
        long property = xsre.get_property(); 
        long requestor = xsre.get_requestor();
        long requestTime = xsre.get_time();
        long format = xsre.get_target();
        int dataFormat = 0;
        boolean conversionSucceeded = false;

        if (ownershipTime != 0 && 
            (requestTime == XlibWrapper.CurrentTime ||
             requestTime > ownershipTime)) {

            property = xsre.get_property();
            
            // Handle MULTIPLE requests as per ICCCM.
            if (format == XDataTransferer.MULTIPLE_ATOM.getAtom()) {
                // The property cannot be 0 for a MULTIPLE request.
                if (property != 0) {
                    // First retrieve the list of requested targets.
                    WindowPropertyGetter wpg = 
                        new WindowPropertyGetter(requestor, XAtom.get(property), 0, 
                                                 MAX_LENGTH, false,
                                                 XlibWrapper.AnyPropertyType);
                    try {
                        wpg.execute();
                    
                        if (wpg.getActualFormat() == 32 &&
                            (wpg.getNumberOfItems() % 2) == 0) {
                            long count = wpg.getNumberOfItems() / 2;
                            long pairsPtr = wpg.getData();
                            boolean writeBack = false;
                            for (int i = 0; i < count; i++) {
                                long target = Native.getLong(pairsPtr, 2*i);
                                long prop = Native.getLong(pairsPtr, 2*i + 1);

                                if (!convertAndStore(requestor, target, prop)) {
                                    // To report failure, we should replace the
                                    // target atom with 0 in the MULTIPLE property.
                                    Native.putLong(pairsPtr, 2*i, 0);
                                    writeBack = true;
                                }
                            }
                            if (writeBack) {
                                synchronized (XToolkit.getAWTLock()) {
                                    XlibWrapper.XChangeProperty(XToolkit.getDisplay(), requestor,
                                                                property, 
                                                                wpg.getActualType(),
                                                                wpg.getActualFormat(),
                                                                XlibWrapper.PropModeReplace,
                                                                wpg.getData(), 
                                                                wpg.getNumberOfItems());
                                }
                            }
                            conversionSucceeded = true;
                        }
                    } finally {
                        wpg.dispose();
                    }
                }
            } else {

                // Support for obsolete clients as per ICCCM.
                if (property == 0) {
                    property = format;
                }

                if (format == XDataTransferer.TARGETS_ATOM.getAtom()) {
                    long nativeDataPtr = 0;
                    int count = 0;
                    dataFormat = 32;

                    // Use a local copy to avoid synchronization.
                    long[] formatsLocal = formats;

                    if (formatsLocal == null) {
                        throw new IllegalStateException("Not an owner.");
                    }

                    count = formatsLocal.length;

                    try {
                        if (count > 0) {
                            nativeDataPtr = Native.allocateLongArray(count);
                            Native.put(nativeDataPtr, formatsLocal);
                        }

                        conversionSucceeded = true;

                        synchronized (XToolkit.getAWTLock()) {
                            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), requestor,
                                                        property, format, dataFormat,
                                                        XlibWrapper.PropModeReplace,
                                                        nativeDataPtr, count);
                        }
                    } finally {
                        if (nativeDataPtr != 0) {
                            XlibWrapper.unsafe.freeMemory(nativeDataPtr);
                            nativeDataPtr = 0;
                        }
                    }
                } else {
                    conversionSucceeded = convertAndStore(requestor, format,
                                                          property);
                }
            }
        }

        if (!conversionSucceeded) {
            // Zero property indicates conversion failure.
            property = 0;
        }

        XSelectionEvent xse = new XSelectionEvent();
        try {
            xse.set_type((int)XlibWrapper.SelectionNotify);
            xse.set_send_event(true);
            xse.set_requestor(requestor);
            xse.set_selection(selectionAtom.getAtom());
            xse.set_target(format);
            xse.set_property(property);
            xse.set_time(requestTime);

            synchronized (XToolkit.getAWTLock()) {
                XlibWrapper.XSendEvent(XToolkit.getDisplay(), requestor, false,
                                       XlibWrapper.NoEventMask, xse.pData);
            }
        } finally {
            xse.dispose();
        }
    }

    private static void checkChange(XSelectionEvent xse) {
        if (targetsPropertyAtoms == null || targetsPropertyAtoms.isEmpty()) {
            // We are not tracking changes.
            return;
        }

        long propertyAtom = xse.get_property();
        long[] formats = null;

        if (propertyAtom == XlibWrapper.None) {
            // We threat None property atom as "empty selection".
            formats = new long[0];
        } else if (!targetsPropertyAtoms.contains(new Long(propertyAtom))) {
            return;
        } else {
            WindowPropertyGetter targetsGetter =
                new WindowPropertyGetter(XWindow.getXAWTRootWindow().getWindow(),
                                         XAtom.get(propertyAtom), 0, MAX_LENGTH,
                                         true, XlibWrapper.AnyPropertyType);
            try {
                targetsGetter.execute();
                formats = getFormats(targetsGetter);
            } finally {
                targetsGetter.dispose();
            }
        }

        XAtom selectionAtom = XAtom.get(xse.get_selection());
        XSelection selection = getSelection(selectionAtom);
        if (selection != null) {
            selection.isSelectionNotifyProcessed = true;
            selection.clipboard.checkChange(formats);
        }
    }

    private static class SelectionEventHandler implements XEventDispatcher {
        public void dispatchEvent(IXAnyEvent ev) {
            switch (ev.get_type()) {
            case (int)XlibWrapper.SelectionNotify: {
                XSelectionEvent xse = new XSelectionEvent(ev.getPData());
                checkChange(xse);
                synchronized (XToolkit.getAWTLock()) {
                    if (propertyGetter != null) {
                        // The property will be None in case of convertion failure.
                        if (xse.get_property() == selectionPropertyAtom.getAtom()) {
                            propertyGetter.execute();
                            propertyGetter = null;
                        } else if (xse.get_property() == 0) {
                            propertyGetter.dispose();
                            propertyGetter = null;
                        }
                    }
                    XToolkit.getAWTLock().notifyAll();
                }
                break;
            }
            case (int)XlibWrapper.SelectionRequest: {
                XSelectionRequestEvent xsre = new XSelectionRequestEvent(ev.getPData());
                long atom = xsre.get_selection();
                XSelection selection = XSelection.getSelection(XAtom.get(atom));
                
                if (selection != null) {
                    selection.handleSelectionRequest(xsre);
                }
                break;
            }
            case (int)XlibWrapper.SelectionClear: {
                XSelectionClearEvent xsce = new XSelectionClearEvent(ev.getPData());
                long atom = xsce.get_selection();
                XSelection selection = XSelection.getSelection(XAtom.get(atom));
                
                if (selection != null) {
                    selection.lostOwnership();
                }
                
                synchronized (XToolkit.getAWTLock()) {
                    XToolkit.getAWTLock().notifyAll();
                }
                break;
            }
            }
        }
    };

    private static class IncrementalDataProvider implements XEventDispatcher {
        private final long requestor;
        private final long property;
        private final long target;
        private final int format;
        private final byte[] data;
        private int offset = 0;

        // NOTE: formats other than 8 are not supported.
        public IncrementalDataProvider(long requestor, long property, 
                                       long target, int format, byte[] data) { 
            if (format != 8) {
                throw new IllegalArgumentException("Unsupported format: " + format);
            }

            this.requestor = requestor;
            this.property = property;
            this.target = target;
            this.format = format;
            this.data = data;

            XWindowAttributes wattr = new XWindowAttributes();
            try {
                synchronized (XToolkit.getAWTLock()) {
                    XlibWrapper.XGetWindowAttributes(XToolkit.getDisplay(), requestor,
                                                     wattr.pData);
                    XlibWrapper.XSelectInput(XToolkit.getDisplay(), requestor,
                                             wattr.get_your_event_mask() |
                                             XlibWrapper.PropertyChangeMask);
                }
            } finally {
                wattr.dispose();
            }
            XToolkit.addEventDispatcher(requestor, this);
        }

        public void dispatchEvent(IXAnyEvent ev) {
            switch (ev.get_type()) {
            case (int)XlibWrapper.PropertyNotify: 
                XPropertyEvent xpe = new XPropertyEvent(ev.getPData());
                if (xpe.get_window() == requestor &&
                    xpe.get_state() == XlibWrapper.PropertyDelete &&
                    xpe.get_atom() == property) {

                    int count = data.length - offset;
                    long nativeDataPtr = 0;
                    if (count > MAX_PROPERTY_SIZE) {
                        count = MAX_PROPERTY_SIZE;
                    }

                    if (count > 0) {
                        nativeDataPtr = XlibWrapper.unsafe.allocateMemory(count);
                        for (int i = 0; i < count; i++) {
                            Native.putByte(nativeDataPtr+i, data[offset + i]);
                        }
                    } else {
                        assert (count == 0);
                        // All data has been transferred.
                        // This zero-length data indicates end of transfer.
                        XToolkit.removeEventDispatcher(requestor, this);
                    }

                    synchronized (XToolkit.getAWTLock()) {
                        XlibWrapper.XChangeProperty(XToolkit.getDisplay(),
                                                    requestor, property, 
                                                    target, format,
                                                    XlibWrapper.PropModeReplace, 
                                                    nativeDataPtr, count);
                    }
                    if (nativeDataPtr != 0) {
                        XlibWrapper.unsafe.freeMemory(nativeDataPtr);
                        nativeDataPtr = 0;
                    }

                    offset += count;
                }
            }
        }
    }

    private static class IncrementalTransferHandler implements XEventDispatcher {
        public void dispatchEvent(IXAnyEvent ev) {
            switch (ev.get_type()) {
            case (int)XlibWrapper.PropertyNotify:
                XPropertyEvent xpe = new XPropertyEvent(ev.getPData());
                if (xpe.get_state() == XlibWrapper.PropertyNewValue &&
                    xpe.get_atom() == selectionPropertyAtom.getAtom()) {
                    synchronized (XToolkit.getAWTLock()) {
                        if (propertyGetter != null) {
                            propertyGetter.execute();
                            propertyGetter = null;
                        }
                        XToolkit.getAWTLock().notifyAll();
                    }
                }
                break;
            }
        }
    };
}

