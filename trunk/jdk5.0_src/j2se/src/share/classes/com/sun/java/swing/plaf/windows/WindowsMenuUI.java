/*
 * @(#)WindowsMenuUI.java	1.24 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.java.swing.plaf.windows;

import java.awt.*;
import java.awt.event.MouseEvent;

import javax.swing.plaf.ComponentUI;
import javax.swing.plaf.basic.BasicMenuUI;
import javax.swing.event.MouseInputListener;
import javax.swing.*;

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
public class WindowsMenuUI extends BasicMenuUI {

    public static ComponentUI createUI(JComponent x) {
	return new WindowsMenuUI();
    }

    protected void installDefaults() {
	super.installDefaults();

	if (!WindowsLookAndFeel.isClassicWindows()) {
	    menuItem.setRolloverEnabled(true);
	}
    }

    /**
     * Draws the background of the menu.
     * @since 1.4
     */
    protected void paintBackground(Graphics g, JMenuItem menuItem, Color bgColor) {
	JMenu menu = (JMenu)menuItem;
	ButtonModel model = menu.getModel();

	// Use superclass method for the old Windows LAF,
        // for submenus, and for XP toplevel if selected or pressed
	if (WindowsLookAndFeel.isClassicWindows() ||
	    !menu.isTopLevelMenu() ||
	    (XPStyle.getXP() != null && (model.isArmed() || model.isSelected()))) {

	    super.paintBackground(g, menu, bgColor);
	    return;
	}

	Color oldColor = g.getColor();
        int menuWidth = menu.getWidth();
        int menuHeight = menu.getHeight();

	UIDefaults table = UIManager.getLookAndFeelDefaults();
	Color highlight = table.getColor("controlLtHighlight");
	Color shadow = table.getColor("controlShadow");

	g.setColor(menu.getBackground());
	g.fillRect(0,0, menuWidth, menuHeight);

        if (menu.isOpaque()) {
            if (model.isArmed() || model.isSelected()) {
		// Draw a lowered bevel border
		g.setColor(shadow);
		g.drawLine(0,0, menuWidth - 1,0);
		g.drawLine(0,0, 0,menuHeight - 2);

		g.setColor(highlight);
		g.drawLine(menuWidth - 1,0, menuWidth - 1,menuHeight - 2);
		g.drawLine(0,menuHeight - 2, menuWidth - 1,menuHeight - 2);
            } else if (model.isRollover() && model.isEnabled()) {
		// Only paint rollover if no other menu on menubar is selected
		boolean otherMenuSelected = false;
		MenuElement[] menus = ((JMenuBar)menu.getParent()).getSubElements();
		for (int i = 0; i < menus.length; i++) {
		    if (((JMenuItem)menus[i]).isSelected()) {
			otherMenuSelected = true;
			break;
		    }
		}
		if (!otherMenuSelected) {
		    if (XPStyle.getXP() != null) {
			g.setColor(selectionBackground); // Uses protected field.
			g.fillRect(0, 0, menuWidth, menuHeight);
		    } else {
			// Draw a raised bevel border
			g.setColor(highlight);
			g.drawLine(0,0, menuWidth - 1,0);
			g.drawLine(0,0, 0,menuHeight - 2);

			g.setColor(shadow);
			g.drawLine(menuWidth - 1,0, menuWidth - 1,menuHeight - 2);
			g.drawLine(0,menuHeight - 2, menuWidth - 1,menuHeight - 2);
		    }
		}
            }
        }
	g.setColor(oldColor);
    }

    /**
     * Method which renders the text of the current menu item.
     * <p>
     * @param g Graphics context
     * @param menuItem Current menu item to render
     * @param textRect Bounding rectangle to render the text.
     * @param text String to render
     * @since 1.4
     */
    protected void paintText(Graphics g, JMenuItem menuItem,
                             Rectangle textRect, String text) {

	JMenu menu = (JMenu)menuItem;
	ButtonModel model = menuItem.getModel();
        Color oldColor = g.getColor();

	// Only paint rollover if no other menu on menubar is selected
	boolean paintRollover = model.isRollover();
	if (paintRollover && menu.isTopLevelMenu()) {
	    MenuElement[] menus = ((JMenuBar)menu.getParent()).getSubElements();
	    for (int i = 0; i < menus.length; i++) {
		if (((JMenuItem)menus[i]).isSelected()) {
		    paintRollover = false;
		    break;
		}
	    }
	}

	if ((model.isSelected() && (WindowsLookAndFeel.isClassicWindows() ||
				    !menu.isTopLevelMenu())) ||
	    (XPStyle.getXP() != null && (paintRollover ||
					 model.isArmed() ||
					 model.isSelected()))) {
	    g.setColor(selectionForeground); // Uses protected field.
	}

        WindowsGraphicsUtils.paintText(g, menuItem, textRect, text, 0);
 
        g.setColor(oldColor);
    }

    protected MouseInputListener createMouseInputListener(JComponent c) {
        return new WindowsMouseInputHandler();
    }

    /**
     * This class implements a mouse handler that sets the rollover flag to
     * true when the mouse enters the menu and false when it exits.
     * @since 1.4
     */
    protected class WindowsMouseInputHandler extends BasicMenuUI.MouseInputHandler {
	public void mouseEntered(MouseEvent evt) {
	    super.mouseEntered(evt);

	    JMenu menu = (JMenu)evt.getSource();
	    ButtonModel model = menu.getModel();
	    if (menu.isRolloverEnabled()) {
		model.setRollover(true);
		menuItem.repaint();
	    }
	}

	public void mouseExited(MouseEvent evt) {
	    super.mouseExited(evt);

	    JMenu menu = (JMenu)evt.getSource();
	    ButtonModel model = menu.getModel();
	    if (menu.isRolloverEnabled()) {
		model.setRollover(false);
		menuItem.repaint();
	    }
	}
    }
}

