/*
 * @(#)UIManager.java	1.115 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package javax.swing;

import java.awt.Component;
import java.awt.Container;
import java.awt.Window;
import java.awt.Font;
import java.awt.Color;
import java.awt.Insets;
import java.awt.Dimension;
import java.awt.KeyboardFocusManager;
import java.awt.KeyEventPostProcessor;
import java.awt.Toolkit;

import java.awt.event.KeyEvent;

import java.security.AccessController;

import javax.swing.plaf.ComponentUI;
import javax.swing.border.Border;

import javax.swing.event.SwingPropertyChangeSupport;
import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeEvent;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;
import java.io.Serializable;
import java.io.File;
import java.io.FileInputStream;
import java.io.BufferedInputStream;

import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.Vector;
import java.util.Locale;

import sun.security.action.GetPropertyAction;

/**
 * This class keeps track of the current look and feel and its
 * defaults.
 * The default look and feel class is chosen in the following manner:
 * <ol>  
 *   <li>If the system property <code>swing.defaultlaf</code> is
 *       non-null, use it as the default look and feel class name.
 *   <li>If the {@link java.util.Properties} file <code>swing.properties</code>
 *       exists and contains the key <code>swing.defaultlaf</code>,
 *       use its value as default look and feel class name. The location of
 *       <code>swing.properties</code> may vary depending upon the
 *       implementation of the Java platform. In Sun's implementation
 *       this will reside in
 *       <code>&amp;java.home>/lib/swing.properties</code>. Refer to
 *       the release notes of the implementation you are using for
 *       further details.
 *   <li>Otherwise use the Java look and feel.
 * </ol> 
 * <p>
 * We manage three levels of defaults: user defaults, look
 * and feel defaults, system defaults.  A call to <code>UIManager.get</code>
 * checks all three levels in order and returns the first non-<code>null</code> 
 * value for a key, if any.  A call to <code>UIManager.put</code> just
 * affects the user defaults.  Note that a call to 
 * <code>setLookAndFeel</code> doesn't affect the user defaults, it just
 * replaces the middle defaults "level".
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with
 * future Swing releases. The current serialization support is
 * appropriate for short term storage or RMI between applications running
 * the same version of Swing.  As of 1.4, support for long term storage
 * of all JavaBeans<sup><font size="-2">TM</font></sup>
 * has been added to the <code>java.beans</code> package.
 * Please see {@link java.beans.XMLEncoder}.
 *
 * @see javax.swing.plaf.metal
 *
 * @version 1.115 12/19/03
 * @author Thomas Ball
 * @author Hans Muller
 */
public class UIManager implements Serializable 
{
    /**
     * This class defines the state managed by the <code>UIManager</code>.  For 
     * Swing applications the fields in this class could just as well
     * be static members of <code>UIManager</code> however we give them
     * "AppContext"
     * scope instead so that applets (and potentially multiple lightweight
     * applications running in a single VM) have their own state. For example,
     * an applet can alter its look and feel, see <code>setLookAndFeel</code>.
     * Doing so has no affect on other applets (or the browser).
     */
    private static class LAFState 
    {
        Properties swingProps;
        private UIDefaults[] tables = new UIDefaults[2];

        boolean initialized = false;
        MultiUIDefaults multiUIDefaults = new MultiUIDefaults(tables);
        LookAndFeel lookAndFeel;
        LookAndFeel multiLookAndFeel = null;
        Vector auxLookAndFeels = null;
        SwingPropertyChangeSupport changeSupport;

        UIDefaults getLookAndFeelDefaults() { return tables[0]; }
        void setLookAndFeelDefaults(UIDefaults x) { tables[0] = x; }

        UIDefaults getSystemDefaults() { return tables[1]; }
        void setSystemDefaults(UIDefaults x) { tables[1] = x; }

        /**
         * Returns the SwingPropertyChangeSupport for the current
         * AppContext.  If <code>create</code> is a true, a non-null
         * <code>SwingPropertyChangeSupport</code> will be returned, if
         * <code>create</code> is false and this has not been invoked
         * with true, null will be returned.
         */
        public synchronized SwingPropertyChangeSupport
                                 getPropertyChangeSupport(boolean create) {
            if (create && changeSupport == null) {
                changeSupport = new SwingPropertyChangeSupport(
                                         UIManager.class);
            }
            return changeSupport;
        }
    }


    /**
     * The <code>AppContext</code> key for our one <code>LAFState</code> instance.
     */
    private static final Object lafStateACKey = new StringBuffer("LookAndFeel State");


    /* Lock object used in place of class object for synchronization. (4187686)
     */
    private static final Object classLock = new Object();


    /* Cache the last referenced LAFState to improve performance 
     * when accessing it.  The cache is based on last thread rather
     * than last AppContext because of the cost of looking up the
     * AppContext each time.  Since most Swing UI work is on the 
     * EventDispatchThread, this hits often enough to justify the
     * overhead.  (4193032)
     */
    private static Thread currentLAFStateThread = null;
    private static LAFState currentLAFState = null;


