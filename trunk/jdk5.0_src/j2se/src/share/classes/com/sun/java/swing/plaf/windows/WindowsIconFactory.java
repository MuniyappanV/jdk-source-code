/*
 * @(#)WindowsIconFactory.java	1.21 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.java.swing.plaf.windows;

import javax.swing.*;
import javax.swing.plaf.UIResource;

import java.awt.*;
import java.io.Serializable;

/**
 * Factory object that can vend Icons appropriate for the Windows L & F.
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with 
 * future Swing releases.  The current serialization support is appropriate
 * for short term storage or RMI between applications running the same
 * version of Swing.  A future release of Swing will provide support for
 * long term persistence.
 *
 * @version 1.21 12/19/03
 * @author David Kloba
 * @author Georges Saab
 * @author Rich Schiavi
 */
public class WindowsIconFactory implements Serializable
{
    private static Icon frame_closeIcon;
    private static Icon frame_iconifyIcon;
    private static Icon frame_maxIcon;
    private static Icon frame_minIcon;
    private static Icon frame_resizeIcon;
    private static Icon checkBoxIcon;
    private static Icon radioButtonIcon;
    private static Icon checkBoxMenuItemIcon;
    private static Icon radioButtonMenuItemIcon;
    private static Icon menuItemCheckIcon;
    private static Icon menuItemArrowIcon;
    private static Icon menuArrowIcon;

    public static Icon getMenuItemCheckIcon() {
	if (menuItemCheckIcon == null) {
	    menuItemCheckIcon = new MenuItemCheckIcon();
	}
	return menuItemCheckIcon;
    }

    public static Icon getMenuItemArrowIcon() {
	if (menuItemArrowIcon == null) {
	    menuItemArrowIcon = new MenuItemArrowIcon();
	}
	return menuItemArrowIcon;
    }

    public static Icon getMenuArrowIcon() {
	if (menuArrowIcon == null) {
	    menuArrowIcon = new MenuArrowIcon();
	}
	return menuArrowIcon;
    }

    public static Icon getCheckBoxIcon() {
	if (checkBoxIcon == null) {
	    checkBoxIcon = new CheckBoxIcon();
	}
	return checkBoxIcon;
    }

    public static Icon getRadioButtonIcon() {
	if (radioButtonIcon == null) {
	    radioButtonIcon = new RadioButtonIcon();
	}
	return radioButtonIcon;
    }

    public static Icon getCheckBoxMenuItemIcon() {
	if (checkBoxMenuItemIcon == null) {
	    checkBoxMenuItemIcon = new CheckBoxMenuItemIcon();
	}
	return checkBoxMenuItemIcon;
    }

    public static Icon getRadioButtonMenuItemIcon() {
	if (radioButtonMenuItemIcon == null) {
	    radioButtonMenuItemIcon = new RadioButtonMenuItemIcon();
	}
	return radioButtonMenuItemIcon;
    }

    public static Icon createFrameCloseIcon() {
	if (frame_closeIcon == null) {
	    frame_closeIcon = new FrameButtonIcon("window.closebutton");
	}
	return frame_closeIcon;
    }

    public static Icon createFrameIconifyIcon() {
	if (frame_iconifyIcon == null) {
	    frame_iconifyIcon = new FrameButtonIcon("window.minbutton");
	}
	return frame_iconifyIcon;
    }

    public static Icon createFrameMaximizeIcon() {
	if (frame_maxIcon == null) {
	    frame_maxIcon = new FrameButtonIcon("window.maxbutton");
	}
	return frame_maxIcon;
    }

    public static Icon createFrameMinimizeIcon() {
	if (frame_minIcon == null) {
	    frame_minIcon = new FrameButtonIcon("window.restorebutton");
	}
	return frame_minIcon;
    }

    public static Icon createFrameResizeIcon() {
	if(frame_resizeIcon == null)
	    frame_resizeIcon = new ResizeIcon();
	return frame_resizeIcon;
    }


    private static class FrameButtonIcon implements Icon, Serializable {
	private String category;

	private FrameButtonIcon(String category) {
	    this.category = category;
	}

