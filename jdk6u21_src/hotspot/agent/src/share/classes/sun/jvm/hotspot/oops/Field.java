/*
 * Copyright (c) 2000, 2003, Oracle and/or its affiliates. All rights reserved.
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
 */

package sun.jvm.hotspot.oops;

import java.io.*;

import sun.jvm.hotspot.runtime.*;

// Super class for all fields in an object
public class Field {

  Field(FieldIdentifier id, long offset, boolean isVMField) {
    this.offset    = offset;
    this.id        = id;
    this.isVMField = isVMField;
  }

  /** Constructor for fields that are named in an InstanceKlass's
      fields array (i.e., named, non-VM fields) */
  Field(InstanceKlass holder, int fieldArrayIndex) {
    this.holder = holder;
    this.fieldArrayIndex = fieldArrayIndex;

    ConstantPool cp      = holder.getConstants();
    TypeArray fields     = holder.getFields();
    short access         = fields.getShortAt(fieldArrayIndex + InstanceKlass.ACCESS_FLAGS_OFFSET);
    short nameIndex      = fields.getShortAt(fieldArrayIndex + InstanceKlass.NAME_INDEX_OFFSET);
    short signatureIndex = fields.getShortAt(fieldArrayIndex + InstanceKlass.SIGNATURE_INDEX_OFFSET);
    offset               = VM.getVM().buildIntFromShorts(fields.getShortAt(fieldArrayIndex + InstanceKlass.LOW_OFFSET),
                                                         fields.getShortAt(fieldArrayIndex + InstanceKlass.HIGH_OFFSET));
    short genericSignatureIndex = fields.getShortAt(fieldArrayIndex + InstanceKlass.GENERIC_SIGNATURE_INDEX_OFFSET);
    Symbol name = cp.getSymbolAt(nameIndex);
    id          = new NamedFieldIdentifier(name.asString());
    signature   = cp.getSymbolAt(signatureIndex);
    if (genericSignatureIndex != 0)  {
       genericSignature = cp.getSymbolAt(genericSignatureIndex);
    } else {
       genericSignature = null;
    }

    fieldType   = new FieldType(signature);
    accessFlags = new AccessFlags(access);
  }

  private long            offset;
  private FieldIdentifier id;
  private boolean         isVMField;
  // Java fields only
  private InstanceKlass   holder;
  private FieldType       fieldType;
  private Symbol          signature;
  private Symbol          genericSignature;
  private AccessFlags     accessFlags;
  private int             fieldArrayIndex;

  /** Returns the byte offset of the field within the object or klass */
  public long getOffset() { return offset; }

  /** Returns the identifier of the field */
  public FieldIdentifier getID() { return id; }

  /** Indicates whether this is a VM field */
  public boolean isVMField() { return isVMField; }

  /** Indicates whether this is a named field */
  public boolean isNamedField() { return (id instanceof NamedFieldIdentifier); }

  public void printOn(PrintStream tty) {
    getID().printOn(tty);
    tty.print(" {" + getOffset() + "} :");
  }

  /** (Named, non-VM fields only) Returns the InstanceKlass containing
      this (static or non-static) field. */
  public InstanceKlass getFieldHolder() {
    return holder;
  }

  /** (Named, non-VM fields only) Returns the index in the fields
      TypeArray for this field. Equivalent to the "index" in the VM's
      fieldDescriptors. */
  public int getFieldArrayIndex() {
    return fieldArrayIndex;
  }

  /** (Named, non-VM fields only) Retrieves the access flags. */
  public long getAccessFlags() { return accessFlags.getValue(); }
  public AccessFlags getAccessFlagsObj() { return accessFlags; }

  /** (Named, non-VM fields only) Returns the type of this field. */
  public FieldType getFieldType() { return fieldType; }

  /** (Named, non-VM fields only) Returns the signature of this
      field. */
  public Symbol getSignature() { return signature; }
  public Symbol getGenericSignature() { return genericSignature; }

  //
  // Following acccessors are for named, non-VM fields only
  //
  public boolean isPublic()                  { return accessFlags.isPublic(); }
  public boolean isPrivate()                 { return accessFlags.isPrivate(); }
  public boolean isProtected()               { return accessFlags.isProtected(); }
  public boolean isPackagePrivate()          { return !isPublic() && !isPrivate() && !isProtected(); }

  public boolean isStatic()                  { return accessFlags.isStatic(); }
  public boolean isFinal()                   { return accessFlags.isFinal(); }
  public boolean isVolatile()                { return accessFlags.isVolatile(); }
  public boolean isTransient()               { return accessFlags.isTransient(); }

  public boolean isSynthetic()               { return accessFlags.isSynthetic(); }
  public boolean isEnumConstant()            { return accessFlags.isEnum();      }

  public boolean equals(Object obj) {
     if (obj == null) {
        return false;
     }

     if (! (obj instanceof Field)) {
        return false;
     }

     Field other = (Field) obj;
     return this.getFieldHolder().equals(other.getFieldHolder()) &&
            this.getID().equals(other.getID());
  }

  public int hashCode() {
     return getFieldHolder().hashCode() ^ getID().hashCode();
  }
}