    /**
     * Return the <code>LAFState</code> object, lazily create one if necessary.
     * All access to the <code>LAFState</code> fields is done via this method,
     * for example:
     * <pre>
     *     getLAFState().initialized = true;
     * </pre>
     */
    private static LAFState getLAFState() {
	// First check whether we're running on the same thread as
	// the last request.
	Thread thisThread = Thread.currentThread();
	if (thisThread == currentLAFStateThread) {
	    return currentLAFState;
	}

        LAFState rv = (LAFState)SwingUtilities.appContextGet(lafStateACKey);
        if (rv == null) {
	    synchronized (classLock) {
		rv = (LAFState)SwingUtilities.appContextGet(lafStateACKey);
		if (rv == null) {
		    SwingUtilities.appContextPut(lafStateACKey, 
						 (rv = new LAFState()));
		}
	    }
        }

	currentLAFStateThread = thisThread;
	currentLAFState = rv;

	return rv;
    }


    /* Keys used for the properties file in <java.home>/lib/swing.properties.
     * See loadUserProperties(), initialize().
     */

    private static final String defaultLAFKey = "swing.defaultlaf";
    private static final String auxiliaryLAFsKey = "swing.auxiliarylaf";
    private static final String multiplexingLAFKey = "swing.plaf.multiplexinglaf";
    private static final String installedLAFsKey = "swing.installedlafs";
    private static final String disableMnemonicKey = "swing.disablenavaids";

    /**
     * Return a swing.properties file key for the attribute of specified 
     * look and feel.  The attr is either "name" or "class", a typical
     * key would be: "swing.installedlaf.windows.name"
     */
    private static String makeInstalledLAFKey(String laf, String attr) {
        return "swing.installedlaf." + laf + "." + attr;
    }

    /**
     * The filename for swing.properties is a path like this (Unix version):
     * <java.home>/lib/swing.properties.  This method returns a bogus
     * filename if java.home isn't defined.  
     */
    private static String makeSwingPropertiesFilename() {
        String sep = File.separator;
        // No need to wrap this in a doPrivileged as it's called from
        // a doPrivileged.
        String javaHome = System.getProperty("java.home");
        if (javaHome == null) {
            javaHome = "<java.home undefined>";
        }
        return javaHome + sep + "lib" + sep + "swing.properties";
    }


    /** 
     * Provides a little information about an installed
     * <code>LookAndFeel</code> for the sake of configuring a menu or
     * for initial application set up.
     *
     * @see UIManager#getInstalledLookAndFeels
     * @see LookAndFeel
     */
    public static class LookAndFeelInfo {
        private String name;
        private String className;

        /**
         * Constructs a <code>UIManager</code>s 
         * <code>LookAndFeelInfo</code> object.
         *
         * @param name      a <code>String</code> specifying the name of
         *			the look and feel
         * @param className a <code>String</code> specifiying the name of
         *			the class that implements the look and feel
         */
        public LookAndFeelInfo(String name, String className) {
            this.name = name;
            this.className = className;
        }

        /**
         * Returns the name of the look and feel in a form suitable
         * for a menu or other presentation
         * @return a <code>String</code> containing the name
         * @see LookAndFeel#getName
         */
        public String getName() {
            return name;
        }

        /**
         * Returns the name of the class that implements this look and feel.
         * @return the name of the class that implements this 
         *		<code>LookAndFeel</code>
         * @see LookAndFeel
         */
        public String getClassName() {
            return className;
        }

        /**
         * Returns a string that displays and identifies this
         * object's properties.
         *
         * @return a <code>String</code> representation of this object
         */
        public String toString() {
            return getClass().getName() + "[" + getName() + " " + getClassName() + "]";
        }
    }


    /**
     * The default value of <code>installedLAFS</code> is used when no
     * swing.properties
     * file is available or if the file doesn't contain a "swing.installedlafs"
     * property.   
     * 
     * @see #initializeInstalledLAFs
     */
    private static LookAndFeelInfo[] installedLAFs;

    static {
        ArrayList iLAFs = new ArrayList(4);
        iLAFs.add(new LookAndFeelInfo(
                      "Metal", "javax.swing.plaf.metal.MetalLookAndFeel"));
        iLAFs.add(new LookAndFeelInfo("CDE/Motif",
                  "com.sun.java.swing.plaf.motif.MotifLookAndFeel"));

        // Only include windows on Windows boxs.
	String osName = (String)AccessController.doPrivileged(
                             new GetPropertyAction("os.name"));
        if (osName != null && osName.indexOf("Windows") != -1) {
            iLAFs.add(new LookAndFeelInfo("Windows",
                        "com.sun.java.swing.plaf.windows.WindowsLookAndFeel"));
            if (Toolkit.getDefaultToolkit().getDesktopProperty(
                    "win.xpstyle.themeActive") != null) {
                iLAFs.add(new LookAndFeelInfo("Windows Classic",
                 "com.sun.java.swing.plaf.windows.WindowsClassicLookAndFeel"));
            }
        }
        else {
            // GTK is not shipped on Windows.
            iLAFs.add(new LookAndFeelInfo("GTK+",
                  "com.sun.java.swing.plaf.gtk.GTKLookAndFeel"));
        }
        installedLAFs = (LookAndFeelInfo[])iLAFs.toArray(
                        new LookAndFeelInfo[iLAFs.size()]);
    }


