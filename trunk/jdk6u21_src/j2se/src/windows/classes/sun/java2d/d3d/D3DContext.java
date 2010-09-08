/*
 * @(#)D3DContext.java	1.6 10/03/23
 *
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.java2d.d3d;

import sun.java2d.pipe.BufferedContext;
import sun.java2d.pipe.RenderBuffer;
import sun.java2d.pipe.RenderQueue;
import sun.java2d.pipe.hw.ContextCapabilities;
import static sun.java2d.pipe.BufferedOpCodes.*;
import static sun.java2d.pipe.hw.ContextCapabilities.*;
import static sun.java2d.d3d.D3DContext.D3DContextCaps.*;

/**
 * Note that the RenderQueue lock must be acquired before calling any of
 * the methods in this class.
 */
class D3DContext extends BufferedContext {

    private final D3DGraphicsDevice device;

    D3DContext(RenderQueue rq, D3DGraphicsDevice device) {
        super(rq);
        this.device = device;
    }

    /**
     * Invalidates the currentContext field to ensure that we properly
     * revalidate the D3DContext (make it current, etc.) next time through
     * the validate() method.  This is typically invoked from methods
     * that affect the current context state (e.g. disposing a context or
     * surface).
     */
    static void invalidateCurrentContext() {
        // assert D3DRenderQueue.getInstance().lock.isHeldByCurrentThread();

        // invalidate the current Java-level context so that we
        // revalidate everything the next time around
        if (currentContext != null) {
            currentContext.invalidateContext();
            currentContext = null;
        }

        // invalidate the context reference at the native level, and
        // then flush the queue so that we have no pending operations
        // dependent on the current context
        D3DRenderQueue rq = D3DRenderQueue.getInstance();
        rq.ensureCapacity(4);
        rq.getBuffer().putInt(INVALIDATE_CONTEXT);
        rq.flushNow();
    }

    /**
     * Sets the current context on the native level to be the one passed as
     * the argument.
     * If the context is not the same as the defaultContext the latter
     * will be reset to null.
     *
     * This call is needed when copying from a SW surface to a Texture
     * (the upload test) or copying from d3d to SW surface to make sure we
     * have the correct current context.
     *
     * @param d3dc the context to be made current on the native level
     */
    static void setScratchSurface(D3DContext d3dc) {
        // assert D3DRenderQueue.getInstance().lock.isHeldByCurrentThread();

        // invalidate the current context
        if (d3dc != currentContext) {
            currentContext = null;
        }

        // set the scratch context
        D3DRenderQueue rq = D3DRenderQueue.getInstance();
        RenderBuffer buf = rq.getBuffer();
        rq.ensureCapacity(8);
        buf.putInt(SET_SCRATCH_SURFACE);
        buf.putInt(d3dc.getDevice().getScreen());
    }

    public RenderQueue getRenderQueue() {
        return D3DRenderQueue.getInstance();
    }

    @Override
    public void saveState() {
        // assert rq.lock.isHeldByCurrentThread();

        // reset all attributes of this and current contexts
        invalidateContext();
        invalidateCurrentContext();

        setScratchSurface(this);

        // save the state on the native level
        rq.ensureCapacity(4);
        buf.putInt(SAVE_STATE);
        rq.flushNow();
    }

    @Override
    public void restoreState() {
        // assert rq.lock.isHeldByCurrentThread();

        // reset all attributes of this and current contexts
        invalidateContext();
        invalidateCurrentContext();

        setScratchSurface(this);

        // restore the state on the native level
        rq.ensureCapacity(4);
        buf.putInt(RESTORE_STATE);
        rq.flushNow();
    }

    D3DGraphicsDevice getDevice() {
        return device;
    }

    static class D3DContextCaps extends ContextCapabilities {
        /**
         * Indicates the presence of pixel shaders (v2.0 or greater).
         * This cap will only be set if the hardware supports the minimum number
         * of texture units.
         */
        static final int CAPS_LCD_SHADER       = (FIRST_PRIVATE_CAP << 0);
        /**
         * Indicates the presence of pixel shaders (v2.0 or greater).
         * This cap will only be set if the hardware meets our
         * minimum requirements.
         */
        static final int CAPS_BIOP_SHADER      = (FIRST_PRIVATE_CAP << 1);
        /**
         * Indicates that the device was successfully initialized and can
         * be safely used.
         */
        static final int CAPS_DEVICE_OK        = (FIRST_PRIVATE_CAP << 2);
        /**
         * Indicates that the device has all of the necessary capabilities
         * to support the Antialiasing Pixel Shader program.
         */
        static final int CAPS_AA_SHADER        = (FIRST_PRIVATE_CAP << 3);

        D3DContextCaps(int caps, String adapterId) {
            super(caps, adapterId);
        }

        @Override
        public String toString() {
            StringBuffer buf = new StringBuffer(super.toString());
            if ((caps & CAPS_LCD_SHADER) != 0) {
                buf.append("CAPS_LCD_SHADER|");
            }
            if ((caps & CAPS_BIOP_SHADER) != 0) {
                buf.append("CAPS_BIOP_SHADER|");
            }
            if ((caps & CAPS_AA_SHADER) != 0) {
                buf.append("CAPS_AA_SHADER|");
            }
            if ((caps & CAPS_DEVICE_OK) != 0) {
                buf.append("CAPS_DEVICE_OK|");
            }
            return buf.toString();
        }
    }
}
