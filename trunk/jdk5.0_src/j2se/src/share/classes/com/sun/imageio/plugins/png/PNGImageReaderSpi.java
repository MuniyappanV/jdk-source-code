/*
 * @(#)PNGImageReaderSpi.java	1.25 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.imageio.plugins.png;

import java.io.IOException;
import java.util.Locale;
import java.util.Iterator;
import javax.imageio.ImageReader;
import javax.imageio.spi.ImageReaderSpi;
import javax.imageio.metadata.IIOMetadataFormat;
import javax.imageio.metadata.IIOMetadataFormatImpl;
import javax.imageio.stream.ImageInputStream;

/**
 * @version 0.5
 */
public class PNGImageReaderSpi extends ImageReaderSpi {

    private static final String vendorName = "Sun Microsystems, Inc.";

    private static final String version = "1.0";

    private static final String[] names = { "png" };

    private static final String[] suffixes = { "png" };
    
    private static final String[] MIMETypes = { "image/png", "image/x-png" };

    private static final String readerClassName =
        "com.sun.imageio.plugins.png.PNGImageReader";

    private static final String[] writerSpiNames = {
        "com.sun.imageio.plugins.png.PNGImageWriterSpi"
    };

    public PNGImageReaderSpi() {
        super(vendorName,
              version,
              names,
              suffixes,
              MIMETypes,
              readerClassName,
              STANDARD_INPUT_TYPE,
              writerSpiNames,
              false,
              null, null,
              null, null,
              true,
              PNGMetadata.nativeMetadataFormatName,
              "com.sun.imageio.plugins.png.PNGMetadataFormat",
              null, null
              );
    }

    public String getDescription(Locale locale) {
        return "Standard PNG image reader";
    }

    public boolean canDecodeInput(Object input) throws IOException {
        if (!(input instanceof ImageInputStream)) {
            return false;
        }

        ImageInputStream stream = (ImageInputStream)input;
        byte[] b = new byte[8];
        stream.mark();
        stream.readFully(b);
        stream.reset();
        
        return (b[0] == (byte)137 &&
                b[1] == (byte)80 &&
                b[2] == (byte)78 &&
                b[3] == (byte)71 &&
                b[4] == (byte)13 &&
                b[5] == (byte)10 &&
                b[6] == (byte)26 &&
                b[7] == (byte)10);
    }

    public ImageReader createReaderInstance(Object extension) {
        return new PNGImageReader(this);
    }
}
