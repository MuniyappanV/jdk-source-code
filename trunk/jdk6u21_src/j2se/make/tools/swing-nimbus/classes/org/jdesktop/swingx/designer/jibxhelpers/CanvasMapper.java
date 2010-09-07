/*
 * @(#)CanvasMapper.java	1.3 10/03/23
 *
 * Copyright (c) 2006, 2009, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package org.jdesktop.swingx.designer.jibxhelpers;

import org.jdesktop.swingx.designer.Canvas;
import org.jdesktop.swingx.designer.utils.HasPath;
import org.jdesktop.swingx.designer.utils.HasResources;
import org.jdesktop.swingx.designer.utils.HasUIDefaults;
import org.jibx.runtime.BindingDirectory;
import org.jibx.runtime.IBindingFactory;
import org.jibx.runtime.IMarshallable;
import org.jibx.runtime.IMarshaller;
import org.jibx.runtime.IMarshallingContext;
import org.jibx.runtime.IUnmarshaller;
import org.jibx.runtime.IUnmarshallingContext;
import org.jibx.runtime.JiBXException;
import org.jibx.runtime.impl.MarshallingContext;
import org.jibx.runtime.impl.UnmarshallingContext;

import javax.swing.UIDefaults;
import java.io.File;

/**
 * CanvasMapper
 *
 * @author Created by Jasper Potts (Jun 12, 2007)
 * @version 1.0
 */
public class CanvasMapper implements IMarshaller, IUnmarshaller {
    private static final String ELEMENT_NAME = "canvas";
    private IBindingFactory bindingFactory;


    public CanvasMapper() {
        try {
            bindingFactory = BindingDirectory.getFactory(Canvas.class);
        } catch (JiBXException e) {
            e.printStackTrace();
        }
    }

    public boolean isExtension(int i) {
        return false;
    }

    public boolean isPresent(IUnmarshallingContext iUnmarshallingContext) throws JiBXException {
        return iUnmarshallingContext.isAt(null, ELEMENT_NAME);
    }

    public void marshal(Object object, IMarshallingContext iMarshallingContext) throws JiBXException {
        if (!(object instanceof Canvas)) {
            throw new JiBXException("Invalid object type for marshaller");
        } else if (!(iMarshallingContext instanceof MarshallingContext)) {
            throw new JiBXException("Invalid object type for marshaller");
        } else {
            // version found, create marshaller for the associated binding
//            IBindingFactory bindingFactory = BindingDirectory.getFactory(object.getClass());
            MarshallingContext context = (MarshallingContext) bindingFactory.createMarshallingContext();
            // configure marshaller for writing document
            context.setXmlWriter(iMarshallingContext.getXmlWriter());
            // output object as document
            ((IMarshallable) object).marshal(context);
        }
    }

    public Object unmarshal(Object object, IUnmarshallingContext iUnmarshallingContext) throws JiBXException {
        // make sure we're at the appropriate start tag
        UnmarshallingContext ctx = (UnmarshallingContext) iUnmarshallingContext;
        if (!ctx.isAt(null, ELEMENT_NAME)) {
            ctx.throwStartTagNameError(null, ELEMENT_NAME);
        }

//        IBindingFactory bindingFactory = BindingDirectory.getFactory(Canvas.class);
        UnmarshallingContext uctx = (UnmarshallingContext) bindingFactory.createUnmarshallingContext();
        uctx.setFromContext(ctx);
        // get the uiDefaults from SynthModel and set them as user context
        UIDefaults uiDefaults = ((HasUIDefaults) ctx.getStackObject(ctx.getStackDepth() - 1)).getUiDefaults();
        uctx.setUserContext(uiDefaults);
        // get has resources
        HasResources hasResources = (HasResources) ctx.getStackObject(ctx.getStackDepth() - 1);
        // get path
        HasPath hasPath = null;
        for (int i = 0; i < ctx.getStackDepth(); i++) {
            if (ctx.getStackObject(i) instanceof HasPath) {
                hasPath = (HasPath) ctx.getStackObject(i);
                break;
            }
        }
        // Unmarshal the Canvas
        Canvas canvas = (Canvas) uctx.unmarshalElement();
        // set canvas's ui defaults
        canvas.setUiDefaults(uiDefaults);
        // get canvas path
        String canvasPath = hasPath.getPath();
        // calc and set resources
        canvas.setResourcesDir(new File(hasResources.getResourcesDir(), canvasPath));
        canvas.setTemplatesDir(new File(hasResources.getTemplatesDir(), canvasPath));
        canvas.setImagesDir(new File(hasResources.getImagesDir(), canvasPath));
        // return canvas
        return canvas;
    }
}