    /** 
     * Returns an array of objects that provide some information about the
     * <code>LookAndFeel</code> implementations that have been installed with this 
     * software development kit.  The <code>LookAndFeel</code> info objects can
     * used by an application to construct a menu of look and feel options for 
     * the user or to set the look and feel at start up time.  Note that 
     * we do not return the <code>LookAndFeel</code> classes themselves here to
     * avoid the cost of unnecessarily loading them.
     * <p>
     * Given a <code>LookAndFeelInfo</code> object one can set the current
     * look and feel like this:
     * <pre>
     * UIManager.setLookAndFeel(info.getClassName());
     * </pre>
     * @return an array of <code>LookAndFeelInfo</code> objects
     * 
     * @see #setLookAndFeel
     */
    public static LookAndFeelInfo[] getInstalledLookAndFeels() {
        maybeInitialize();
        LookAndFeelInfo[] ilafs = installedLAFs;
        LookAndFeelInfo[] rv = new LookAndFeelInfo[ilafs.length];
        System.arraycopy(ilafs, 0, rv, 0, ilafs.length);
        return rv;
    }


    /**
     * Replaces the current array of installed <code>LookAndFeelInfos</code>.
     * @param infos  new array of <code>LookAndFeelInfo</code> objects
     * 
     * @see #getInstalledLookAndFeels
     */
    public static void setInstalledLookAndFeels(LookAndFeelInfo[] infos)
        throws SecurityException
    {
        LookAndFeelInfo[] newInfos = new LookAndFeelInfo[infos.length];
        System.arraycopy(infos, 0, newInfos, 0, infos.length);
        installedLAFs = newInfos;
    }


    /**
     * Adds the specified look and feel to the current array and
     * then calls {@link #setInstalledLookAndFeels}.
     * @param info a <code>LookAndFeelInfo</code> object that names the
     *		look and feel and identifies that class that implements it
     */
    public static void installLookAndFeel(LookAndFeelInfo info) {
        LookAndFeelInfo[] infos = getInstalledLookAndFeels();
        LookAndFeelInfo[] newInfos = new LookAndFeelInfo[infos.length + 1];
        System.arraycopy(infos, 0, newInfos, 0, infos.length);
        newInfos[infos.length] = info;
        setInstalledLookAndFeels(newInfos);
    }


    /**
     * Creates a new look and feel and adds it to the current array.
     * Then calls {@link #setInstalledLookAndFeels}.
     *
     * @param name      a <code>String</code> specifying the name of the
     *			look and feel
     * @param className a <code>String</code> specifying the class name
     *			that implements the look and feel
     */
    public static void installLookAndFeel(String name, String className) {
        installLookAndFeel(new LookAndFeelInfo(name, className));
    }


    /**
     * Returns the current default look and feel or <code>null</code>.
     *
     * @return the current default look and feel, or <code>null</code>
     * @see #setLookAndFeel
     */
    public static LookAndFeel getLookAndFeel() {
        maybeInitialize();
        return getLAFState().lookAndFeel;
    }
    

    /**
     * Sets the current default look and feel using a
     * <code>LookAndFeel</code> object.  
     * <p>
     * This is a JavaBeans bound property.
     *
     * @param newLookAndFeel the <code>LookAndFeel</code> object
     * @exception UnsupportedLookAndFeelException if
     *		<code>lnf.isSupportedLookAndFeel()</code> is false
     * @see #getLookAndFeel
     */
    public static void setLookAndFeel(LookAndFeel newLookAndFeel) 
        throws UnsupportedLookAndFeelException 
    {
        if ((newLookAndFeel != null) && !newLookAndFeel.isSupportedLookAndFeel()) {
            String s = newLookAndFeel.toString() + " not supported on this platform";
            throw new UnsupportedLookAndFeelException(s);
        }

        LAFState lafState = getLAFState();
        LookAndFeel oldLookAndFeel = lafState.lookAndFeel;
        if (oldLookAndFeel != null) {
            oldLookAndFeel.uninitialize();
        }

        lafState.lookAndFeel = newLookAndFeel;
        if (newLookAndFeel != null) {
            sun.swing.DefaultLookup.setDefaultLookup(null);
            newLookAndFeel.initialize();
            lafState.setLookAndFeelDefaults(newLookAndFeel.getDefaults());
        }
        else {
            lafState.setLookAndFeelDefaults(null);
        }

        SwingPropertyChangeSupport changeSupport = lafState.
                                         getPropertyChangeSupport(false);
        if (changeSupport != null) {
            changeSupport.firePropertyChange("lookAndFeel", oldLookAndFeel,
                                             newLookAndFeel);
        }
    }

    
    /**
     * Sets the current default look and feel using a class name.
     *
     * @param className  a string specifying the name of the class that implements
     *        the look and feel
     * @exception ClassNotFoundException if the <code>LookAndFeel</code>
     *		 class could not be found
     * @exception InstantiationException if a new instance of the class
     *		couldn't be created
     * @exception IllegalAccessException if the class or initializer isn't accessible
     * @exception UnsupportedLookAndFeelException if
     *		<code>lnf.isSupportedLookAndFeel()</code> is false
     */
    public static void setLookAndFeel(String className) 
        throws ClassNotFoundException, 
               InstantiationException, 
               IllegalAccessException,
               UnsupportedLookAndFeelException 
    {
        if ("javax.swing.plaf.metal.MetalLookAndFeel".equals(className)) {
            // Avoid reflection for the common case of metal.
            setLookAndFeel(new javax.swing.plaf.metal.MetalLookAndFeel());
        }
        else {
            Class lnfClass = SwingUtilities.loadSystemClass(className);
            setLookAndFeel((LookAndFeel)(lnfClass.newInstance()));
        }
    }


