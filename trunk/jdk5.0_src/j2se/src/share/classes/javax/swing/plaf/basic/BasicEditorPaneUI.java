/*
 * @(#)BasicEditorPaneUI.java	1.32 04/07/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package javax.swing.plaf.basic;

import java.awt.*;
import java.awt.event.*;
import java.beans.*;
import java.net.URL;
import java.net.MalformedURLException;
import javax.swing.*;
import javax.swing.text.*;
import javax.swing.text.html.*;
import javax.swing.plaf.*;
import javax.swing.border.*;


/**
 * Provides the look and feel for a JEditorPane.
 * <p>
 * <strong>Warning:</strong>
 * Serialized objects of this class will not be compatible with
 * future Swing releases. The current serialization support is
 * appropriate for short term storage or RMI between applications running
 * the same version of Swing.  As of 1.4, support for long term storage
 * of all JavaBeans<sup><font size="-2">TM</font></sup>
 * has been added to the <code>java.beans</code> package.
 * Please see {@link java.beans.XMLEncoder}.
 *
 * @author  Timothy Prinzing
 * @version 1.32 07/23/04
 */
public class BasicEditorPaneUI extends BasicTextUI {

    /**
     * Creates a UI for the JTextPane.
     *
     * @param c the JTextPane component
     * @return the UI
     */
    public static ComponentUI createUI(JComponent c) {
        return new BasicEditorPaneUI();
    }

    /**
     * Creates a new BasicEditorPaneUI.
     */
    public BasicEditorPaneUI() {
	super();
    }

    /**
     * Fetches the name used as a key to lookup properties through the
     * UIManager.  This is used as a prefix to all the standard
     * text properties.
     *
     * @return the name ("EditorPane")
     */
    protected String getPropertyPrefix() {
	return "EditorPane";
    }

    /**
     *{@inheritDoc}
     *
     * @since 1.5
     */
    public void installUI(JComponent c) {
        super.installUI(c);
        updateDisplayProperties(c.getFont(),
                                c.getForeground());
    }

    /**
     *{@inheritDoc}
     *
     * @since 1.5
     */
    public void uninstallUI(JComponent c) {
        cleanDisplayProperties();
        super.uninstallUI(c);
    }
    
    /**
     * Fetches the EditorKit for the UI.  This is whatever is
     * currently set in the associated JEditorPane.
     *
     * @return the editor capabilities
     * @see TextUI#getEditorKit
     */
    public EditorKit getEditorKit(JTextComponent tc) {
	JEditorPane pane = (JEditorPane) getComponent();
	return pane.getEditorKit();
    }

    /**
     * Fetch an action map to use.  The map for a JEditorPane
     * is not shared because it changes with the EditorKit.
     */
    ActionMap getActionMap() {
        ActionMap am = new ActionMapUIResource();
        am.put("requestFocus", new FocusAction());
	EditorKit editorKit = getEditorKit(getComponent());
	if (editorKit != null) {
	    Action[] actions = editorKit.getActions();
	    if (actions != null) {
		addActions(am, actions);
	    }
	}
        am.put(TransferHandler.getCutAction().getValue(Action.NAME),
                TransferHandler.getCutAction());
        am.put(TransferHandler.getCopyAction().getValue(Action.NAME),
                TransferHandler.getCopyAction());
        am.put(TransferHandler.getPasteAction().getValue(Action.NAME),
                TransferHandler.getPasteAction());
	return am;
    }

    /**
     * This method gets called when a bound property is changed
     * on the associated JTextComponent.  This is a hook
     * which UI implementations may change to reflect how the
     * UI displays bound properties of JTextComponent subclasses.
     * This is implemented to rebuild the ActionMap based upon an
     * EditorKit change.
     *
     * @param evt the property change event
     */
    protected void propertyChange(PropertyChangeEvent evt) {
        String name = evt.getPropertyName();
	if ("editorKit".equals(name)) {
	    ActionMap map = SwingUtilities.getUIActionMap(getComponent());
	    if (map != null) {
		Object oldValue = evt.getOldValue();
		if (oldValue instanceof EditorKit) {
		    Action[] actions = ((EditorKit)oldValue).getActions();
		    if (actions != null) {
			removeActions(map, actions);
		    }
		}
		Object newValue = evt.getNewValue();
		if (newValue instanceof EditorKit) {
		    Action[] actions = ((EditorKit)newValue).getActions();
		    if (actions != null) {
			addActions(map, actions);
		    }
		}
	    }
	    updateFocusTraversalKeys();
	} else if ("editable".equals(name)) {
	    updateFocusTraversalKeys();
	} else if ("foreground".equals(name)
                   || "font".equals(name)
                   || "document".equals(name)
                   || JEditorPane.W3C_LENGTH_UNITS.equals(name)
                   || JEditorPane.HONOR_DISPLAY_PROPERTIES.equals(name)
                   ) {
            JComponent c = getComponent();
            updateDisplayProperties(c.getFont(), c.getForeground());
            if ( JEditorPane.W3C_LENGTH_UNITS.equals(name)
                 || JEditorPane.HONOR_DISPLAY_PROPERTIES.equals(name) ) {
                modelChanged();
            }
            if ("foreground".equals(name)) {
                Object honorDisplayPropertiesObject = c.
                    getClientProperty(JEditorPane.HONOR_DISPLAY_PROPERTIES);
                boolean honorDisplayProperties = false;
                if (honorDisplayPropertiesObject instanceof Boolean) {
                    honorDisplayProperties = 
                        ((Boolean)honorDisplayPropertiesObject).booleanValue();
                }
                if (honorDisplayProperties) {
                    modelChanged();
                }
            }

               
        }
    }

