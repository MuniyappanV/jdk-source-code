/*
 * @(#)LangReflectAccess.java	1.12 04/01/28
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.reflect;

import java.lang.reflect.*;

/** An interface which gives privileged packages Java-level access to
    internals of java.lang.reflect. */

public interface LangReflectAccess {
    /** Creates a new java.lang.reflect.Field. Access checks as per
        java.lang.reflect.AccessibleObject are not overridden. */
    public Field newField(Class declaringClass,
                          String name,
                          Class type,
                          int modifiers,
                          int slot,
                          String signature,
                          byte[] annotations);

    /** Creates a new java.lang.reflect.Method. Access checks as per
      java.lang.reflect.AccessibleObject are not overridden. */
    public Method newMethod(Class declaringClass,
                            String name,
                            Class[] parameterTypes,
                            Class returnType,
                            Class[] checkedExceptions,
                            int modifiers,
                            int slot,
                            String signature,
                            byte[] annotations,
                            byte[] parameterAnnotations,
                            byte[] annotationDefault);

    /** Creates a new java.lang.reflect.Constructor. Access checks as
      per java.lang.reflect.AccessibleObject are not overridden. */
    public <T> Constructor<T> newConstructor(Class<T> declaringClass,
					     Class[] parameterTypes,
					     Class[] checkedExceptions,
					     int modifiers,
					     int slot,
                                             String signature,
                                             byte[] annotations,
                                             byte[] parameterAnnotations);

    /** Gets the MethodAccessor object for a java.lang.reflect.Method */
    public MethodAccessor getMethodAccessor(Method m);

    /** Sets the MethodAccessor object for a java.lang.reflect.Method */
    public void setMethodAccessor(Method m, MethodAccessor accessor);

    /** Gets the ConstructorAccessor object for a
        java.lang.reflect.Constructor */
    public ConstructorAccessor getConstructorAccessor(Constructor c);

    /** Sets the ConstructorAccessor object for a
        java.lang.reflect.Constructor */
    public void setConstructorAccessor(Constructor c,
                                       ConstructorAccessor accessor);

    /** Gets the "slot" field from a Constructor (used for serialization) */
    public int getConstructorSlot(Constructor c);

    /** Gets the "signature" field from a Constructor (used for serialization) */
    public String getConstructorSignature(Constructor c);

    /** Gets the "annotations" field from a Constructor (used for serialization) */
    public byte[] getConstructorAnnotations(Constructor c);

    /** Gets the "parameterAnnotations" field from a Constructor (used for serialization) */
    public byte[] getConstructorParameterAnnotations(Constructor c);

    //
    // Copying routines, needed to quickly fabricate new Field,
    // Method, and Constructor objects from templates
    //

    /** Makes a "child" copy of a Method */
    public Method      copyMethod(Method arg);

    /** Makes a "child" copy of a Field */
    public Field       copyField(Field arg);

    /** Makes a "child" copy of a Constructor */
    public <T> Constructor<T> copyConstructor(Constructor<T> arg);
}