    /**
     * Returns the name of the <code>LookAndFeel</code> class that implements
     * the native systems look and feel if there is one, otherwise
     * the name of the default cross platform <code>LookAndFeel</code>
     * class. If the system property <code>swing.systemlaf</code> has been
     * defined, its value will be returned.
     * 
     * @return the <code>String</code> of the <code>LookAndFeel</code>
     *		class
     * 
     * @see #setLookAndFeel
     * @see #getCrossPlatformLookAndFeelClassName
     */
    public static String getSystemLookAndFeelClassName() {
	String systemLAF = (String)AccessController.doPrivileged(
                             new GetPropertyAction("swing.systemlaf"));
        if (systemLAF != null) {
            return systemLAF;
        }
	String osName = (String)AccessController.doPrivileged(
                             new GetPropertyAction("os.name"));

        if (osName != null) {
            if (osName.indexOf("Windows") != -1) {
                return "com.sun.java.swing.plaf.windows.WindowsLookAndFeel";
            }
            else {
                String desktop = (String)AccessController.doPrivileged(
                             new GetPropertyAction("sun.desktop"));
                if ("gnome".equals(desktop)) {
                    // May be set on Linux and Solaris boxs.
                    return "com.sun.java.swing.plaf.gtk.GTKLookAndFeel";
                }
                if ((osName.indexOf("Solaris") != -1) || 
		             (osName.indexOf("SunOS") != -1)) {
                    return "com.sun.java.swing.plaf.motif.MotifLookAndFeel";
                }
            }
        }
        return getCrossPlatformLookAndFeelClassName();
    }


    /**
     * Returns the name of the <code>LookAndFeel</code> class that implements
     * the default cross platform look and feel -- the Java
     * Look and Feel (JLF).  If the system property
     * <code>swing.crossplatformlaf</code> has been
     * defined, its value will be returned.
     * 
     * @return  a string with the JLF implementation-class
     * @see #setLookAndFeel
     * @see #getSystemLookAndFeelClassName
     */
    public static String getCrossPlatformLookAndFeelClassName() {
	String laf = (String)AccessController.doPrivileged(
                             new GetPropertyAction("swing.crossplatformlaf"));
        if (laf != null) {
            return laf;
        }
        return "javax.swing.plaf.metal.MetalLookAndFeel";
    }


    /**
     * Returns the default values for this look and feel.
     *
     * @return a <code>UIDefaults</code> object containing the default values
     */
    public static UIDefaults getDefaults() {
        maybeInitialize();
        return getLAFState().multiUIDefaults;
    }
    
    /**
     * Returns a drawing font from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the font
     * @return the <code>Font</code> object
     */
    public static Font getFont(Object key) { 
        return getDefaults().getFont(key); 
    }

    /**
     * Returns a drawing font from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the font
     * @param l the <code>Locale</code> for which the font is desired
     * @return the <code>Font</code> object
     * @since 1.4
     */
    public static Font getFont(Object key, Locale l) { 
        return getDefaults().getFont(key,l); 
    }

    /**
     * Returns a drawing color from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the color
     * @return the <code>Color</code> object
     */
    public static Color getColor(Object key) { 
        return getDefaults().getColor(key); 
    }

    /**
     * Returns a drawing color from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the color
     * @param l the <code>Locale</code> for which the color is desired
     * @return the <code>Color</code> object
     * @since 1.4
     */
    public static Color getColor(Object key, Locale l) { 
        return getDefaults().getColor(key,l); 
    }

    /**
     * Returns an <code>Icon</code> from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the icon
     * @return the <code>Icon</code> object
     */
    public static Icon getIcon(Object key) { 
        return getDefaults().getIcon(key); 
    }

    /**
     * Returns an <code>Icon</code> from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the icon
     * @param l the <code>Locale</code> for which the icon is desired
     * @return the <code>Icon</code> object
     * @since 1.4
     */
    public static Icon getIcon(Object key, Locale l) { 
        return getDefaults().getIcon(key,l); 
    }

    /**
     * Returns a border from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the border
     * @return the <code>Border</code> object
     */
    public static Border getBorder(Object key) { 
        return getDefaults().getBorder(key); 
    }

