/*
 * @(#)XAtom.java	1.23 04/01/22
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.awt.X11;

  /**
     * XAtom is a class that allows you to create and modify X Window properties.
     * An X Atom is an identifier for a property that you can set on any X Window.
     * Standard X Atom are defined by X11 and these atoms are defined in this class
     * for convenience. Common X Atoms like <code>XA_WM_NAME</code> are used to communicate with the
     * Window manager to let it know the Window name. The use and protocol for these
     * atoms are defined in the Inter client communications converntions manual.
     * User specified XAtoms are defined by specifying a name that gets Interned
     * by the XServer and an <code>XAtom</code> object is returned. An <code>XAtom</code> can also be created
     * by using a pre-exisiting atom like <code>XA_WM_CLASS</code>. A <code>display</code> has to be specified
     * in order to create an <code>XAtom</code>. <p> <p>
     *
     * Once an <code>XAtom</code> instance is created, you can call get and set property methods to
     * set the values for a particular window. <p> <p>
     *
     *
     * Example usage : To set the window name for a top level: <p>
     * <code>
     * XAtom xa = new XAtom(display,XAtom.XA_WM_NAME); <p>
     * xa.setProperty(window,"Hello World");<p></code>
     *<p>
     *<p>
     * To get the cut buffer :<p>
     * <p><code>
     * XAtom xa = new XAtom(display,XAtom.XA_CUT_BUFFER0);<p>
     * String selection = xa.getProperty(root_window);<p></code>
     * @version     1.0, 9/6/02
     * @author  Bino George
     * @since       JDK1.5
     */

import sun.misc.Unsafe;
import java.util.HashMap;

public class XAtom {

    /* Predefined Atoms - automatically extracted from XAtom.h */
    private static Unsafe unsafe = XlibWrapper.unsafe;
    private static XAtom[] emptyList = new XAtom[0];