	public void paintIcon(Component c, Graphics g, int x0, int y0) {
	    int width = getIconWidth();
	    int height = getIconHeight();

	    XPStyle xp = XPStyle.getXP();
	    if (xp != null) {
		XPStyle.Skin skin = xp.getSkin(category);
		JButton b = (JButton)c;
		ButtonModel model = b.getModel();
		int index = 0;
		if (!model.isEnabled()) {
		    index = 3;
		} else if (model.isArmed() && model.isPressed()) {
		    index = 2;
		} else if (model.isRollover()) {
		    index = 1;
		}
		// Find out if frame is inactive
		JInternalFrame jif = (JInternalFrame)SwingUtilities.
					getAncestorOfClass(JInternalFrame.class, b);
		if (jif != null && !jif.isSelected()) {
		    index += 4;
		}
		skin.paintSkin(g, 0, 0, width, height, index);
	    } else {
		g.setColor(Color.black);
		int x = width / 12 + 2;
		int y = height / 5;
		int h = height - y * 2 - 1;
		int w = width * 3/4 -3;
		int thickness2 = Math.max(height / 8, 2);
		int thickness  = Math.max(width / 15, 1);
		if (category == "window.closebutton") {
		    int lineWidth;
		    if      (width > 47) lineWidth = 6;
		    else if (width > 37) lineWidth = 5;
		    else if (width > 26) lineWidth = 4;
		    else if (width > 16) lineWidth = 3;
		    else if (width > 12) lineWidth = 2;
		    else                 lineWidth = 1;
		    y = height / 12 + 2;
		    if (lineWidth == 1) {
			if (w % 2 == 1) { x++; w++; }
			g.drawLine(x,     y, x+w-2, y+w-2);
			g.drawLine(x+w-2, y, x,     y+w-2);
		    } else if (lineWidth == 2) {
			if (w > 6) { x++; w--; }
			g.drawLine(x,     y, x+w-2, y+w-2);
			g.drawLine(x+w-2, y, x,     y+w-2);
			g.drawLine(x+1,   y, x+w-1, y+w-2);
			g.drawLine(x+w-1, y, x+1,   y+w-2);
		    } else {
			x += 2; y++; w -= 2;
			g.drawLine(x,     y,   x+w-1, y+w-1);
			g.drawLine(x+w-1, y,   x,     y+w-1);
			g.drawLine(x+1,   y,   x+w-1, y+w-2);
			g.drawLine(x+w-2, y,   x,     y+w-2);
			g.drawLine(x,     y+1, x+w-2, y+w-1);
			g.drawLine(x+w-1, y+1, x+1,   y+w-1);
			for (int i = 4; i <= lineWidth; i++) {
			    g.drawLine(x+i-2,   y,     x+w-1,   y+w-i+1);
			    g.drawLine(x,       y+i-2, x+w-i+1, y+w-1);
			    g.drawLine(x+w-i+1, y,     x,       y+w-i+1);
			    g.drawLine(x+w-1,   y+i-2, x+i-2,   y+w-1);
			}
		    }
		} else if (category == "window.minbutton") {
		    g.fillRect(x, y+h-thickness2, w-w/3, thickness2);
		} else if (category == "window.maxbutton") {
		    g.fillRect(x, y, w, thickness2);
		    g.fillRect(x, y, thickness, h);
		    g.fillRect(x+w-thickness, y, thickness, h);
		    g.fillRect(x, y+h-thickness, w, thickness);
		} else if (category == "window.restorebutton") {
		    g.fillRect(x+w/3, y, w-w/3, thickness2);
		    g.fillRect(x+w/3, y, thickness, h/3);
		    g.fillRect(x+w-thickness, y, thickness, h-h/3);
		    g.fillRect(x+w-w/3, y+h-h/3-thickness, w/3, thickness);

		    g.fillRect(x, y+h/3, w-w/3, thickness2);
		    g.fillRect(x, y+h/3, thickness, h-h/3);
		    g.fillRect(x+w-w/3-thickness, y+h/3, thickness, h-h/3);
		    g.fillRect(x, y+h-thickness, w-w/3, thickness);
		}
	    }
	}

