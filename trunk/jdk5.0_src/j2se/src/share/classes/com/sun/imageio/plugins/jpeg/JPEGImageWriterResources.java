/*
 * @(#)JPEGImageWriterResources.java	1.5 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.imageio.plugins.jpeg;

import java.util.ListResourceBundle;

public class JPEGImageWriterResources extends ListResourceBundle {

    static final Object[][] contents = {
        {Integer.toString(JPEGImageWriter.WARNING_DEST_IGNORED),
         "Only Rasters or band subsets may be written with a destination type. " 
         + "Destination type ignored."},
        {Integer.toString(JPEGImageWriter.WARNING_STREAM_METADATA_IGNORED),
         "Stream metadata ignored on write"},
        {Integer.toString(JPEGImageWriter.WARNING_DEST_METADATA_COMP_MISMATCH),
         "Metadata component ids incompatible with destination type. " 
         + "Metadata modified."},
        {Integer.toString(JPEGImageWriter.WARNING_DEST_METADATA_JFIF_MISMATCH),
         "Metadata JFIF settings incompatible with destination type. " 
         + "Metadata modified."},
        {Integer.toString(JPEGImageWriter.WARNING_DEST_METADATA_ADOBE_MISMATCH),
         "Metadata Adobe settings incompatible with destination type. "
         + "Metadata modified."},
        {Integer.toString(JPEGImageWriter.WARNING_IMAGE_METADATA_JFIF_MISMATCH),
         "Metadata JFIF settings incompatible with image type. " 
         + "Metadata modified."},
        {Integer.toString(JPEGImageWriter.WARNING_IMAGE_METADATA_ADOBE_MISMATCH),
         "Metadata Adobe settings incompatible with image type. "
         + "Metadata modified."},
        {Integer.toString(JPEGImageWriter.WARNING_METADATA_NOT_JPEG_FOR_RASTER),
         "Metadata must be JPEGMetadata when writing a Raster. "
         + "Metadata ignored."},
        {Integer.toString(JPEGImageWriter.WARNING_NO_BANDS_ON_INDEXED),
         "Band subset not allowed for an IndexColorModel image.  "
         + "Band subset ignored."},
        {Integer.toString(JPEGImageWriter.WARNING_ILLEGAL_THUMBNAIL),
         "Thumbnails must be simple (possibly index) RGB or grayscale.  "
         + "Incompatible thumbnail ignored."},
        {Integer.toString(JPEGImageWriter.WARNING_IGNORING_THUMBS ),
         "Thumbnails ignored for non-JFIF-compatible image."},
        {Integer.toString(JPEGImageWriter.WARNING_FORCING_JFIF ),
         "Thumbnails require JFIF marker segment.  "
         + "Missing node added to metadata."},
        {Integer.toString(JPEGImageWriter.WARNING_THUMB_CLIPPED ),
         "Thumbnail clipped."},
        {Integer.toString(JPEGImageWriter.WARNING_METADATA_ADJUSTED_FOR_THUMB ),
         "Metadata adjusted (made JFIF-compatible) for thumbnail."},
        {Integer.toString(JPEGImageWriter.WARNING_NO_RGB_THUMB_AS_INDEXED ),
         "RGB thumbnail can't be written as indexed.  Written as RGB"},
        {Integer.toString(JPEGImageWriter.WARNING_NO_GRAY_THUMB_AS_INDEXED),
       "Grayscale thumbnail can't be written as indexed.  Written as JPEG"},
    };

    public JPEGImageWriterResources() {}
         
    public Object[][] getContents() {
        return contents;
    }


}
