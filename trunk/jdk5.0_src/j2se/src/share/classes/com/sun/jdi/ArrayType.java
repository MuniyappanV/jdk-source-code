/*
 * @(#)ArrayType.java	1.21 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.jdi;

import java.util.List;

/**
 * Provides access to the class of an array and the type of 
 * its components in the target VM. 
 *
 * @see ArrayReference
 *
 * @author Robert Field
 * @author Gordon Hirsch
 * @author James McIlree
 * @since  1.3
 */
public interface ArrayType extends ReferenceType {

    /**
     * Creates a new instance of this array class in the target VM.
     * The array is created with the given length and each component 
     * is initialized to is standard default value.
     *
     * @param length the number of components in the new array
     * @return the newly created {@link ArrayReference} mirroring
     * the new object in the target VM.
     *
     * @throws VMCannotBeModifiedException if the VirtualMachine is read-only - see {@link VirtualMachine#canBeModified()}.
     */
    ArrayReference newInstance(int length);

    /**
     * Gets the JNI signature of the components of this
     * array class. The signature 
     * describes the declared type of the components. If the components
     * are objects, their actual type in a particular run-time context
     * may be a subclass of the declared class.
     *
     * @return a string containing the JNI signature of array components.
     */
    String componentSignature();

    /**
     * Returns a text representation of the component
     * type of this array.
     *
     * @return a text representation of the component type.
     */
    String componentTypeName();

    /**
     * Returns the component type of this array,
     * as specified in the array declaration.
     * <P>
     * Note: The component type of a array will always be
     * created or loaded before the array - see the
     * <a href="http://java.sun.com/docs/books/vmspec/">Java Virtual
     * Machine Specification</a>, section
     * <a href="http://java.sun.com/docs/books/vmspec/2nd-edition/html/ConstantPool.doc.html#79473">5.3.3
     * Creating Array Classes</a>.
     * However, although the component type will be loaded it may 
     * not yet be prepared, in which case the type will be returned
     * but attempts to perform some operations on the returned type
     * (e.g. {@link ReferenceType#fields() fields()}) will throw
     * a {@link ClassNotPreparedException}.
     * Use {@link ReferenceType#isPrepared()} to determine if
     * a reference type is prepared.
     *
     * @see Type
     * @see Field#type() Field.type() - for usage examples
     * @return the {@link Type} of this array's components.
     */
    Type componentType() throws ClassNotLoadedException;
}

