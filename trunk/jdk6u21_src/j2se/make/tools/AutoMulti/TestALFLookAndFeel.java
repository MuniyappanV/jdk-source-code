/*
 * @(#)TestALFLookAndFeel.java	1.6 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
//package com.myco.myalaf;  //search for myalaf for other refs to package name

import java.util.Vector;
import java.lang.reflect.Method;
import javax.swing.*;
import javax.swing.plaf.*;

/**
 * <p>An auxiliary look and feel used for testing the Multiplexing
 * look and feel.
 * <p>
 * 
 * @see UIManager#addAuxiliaryLookAndFeel
 * @see javax.swing.plaf.multi
 *
 * @version 1.6 03/23/10
 * @author Kathy Walrath
 * @author Will Walker
 */
public class TestALFLookAndFeel extends LookAndFeel {

//////////////////////////////
// LookAndFeel methods
//////////////////////////////

    /**
     * Returns a string, suitable for use in menus,
     * that identifies this look and feel.
     *
     * @return a string such as "Test Auxiliary Look and Feel"
     */
    public String getName() {
        return "Test Auxiliary Look and Feel";
    }
    
    /**
     * Returns a string, suitable for use by applications/services,
     * that identifies this look and feel.
     * 
     * @return "TestALF"
     */
    public String getID() {
	return "TestALF";
    }

    /**
     * Returns a one-line description of this look and feel.
     * 
     * @return a descriptive string such as "Allows multiple UI instances per component instance"
     */
    public String getDescription() {
        return "Allows multiple UI instances per component instance";
    }

    /**
     * Returns <code>false</code>;
     * this look and feel is not native to any platform.
     *
     * @return <code>false</code>
     */
    public boolean isNativeLookAndFeel() {
	return false;
    }

    /**
     * Returns <code>true</code>;
     * every platform permits this look and feel.
     *
     * @return <code>true</code>
     */
    public boolean isSupportedLookAndFeel() {
	return true;
    }

    /**
     * Creates, initializes, and returns
     * the look and feel specific defaults.
     * For this look and feel,
     * the defaults consist solely of 
     * mappings of UI class IDs
     * (such as "ButtonUI")
     * to <code>ComponentUI</code> class names
     * (such as "com.myco.myalaf.MultiButtonUI").
     *
     * @return an initialized <code>UIDefaults</code> object
     * @see javax.swing.JComponent#getUIClassID
     */
    public UIDefaults getDefaults() {
	System.out.println("In the TestALFLookAndFeel getDefaults method.");
        UIDefaults table = new TestALFUIDefaults();
	//String prefix = "com.myco.myalaf.TestALF";
	String prefix = "TestALF";
	Object[] uiDefaults = {
		   "ButtonUI", prefix + "ButtonUI",
	 "CheckBoxMenuItemUI", prefix + "MenuItemUI",
		 "CheckBoxUI", prefix + "ButtonUI",
             "ColorChooserUI", prefix + "ColorChooserUI",
		 "ComboBoxUI", prefix + "ComboBoxUI",
	      "DesktopIconUI", prefix + "DesktopIconUI",
	      "DesktopPaneUI", prefix + "DesktopPaneUI",
               "EditorPaneUI", prefix + "TextUI",
              "FileChooserUI", prefix + "FileChooserUI",
       "FormattedTextFieldUI", prefix + "TextUI",
	    "InternalFrameUI", prefix + "InternalFrameUI",
		    "LabelUI", prefix + "LabelUI",
		     "ListUI", prefix + "ListUI",
		  "MenuBarUI", prefix + "MenuBarUI",
		 "MenuItemUI", prefix + "MenuItemUI",
		     "MenuUI", prefix + "MenuItemUI",
	       "OptionPaneUI", prefix + "OptionPaneUI",
	            "PanelUI", prefix + "PanelUI",
	    "PasswordFieldUI", prefix + "TextUI",
       "PopupMenuSeparatorUI", prefix + "SeparatorUI",
		"PopupMenuUI", prefix + "PopupMenuUI",
	      "ProgressBarUI", prefix + "ProgressBarUI",
      "RadioButtonMenuItemUI", prefix + "MenuItemUI",
	      "RadioButtonUI", prefix + "ButtonUI",
	         "RootPaneUI", prefix + "RootPaneUI",
		"ScrollBarUI", prefix + "ScrollBarUI",
	       "ScrollPaneUI", prefix + "ScrollPaneUI",
		"SeparatorUI", prefix + "SeparatorUI",
		   "SliderUI", prefix + "SliderUI",
		  "SpinnerUI", prefix + "SpinnerUI",
		"SplitPaneUI", prefix + "SplitPaneUI",
	       "TabbedPaneUI", prefix + "TabbedPaneUI",
	      "TableHeaderUI", prefix + "TableHeaderUI",
		    "TableUI", prefix + "TableUI",
		 "TextAreaUI", prefix + "TextUI",
		"TextFieldUI", prefix + "TextUI",
		 "TextPaneUI", prefix + "TextUI",
	     "ToggleButtonUI", prefix + "ButtonUI",
         "ToolBarSeparatorUI", prefix + "SeparatorUI",
		  "ToolBarUI", prefix + "ToolBarUI",
		  "ToolTipUI", prefix + "ToolTipUI",
		     "TreeUI", prefix + "TreeUI",
		 "ViewportUI", prefix + "ViewportUI",
	};

	table.putDefaults(uiDefaults);
	return table;
    }

}

/**
 * We want the Test auxiliary look and feel to be quiet and fallback
 * gracefully if it cannot find a UI.  This class overrides the
 * getUIError method of UIDefaults, which is the method that 
 * emits error messages when it cannot find a UI class in the
 * LAF.
 */
class TestALFUIDefaults extends UIDefaults {
    protected void getUIError(String msg) {
	System.err.println("Test auxiliary L&F:  " + msg);
    }
}
