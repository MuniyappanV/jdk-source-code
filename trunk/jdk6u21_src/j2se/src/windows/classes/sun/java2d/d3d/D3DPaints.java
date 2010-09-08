/*
 * @(#)D3DPaints.java	1.3 10/03/23
 *
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.java2d.d3d;

import java.awt.LinearGradientPaint;
import java.awt.MultipleGradientPaint;
import java.awt.MultipleGradientPaint.ColorSpaceType;
import java.awt.MultipleGradientPaint.CycleMethod;
import java.awt.RadialGradientPaint;
import java.awt.TexturePaint;
import java.awt.image.BufferedImage;
import java.util.HashMap;
import java.util.Map;
import sun.java2d.SunGraphics2D;
import sun.java2d.SurfaceData;
import sun.java2d.loops.CompositeType;
import sun.java2d.pipe.BufferedPaints;
import static sun.java2d.pipe.BufferedPaints.*;
import static sun.java2d.d3d.D3DContext.D3DContextCaps.*;

abstract class D3DPaints {

    /**
     * Holds all registered implementations, using the corresponding
     * SunGraphics2D.PAINT_* constant as the hash key.
     */
    private static Map<Integer, D3DPaints> impls =
        new HashMap<Integer, D3DPaints>(4, 1.0f);

    static {
        impls.put(SunGraphics2D.PAINT_GRADIENT, new Gradient());
        impls.put(SunGraphics2D.PAINT_LIN_GRADIENT, new LinearGradient());
        impls.put(SunGraphics2D.PAINT_RAD_GRADIENT, new RadialGradient());
        impls.put(SunGraphics2D.PAINT_TEXTURE, new Texture());
    }

    /**
     * Attempts to locate an implementation corresponding to the paint state
     * of the provided SunGraphics2D object.  If no implementation can be
     * found, or if the paint cannot be accelerated under the conditions
     * of the SunGraphics2D, this method returns false; otherwise, returns
     * true.
     */
    static boolean isValid(SunGraphics2D sg2d) {
        D3DPaints impl = impls.get(sg2d.paintState);
        return (impl != null && impl.isPaintValid(sg2d));
    }

    /**
     * Returns true if this implementation is able to accelerate the
     * Paint object associated with, and under the conditions of, the
     * provided SunGraphics2D instance; otherwise returns false.
     */
    abstract boolean isPaintValid(SunGraphics2D sg2d);

/************************* GradientPaint support ****************************/

    private static class Gradient extends D3DPaints {
        private Gradient() {}

        /**
         * Returns true if the given GradientPaint instance can be
         * used by the accelerated D3DPaints.Gradient implementation.
         * A GradientPaint is considered valid only if the destination
         * has support for fragment shaders.
         */
        @Override
        boolean isPaintValid(SunGraphics2D sg2d) {
            D3DSurfaceData dstData = (D3DSurfaceData)sg2d.surfaceData;
            D3DGraphicsDevice gd = (D3DGraphicsDevice)
                dstData.getDeviceConfiguration().getDevice();
            return gd.isCapPresent(CAPS_LCD_SHADER);
        }
    }

