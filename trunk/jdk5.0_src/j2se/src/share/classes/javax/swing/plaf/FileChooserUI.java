/*
 * @(#)FileChooserUI.java	1.19 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.swing.plaf;

import javax.swing.JFileChooser;
import javax.swing.filechooser.FileFilter;
import javax.swing.filechooser.FileView;
import java.io.File;

/**
 * Pluggable look and feel interface for <code>JFileChooser</code>.
 *
 * @version 1.19 12/19/03
 * @author Jeff Dinkins
 */

public abstract class FileChooserUI extends ComponentUI
{
    public abstract FileFilter getAcceptAllFileFilter(JFileChooser fc);
    public abstract FileView getFileView(JFileChooser fc);

    public abstract String getApproveButtonText(JFileChooser fc);
    public abstract String getDialogTitle(JFileChooser fc);

    public abstract void rescanCurrentDirectory(JFileChooser fc);
    public abstract void ensureFileIsVisible(JFileChooser fc, File f);
}

