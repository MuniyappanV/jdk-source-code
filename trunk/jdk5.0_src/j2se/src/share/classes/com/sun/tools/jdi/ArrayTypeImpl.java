/*
 * @(#)ArrayTypeImpl.java	1.29 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package com.sun.tools.jdi;

import com.sun.jdi.*;

import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Map;

public class ArrayTypeImpl extends ReferenceTypeImpl
    implements ArrayType
{
    protected ArrayTypeImpl(VirtualMachine aVm, long aRef) {
        super(aVm, aRef);
    }

    public ArrayReference newInstance(int length) {
        try {
            return (ArrayReference)JDWP.ArrayType.NewInstance.
                                       process(vm, this, length).newArray;
        } catch (JDWPException exc) {
            throw exc.toJDIException();
        }
    }

    public String componentSignature() {
        return signature().substring(1); // Just skip the leading '['
    }

    public String componentTypeName() {
        JNITypeParser parser = new JNITypeParser(componentSignature());
        return parser.typeName();
    }

    Type type() throws ClassNotLoadedException {
        return findType(componentSignature());
    }

    void addVisibleMethods(Map map) {
        // arrays don't have methods
    }

    public List allMethods() {
        return new ArrayList(0);   // arrays don't have methods
    }

    /*
     * Find the type object, if any, of a component type of this array. 
     * The component type does not have to be immediate; e.g. this method
     * can be used to find the component Foo of Foo[][]. This method takes
     * advantage of the property that an array and its component must have
     * the same class loader. Since array set operations don't have an
     * implicit enclosing type like field and variable set operations, 
     * this method is sometimes needed for proper type checking.
     */
    Type findComponentType(String signature) throws ClassNotLoadedException {
        byte tag = (byte)signature.charAt(0); 
        if (PacketStream.isObjectTag(tag)) {
            // It's a reference type
            JNITypeParser parser = new JNITypeParser(componentSignature());
            List list = vm.classesByName(parser.typeName());
            Iterator iter = list.iterator();
            while (iter.hasNext()) {
                ReferenceType type = (ReferenceType)iter.next();
                ClassLoaderReference cl = type.classLoader();
                if ((cl == null)? 
                         (classLoader() == null) : 
                         (cl.equals(classLoader()))) {
                    return type;
                }
            }
            // Component class has not yet been loaded
            throw new ClassNotLoadedException(componentTypeName());
        } else {
            // It's a primitive type
            return vm.primitiveTypeMirror(tag);
        }
    }

    public Type componentType() throws ClassNotLoadedException {
        return findComponentType(componentSignature());
    }

    static boolean isComponentAssignable(Type destination, Type source) {
        if (source instanceof PrimitiveType) {
            // Assignment of primitive arrays requires identical
            // component types.
            return source.equals(destination);
        } else {
            if (destination instanceof PrimitiveType) {
                return false;
            }
            
            ReferenceTypeImpl refSource = (ReferenceTypeImpl)source;
            ReferenceTypeImpl refDestination = (ReferenceTypeImpl)destination;
            // Assignment of object arrays requires availability
            // of widening conversion of component types
            return refSource.isAssignableTo(refDestination);
        }
    }

    /*
     * Return true if an instance of the  given reference type 
     * can be assigned to a variable of this type
     */
    boolean isAssignableTo(ReferenceType destType) {
        if (destType instanceof ArrayType) {
            try {
                Type destComponentType = ((ArrayType)destType).componentType();
                return isComponentAssignable(destComponentType, componentType());
            } catch (ClassNotLoadedException e) {
                // One or both component types has not yet been 
                // loaded => can't assign
                return false;
            }
        } else if (destType instanceof InterfaceType) {
            // Only valid InterfaceType assignee is Cloneable
            return destType.name().equals("java.lang.Cloneable");
        } else {
            // Only valid ClassType assignee is Object
            return destType.name().equals("java.lang.Object");
        }
    }

    List inheritedTypes() {
        return new ArrayList(0);
    }

    void getModifiers() {
        if (modifiers != -1) {
            return;
        }
        /*
         * For object arrays, the return values for Interface
         * Accessible.isPrivate(), Accessible.isProtected(),
         * etc... are the same as would be returned for the
         * component type.  Fetch the modifier bits from the
         * component type and use those.
         *
         * For primitive arrays, the modifiers are always
         *   VMModifiers.FINAL | VMModifiers.PUBLIC
         *
         * Reference com.sun.jdi.Accessible.java.
         */
        try {
            Type t = componentType();
            if (t instanceof PrimitiveType) {
                modifiers = VMModifiers.FINAL | VMModifiers.PUBLIC;
            } else {
                ReferenceType rt = (ReferenceType)t;
                modifiers = rt.modifiers();
            }
        } catch (ClassNotLoadedException cnle) {
            cnle.printStackTrace();
        }
    }

    public String toString() {
       return "array class " + name() + " (" + loaderString() + ")";
    }

    /*
     * Save a pointless trip over the wire for these methods
     * which have undefined results for arrays.
     */
    public boolean isPrepared() { return true; }
    public boolean isVerified() { return true; }
    public boolean isInitialized() { return true; }
    public boolean failedToInitialize() { return false; }
    public boolean isAbstract() { return false; }

    /*
     * Defined always to be true for arrays
     */
    public boolean isFinal() { return true; }
}
