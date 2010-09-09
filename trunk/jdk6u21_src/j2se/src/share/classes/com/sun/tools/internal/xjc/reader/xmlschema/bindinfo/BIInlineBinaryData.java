/*
 * Copyright (c) 1997, 2008, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms. 
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
package com.sun.tools.internal.xjc.reader.xmlschema.bindinfo;

import javax.xml.bind.annotation.XmlRootElement;
import javax.xml.bind.annotation.XmlInlineBinaryData;
import javax.xml.namespace.QName;

import com.sun.tools.internal.xjc.reader.Const;
import com.sun.tools.internal.xjc.reader.Ring;
import com.sun.tools.internal.xjc.reader.xmlschema.BGMBuilder;
import com.sun.tools.internal.xjc.model.CPropertyInfo;
import com.sun.xml.internal.xsom.XSComponent;

/**
 * Generates {@link @XmlInlineBinaryData}.
 * 
 * @author Kohsuke Kawaguchi
 */
@XmlRootElement(name="inlineBinaryData")
public class BIInlineBinaryData extends AbstractDeclarationImpl {

    /**
     * If the given component has {@link BIInlineBinaryData} customization,
     * reflect that to the specified property.
     */
    public static void handle(XSComponent source, CPropertyInfo prop) {
        BIInlineBinaryData inline = Ring.get(BGMBuilder.class).getBindInfo(source).get(BIInlineBinaryData.class);
        if(inline!=null) {
            prop.inlineBinaryData = true;
            inline.markAsAcknowledged();
        }
    }


    public final QName getName() { return NAME; }

    /** Name of the declaration. */
    public static final QName NAME = new QName(Const.JAXB_NSURI,"inlineBinaryData");
}