	public int getIconWidth() {
	    int width;
	    if (XPStyle.getXP() != null) {
		// Fix for XP bug where sometimes these sizes aren't updated properly
		// Assume for now that XP buttons are always square
		width = UIManager.getInt("InternalFrame.titleButtonHeight") -2;
	    } else {
		width = UIManager.getInt("InternalFrame.titleButtonWidth") -2;
	    }
	    if (XPStyle.getXP() != null) {
		width -= 2;
	    }
	    return width;
	}

	public int getIconHeight() {
	    int height = UIManager.getInt("InternalFrame.titleButtonHeight")-4;
	    return height;
	}
    }



        private static class ResizeIcon implements Icon, Serializable {
            public void paintIcon(Component c, Graphics g, int x, int y) {
                g.setColor(UIManager.getColor("InternalFrame.resizeIconHighlight"));
                g.drawLine(0, 11, 11, 0);
                g.drawLine(4, 11, 11, 4);
                g.drawLine(8, 11, 11, 8);

                g.setColor(UIManager.getColor("InternalFrame.resizeIconShadow"));
                g.drawLine(1, 11, 11, 1);
                g.drawLine(2, 11, 11, 2);
                g.drawLine(5, 11, 11, 5);
                g.drawLine(6, 11, 11, 6);
                g.drawLine(9, 11, 11, 9);
                g.drawLine(10, 11, 11, 10);
            }
            public int getIconWidth() { return 13; }
            public int getIconHeight() { return 13; }
        };

    private static class CheckBoxIcon implements Icon, Serializable
    {
	final static int csize = 13;
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    JCheckBox cb = (JCheckBox) c;
	    ButtonModel model = cb.getModel();
	    XPStyle xp = XPStyle.getXP();

	    if (xp != null) {
		int index = 0;
		if (!model.isEnabled()) {
		    index = 3;	// disabled
		} else if (model.isPressed() && model.isArmed()) {
		    index = 2;	// pressed
		} else if (model.isRollover()) {
		    index = 1;	// rollover
		}
		if (model.isSelected()) {
		    index += 4;
		}
		xp.getSkin("button.checkbox").paintSkin(g, x, y, index);
	    } else {
		// outer bevel
		if(!cb.isBorderPaintedFlat()) {
		    // Outer top/left
		    g.setColor(UIManager.getColor("CheckBox.shadow"));
		    g.drawLine(x, y, x+11, y);
		    g.drawLine(x, y+1, x, y+11);

		    // Outer bottom/right
		    g.setColor(UIManager.getColor("CheckBox.highlight"));
		    g.drawLine(x+12, y, x+12, y+12);
		    g.drawLine(x, y+12, x+11, y+12);

		    // Inner top.left
		    g.setColor(UIManager.getColor("CheckBox.darkShadow"));
		    g.drawLine(x+1, y+1, x+10, y+1);
		    g.drawLine(x+1, y+2, x+1, y+10);

		    // Inner bottom/right
		    g.setColor(UIManager.getColor("CheckBox.light"));
		    g.drawLine(x+1, y+11, x+11, y+11);
		    g.drawLine(x+11, y+1, x+11, y+10);

		    // inside box 
		    if((model.isPressed() && model.isArmed()) || !model.isEnabled()) {
			g.setColor(UIManager.getColor("CheckBox.background"));
		    } else {
			g.setColor(UIManager.getColor("CheckBox.interiorBackground"));
		    }
		    g.fillRect(x+2, y+2, csize-4, csize-4);
		} else {
		    g.setColor(UIManager.getColor("CheckBox.shadow"));
		    g.drawRect(x+1, y+1, csize-3, csize-3);

		    if((model.isPressed() && model.isArmed()) || !model.isEnabled()) {
			g.setColor(UIManager.getColor("CheckBox.background"));
		    } else {
			g.setColor(UIManager.getColor("CheckBox.interiorBackground"));
		    }
		    g.fillRect(x+2, y+2, csize-4, csize-4);
		}

		if(model.isEnabled()) {
		    g.setColor(UIManager.getColor("CheckBox.darkShadow"));
		} else {
		    g.setColor(UIManager.getColor("CheckBox.shadow"));
		}

		// paint check
		if (model.isSelected()) {
		    g.drawLine(x+9, y+3, x+9, y+3);
		    g.drawLine(x+8, y+4, x+9, y+4);
		    g.drawLine(x+7, y+5, x+9, y+5);
		    g.drawLine(x+6, y+6, x+8, y+6);
		    g.drawLine(x+3, y+7, x+7, y+7);
		    g.drawLine(x+4, y+8, x+6, y+8);
		    g.drawLine(x+5, y+9, x+5, y+9);
		    g.drawLine(x+3, y+5, x+3, y+5);
		    g.drawLine(x+3, y+6, x+4, y+6);
		}
	    }
	}