    /**
     * Returns a border from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the border
     * @param l the <code>Locale</code> for which the border is desired
     * @return the <code>Border</code> object
     * @since 1.4
     */
    public static Border getBorder(Object key, Locale l) { 
        return getDefaults().getBorder(key,l); 
    }

    /**
     * Returns a string from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the string
     * @return the <code>String</code>
     */
    public static String getString(Object key) { 
        return getDefaults().getString(key); 
    }

    /**
     * Returns a string from the defaults table that is appropriate for the
     * given locale.
     *
     * @param key  an <code>Object</code> specifying the string
     * @param l the <code>Locale</code> for which the string is desired
     * @return the <code>String</code>
     */
    public static String getString(Object key, Locale l) { 
        return getDefaults().getString(key,l); 
    }

    /**
     * Returns a string from the defaults table that is appropriate for the
     * given locale.
     *
     * @param key  an <code>Object</code> specifying the string
     * @param c Component used to determine Locale, null implies use the
     *          default Locale.
     * @return the <code>String</code>
     */
    static String getString(Object key, Component c) { 
        Locale l = (c == null) ? Locale.getDefault() : c.getLocale();
        return getString(key, l);
    }

    /**
     * Returns an integer from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the int
     * @return the int
     */
    public static int getInt(Object key) {
        return getDefaults().getInt(key);
    }

    /**
     * Returns an integer from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the int
     * @param l the <code>Locale</code> for which the int is desired
     * @return the int
     * @since 1.4
     */
    public static int getInt(Object key, Locale l) {
        return getDefaults().getInt(key,l);
    }

    /**
     * Returns an integer from the defaults table. If <code>key</code> does
     * not map to a valid <code>Integer</code>, or can not be convered from
     * a <code>String</code> to an integer, <code>default</code> is
     * returned.
     *
     * @param key  an <code>Object</code> specifying the int
     * @param defaultValue Returned value if <code>key</code> is not available,
     *                     or is not an Integer
     * @return the int
     */
    static int getInt(Object key, int defaultValue) {
        Object value = UIManager.get(key);

        if (value instanceof Integer) {
            return ((Integer)value).intValue();
        }
        if (value instanceof String) {
            try {
                return Integer.parseInt((String)value);
            } catch (NumberFormatException nfe) {}
        }
        return defaultValue;
    }

    /**
     * Returns a boolean from the defaults table which is associated with
     * the key value. If the key is not found or the key doesn't represent
     * a boolean value then false will be returned.
     *
     * @param key  an <code>Object</code> specifying the key for the desired boolean value
     * @return the boolean value corresponding to the key
     * @since 1.4
     */
    public static boolean getBoolean(Object key) {
        return getDefaults().getBoolean(key);
    }

    /**
     * Returns a boolean from the defaults table which is associated with
     * the key value and the given <code>Locale</code>. If the key is not
     * found or the key doesn't represent
     * a boolean value then false will be returned.
     *
     * @param key  an <code>Object</code> specifying the key for the desired 
     *             boolean value
     * @param l the <code>Locale</code> for which the boolean is desired
     * @return the boolean value corresponding to the key
     * @since 1.4
     */
    public static boolean getBoolean(Object key, Locale l) {
        return getDefaults().getBoolean(key,l);
    }

    /**
     * Returns an <code>Insets</code> object from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the <code>Insets</code> object
     * @return the <code>Insets</code> object
     */
    public static Insets getInsets(Object key) {
        return getDefaults().getInsets(key);
    }

    /**
     * Returns an <code>Insets</code> object from the defaults table that is 
     * appropriate for the given locale.
     *
     * @param key  an <code>Object</code> specifying the <code>Insets</code> object
     * @param l the <code>Locale</code> for which the object is desired
     * @return the <code>Insets</code> object
     * @since 1.4
     */
    public static Insets getInsets(Object key, Locale l) {
        return getDefaults().getInsets(key,l);
    }

    /**
     * Returns a dimension from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the dimension object
     * @return the <code>Dimension</code> object
     */
    public static Dimension getDimension(Object key) {
        return getDefaults().getDimension(key);
    }

    /**
     * Returns a dimension from the defaults table that is appropriate
     * for the given locale.
     *
     * @param key  an <code>Object</code> specifying the dimension object
     * @param l the <code>Locale</code> for which the object is desired
     * @return the <code>Dimension</code> object
     * @since 1.4
     */
    public static Dimension getDimension(Object key, Locale l) {
        return getDefaults().getDimension(key,l);
    }

    /**
     * Returns an object from the defaults table.
     *
     * @param key  an <code>Object</code> specifying the desired object
     * @return the <code>Object</code>
     */
    public static Object get(Object key) { 
        return getDefaults().get(key); 
    }

    /**
     * Returns an object from the defaults table that is appropriate for
     * the given locale.
     *
     * @param key  an <code>Object</code> specifying the desired object
     * @param l the <code>Locale</code> for which the object is desired
     * @return the <code>Object</code>
     */
    public static Object get(Object key, Locale l) { 
        return getDefaults().get(key,l); 
    }

