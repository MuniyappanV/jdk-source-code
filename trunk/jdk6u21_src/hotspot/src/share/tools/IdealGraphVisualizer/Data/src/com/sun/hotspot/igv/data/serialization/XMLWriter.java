/*
 * Copyright (c) 1998, 2008, Oracle and/or its affiliates. All rights reserved.
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
package com.sun.hotspot.igv.data.serialization;

import com.sun.hotspot.igv.data.Properties;
import com.sun.hotspot.igv.data.Property;
import java.io.IOException;
import java.io.Writer;
import java.util.Stack;

/**
 *
 * @author Thomas Wuerthinger
 */
public class XMLWriter extends Writer {

    private Writer inner;
    private Stack<String> elementStack;

    public XMLWriter(Writer inner) {
        this.inner = inner;
        elementStack = new Stack<String>();
    }

    @Override
    public void write(char[] arr) throws IOException {
        write(arr, 0, arr.length);
    }

    public void write(char[] cbuf, int off, int len) throws IOException {
        for (int i = off; i < off + len; i++) {
            char c = cbuf[i];
            if (c == '>') {
                inner.write("&gt;");
            } else if (c == '<') {
                inner.write("&lt;");
            } else if (c == '&') {
                inner.write("&amp;");
            } else {
                inner.write(c);
            }
        }
    }

    public void flush() throws IOException {
        inner.flush();
    }

    public void close() throws IOException {
        inner.close();
    }

    public void endTag() throws IOException {
        inner.write("</" + elementStack.pop() + ">\n");
    }

    public void startTag(String name) throws IOException {
        inner.write("<" + name + ">\n");
        elementStack.push(name);
    }

    public void simpleTag(String name) throws IOException {
        inner.write("<" + name + "/>\n");
    }

    public void startTag(String name, Properties attributes) throws IOException {
        inner.write("<" + name);
        elementStack.push(name);

        for (Property p : attributes) {
            inner.write(" " + p.getName() + "=\"");
            write(p.getValue().toCharArray());
            inner.write("\"");
        }

        inner.write(">\n");
    }

    public void simpleTag(String name, Properties attributes) throws IOException {
        inner.write("<" + name);

        for (Property p : attributes) {
            inner.write(" " + p.getName() + "=\"");
            write(p.getValue().toCharArray());
            inner.write("\"");
        }

        inner.write("/>\n");
    }

    public void writeProperties(Properties props) throws IOException {
        if (props.getProperties().hasNext() == false) {
            return;
        }

        startTag(Parser.PROPERTIES_ELEMENT);

        for (Property p : props) {
            startTag(Parser.PROPERTY_ELEMENT, new Properties(Parser.PROPERTY_NAME_PROPERTY, p.getName()));
            this.write(p.getValue().toCharArray());
            endTag();
        }

        endTag();
    }
}
