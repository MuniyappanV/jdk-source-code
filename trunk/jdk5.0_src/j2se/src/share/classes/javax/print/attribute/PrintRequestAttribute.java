/*
 * @(#)PrintRequestAttribute.java	1.5 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */


package javax.print.attribute;

/**
 * Interface PrintRequestAttribute is a tagging interface which a printing 
 * attribute class implements to indicate the attribute denotes a
 * requested setting for a print job. 
 * <p>
 * Attributes which are tagged with PrintRequestAttribute and are also tagged
 * as PrintJobAttribute, represent the subset of job attributes which 
 * can be part of the specification of a job request.
 * <p>
 * If an attribute implements {@link DocAttribute  DocAttribute}
 * as well as PrintRequestAttribute, the client may include the 
 * attribute in a <code>Doc</code>}'s attribute set to specify 
 * a job setting which pertains just to that doc. 
 * <P>
 *
 * @see DocAttributeSet
 * @see PrintRequestAttributeSet
 *
 * @author  Alan Kaminsky
 */

public interface PrintRequestAttribute extends Attribute {
}
