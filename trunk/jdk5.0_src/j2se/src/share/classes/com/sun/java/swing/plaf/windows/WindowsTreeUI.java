/*
 * @(#)WindowsTreeUI.java	1.23 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.java.swing.plaf.windows;

import java.awt.*;
import java.awt.event.*;

import java.io.*;
import java.util.*;

import javax.swing.plaf.basic.*;
import javax.swing.*;
import javax.swing.plaf.*;

import javax.swing.tree.*;


/**
 * A Windows tree.
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with
 * future Swing releases.  The current serialization support is appropriate
 * for short term storage or RMI between applications running the same
 * version of Swing.  A future release of Swing will provide support for
 * long term persistence.
 *
 * @version 1.23 12/19/03
 * @author Scott Violet
 */
public class WindowsTreeUI extends BasicTreeUI {

    public static ComponentUI createUI( JComponent c )
      {
	return new WindowsTreeUI();
      }


    /**
      * Ensures that the rows identified by beginRow through endRow are
      * visible.
      */
    protected void ensureRowsAreVisible(int beginRow, int endRow) {
	if(tree != null && beginRow >= 0 && endRow < getRowCount(tree)) {
            Rectangle visRect = tree.getVisibleRect();
	    if(beginRow == endRow) {
		Rectangle     scrollBounds = getPathBounds(tree, getPathForRow
							   (tree, beginRow));

		if(scrollBounds != null) {
                    scrollBounds.x = visRect.x;
                    scrollBounds.width = visRect.width;
		    tree.scrollRectToVisible(scrollBounds);
		}
	    }
	    else {
		Rectangle   beginRect = getPathBounds(tree, getPathForRow
						      (tree, beginRow));
		Rectangle   testRect = beginRect;
		int         beginY = beginRect.y;
		int         maxY = beginY + visRect.height;

		for(int counter = beginRow + 1; counter <= endRow; counter++) {
		    testRect = getPathBounds(tree,
					     getPathForRow(tree, counter));
		    if((testRect.y + testRect.height) > maxY)
			counter = endRow;
		}
		tree.scrollRectToVisible(new Rectangle(visRect.x, beginY, 1,
						  testRect.y + testRect.height-
						  beginY));
	    }
	}
    }

    static protected final int HALF_SIZE = 4;
    static protected final int SIZE = 9;

    /**
     * Returns the default cell renderer that is used to do the
     * stamping of each node.
     */
    protected TreeCellRenderer createDefaultCellRenderer() {
	return new WindowsTreeCellRenderer();
    }

    /**
     * The minus sign button icon
     * <p>
     * <strong>Warning:</strong>
     * Serialized objects of this class will not be compatible with
     * future Swing releases.  The current serialization support is appropriate
     * for short term storage or RMI between applications running the same
     * version of Swing.  A future release of Swing will provide support for
     * long term persistence.
     */
    public static class ExpandedIcon implements Icon, Serializable {
	XPStyle xp = XPStyle.getXP();
	XPStyle.Skin skin = (xp != null) ? xp.getSkin("treeview.glyph") : null;

        static public Icon createExpandedIcon() {
	    return new ExpandedIcon();
	}

	public void paintIcon(Component c, Graphics g, int x, int y) {
	    if (skin != null) {
		skin.paintSkin(g, x, y, 1);
		return;
	    }

	    Color     backgroundColor = c.getBackground();

	    if(backgroundColor != null)
		g.setColor(backgroundColor);
	    else
		g.setColor(Color.white);
	    g.fillRect(x, y, SIZE-1, SIZE-1);
	    g.setColor(Color.gray);
	    g.drawRect(x, y, SIZE-1, SIZE-1);
	    g.setColor(Color.black);
	    g.drawLine(x + 2, y + HALF_SIZE, x + (SIZE - 3), y + HALF_SIZE);
	}
	public int getIconWidth() { return (skin != null) ? skin.getWidth() : SIZE; }
	public int getIconHeight() { return (skin != null) ? skin.getHeight() : SIZE; }
    }

    /**
     * The plus sign button icon
     * <p>
     * <strong>Warning:</strong>
     * Serialized objects of this class will not be compatible with
     * future Swing releases.  The current serialization support is appropriate
     * for short term storage or RMI between applications running the same
     * version of Swing.  A future release of Swing will provide support for
     * long term persistence.
     */
    public static class CollapsedIcon extends ExpandedIcon {
        static public Icon createCollapsedIcon() {
	    return new CollapsedIcon();
	}

	public void paintIcon(Component c, Graphics g, int x, int y) {
	    if (skin != null) {
		skin.paintSkin(g, x, y, 0);
	    } else {
	    super.paintIcon(c, g, x, y);
	    g.drawLine(x + HALF_SIZE, y + 2, x + HALF_SIZE, y + (SIZE - 3));
	    }
	}
    }

    public class WindowsTreeCellRenderer extends DefaultTreeCellRenderer {

	/**
	 * Configures the renderer based on the passed in components.
	 * The value is set from messaging the tree with
	 * <code>convertValueToText</code>, which ultimately invokes
	 * <code>toString</code> on <code>value</code>.
	 * The foreground color is set based on the selection and the icon
	 * is set based on on leaf and expanded.
	 */
	public Component getTreeCellRendererComponent(JTree tree, Object value,
						      boolean sel,
						      boolean expanded,
						      boolean leaf, int row,
						      boolean hasFocus) {
	    super.getTreeCellRendererComponent(tree, value, sel,
					       expanded, leaf, row,
					       hasFocus);
	    // Windows displays the open icon when the tree item selected.
	    if (!tree.isEnabled()) {
		setEnabled(false);
		if (leaf) {
		    setDisabledIcon(getLeafIcon());
		} else if (sel) {
		    setDisabledIcon(getOpenIcon());
		} else {
		    setDisabledIcon(getClosedIcon());
		}
	    }
	    else {
		setEnabled(true);
		if (leaf) {
		    setIcon(getLeafIcon());
		} else if (sel) {
		    setIcon(getOpenIcon());
		} else {
		    setIcon(getClosedIcon());
		}
	    }
	    return this;
	}
	
    }

}
