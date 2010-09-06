/*
 * @(#)JarInputStream.java	1.33 04/04/21
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.util.jar;

import java.util.zip.*;
import java.io.*;
import sun.security.util.ManifestEntryVerifier;

/**
 * The <code>JarInputStream</code> class is used to read the contents of
 * a JAR file from any input stream. It extends the class
 * <code>java.util.zip.ZipInputStream</code> with support for reading
 * an optional <code>Manifest</code> entry. The <code>Manifest</code>
 * can be used to store meta-information about the JAR file and its entries.
 *
 * @author  David Connelly
 * @version 1.33, 04/21/04
 * @see	    Manifest
 * @see	    java.util.zip.ZipInputStream
 * @since   1.2
 */
public
class JarInputStream extends ZipInputStream {
    private Manifest man;
    private JarEntry first;
    private JarVerifier jv;
    private ManifestEntryVerifier mev;


    /**
     * Creates a new <code>JarInputStream</code> and reads the optional
     * manifest. If a manifest is present, also attempts to verify
     * the signatures if the JarInputStream is signed.
     * @param in the actual input stream
     * @exception IOException if an I/O error has occurred
     */
    public JarInputStream(InputStream in) throws IOException {
	this(in, true);
    }

    /**
     * Creates a new <code>JarInputStream</code> and reads the optional
     * manifest. If a manifest is present and verify is true, also attempts 
     * to verify the signatures if the JarInputStream is signed.
     *
     * @param in the actual input stream
     * @param verify whether or not to verify the JarInputStream if
     * it is signed.
     * @exception IOException if an I/O error has occurred
     */
    public JarInputStream(InputStream in, boolean verify) throws IOException {
	super(in);
	JarEntry e = (JarEntry)super.getNextEntry();

        if (e != null && e.getName().equalsIgnoreCase("META-INF/"))
            e = (JarEntry)super.getNextEntry();

        if (e != null && JarFile.MANIFEST_NAME.equalsIgnoreCase(e.getName())) {
            man = new Manifest();
            byte bytes[] = getBytes(new BufferedInputStream(this));
            man.read(new ByteArrayInputStream(bytes));
            //man.read(new BufferedInputStream(this));
            closeEntry();
            if (verify) {
                jv = new JarVerifier(man, bytes);
                mev = new ManifestEntryVerifier(man);
            }
            first = getNextJarEntry();
        } else {
            first = e;
        }
    }

    private byte[] getBytes(InputStream is)
	throws IOException
    {
	byte[] buffer = new byte[8192];
	ByteArrayOutputStream baos = new ByteArrayOutputStream(2048);

	int n;

	baos.reset();
	while ((n = is.read(buffer, 0, buffer.length)) != -1) {
	    baos.write(buffer, 0, n);
	}
	return baos.toByteArray();
    }

    /**
     * Returns the <code>Manifest</code> for this JAR file, or
     * <code>null</code> if none.
     *
     * @return the <code>Manifest</code> for this JAR file, or
     *         <code>null</code> if none.
     */
    public Manifest getManifest() {
	return man;
    }

    /**
     * Reads the next ZIP file entry and positions the stream at the
     * beginning of the entry data. If verification has been enabled,
     * any invalid signature detected while positioning the stream for
     * the next entry will result in an exception.
     * @exception ZipException if a ZIP file error has occurred
     * @exception IOException if an I/O error has occurred
     * @exception SecurityException if any of the jar file entries
     *         are incorrectly signed.
     */
    public ZipEntry getNextEntry() throws IOException {
	JarEntry e;
	if (first == null) {
	    e = (JarEntry)super.getNextEntry();
	} else {
	    e = first;
	    first = null;
	}
	if (jv != null && e != null) {
	    // At this point, we might have parsed all the meta-inf
	    // entries and have nothing to verify. If we have
	    // nothing to verify, get rid of the JarVerifier object.
	    if (jv.nothingToVerify() == true) {
		jv = null;
		mev = null;
	    } else {
		jv.beginEntry(e, mev);
	    }
	}
	return e;
    }

    /**
     * Reads the next JAR file entry and positions the stream at the
     * beginning of the entry data. If verification has been enabled,
     * any invalid signature detected while positioning the stream for
     * the next entry will result in an exception.
     * @return the next JAR file entry, or null if there are no more entries
     * @exception ZipException if a ZIP file error has occurred
     * @exception IOException if an I/O error has occurred
     * @exception SecurityException if any of the jar file entries
     *         are incorrectly signed.
     */
    public JarEntry getNextJarEntry() throws IOException {
	return (JarEntry)getNextEntry();
    }

    /**
     * Reads from the current JAR file entry into an array of bytes.
     * Blocks until some input is available.
     * If verification has been enabled, any invalid signature
     * on the current entry will be reported at some point before the
     * end of the entry is reached.
     * @param b the buffer into which the data is read
     * @param off the start offset of the data
     * @param len the maximum number of bytes to read
     * @return the actual number of bytes read, or -1 if the end of the
     *         entry is reached
     * @exception ZipException if a ZIP file error has occurred
     * @exception IOException if an I/O error has occurred
     * @exception SecurityException if any of the jar file entries
     *         are incorrectly signed.
     */
    public int read(byte[] b, int off, int len) throws IOException {
	int n;
	if (first == null) {
	    n = super.read(b, off, len);
	} else {
	    n = -1;
	}
	if (jv != null) {
	    jv.update(n, b, off, len, mev);
	}
	return n;
    }

    /**
     * Creates a new <code>JarEntry</code> (<code>ZipEntry</code>) for the
     * specified JAR file entry name. The manifest attributes of
     * the specified JAR file entry name will be copied to the new 
     * <CODE>JarEntry</CODE>.
     *
     * @param name the name of the JAR/ZIP file entry
     * @return the <code>JarEntry</code> object just created
     */
    protected ZipEntry createZipEntry(String name) {
	JarEntry e = new JarEntry(name);
	if (man != null) {
	    e.attr = man.getAttributes(name);
	}
	return e;
    }
}