    /**
     * Stores an object in the defaults table.
     *
     * @param key    an <code>Object</code> specifying the retrieval key
     * @param value  the <code>Object</code> to store
     * @return the <code>Object</code> returned by {@link UIDefaults#put}
     */
    public static Object put(Object key, Object value) { 
        return getDefaults().put(key, value); 
    }

    /**
     * Returns the L&F object that renders the target component.
     *
     * @param target  the <code>JComponent</code> to render
     * @return the <code>ComponentUI</code> object that renders the target component
     */
    public static ComponentUI getUI(JComponent target) {
        maybeInitialize();
        ComponentUI ui = null;
        LookAndFeel multiLAF = getLAFState().multiLookAndFeel;
        if (multiLAF != null) {
            // This can return null if the multiplexing look and feel
            // doesn't support a particular UI.
            ui = multiLAF.getDefaults().getUI(target);
        }
        if (ui == null) {
            ui = getDefaults().getUI(target);
        }
        return ui;
    }


    /**
     * Returns the default values for this look and feel.
     *
     * @return an <code>UIDefaults</code> object containing the default values
     */
    public static UIDefaults getLookAndFeelDefaults() {
        maybeInitialize();
        return getLAFState().getLookAndFeelDefaults();
    }

    /**
     * Finds the Multiplexing <code>LookAndFeel</code>.
     */
    private static LookAndFeel getMultiLookAndFeel() {
	LookAndFeel multiLookAndFeel = getLAFState().multiLookAndFeel;
	if (multiLookAndFeel == null) {
            String defaultName = "javax.swing.plaf.multi.MultiLookAndFeel";
            String className = getLAFState().swingProps.getProperty(multiplexingLAFKey, defaultName);
            try {
                Class lnfClass = SwingUtilities.loadSystemClass(className);
                multiLookAndFeel = (LookAndFeel)lnfClass.newInstance();
            } catch (Exception exc) {
                System.err.println("UIManager: failed loading " + className);
            }
	}
	return multiLookAndFeel;
    }

    /**
     * Adds a <code>LookAndFeel</code> to the list of auxiliary look and feels.
     * The auxiliary look and feels tell the multiplexing look and feel what
     * other <code>LookAndFeel</code> classes for a component instance are to be used 
     * in addition to the default <code>LookAndFeel</code> class when creating a 
     * multiplexing UI.  The change will only take effect when a new
     * UI class is created or when the default look and feel is changed
     * on a component instance.
     * <p>Note these are not the same as the installed look and feels.
     *
     * @param laf the <code>LookAndFeel</code> object
     * @see #removeAuxiliaryLookAndFeel
     * @see #setLookAndFeel
     * @see #getAuxiliaryLookAndFeels
     * @see #getInstalledLookAndFeels
     */
    static public void addAuxiliaryLookAndFeel(LookAndFeel laf) {
        maybeInitialize();

        if (!laf.isSupportedLookAndFeel()) {
            // Ideally we would throw an exception here, but it's too late
            // for that.
            return;
        }
        Vector v = getLAFState().auxLookAndFeels;
        if (v == null) {
            v = new Vector();
        } 

	if (!v.contains(laf)) {
	    v.addElement(laf);
	    laf.initialize();
            getLAFState().auxLookAndFeels = v;

	    if (getLAFState().multiLookAndFeel == null) {
	        getLAFState().multiLookAndFeel = getMultiLookAndFeel();
            }
	}
    }

    /**
     * Removes a <code>LookAndFeel</code> from the list of auxiliary look and feels.
     * The auxiliary look and feels tell the multiplexing look and feel what
     * other <code>LookAndFeel</code> classes for a component instance are to be used 
     * in addition to the default <code>LookAndFeel</code> class when creating a 
     * multiplexing UI.  The change will only take effect when a new
     * UI class is created or when the default look and feel is changed
     * on a component instance.
     * <p>Note these are not the same as the installed look and feels.
     * @return true if the <code>LookAndFeel</code> was removed from the list
     * @see #removeAuxiliaryLookAndFeel
     * @see #getAuxiliaryLookAndFeels
     * @see #setLookAndFeel
     * @see #getInstalledLookAndFeels
     */
    static public boolean removeAuxiliaryLookAndFeel(LookAndFeel laf) {
        maybeInitialize();

	boolean result;

        Vector v = getLAFState().auxLookAndFeels;
        if ((v == null) || (v.size() == 0)) {
            return false;
        } 
	
	result = v.removeElement(laf);
	if (result) {
	    if (v.size() == 0) {
	        getLAFState().auxLookAndFeels = null;
	        getLAFState().multiLookAndFeel = null;
	    } else {
	        getLAFState().auxLookAndFeels = v;
            }
        }
	laf.uninitialize();

	return result;
    }