    public static final long XA_PRIMARY=1;
    public static final long XA_SECONDARY=2;
    public static final long XA_ARC=3;
    public static final long XA_ATOM=4;
    public static final long XA_BITMAP=5;
    public static final long XA_CARDINAL=6;
    public static final long XA_COLORMAP=7;
    public static final long XA_CURSOR=8;
    public static final long XA_CUT_BUFFER0=9;
    public static final long XA_CUT_BUFFER1=10;
    public static final long XA_CUT_BUFFER2=11;
    public static final long XA_CUT_BUFFER3=12;
    public static final long XA_CUT_BUFFER4=13;
    public static final long XA_CUT_BUFFER5=14;
    public static final long XA_CUT_BUFFER6=15;
    public static final long XA_CUT_BUFFER7=16;
    public static final long XA_DRAWABLE=17;
    public static final long XA_FONT=18;
    public static final long XA_INTEGER=19;
    public static final long XA_PIXMAP=20;
    public static final long XA_POINT=21;
    public static final long XA_RECTANGLE=22;
    public static final long XA_RESOURCE_MANAGER=23;
    public static final long XA_RGB_COLOR_MAP=24;
    public static final long XA_RGB_BEST_MAP=25;
    public static final long XA_RGB_BLUE_MAP=26;
    public static final long XA_RGB_DEFAULT_MAP=27;
    public static final long XA_RGB_GRAY_MAP=28;
    public static final long XA_RGB_GREEN_MAP=29;
    public static final long XA_RGB_RED_MAP=30;
    public static final long XA_STRING=31;
    public static final long XA_VISUALID=32;
    public static final long XA_WINDOW=33;
    public static final long XA_WM_COMMAND=34;
    public static final long XA_WM_HINTS=35;
    public static final long XA_WM_CLIENT_MACHINE=36;
    public static final long XA_WM_ICON_NAME=37;
    public static final long XA_WM_ICON_SIZE=38;
    public static final long XA_WM_NAME=39;
    public static final long XA_WM_NORMAL_HINTS=40;
    public static final long XA_WM_SIZE_HINTS=41;
    public static final long XA_WM_ZOOM_HINTS=42;
    public static final long XA_MIN_SPACE=43;
    public static final long XA_NORM_SPACE=44;
    public static final long XA_MAX_SPACE=45;
    public static final long XA_END_SPACE=46;
    public static final long XA_SUPERSCRIPT_X=47;
    public static final long XA_SUPERSCRIPT_Y=48;
    public static final long XA_SUBSCRIPT_X=49;
    public static final long XA_SUBSCRIPT_Y=50;
    public static final long XA_UNDERLINE_POSITION=51;
    public static final long XA_UNDERLINE_THICKNESS=52 ;
    public static final long XA_STRIKEOUT_ASCENT=53;
    public static final long XA_STRIKEOUT_DESCENT=54;
    public static final long XA_ITALIC_ANGLE=55;
    public static final long XA_X_HEIGHT=56;
    public static final long XA_QUAD_WIDTH=57;
    public static final long XA_WEIGHT=58;
    public static final long XA_POINT_SIZE=59;
    public static final long XA_RESOLUTION=60;
    public static final long XA_COPYRIGHT=61;
    public static final long XA_NOTICE=62;
    public static final long XA_FONT_NAME=63;
    public static final long XA_FAMILY_NAME=64;
    public static final long XA_FULL_NAME=65;
    public static final long XA_CAP_HEIGHT=66;
    public static final long XA_WM_CLASS=67;
    public static final long XA_WM_TRANSIENT_FOR=68;
    public static final long XA_LAST_PREDEFINED=68;
    static HashMap atomToAtom = new HashMap();
    static HashMap nameToAtom = new HashMap();
    static void register(XAtom at) {
        if (at == null) {
            return;
        }
        if (at.atom == 0 || at.name == null) {
            return;
        }
        atomToAtom.put(new Long(at.atom), at);
        nameToAtom.put(at.name, at);
    }
    static XAtom lookup(long atom) {
        XAtom at = (XAtom)atomToAtom.get(new Long(atom));
        return at;
    }
    static XAtom lookup(String name) {
        return (XAtom)nameToAtom.get(name);
    }
    /*
     * [das]Suggestion:
     * 1.Make XAtom immutable.
     * 2.Replace public ctors with factory methods (e.g. get() below).
     */
    static XAtom get(long atom) {
        XAtom xatom = lookup(atom);
        if (xatom == null) {
            xatom = new XAtom(XToolkit.getDisplay(), atom);
        }
        return xatom;
    }
    static XAtom get(String name) {
        XAtom xatom = lookup(name);
        if (xatom == null) {
            xatom = new XAtom(name);
        }
        return xatom;
    }
    public final String getName() {
        return name;
    }
    static String asString(long atom) {
        XAtom at = lookup(atom);
        if (at == null) {
            return Long.toString(atom);
        } else {
            return at.toString();
        }
    }
    void register() {
        register(this);
    }
    public String toString() {
        if (name != null) {
            return name + ":" + atom;
        } else {
            return Long.toString(atom);
        }
    }

    /* interned value of Atom */
    long atom = 0;

    /* name of atom */
    String name;

    /* display for X connection */
    long display;


    /**  This constructor will create and intern a new XAtom that is specified
     *  by the supplied name.
     *
     * @param <code> display  </code> X display to use
     * @param <code name </code> name of the XAtom to create.
     * @since 1.5
     */

    private XAtom(long display, String name) {
        this(display, name, true);
    }

    private XAtom(String name) {
        this(XToolkit.getDisplay(), name, true);
    }

    public XAtom(String name, boolean autoIntern) {
        this(XToolkit.getDisplay(), name, autoIntern);
    }

    /**  This constructor will create an instance of XAtom that is specified
     *  by the predefined XAtom specified by u <code> latom </code>
     *
     * @param <code> display  </code> X display to use.
     * @param <code atom </code> a predefined XAtom.
     * @since 1.5
     */
    public XAtom(long display, long atom) {
        this.atom = atom;
        synchronized (XToolkit.getAWTLock()) {
            this.name = XlibWrapper.XGetAtomName(display, atom);
        }
        this.display = display;
    }

    /**  This constructor will create the instance,
     *  and if <code>autoIntern</code> is true intern a new XAtom that is specified
     *  by the supplied name.
     *
     * @param <code> display  </code> X display to use
     * @param <code name </code> name of the XAtom to create.
     * @since 1.5
     */

    public XAtom(long display, String name, boolean autoIntern) {
        this.name = name;
        this.display = display;
        if (autoIntern) {
            synchronized (XToolkit.getAWTLock()) {
                atom = XlibWrapper.InternAtom(display,name,0);
            }
            register();
        }
    }

