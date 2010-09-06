/*
 * @(#)GIFStreamMetadataFormatResources.java	1.6 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.imageio.plugins.gif;

import java.util.ListResourceBundle;

public class GIFStreamMetadataFormatResources extends ListResourceBundle {

    static final Object[][] contents = {
        // Node name, followed by description
        { "Version", "The file version, either 87a or 89a" },
        { "LogicalScreenDescriptor",
          "The logical screen descriptor, except for the global color table" },
        { "GlobalColorTable", "The global color table" },
        { "ColorTableEntry", "A global color table entry" },

        // Node name + "/" + AttributeName, followed by description
        { "Version/value",
          "The version string" },
        { "LogicalScreenDescriptor/logicalScreenWidth",
          "The width in pixels of the whole picture" },
        { "LogicalScreenDescriptor/logicalScreenHeight",
          "The height in pixels of the whole picture" },
        { "LogicalScreenDescriptor/colorResolution",
          "The number of bits of color resolution, beteen 1 and 8" },
        { "LogicalScreenDescriptor/pixelAspectRatio",
          "If 0, indicates square pixels, else W/H = (value + 15)/64" },
        { "GlobalColorTable/sizeOfGlobalColorTable",
          "The number of entries in the global color table" },
        { "GlobalColorTable/backgroundColorIndex",
          "The index of the color table entry to be used as a background" },
        { "GlobalColorTable/sortFlag",
          "True if the global color table is sorted by frequency" },
        { "ColorTableEntry/index", "The index of the color table entry" },
        { "ColorTableEntry/red",
          "The red value for the color table entry" },
        { "ColorTableEntry/green",
          "The green value for the color table entry" },
        { "ColorTableEntry/blue",
          "The blue value for the color table entry" },
    };

    public GIFStreamMetadataFormatResources() {}

    public Object[][] getContents() {
        return contents;
    }
}
