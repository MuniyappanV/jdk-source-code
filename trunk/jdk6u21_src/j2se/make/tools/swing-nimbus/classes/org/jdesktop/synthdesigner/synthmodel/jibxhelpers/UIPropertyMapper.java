/*
 * @(#)UIPropertyMapper.java	1.4 10/03/23
 *
 * Copyright (c) 2006, 2009, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
package org.jdesktop.synthdesigner.synthmodel.jibxhelpers;

import org.jdesktop.swingx.designer.jibxhelpers.DimensionMapper;
import org.jdesktop.swingx.designer.jibxhelpers.InsetsMapper;
import org.jdesktop.synthdesigner.synthmodel.UIProperty;
import org.jibx.runtime.IMarshallable;
import org.jibx.runtime.IMarshaller;
import org.jibx.runtime.IMarshallingContext;
import org.jibx.runtime.IUnmarshaller;
import org.jibx.runtime.IUnmarshallingContext;
import org.jibx.runtime.JiBXException;
import org.jibx.runtime.impl.MarshallingContext;
import org.jibx.runtime.impl.UnmarshallingContext;

/**
 * UIPropertyMapper
 *
 * @author Created by Jasper Potts (Jul 10, 2007)
 * @version 1.0
 */
public class UIPropertyMapper implements IMarshaller, IUnmarshaller {
    private static final String ELEMENT_NAME = "uiProperty";
    private static final String NAME_NAME = "name";
    private static final String TYPE_NAME = "type";
    private static final String VALUE_NAME = "value";

    public boolean isExtension(int i) {
        return false;
    }

    public boolean isPresent(IUnmarshallingContext iUnmarshallingContext) throws JiBXException {
        return iUnmarshallingContext.isAt(null, ELEMENT_NAME);
    }

    public void marshal(Object object, IMarshallingContext iMarshallingContext) throws JiBXException {
        if (!(object instanceof UIProperty)) {
            throw new JiBXException("Invalid object type for marshaller");
        } else if (!(iMarshallingContext instanceof MarshallingContext)) {
            throw new JiBXException("Invalid object type for marshaller");
        } else {
            MarshallingContext ctx = (MarshallingContext) iMarshallingContext;
            UIProperty property = (UIProperty) object;
            ctx.startTagAttributes(0, ELEMENT_NAME);
            ctx.attribute(0, NAME_NAME, property.getName());
            ctx.attribute(0, TYPE_NAME, property.getType().toString());
            switch (property.getType()) {
                case BOOLEAN:
                case DOUBLE:
                case INT:
                case FLOAT:
                case STRING:
                    ctx.attribute(0, VALUE_NAME, property.getValue().toString());
                    ctx.closeStartEmpty();
                    break;
                case INSETS:
                    ctx.closeStartContent();
                    new InsetsMapper().marshal(property.getValue(), ctx);
                    ctx.endTag(0, ELEMENT_NAME);
                    break;
                case COLOR:
                case FONT:
                    ctx.closeStartContent();
                    if (property.getValue() instanceof IMarshallable) {
                        ((IMarshallable) property.getValue()).marshal(ctx);
                    } else {
                        throw new JiBXException("Mapped value is not marshallable");
                    }
                    ctx.endTag(0, ELEMENT_NAME);
                    break;
                case DIMENSION:
                    ctx.closeStartContent();
                    new DimensionMapper().marshal(property.getValue(), ctx);
                    ctx.endTag(0, ELEMENT_NAME);
                    break;
                case BORDER:
                    ctx.closeStartContent();
                    new BorderMapper().marshal(property.getValue(), ctx);
                    ctx.endTag(0, ELEMENT_NAME);
                    break;
            }
        }
    }

    public Object unmarshal(Object object, IUnmarshallingContext iUnmarshallingContext) throws JiBXException {
        // make sure we're at the appropriate start tag
        UnmarshallingContext ctx = (UnmarshallingContext) iUnmarshallingContext;
        if (!ctx.isAt(null, ELEMENT_NAME)) {
            ctx.throwStartTagNameError(null, ELEMENT_NAME);
        }
        // get values
        Object value = null;
        String name = ctx.attributeText(null, NAME_NAME, null);
        UIProperty.PropertyType type = UIProperty.PropertyType.valueOf(ctx.attributeText(null, TYPE_NAME, null));
        switch (type) {
            case BOOLEAN:
                value = Boolean.parseBoolean(ctx.attributeText(null, VALUE_NAME, null));
                break;
            case DOUBLE:
                value = Double.parseDouble(ctx.attributeText(null, VALUE_NAME, null));
                break;
            case INT:
                value = Integer.parseInt(ctx.attributeText(null, VALUE_NAME, null));
                break;
            case FLOAT:
                value = Float.parseFloat(ctx.attributeText(null, VALUE_NAME, null));
                break;
            case STRING:
                value = ctx.attributeText(null, VALUE_NAME, null);
                break;
            case INSETS:
                ctx.parsePastStartTag(null, ELEMENT_NAME);
                value = new InsetsMapper().unmarshal(value, ctx);
                break;
            case COLOR:
            case FONT:
                ctx.parsePastStartTag(null, ELEMENT_NAME);
                value = ctx.unmarshalElement();
                break;
            case DIMENSION:
                ctx.parsePastStartTag(null, ELEMENT_NAME);
                value = new DimensionMapper().unmarshal(value, ctx);
                break;
            case BORDER:
                ctx.parsePastStartTag(null, ELEMENT_NAME);
                value = new BorderMapper().unmarshal(value, ctx);
                break;
        }
        ctx.parsePastEndTag(null, ELEMENT_NAME);
        // create
        return new UIProperty(name, type, value);
    }
}
