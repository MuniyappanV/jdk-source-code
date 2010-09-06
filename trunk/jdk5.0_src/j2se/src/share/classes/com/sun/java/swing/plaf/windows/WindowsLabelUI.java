/*
 * @(#)WindowsLabelUI.java	1.18 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.java.swing.plaf.windows;

import com.sun.java.swing.SwingUtilities2;
import java.awt.Color;
import java.awt.Graphics;

import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.UIManager;

import javax.swing.plaf.ComponentUI;

import javax.swing.plaf.basic.BasicLabelUI;



/**
 * Windows rendition of the component.
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with
 * future Swing releases.  The current serialization support is appropriate
 * for short term storage or RMI between applications running the same
 * version of Swing.  A future release of Swing will provide support for
 * long term persistence.
 */
public class WindowsLabelUI extends BasicLabelUI {

    private final static WindowsLabelUI windowsLabelUI = new WindowsLabelUI();

    // ********************************
    //          Create PLAF
    // ********************************
    public static ComponentUI createUI(JComponent c){
	return windowsLabelUI;
    }

    protected void paintEnabledText(JLabel l, Graphics g, String s, 
				    int textX, int textY) {
	int mnemonicIndex = l.getDisplayedMnemonicIndex();
	// W2K Feature: Check to see if the Underscore should be rendered.
	if (WindowsLookAndFeel.isMnemonicHidden() == true) {
	    mnemonicIndex = -1;
	}

        g.setColor(l.getForeground());
        SwingUtilities2.drawStringUnderlineCharAt(l, g, s, mnemonicIndex,
                                                     textX, textY);
    }

    protected void paintDisabledText(JLabel l, Graphics g, String s, 
				     int textX, int textY) {
	int mnemonicIndex = l.getDisplayedMnemonicIndex();
	// W2K Feature: Check to see if the Underscore should be rendered.
	if (WindowsLookAndFeel.isMnemonicHidden() == true) {
	    mnemonicIndex = -1;
	}
	if ( UIManager.getColor("Label.disabledForeground") instanceof Color &&
	     UIManager.getColor("Label.disabledShadow") instanceof Color) {
	    g.setColor( UIManager.getColor("Label.disabledShadow") );
	    SwingUtilities2.drawStringUnderlineCharAt(l, g, s,
							 mnemonicIndex,
							 textX + 1, textY + 1);
	    g.setColor( UIManager.getColor("Label.disabledForeground") );
	    SwingUtilities2.drawStringUnderlineCharAt(l, g, s,
							 mnemonicIndex,
							 textX, textY);
	} else {
	    Color background = l.getBackground();
	    g.setColor(background.brighter());
	    SwingUtilities2.drawStringUnderlineCharAt(l,g, s, mnemonicIndex,
							 textX + 1, textY + 1);
	    g.setColor(background.darker());
            SwingUtilities2.drawStringUnderlineCharAt(l,g, s, mnemonicIndex,
							 textX, textY);
	}
    }
}

