/*
 * @(#)XProtocol.java	1.12 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.awt.X11;

import java.util.logging.*;

import java.util.*;

class XProtocol {
    private final static Logger log = Logger.getLogger("sun.awt.X11.XProtocol");

    private Map<XAtom, XAtomList> atomToList = new HashMap<XAtom, XAtomList>();
    private Map<XAtom, Long> atomToAnchor = new HashMap<XAtom, Long>();

    /*
     * Temporary error handler that ensures that we know if
     * XChangeProperty succeeded or not.
     */
    static XToolkit.XErrorHandler VerifyChangePropertyHandler = new XToolkit.XErrorHandler() {
            public int handleError(long display, XErrorEvent err) {
                XToolkit.XERROR_SAVE(err);
                if (err.get_request_code() == XlibWrapper.X_ChangeProperty) {
                    return 0;
                } else {
                    return XToolkit.SAVED_ERROR_HANDLER(display, err);
                }
            }
        };
    volatile boolean firstCheck = true;
    /*
     * Check that that the list of protocols specified by WM in property
     * named LIST_NAME on the root window contains protocol PROTO.
     */
    boolean checkProtocol(XAtom listName, XAtom protocol) {
        XAtomList protocols = atomToList.get(listName);

        if (protocols != null) {
            return protocols.contains(protocol);
        }

        protocols = listName.getAtomListPropertyList(XToolkit.getDefaultRootWindow());
        atomToList.put(listName, protocols);
        try {
            return protocols.contains(protocol);
        } finally {
            if (firstCheck) {
                firstCheck = false;
                if (log.isLoggable(Level.FINE)) {
                    log.log(Level.FINE, "{0}:{1} supports {2}",
                            new Object[] {String.valueOf(this), String.valueOf(listName),
                                          String.valueOf(protocols)});
                }
            }
        }
    }

    /*
     * Check for anchor_prop(anchor_type) on root, take the value as the
     * window id and check if that window exists and has anchor_prop(anchor_type)
     * with the same value (i.e. pointing back to self).
     *
     * Returns the anchor window, as some WM may put interesting stuff in
     * its properties (e.g. sawfish).
     */
    long checkAnchorImpl(XAtom anchorProp, long anchorType) {
        long root_xref, self_xref;

        XToolkit.awtLock();
        try {
            root_xref = anchorProp.get32Property(XToolkit.getDefaultRootWindow(),
                                                 anchorType);
        } finally {
            XToolkit.awtUnlock();
        }
        if (root_xref == 0) {
            return 0;
        }
        self_xref = anchorProp.get32Property(root_xref, anchorType);
        if (self_xref != root_xref) {
            return 0;
        }
        return self_xref;
    }
    public long checkAnchor(XAtom anchorProp, long anchorType) {
        Long val = atomToAnchor.get(anchorProp);
        if (val != null) {
            return val.longValue();
        }
        long res = checkAnchorImpl(anchorProp, anchorType);
        atomToAnchor.put(anchorProp, res);
        return res;
    }
    public long checkAnchor(XAtom anchorProp, XAtom anchorType) {
        return checkAnchor(anchorProp, anchorType.getAtom());
    }
    
}