    /**
     * Creates uninitialized instance of
     */
    public XAtom() {
    }

    /**  Sets the window property for the specified window
     * @param <code> window  </code> window id to use
     * @param <code str </code> value to set to.
     * @since 1.5
     */
    public void setProperty(long window, String str) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.SetProperty(display,window,atom,str);
        }
    }

    /**
     * Sets STRING/8 type property. Explicitly converts str to Latin-1 byte sequence.
     */
    public void setProperty8(long window, String str) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        byte[] bdata = null;
        try {
            bdata = str.getBytes("ISO-8859-1");
        } catch (java.io.UnsupportedEncodingException uee) {
            uee.printStackTrace();
        }
        if (bdata != null) {
            setAtomData(window, XA_STRING, bdata);
        }
    }


    /**  Gets the window property for the specified window
     * @param <code> window  </code> window id to use
     * @param <code str </code> value to set to.
     * @return string with the property.
     * @since 1.5
     */
    public String getProperty(long window) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            return XlibWrapper.GetProperty(display,window,atom);
        }
    }


    /*
     * Auxiliary function that returns the value of 'property' of type
     * 'property_type' on window 'window'.  Format of the property must be 32.
     */
    public long get32Property(long window, long property_type) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, 1,
                                     false, property_type);
        try {
            int status = getter.execute(XToolkit.IgnoreBadWindowHandler);
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return 0;
            }
            if (getter.getActualType() != property_type || getter.getActualFormat() != 32) {
                return 0;
            }
            return Native.getCard32(getter.getData());
        } finally {
            getter.dispose();
        }
    }

    /**
     *  Returns value of property of type CARDINAL/32 of this window
     */
    public long getCard32Property(XBaseWindow window) {
        return get32Property(window.getWindow(), XA_CARDINAL);
    }

    /**
     * Sets property of type CARDINAL on the window
     */
    public void setCard32Property(long window, long value) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            Native.putCard32(XlibWrapper.larg1, value);
            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), window,
                atom, XA_CARDINAL, 32, XlibWrapper.PropModeReplace,
                XlibWrapper.larg1, 1);
        }
    }

    /**
     * Sets property of type CARDINAL/32 on the window
     */
    public void setCard32Property(XBaseWindow window, long value) {
        setCard32Property(window.getWindow(), value);
    }

    /**
     * Gets uninterpreted set of data from property and stores them in data_ptr.
     * Property type is the same as current atom, property is current atom.
     * Property format is 32. Property 'delete' is false.
     * Returns boolean if requested type, format, length match returned values
     * and returned data pointer is not null.
     */
    boolean getAtomData(long window, long data_ptr, int length) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, (long)length,
                                     false, this);
        try {
            int status = getter.execute();
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return false;
            }
            if (getter.getActualType() != atom
                || getter.getActualFormat() != 32
                || getter.getNumberOfItems() != length
                )
                {
                    return false;
                }
            XlibWrapper.memcpy(data_ptr, getter.getData(), length*getAtomSize());
            return true;
        } finally {
            getter.dispose();
        }
    }

    /**
     * Gets uninterpreted set of data from property and stores them in data_ptr.
     * Property type is <code>type</code>, property is current atom.
     * Property format is 32. Property 'delete' is false.
     * Returns boolean if requested type, format, length match returned values
     * and returned data pointer is not null.
     */
    boolean getAtomData(long window, long type, long data_ptr, int length) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, (long)length,
                                     false, type);
        try {
            int status = getter.execute();
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return false;
            }
            if (getter.getActualType() != type
                || getter.getActualFormat() != 32
                || getter.getNumberOfItems() != length
                )
                {
                    return false;
                }
            XlibWrapper.memcpy(data_ptr, getter.getData(), length*getAtomSize());
            return true;
        } finally {
            getter.dispose();
        }
    }

    /**
     * Sets uninterpreted set of data into property from data_ptr.
     * Property type is the same as current atom, property is current atom.
     * Property format is 32. Mode is PropModeReplace. length is a number
     * of items pointer by data_ptr.
     */
    void setAtomData(long window, long data_ptr, int length) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), window,
                atom, atom, 32, XlibWrapper.PropModeReplace,
                data_ptr, length);
        }
    }

    /**
     * Sets uninterpreted set of data into property from data_ptr.
     * Property type is <code>type</code>, property is current atom.
     * Property format is 32. Mode is PropModeReplace. length is a number
     * of items pointer by data_ptr.
     */
    void setAtomData(long window, long type, long data_ptr, int length) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), window,
                atom, type, 32, XlibWrapper.PropModeReplace,
                data_ptr, length);
        }
    }

    /**
     * Sets uninterpreted set of data into property from data_ptr.
     * Property type is <code>type</code>, property is current atom.
     * Property format is 8. Mode is PropModeReplace. length is a number
     * of bytes pointer by data_ptr.
     */
    void setAtomData8(long window, long type, long data_ptr, int length) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), window,
                atom, type, 8, XlibWrapper.PropModeReplace,
                data_ptr, length);
        }
    }



    /**
     * Deletes property specified by this item on the window.
     */
    public void DeleteProperty(long window) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
             XlibWrapper.XDeleteProperty(XToolkit.getDisplay(), window, atom);
        }
    }

    /**
     * Deletes property specified by this item on the window.
     */
    public void DeleteProperty(XBaseWindow window) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window.getWindow());
        synchronized (XToolkit.getAWTLock()) {
            XlibWrapper.XDeleteProperty(XToolkit.getDisplay(), 
                window.getWindow(), atom);
        }
    }
    
    public void setAtomData(long window, long property_type, byte[] data) {
        long bdata = Native.toData(data);
        try {
            setAtomData8(window, property_type, bdata, data.length);
        } finally {
            unsafe.freeMemory(bdata);
        }
    }

    /*
     * Auxiliary function that returns the value of 'property' of type
     * 'property_type' on window 'window'.  Format of the property must be 8.
     */
    public byte[] getByteArrayProperty(long window, long property_type) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, 0xFFFF,
                                     false, property_type);
        try {
            int status = getter.execute(XToolkit.IgnoreBadWindowHandler);
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return null;
            }
            if (getter.getActualType() != property_type || getter.getActualFormat() != 8) {
                return null;
            }
            byte[] res = XlibWrapper.getStringBytes(getter.getData());
            return res;
        } finally {
            getter.dispose();
        }
    }

    /**
     * Interns the XAtom
     */
    public void intern(boolean onlyIfExists) {
        synchronized (XToolkit.getAWTLock()) {
            atom = XlibWrapper.InternAtom(display,name, onlyIfExists?1:0);
        }
        register();
    }

    public boolean isInterned() {
        if (atom == 0) {
            synchronized (XToolkit.getAWTLock()) {
                atom = XlibWrapper.InternAtom(display, name, 1);
            }
            if (atom == 0) {
                return false;
            } else {
                return true;
            }
        } else {
            return true;
        }
    }

    /**
     * Initializes atom with name and display values
     */
    public void setValues(long display, String name, boolean autoIntern) {
        this.display = display;
        this.name = name;
        if (autoIntern) {
            synchronized (XToolkit.getAWTLock()) {
                atom = XlibWrapper.InternAtom(display,name,0);
            }
            register();
        }
    }

    public void setValues(long display, long atom) {
        this.display = display;
        this.atom = atom;
        register();
    }
    public void setValues(long display, String name, long atom) {
        this.display = display;
        this.atom = atom;
        this.name = name;
        register();
    }

    static int getAtomSize() {
        return Native.getLongSize();
    }

    /*
     * Returns the value of property ATOM[]/32 as array of XAtom objects
     * @return array of atoms, array of length 0 if the atom list is empty
     *         or has different format
     */
    XAtom[] getAtomListProperty(long window) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);

        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, 0xFFFF,
                                     false, XA_ATOM);
        try {
            int status = getter.execute();
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return emptyList;
            }
            if (getter.getActualType() != XA_ATOM || getter.getActualFormat() != 32) {
                return emptyList;
            }

            int count = (int)getter.getNumberOfItems();
            if (count == 0) {
                return emptyList;
            }
            long list_atoms = getter.getData();
            XAtom[] res = new XAtom[count];
            for (int index = 0; index < count; index++) {
                res[index] = XAtom.get(XAtom.getAtom(list_atoms+index*getAtomSize()));
            }
            return res;
        } finally {
            getter.dispose();
        }
    }

    /*
     * Returns the value of property of type ATOM[]/32 as XAtomList
     * @return list of atoms, empty list if the atom list is empty
     *         or has different format
     */
    XAtomList getAtomListPropertyList(long window) {
        return new XAtomList(getAtomListProperty(window));
    }
    XAtomList getAtomListPropertyList(XBaseWindow window) {
        return getAtomListPropertyList(window.getWindow());
    }
    XAtom[] getAtomListProperty(XBaseWindow window) {
        return getAtomListProperty(window.getWindow());
    }

    /**
     * Sets property value of type ATOM list to the list of atoms.
     */
    void setAtomListProperty(long window, XAtom[] atoms) {
        long data = toData(atoms);
        setAtomData(window, XAtom.XA_ATOM, data, atoms.length);
        unsafe.freeMemory(data);
    }

    /**
     * Sets property value of type ATOM list to the list of atoms specified by XAtomList
     */
    void setAtomListProperty(long window, XAtomList atoms) {
        long data = atoms.getAtomsData();
        setAtomData(window, XAtom.XA_ATOM, data, atoms.size());
        unsafe.freeMemory(data);
    }
    /**
     * Sets property value of type ATOM list to the list of atoms.
     */
    public void setAtomListProperty(XBaseWindow window, XAtom[] atoms) {
        setAtomListProperty(window.getWindow(), atoms);
    }

    /**
     * Sets property value of type ATOM list to the list of atoms specified by XAtomList
     */
    public void setAtomListProperty(XBaseWindow window, XAtomList atoms) {
        setAtomListProperty(window.getWindow(), atoms);
    }

    long getAtom() {
        return atom;
    }

    void putAtom(long ptr) {
        Native.putLong(ptr, atom);
    }

    static long getAtom(long ptr) {
        return Native.getLong(ptr);
    }
    /**
     * Allocated memory to hold the list of native atom data and returns unsafe pointer to it
     * Caller should free the memory by himself.
     */
    static long toData(XAtom[] atoms) {
        long data = unsafe.allocateMemory(getAtomSize() * atoms.length);
        for (int i = 0; i < atoms.length; i++ ) {
            if (atoms[i] != null) {
                atoms[i].putAtom(data + i * getAtomSize());
            }
        }
        return data;
    }

    void checkWindow(long window) {
        if (window == 0) {
            throw new IllegalArgumentException("Window must not be zero");
        }
    }

    public boolean equals(Object o) {
        if (!(o instanceof XAtom)) {
            return false;
        }
        XAtom ot = (XAtom)o;
        return (atom == ot.atom && display == ot.display);
    }
    public int hashCode() {
        return (int)((atom ^ display)& 0xFFFFL);
    }

    /**
     * Sets property on the <code>window</code> to the value <code>window_value</window>
     * Property is assumed to be of type WINDOW/32
     */ 
    public void setWindowProperty(long window, long window_value) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        synchronized (XToolkit.getAWTLock()) {
            Native.putWindow(XlibWrapper.larg1, window_value);
            XlibWrapper.XChangeProperty(XToolkit.getDisplay(), window,
                                    atom, XA_WINDOW, 32, XlibWrapper.PropModeReplace,
                                    XlibWrapper.larg1, 1);
        }        
    }
    public void setWindowProperty(XBaseWindow window, XBaseWindow window_value) {
        setWindowProperty(window.getWindow(), window_value.getWindow());
    }

    /**
     * Gets property on the <code>window</code>. Property is assumed to be
     * of type WINDOW/32.
     */
    public long getWindowProperty(long window) {
        if (atom == 0) {
            throw new IllegalStateException("Atom should be initialized");
        }
        checkWindow(window);
        WindowPropertyGetter getter =
            new WindowPropertyGetter(window, this, 0, 1,
                                     false, XA_WINDOW);
        try {
            int status = getter.execute(XToolkit.IgnoreBadWindowHandler);
            if (status != XlibWrapper.Success || getter.getData() == 0) {
                return 0;
            }
            if (getter.getActualType() != XA_WINDOW || getter.getActualFormat() != 32) {
                return 0;
            }
            return Native.getWindow(getter.getData());
        } finally {
            getter.dispose();
        }
    }    
}

