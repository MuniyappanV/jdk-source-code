/*
 * @(#)FileCacheImageOutputStream.java	1.23 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.imageio.stream;

import java.io.DataInput;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;

/**
 * An implementation of <code>ImageOutputStream</code> that writes its
 * output to a regular <code>OutputStream</code>.  A file is used to
 * cache data until it is flushed to the output stream.
 *
 * @version 0.5
 */
public class FileCacheImageOutputStream extends ImageOutputStreamImpl {

    private OutputStream stream;

    private File cacheFile;

    private RandomAccessFile cache;
    
    // Pos after last (rightmost) byte written
    private long maxStreamPos = 0L;

    /**
     * Constructs a <code>FileCacheImageOutputStream</code> that will write
     * to a given <code>outputStream</code>.
     *
     * <p> A temporary file is used as a cache.  If
     * <code>cacheDir</code>is non-<code>null</code> and is a
     * directory, the file will be created there.  If it is
     * <code>null</code>, the system-dependent default temporary-file
     * directory will be used (see the documentation for
     * <code>File.createTempFile</code> for details).
     *
     * @param stream an <code>OutputStream</code> to write to.
     * @param cacheDir a <code>File</code> indicating where the 
     * cache file should be created, or <code>null</code> to use the
     * system directory.
     *
     * @exception IllegalArgumentException if <code>stream</code>
     * is <code>null</code>.
     * @exception IllegalArgumentException if <code>cacheDir</code> is
     * non-<code>null</code> but is not a directory.
     * @exception IOException if a cache file cannot be created.
     */
    public FileCacheImageOutputStream(OutputStream stream, File cacheDir)
        throws IOException {
        if (stream == null) {
            throw new IllegalArgumentException("stream == null!");
        }
        if ((cacheDir != null) && !(cacheDir.isDirectory())) {
            throw new IllegalArgumentException("Not a directory!");
        }
        this.stream = stream;
        this.cacheFile =
            File.createTempFile("imageio", ".tmp", cacheDir);
        cacheFile.deleteOnExit();
        this.cache = new RandomAccessFile(cacheFile, "rw");
    }

    public int read() throws IOException {
        bitOffset = 0;
        int val =  cache.read();
        if (val != -1) {
            ++streamPos;
        }
        return val;
    }

    public int read(byte[] b, int off, int len) throws IOException {
        bitOffset = 0;
        int nbytes = cache.read(b, off, len);
        if (nbytes != -1) {
            streamPos += nbytes;
        }
        return nbytes;
    }

    public void write(int b) throws IOException {
        flushBits();
        cache.write(b);
        ++streamPos;
        maxStreamPos = Math.max(maxStreamPos, streamPos);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        flushBits();
        cache.write(b, off, len);
        streamPos += len;
        maxStreamPos = Math.max(maxStreamPos, streamPos);
    }

    public long length() {
        try {
            return cache.length();
        } catch (IOException e) {
            return -1L;
        }
    }

    /**
     * Sets the current stream position and resets the bit offset to
     * 0.  It is legal to seek past the end of the file; an
     * <code>EOFException</code> will be thrown only if a read is
     * performed.  The file length will not be increased until a write
     * is performed.
     *
     * @exception IndexOutOfBoundsException if <code>pos</code> is smaller
     * than the flushed position.
     * @exception IOException if any other I/O error occurs.
     */
    public void seek(long pos) throws IOException {
        checkClosed();

        if (pos < flushedPos) {
            throw new IndexOutOfBoundsException();
        }

        cache.seek(pos);
        this.streamPos = cache.getFilePointer();
        maxStreamPos = Math.max(maxStreamPos, streamPos);
        this.bitOffset = 0;
    }

    /**
     * Returns <code>true</code> since this
     * <code>ImageOutputStream</code> caches data in order to allow
     * seeking backwards.
     *
     * @return <code>true</code>.
     *
     * @see #isCachedMemory
     * @see #isCachedFile
     */
    public boolean isCached() {
        return true;
    }

    /**
     * Returns <code>true</code> since this
     * <code>ImageOutputStream</code> maintains a file cache.
     *
     * @return <code>true</code>.
     *
     * @see #isCached
     * @see #isCachedMemory
     */
    public boolean isCachedFile() {
        return true;
    }

    /**
     * Returns <code>false</code> since this
     * <code>ImageOutputStream</code> does not maintain a main memory
     * cache.
     *
     * @return <code>false</code>.
     *
     * @see #isCached
     * @see #isCachedFile
     */
    public boolean isCachedMemory() {
        return false;
    }

    /**
     * Closes this <code>FileCacheImageOututStream</code>.  All
     * pending data is flushed to the output, and the cache file
     * is closed and removed.  The destination <code>OutputStream</code>
     * is not closed.
     *
     * @exception IOException if an error occurs.
     */
    public void close() throws IOException {
        maxStreamPos = cache.length();

        seek(maxStreamPos);
        flushBefore(maxStreamPos);
        super.close();
        cache.close();
        cacheFile.delete();
        stream.flush();
        stream = null;
    }

    public void flushBefore(long pos) throws IOException {
        long oFlushedPos = flushedPos;
        super.flushBefore(pos);

        long flushBytes = flushedPos - oFlushedPos;
        if (flushBytes > 0) {
            int bufLen = 512;
            byte[] buf = new byte[bufLen];
            cache.seek(oFlushedPos);
            while (flushBytes > 0) {
                int len = (int)Math.min(flushBytes, bufLen);
                cache.readFully(buf, 0, len);
                stream.write(buf, 0, len);
                flushBytes -= len;
            }
            stream.flush();
        }
    }
}
