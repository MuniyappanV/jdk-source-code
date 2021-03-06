/*
 * @(#)MPanelPeer.java	1.37 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package sun.awt.motif;

import java.awt.*;
import java.awt.peer.*;

import sun.awt.SunGraphicsCallback;

class MPanelPeer extends MCanvasPeer implements PanelPeer {

    MPanelPeer() {}

    MPanelPeer(Component target) {
	super(target);
    }

    MPanelPeer(Component target, Object arg) {
	super(target, arg);
    }

    public Insets getInsets() {
	return new Insets(0, 0, 0, 0);
    }

    public void paint(Graphics g) {
        super.paint(g);
        SunGraphicsCallback.PaintHeavyweightComponentsCallback.getInstance().
            runComponents(((Container)target).getComponents(), g,
                          SunGraphicsCallback.LIGHTWEIGHTS |
                          SunGraphicsCallback.HEAVYWEIGHTS);
    }
    public void print(Graphics g) {
        super.print(g);
        SunGraphicsCallback.PrintHeavyweightComponentsCallback.getInstance().
            runComponents(((Container)target).getComponents(), g,
                          SunGraphicsCallback.LIGHTWEIGHTS |
                          SunGraphicsCallback.HEAVYWEIGHTS);
    }

    public void setBackground(Color c) {
	Component comp;
	int i;

	Container cont = (Container) target;
	synchronized(target.getTreeLock()) {
	    int n = cont.getComponentCount();
	    for(i=0; i < n; i++) {
	        comp = cont.getComponent(i);
	        MComponentPeer peer = (MComponentPeer) MToolkit.targetToPeer(comp);
	        if (peer != null) {
		    Color color = comp.getBackground();
                    if (color == null || color.equals(c)) {
			peer.setBackground(c);
		        peer.pSetBackground(c);
		    }
		    if ((comp instanceof java.awt.List) ||
			   (comp instanceof java.awt.TextArea) ||
			   (comp instanceof java.awt.ScrollPane)) {
		        peer.pSetScrollbarBackground(c);
		    }
 		}
	    }
	}
	pSetBackground(c);
    }

    public void setForeground(Color c) {
	Component comp;
	int i;

	Container cont = (Container) target;
	synchronized(target.getTreeLock()) {
	    int n = cont.getComponentCount();
	    for(i=0; i < n; i++) {
	        comp = cont.getComponent(i);
	        MComponentPeer peer = (MComponentPeer) MToolkit.targetToPeer(comp);
	        if (peer != null) {
		    Color color = comp.getForeground();
                    if (color == null || color.equals(c)) {
			peer.setForeground(c);
		        peer.pSetForeground(c);
		    }
		    if ((comp instanceof java.awt.List) ||
			   (comp instanceof java.awt.TextArea) ||
			   (comp instanceof java.awt.ScrollPane)) {
		        peer.pSetInnerForeground(c);
		    }
 		}
	    }
	}
	pSetForeground(c);
    }

    /**
     * DEPRECATED:  Replaced by getInsets().
     */
    public Insets insets() {
	return getInsets();
    }

    /**
     * Recursive method that handles the propagation of the displayChanged
     * event into the entire hierarchy of peers.
     * Unlike on win32, on X we don't worry about handling on-the-fly
     * display settings changes, only windows being dragged across Xinerama
     * screens.  Thus, we only need to tell MCanvasPeers, not all
     * MComponentPeers.
     */
     private void recursiveDisplayChanged(Component c, int screenNum) {
        if (c instanceof Container) {
            Component children[] = ((Container)c).getComponents();
            for (int i = 0; i < children.length; ++i) {
                recursiveDisplayChanged(children[i], screenNum);
            }
        }
        ComponentPeer peer = c.getPeer();
        if (peer != null && peer instanceof MCanvasPeer) {
            MCanvasPeer mPeer = (MCanvasPeer)peer;
            mPeer.displayChanged(screenNum);
        }
    }

    /*
     * Often up-called from a MWindowPeer instance.
     * Calls displayChanged() on all child canvas' peers.
     * Recurses into Container children to ensure all canvases
     * get the message.
     */
    public void displayChanged(int screenNum) {
       // Don't do super call because MWindowPeer has already updated its GC

       Component children[] = ((Container)target).getComponents();
        
       for (int i = 0; i < children.length; i++) {
           recursiveDisplayChanged(children[i], screenNum);
       }
   }

    protected boolean shouldFocusOnClick() {
        // Return false if this container has children so in that case it won't
        // be focused. Return true otherwise.
        return ((Container)target).getComponentCount() == 0;
    }

    private native void pEnsureIndex(ComponentPeer child, int index);
    private native void pRestack();

    private int restack(Container cont, int ind) {
        for (int i = 0; i < cont.getComponentCount(); i++) {
            Component comp = cont.getComponent(i);
            if (!comp.isLightweight()) {
                if (comp.getPeer() != null) {
                    pEnsureIndex(comp.getPeer(), ind++);
                }
            }
            if (comp.isLightweight() && comp instanceof Container) {
                ind = restack((Container)comp, ind);
            }
        }
        return ind;
    }

    /**
     * @see java.awt.peer.ContainerPeer#restack
     */
    public void restack() {
        Container cont = (Container)target;
        restack(cont, 0);
        pRestack();
    }

    /**
     * @see java.awt.peer.ContainerPeer#isRestackSupported
     */
    public boolean isRestackSupported() {
        return true;
    }
}