    /**
     * Returns the list of auxiliary look and feels (can be <code>null</code>).
     * The auxiliary look and feels tell the multiplexing look and feel what
     * other <code>LookAndFeel</code> classes for a component instance are 
     * to be used in addition to the default LookAndFeel class when creating a 
     * multiplexing UI.  
     * <p>Note these are not the same as the installed look and feels.
     *
     * @return list of auxiliary <code>LookAndFeel</code>s or <code>null</code>
     * @see #addAuxiliaryLookAndFeel
     * @see #removeAuxiliaryLookAndFeel
     * @see #setLookAndFeel
     * @see #getInstalledLookAndFeels
     */
    static public LookAndFeel[] getAuxiliaryLookAndFeels() {
        maybeInitialize();

        Vector v = getLAFState().auxLookAndFeels;
        if ((v == null) || (v.size() == 0)) {
            return null;
        } 
        else {
            LookAndFeel[] rv = new LookAndFeel[v.size()];
            for (int i = 0; i < rv.length; i++) {
                rv[i] = (LookAndFeel)v.elementAt(i);
            }
            return rv;
        }
    }


    /**
     * Adds a <code>PropertyChangeListener</code> to the listener list.
     * The listener is registered for all properties.
     *
     * @param listener  the <code>PropertyChangeListener</code> to be added
     * @see java.beans.PropertyChangeSupport
     */
    public static void addPropertyChangeListener(PropertyChangeListener listener) 
    {
	synchronized (classLock) {
	    getLAFState().getPropertyChangeSupport(true).
                             addPropertyChangeListener(listener);
	}
    }


    /**
     * Removes a <code>PropertyChangeListener</code> from the listener list.
     * This removes a <code>PropertyChangeListener</code> that was registered
     * for all properties.
     *
     * @param listener  the <code>PropertyChangeListener</code> to be removed
     * @see java.beans.PropertyChangeSupport
     */
    public static void removePropertyChangeListener(PropertyChangeListener listener) 
    {
        synchronized (classLock) {
	    getLAFState().getPropertyChangeSupport(true).
                          removePropertyChangeListener(listener);
	}
    }


    /**
     * Returns an array of all the <code>PropertyChangeListener</code>s added
     * to this UIManager with addPropertyChangeListener().
     *
     * @return all of the <code>PropertyChangeListener</code>s added or an empty
     *         array if no listeners have been added
     * @since 1.4
     */
    public static PropertyChangeListener[] getPropertyChangeListeners() {
        synchronized(classLock) {
            return getLAFState().getPropertyChangeSupport(true).
                      getPropertyChangeListeners();
        }
    }

