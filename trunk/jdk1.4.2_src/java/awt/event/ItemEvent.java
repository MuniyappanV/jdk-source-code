/*
 * @(#)ItemEvent.java	1.26 03/01/23
 *
 * Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.awt.event;

import java.awt.Component;
import java.awt.AWTEvent;
import java.awt.Event;
import java.awt.ItemSelectable;

/**
 * A semantic event which indicates that an item was selected or deselected.
 * This high-level event is generated by an ItemSelectable object (such as a 
 * List) when an item is selected or deselected by the user.
 * The event is passed to every <code>ItemListener</code> object which
 * registered to receive such events using the component's
 * <code>addItemListener</code> method. 
 * <P>
 * The object that implements the <code>ItemListener</code> interface gets
 * this <code>ItemEvent</code> when the event occurs. The listener is
 * spared the details of processing individual mouse movements and mouse
 * clicks, and can instead process a "meaningful" (semantic) event like
 * "item selected" or "item deselected". 
 *
 * @version 1.26 01/23/03
 * @author Carl Quinn
 *
 * @see java.awt.ItemSelectable
 * @see ItemListener
 * @see <a href="http://java.sun.com/docs/books/tutorial/post1.0/ui/itemlistener.html">Tutorial: Writing an Item Listener</a>
 * @see <a href="http://www.awl.com/cp/javaseries/jcl1_2.html">Reference: The Java Class Libraries (update file)</a>
 *
 * @since 1.1
 */
public class ItemEvent extends AWTEvent {

    /**
     * The first number in the range of ids used for item events.
     */
    public static final int ITEM_FIRST		= 701;

    /**
     * The last number in the range of ids used for item events.
     */
    public static final int ITEM_LAST		= 701;

    /** 
     * This event id indicates that an item's state changed.
     */
    public static final int ITEM_STATE_CHANGED	= ITEM_FIRST; //Event.LIST_SELECT 

    /**
     * This state-change value indicates that an item was selected.
     */
    public static final int SELECTED = 1;

    /** 
     * This state-change-value indicates that a selected item was deselected.
     */
    public static final int DESELECTED	= 2;

    /**
     * The item whose selection state has changed.
     *
     * @serial
     * @see #getItem()
     */
    Object item;

    /**
     * <code>stateChange</code> indicates whether the <code>item</code>
     * was selected or deselected.
     *
     * @serial
     * @see #getStateChange()
     */
    int stateChange;

    /*
     * JDK 1.1 serialVersionUID 
     */
    private static final long serialVersionUID = -608708132447206933L;

    /**
     * Constructs an <code>ItemEvent</code> object.
     * <p>Note that passing in an invalid <code>id</code> results in
     * unspecified behavior.
     *
     * @param source the <code>ItemSelectable</code> object
     *               that originated the event
     * @param id     an integer that identifies the event type
     * @param item   an object -- the item affected by the event
     * @param stateChange 
     *               an integer that indicates whether the item was
     *               selected or deselected
     */
    public ItemEvent(ItemSelectable source, int id, Object item, int stateChange) {
        super(source, id);
	this.item = item;
        this.stateChange = stateChange;
    }

    /**
     * Returns the originator of the event.
     *
     * @return the ItemSelectable object that originated the event.
     */
    public ItemSelectable getItemSelectable() {
        return (ItemSelectable)source;
    }

   /**
    * Returns the item affected by the event.
    *
    * @return the item (object) that was affected by the event
    */
    public Object getItem() {
        return item;
    }

   /**
    * Returns the type of state change (selected or deselected).
    *
    * @return an integer that indicates whether the item was selected
    *         or deselected
    *
    * @see #SELECTED
    * @see #DESELECTED
    */
    public int getStateChange() {
        return stateChange;
    }

    /**
     * Returns a parameter string identifying this item event.
     * This method is useful for event-logging and for debugging.
     *
     * @return a string identifying the event and its attributes
     */
    public String paramString() {
        String typeStr;
        switch(id) {
          case ITEM_STATE_CHANGED:
              typeStr = "ITEM_STATE_CHANGED";
              break;
          default:
              typeStr = "unknown type";
        }

        String stateStr;
        switch(stateChange) {
          case SELECTED:
              stateStr = "SELECTED";
              break;
          case DESELECTED:
              stateStr = "DESELECTED";
              break;
          default:
              stateStr = "unknown type";
        }
        return typeStr + ",item="+item + ",stateChange="+stateStr;
    }

}