/************************** TexturePaint support ****************************/

    private static class Texture extends D3DPaints {
        private Texture() {}

        /**
         * Returns true if the given TexturePaint instance can be used by the
         * accelerated BufferedPaints.Texture implementation.
         * 
         * A TexturePaint is considered valid if the following conditions
         * are met:
         *   - the texture image dimensions are power-of-two
         *   - the texture image can be (or is already) cached in a D3D
         *     texture object
         */
        @Override
        public boolean isPaintValid(SunGraphics2D sg2d) {
            TexturePaint paint = (TexturePaint)sg2d.paint;
            D3DSurfaceData dstData = (D3DSurfaceData)sg2d.surfaceData;
            BufferedImage bi = paint.getImage();

            // verify that the texture image dimensions are pow2
            D3DGraphicsDevice gd =
                (D3DGraphicsDevice)dstData.getDeviceConfiguration().getDevice();
            int imgw = bi.getWidth();
            int imgh = bi.getHeight();
            if (!gd.isCapPresent(CAPS_TEXNONPOW2)) {
                if ((imgw & (imgw - 1)) != 0 || (imgh & (imgh - 1)) != 0) {
                    return false;
                }
            }
            // verify that the texture image is square if it has to be
            if (!gd.isCapPresent(CAPS_TEXNONSQUARE) && imgw != imgh)
            {
                return false;
            }

            SurfaceData srcData =
                SurfaceData.getSourceSurfaceData(bi, dstData,
                                                 CompositeType.SrcOver,
                                                 null, false);
            if (!(srcData instanceof D3DSurfaceData)) {
                // REMIND: this is a hack that attempts to cache the system
                //         memory image from the TexturePaint instance into a
                //         D3D texture...
                srcData =
                    SurfaceData.getSourceSurfaceData(bi, dstData,
                                                     CompositeType.SrcOver,
                                                     null, false);
                if (!(srcData instanceof D3DSurfaceData)) {
                    return false;
                }
            }

            // verify that the source surface is actually a texture
            D3DSurfaceData d3dData = (D3DSurfaceData)srcData;
            if (d3dData.getType() != D3DSurfaceData.TEXTURE) {
                return false;
            }

            return true;
        }
    }

/****************** Shared MultipleGradientPaint support ********************/

    private static abstract class MultiGradient extends D3DPaints {

        /**
         * Note that this number is lower than the MULTI_MAX_FRACTIONS
         * defined in the superclass.  The D3D pipeline now uses a
         * slightly more complicated shader (to avoid the gradient banding
         * issues), which has a higher instruction count.  To ensure that
         * all versions of the shader can be compiled for PS 2.0 hardware,
         * we need to cap this maximum value at 8.
         */
        public static final int MULTI_MAX_FRACTIONS_D3D = 8;

        protected MultiGradient() {}

        /**
         * Returns true if the given MultipleGradientPaint instance can be
         * used by the accelerated D3DPaints.MultiGradient implementation.
         * A MultipleGradientPaint is considered valid if the following
         * conditions are met:
         *   - the number of gradient "stops" is <= MAX_FRACTIONS
         *   - the destination has support for fragment shaders
         */
        @Override
        boolean isPaintValid(SunGraphics2D sg2d) {
            MultipleGradientPaint paint = (MultipleGradientPaint)sg2d.paint;
            // REMIND: ugh, this creates garbage; would be nicer if
            // we had a MultipleGradientPaint.getNumStops() method...
            if (paint.getFractions().length > MULTI_MAX_FRACTIONS_D3D) {
                return false;
            }

            D3DSurfaceData dstData = (D3DSurfaceData)sg2d.surfaceData;
            D3DGraphicsDevice gd = (D3DGraphicsDevice)
                dstData.getDeviceConfiguration().getDevice();
            if (!gd.isCapPresent(CAPS_LCD_SHADER)) {
                return false;
            }
            return true;
        }
    }

/********************** LinearGradientPaint support *************************/

    private static class LinearGradient extends MultiGradient {
        private LinearGradient() {}

        @Override
        boolean isPaintValid(SunGraphics2D sg2d) {
            LinearGradientPaint paint = (LinearGradientPaint)sg2d.paint;

            if (paint.getFractions().length == 2 &&
                paint.getCycleMethod() != CycleMethod.REPEAT &&
                paint.getColorSpace() != ColorSpaceType.LINEAR_RGB)
            {
                D3DSurfaceData dstData = (D3DSurfaceData)sg2d.surfaceData;
                D3DGraphicsDevice gd = (D3DGraphicsDevice)
                    dstData.getDeviceConfiguration().getDevice();
                if (gd.isCapPresent(CAPS_LCD_SHADER)) {
                    // we can delegate to the optimized two-color gradient
                    // codepath, which should be faster
                    return true;
                }
            }

            return super.isPaintValid(sg2d);
        }
    }

/********************** RadialGradientPaint support *************************/

    private static class RadialGradient extends MultiGradient {
        private RadialGradient() {}
    }
}