    void removeActions(ActionMap map, Action[] actions) {
	int n = actions.length;
	for (int i = 0; i < n; i++) {
	    Action a = actions[i];
	    map.remove(a.getValue(Action.NAME));
	}
    }

    void addActions(ActionMap map, Action[] actions) {
	int n = actions.length;
	for (int i = 0; i < n; i++) {
	    Action a = actions[i];
	    map.put(a.getValue(Action.NAME), a);
	}
    }

    void updateDisplayProperties(Font font, Color fg) {
        JComponent c = getComponent();
        Object honorDisplayPropertiesObject = c.
            getClientProperty(JEditorPane.HONOR_DISPLAY_PROPERTIES);
        boolean honorDisplayProperties = false;
        Object w3cLengthUnitsObject = c.getClientProperty(JEditorPane.
                                                          W3C_LENGTH_UNITS);
        boolean w3cLengthUnits = false;
        if (honorDisplayPropertiesObject instanceof Boolean) {
            honorDisplayProperties = 
                ((Boolean)honorDisplayPropertiesObject).booleanValue();
        }
        if (w3cLengthUnitsObject instanceof Boolean) {
            w3cLengthUnits = ((Boolean)w3cLengthUnitsObject).booleanValue();
        }
        if (this instanceof BasicTextPaneUI
            || honorDisplayProperties) {
             //using equals because can not use UIResource for Boolean
            Document doc = getComponent().getDocument();
            if (doc instanceof StyledDocument) {
                if (doc instanceof HTMLDocument
                    && honorDisplayProperties) {
                    updateCSS(font, fg);
                } else {
                    updateStyle(font, fg);
                }
            }
        } else {
            cleanDisplayProperties();
        }
        if ( w3cLengthUnits ) {
            Document doc = getComponent().getDocument();
            if (doc instanceof HTMLDocument) {
                StyleSheet documentStyleSheet = 
                    ((HTMLDocument)doc).getStyleSheet();
                documentStyleSheet.addRule("W3C_LENGTH_UNITS_ENABLE");
            }
        } else {
            Document doc = getComponent().getDocument();
            if (doc instanceof HTMLDocument) {
                StyleSheet documentStyleSheet = 
                    ((HTMLDocument)doc).getStyleSheet();
                documentStyleSheet.addRule("W3C_LENGTH_UNITS_DISABLE");
            }

        }
    }

    void cleanDisplayProperties() {
        Document document = getComponent().getDocument();
        if (document instanceof HTMLDocument) {
            StyleSheet documentStyleSheet = 
                ((HTMLDocument)document).getStyleSheet();
            StyleSheet[] styleSheets = documentStyleSheet.getStyleSheets();
            if (styleSheets != null) {
                for (StyleSheet s : styleSheets) {
                    if (s instanceof StyleSheetUIResource) {
                        documentStyleSheet.removeStyleSheet(s);
                        documentStyleSheet.addRule("BASE_SIZE_DISABLE");
                        break;
                    }
                }
            }
        }
    }
    
    static class StyleSheetUIResource extends StyleSheet implements UIResource {
    }
    
    private void updateCSS(Font font, Color fg) {
        JTextComponent component = getComponent();
        Document document = component.getDocument();
        if (document instanceof HTMLDocument) {
            StyleSheet styleSheet = new StyleSheetUIResource();
            StyleSheet documentStyleSheet = 
                ((HTMLDocument)document).getStyleSheet();
            StyleSheet[] styleSheets = documentStyleSheet.getStyleSheets();
            if (styleSheets != null) {
                for (StyleSheet s : styleSheets) {
                    if (s instanceof StyleSheetUIResource) {
                        documentStyleSheet.removeStyleSheet(s);
                    }
                }
            }
            String cssRule = com.sun.java.swing.
                SwingUtilities2.displayPropertiesToCSS(font,
                                                       fg);
            styleSheet.addRule(cssRule);
            documentStyleSheet.addStyleSheet(styleSheet);
            documentStyleSheet.addRule("BASE_SIZE " + 
                                       component.getFont().getSize());
        }
    }

    private void updateStyle(Font font, Color fg) {
        updateFont(font);
        updateForeground(fg);
    }

    /**
     * Update the color in the default style of the document.
     *
     * @param color the new color to use or null to remove the color attribute
     *              from the document's style
     */
    private void updateForeground(Color color) {
        StyledDocument doc = (StyledDocument)getComponent().getDocument();
        Style style = doc.getStyle(StyleContext.DEFAULT_STYLE);

        if (style == null) {
            return;
        }

        if (color == null) {
            style.removeAttribute(StyleConstants.Foreground);
        } else {
            StyleConstants.setForeground(style, color);
        }
    }
    
    /**
     * Update the font in the default style of the document.
     *
     * @param font the new font to use or null to remove the font attribute
     *             from the document's style
     */
    private void updateFont(Font font) {
        StyledDocument doc = (StyledDocument)getComponent().getDocument();
        Style style = doc.getStyle(StyleContext.DEFAULT_STYLE);

        if (style == null) {
            return;
        }

        if (font == null) {
            style.removeAttribute(StyleConstants.FontFamily);
            style.removeAttribute(StyleConstants.FontSize);
            style.removeAttribute(StyleConstants.Bold);
            style.removeAttribute(StyleConstants.Italic);
        } else {
            StyleConstants.setFontFamily(style, font.getName());
            StyleConstants.setFontSize(style, font.getSize());
            StyleConstants.setBold(style, font.isBold());
            StyleConstants.setItalic(style, font.isItalic());
        }
    }
}