    private static Properties loadSwingProperties()
    {
	/* Don't bother checking for Swing properties if untrusted, as
	 * there's no way to look them up without triggering SecurityExceptions.
	 */
        if (UIManager.class.getClassLoader() != null) {
	    return new Properties();
	}
	else {
	    final Properties props = new Properties();

            java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction() {
                public Object run() {
		    try {
			File file = new File(makeSwingPropertiesFilename());

                        if (file.exists()) {
                            // InputStream has been buffered in Properties
                            // class
                            FileInputStream ins = new FileInputStream(file);
                            props.load(ins);
                            ins.close();
                        }
		    } 
		    catch (Exception e) {
			// No such file, or file is otherwise non-readable.
		    }

		    // Check whether any properties were overridden at the
		    // command line.
		    checkProperty(props, defaultLAFKey);
		    checkProperty(props, auxiliaryLAFsKey);
		    checkProperty(props, multiplexingLAFKey);
		    checkProperty(props, installedLAFsKey);
		    checkProperty(props, disableMnemonicKey);
                    // Don't care about return value.
                    return null;
		}
	    });
	    return props;
	}
    }

    private static void checkProperty(Properties props, String key) {
        // No need to do catch the SecurityException here, this runs
        // in a doPrivileged.
        String value = System.getProperty(key);
        if (value != null) {
            props.put(key, value);
        }
    }


    /**
     * If a swing.properties file exist and it has a swing.installedlafs property
     * then initialize the <code>installedLAFs</code> field.
     * 
     * @see #getInstalledLookAndFeels
     */
    private static void initializeInstalledLAFs(Properties swingProps) 
    {
        String ilafsString = swingProps.getProperty(installedLAFsKey);
        if (ilafsString == null) {
            return;
        }

        /* Create a vector that contains the value of the swing.installedlafs
         * property.  For example given "swing.installedlafs=motif,windows"
         * lafs = {"motif", "windows"}.
         */
        Vector lafs = new Vector();
        StringTokenizer st = new StringTokenizer(ilafsString, ",", false);
        while (st.hasMoreTokens()) {
            lafs.addElement(st.nextToken());
        }

        /* Look up the name and class for each name in the "swing.installedlafs"
         * list.  If they both exist then add a LookAndFeelInfo to 
         * the installedLafs array.
         */
        Vector ilafs = new Vector(lafs.size());
        for(int i = 0; i < lafs.size(); i++) {
            String laf = (String)lafs.elementAt(i);
            String name = swingProps.getProperty(makeInstalledLAFKey(laf, "name"), laf);
            String cls = swingProps.getProperty(makeInstalledLAFKey(laf, "class"));
            if (cls != null) {
                ilafs.addElement(new LookAndFeelInfo(name, cls));
            }
        }

        installedLAFs = new LookAndFeelInfo[ilafs.size()];
        for(int i = 0; i < ilafs.size(); i++) {
            installedLAFs[i] = (LookAndFeelInfo)(ilafs.elementAt(i));
        }
    }


    /**
     * If the user has specified a default look and feel, use that.  
     * Otherwise use the look and feel that's native to this platform.
     * If this code is called after the application has explicitly
     * set it's look and feel, do nothing.
     *
     * @see #maybeInitialize
     */
    private static void initializeDefaultLAF(Properties swingProps)
    {
        if (getLAFState().lookAndFeel != null) {
            return;
        }

        String metalLnf = getCrossPlatformLookAndFeelClassName();
        String lnfDefault = metalLnf;

        String lnfName = "<undefined>" ;
        try {
            lnfName = swingProps.getProperty(defaultLAFKey, lnfDefault);
            setLookAndFeel(lnfName);
        } catch (Exception e) {
            try {
                lnfName = swingProps.getProperty(defaultLAFKey, metalLnf);
                setLookAndFeel(lnfName);
            } catch (Exception e2) {
                throw new Error("can't load " + lnfName);
            }
        }
    }


    private static void initializeAuxiliaryLAFs(Properties swingProps)
    {
        String auxLookAndFeelNames = swingProps.getProperty(auxiliaryLAFsKey);
        if (auxLookAndFeelNames == null) {
            return;
        }

        Vector auxLookAndFeels = new Vector();

        StringTokenizer p = new StringTokenizer(auxLookAndFeelNames,",");
        String factoryName;

        /* Try to load each LookAndFeel subclass in the list.
         */

        while (p.hasMoreTokens()) {
            String className = p.nextToken();
            try {
                Class lnfClass = SwingUtilities.loadSystemClass(className);
		LookAndFeel newLAF = (LookAndFeel)lnfClass.newInstance();
		newLAF.initialize();
                auxLookAndFeels.addElement(newLAF);
            } 
            catch (Exception e) {
                System.err.println("UIManager: failed loading auxiliary look and feel " + className);
            }
        }

        /* If there were problems and no auxiliary look and feels were 
         * loaded, make sure we reset auxLookAndFeels to null.
         * Otherwise, we are going to use the MultiLookAndFeel to get
         * all component UI's, so we need to load it now.
         */
        if (auxLookAndFeels.size() == 0) {
            auxLookAndFeels = null;
        } 
        else {
	    getLAFState().multiLookAndFeel = getMultiLookAndFeel();
	    if (getLAFState().multiLookAndFeel == null) {
                auxLookAndFeels = null;
	    }
        }

        getLAFState().auxLookAndFeels = auxLookAndFeels;
    }


    private static void initializeSystemDefaults(Properties swingProps) {
	getLAFState().swingProps = swingProps;
    }


    /* 
     * This method is called before any code that depends on the 
     * <code>AppContext</code> specific LAFState object runs.  When the AppContext
     * corresponds to a set of applets it's possible for this method 
     * to be re-entered, which is why we grab a lock before calling
     * initialize().
     */
    private static void maybeInitialize() {
	synchronized (classLock) {
	    if (!getLAFState().initialized) {
		getLAFState().initialized = true;
		initialize();
	    }
        }
    }


    /*
     * Only called by maybeInitialize().
     */
    private static void initialize() {
        Properties swingProps = loadSwingProperties();
        initializeSystemDefaults(swingProps);
        initializeDefaultLAF(swingProps);
        initializeAuxiliaryLAFs(swingProps);
        initializeInstalledLAFs(swingProps);

        // Enable the Swing default LayoutManager.
        String toolkitName = Toolkit.getDefaultToolkit().getClass().getName();
        // don't set default policy if this is XAWT.
        if (!"sun.awt.X11.XToolkit".equals(toolkitName)) {
            if (FocusManager.isFocusManagerEnabled()) {
                KeyboardFocusManager.getCurrentKeyboardFocusManager().
                    setDefaultFocusTraversalPolicy(
                        new LayoutFocusTraversalPolicy());
            }
        }
        // Install a hook that will be invoked if no one consumes the
        // KeyEvent.  If the source isn't a JComponent this will process
        // key bindings, if the source is a JComponent it implies that
        // processKeyEvent was already invoked and thus no need to process
        // the bindings again, unless the Component is disabled, in which
        // case KeyEvents will no longer be dispatched to it so that we
        // handle it here.
        KeyboardFocusManager.getCurrentKeyboardFocusManager().
                addKeyEventPostProcessor(new KeyEventPostProcessor() {
                    public boolean postProcessKeyEvent(KeyEvent e) {
                        Component c = e.getComponent();

                        if ((!(c instanceof JComponent) ||
                             (c != null && !((JComponent)c).isEnabled())) &&
                                JComponent.KeyboardState.shouldProcess(e) &&
                                SwingUtilities.processKeyBindings(e)) {
                            e.consume();
                            return true;
                        }
                        return false;
                    }
                });
    }
}