	public int getIconWidth() {
	    XPStyle xp = XPStyle.getXP();
	    if (xp != null) {
		return xp.getSkin("button.checkbox").getWidth();
	    } else {
		return csize;
	    }
	}
		
	public int getIconHeight() {
	    XPStyle xp = XPStyle.getXP();
	    if (xp != null) {
		return xp.getSkin("button.checkbox").getHeight();
	    } else {
		return csize;
	    }
	}
    }

    private static class RadioButtonIcon implements Icon, UIResource, Serializable
    {
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    AbstractButton b = (AbstractButton) c;
	    ButtonModel model = b.getModel();
	    XPStyle xp = XPStyle.getXP();

	    if (xp != null) {
		XPStyle.Skin skin = xp.getSkin("button.radiobutton");
		int index = 0;
		if (!model.isEnabled()) {
		    index = 3;	// disabled
		} else if (model.isPressed() && model.isArmed()) {
		    index = 2;	// pressed
		} else if (model.isRollover()) {
		    index = 1;	// rollover
		}
		if (model.isSelected()) {
		    index += 4;
		}
		skin.paintSkin(g, x, y, index);
	    } else {
		// fill interior
		if((model.isPressed() && model.isArmed()) || !model.isEnabled()) {
		    g.setColor(UIManager.getColor("RadioButton.background"));
		} else {
		    g.setColor(UIManager.getColor("RadioButton.interiorBackground"));
		}
		g.fillRect(x+2, y+2, 8, 8);


		    // outter left arc
		g.setColor(UIManager.getColor("RadioButton.shadow"));
		g.drawLine(x+4, y+0, x+7, y+0);
		g.drawLine(x+2, y+1, x+3, y+1);
		g.drawLine(x+8, y+1, x+9, y+1);
		g.drawLine(x+1, y+2, x+1, y+3);
		g.drawLine(x+0, y+4, x+0, y+7);
		g.drawLine(x+1, y+8, x+1, y+9);

		// outter right arc
		g.setColor(UIManager.getColor("RadioButton.highlight"));
		g.drawLine(x+2, y+10, x+3, y+10);
		g.drawLine(x+4, y+11, x+7, y+11);
		g.drawLine(x+8, y+10, x+9, y+10);
		g.drawLine(x+10, y+9, x+10, y+8);
		g.drawLine(x+11, y+7, x+11, y+4);
		g.drawLine(x+10, y+3, x+10, y+2);


		// inner left arc
		g.setColor(UIManager.getColor("RadioButton.darkShadow"));
		g.drawLine(x+4, y+1, x+7, y+1);
		g.drawLine(x+2, y+2, x+3, y+2);
		g.drawLine(x+8, y+2, x+9, y+2);
		g.drawLine(x+2, y+3, x+2, y+3);
		g.drawLine(x+1, y+4, x+1, y+7);
		g.drawLine(x+2, y+8, x+2, y+8);


		// inner right arc
		g.setColor(UIManager.getColor("RadioButton.light"));
		g.drawLine(x+2,  y+9,  x+3,  y+9);
		g.drawLine(x+4,  y+10, x+7,  y+10);
		g.drawLine(x+8,  y+9,  x+9,  y+9);
		g.drawLine(x+9,  y+8,  x+9,  y+8);
		g.drawLine(x+10, y+7,  x+10, y+4);
		g.drawLine(x+9,  y+3,  x+9,  y+3);


		// indicate whether selected or not
		if(model.isSelected()) {
		    g.setColor(UIManager.getColor("RadioButton.darkShadow"));
		    g.fillRect(x+4, y+5, 4, 2);
		    g.fillRect(x+5, y+4, 2, 4);
		} 
	    }
	}

	public int getIconWidth() {
	    XPStyle xp = XPStyle.getXP();
	    if (xp != null) {
		return xp.getSkin("button.radiobutton").getWidth();
	    } else {
		return 13;
	    }
	}
		
	public int getIconHeight() {
	    XPStyle xp = XPStyle.getXP();
	    if (xp != null) {
		return xp.getSkin("button.radiobutton").getHeight();
	    } else {
		return 13;
	    }
	}
    } // end class RadioButtonIcon


    private static class CheckBoxMenuItemIcon implements Icon, UIResource, Serializable
    {
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    AbstractButton b = (AbstractButton) c;
	    ButtonModel model = b.getModel();
	    boolean isSelected = model.isSelected();
	    if (isSelected) {
		y = y - getIconHeight() / 2;
		g.drawLine(x+9, y+3, x+9, y+3);
		g.drawLine(x+8, y+4, x+9, y+4);
		g.drawLine(x+7, y+5, x+9, y+5);
		g.drawLine(x+6, y+6, x+8, y+6);
		g.drawLine(x+3, y+7, x+7, y+7);
		g.drawLine(x+4, y+8, x+6, y+8);
		g.drawLine(x+5, y+9, x+5, y+9);
		g.drawLine(x+3, y+5, x+3, y+5);
		g.drawLine(x+3, y+6, x+4, y+6);
	    }
	}
	public int getIconWidth() { return 9; }
	public int getIconHeight() { return 9; }

    } // End class CheckBoxMenuItemIcon

    
    private static class RadioButtonMenuItemIcon implements Icon, UIResource, Serializable
    {
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    AbstractButton b = (AbstractButton) c;
	    ButtonModel model = b.getModel();
	    if (b.isSelected() == true) {
               g.fillArc(0,0,getIconWidth()-2, getIconHeight()-2, 0, 360);
	    }
	}
	public int getIconWidth() { return 12; }
	public int getIconHeight() { return 12; }

    } // End class RadioButtonMenuItemIcon


    private static class MenuItemCheckIcon implements Icon, UIResource, Serializable{
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    /* For debugging:
	       Color oldColor = g.getColor();
	    g.setColor(Color.orange);
	    g.fill3DRect(x,y,getIconWidth(), getIconHeight(), true);
	    g.setColor(oldColor);
	    */
	}
	public int getIconWidth() { return 9; }
	public int getIconHeight() { return 9; }

    } // End class MenuItemCheckIcon

    private static class MenuItemArrowIcon implements Icon, UIResource, Serializable {
	public void paintIcon(Component c, Graphics g, int x, int y) {
	    /* For debugging:
	    Color oldColor = g.getColor();
	    g.setColor(Color.green);
	    g.fill3DRect(x,y,getIconWidth(), getIconHeight(), true);
	    g.setColor(oldColor);
	    */
	}
	public int getIconWidth() { return 4; }
	public int getIconHeight() { return 8; }

    } // End class MenuItemArrowIcon

    private static class MenuArrowIcon implements Icon, UIResource, Serializable {
	public void paintIcon(Component c, Graphics g, int x, int y) {
            g.translate(x,y);
            if( WindowsUtils.isLeftToRight(c) ) {
                g.drawLine( 0, 0, 0, 7 );
                g.drawLine( 1, 1, 1, 6 );
                g.drawLine( 2, 2, 2, 5 );
                g.drawLine( 3, 3, 3, 4 );
            } else {
                g.drawLine( 4, 0, 4, 7 );
                g.drawLine( 3, 1, 3, 6 );
                g.drawLine( 2, 2, 2, 5 );
                g.drawLine( 1, 3, 1, 4 );
            }
            g.translate(-x,-y);
	}
	public int getIconWidth() { return 4; }
	public int getIconHeight() { return 8; }
    } // End class MenuArrowIcon
}

