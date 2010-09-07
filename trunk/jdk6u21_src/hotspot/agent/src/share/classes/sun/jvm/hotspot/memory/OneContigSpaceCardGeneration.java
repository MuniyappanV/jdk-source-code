/*
 * Copyright (c) 2000, Oracle and/or its affiliates. All rights reserved.
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

package sun.jvm.hotspot.memory;

import java.io.*;
import java.util.*;

import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.runtime.*;
import sun.jvm.hotspot.types.*;

/** <P> OneSpaceOldGeneration models a heap of old objects contained
    in a single contiguous space. </P>

    <P> Garbage collection is performed using mark-compact. </P> */

public abstract class OneContigSpaceCardGeneration extends CardGeneration {
  private static AddressField theSpaceField;

  static {
    VM.registerVMInitializedObserver(new Observer() {
        public void update(Observable o, Object data) {
          initialize(VM.getVM().getTypeDataBase());
        }
      });
  }

  private static synchronized void initialize(TypeDataBase db) {
    Type type = db.lookupType("OneContigSpaceCardGeneration");

    theSpaceField = type.getAddressField("_the_space");
  }

  public OneContigSpaceCardGeneration(Address addr) {
    super(addr);
  }

  public ContiguousSpace theSpace() {
    return (ContiguousSpace) VMObjectFactory.newObject(ContiguousSpace.class, theSpaceField.getValue(addr));
  }

  public boolean isIn(Address p) {
    return theSpace().contains(p);
  }

  /** Space queries */
  public long capacity()            { return theSpace().capacity();                                }
  public long used()                { return theSpace().used();                                    }
  public long free()                { return theSpace().free();                                    }
  public long contiguousAvailable() { return theSpace().free() + virtualSpace().uncommittedSize(); }

  public void spaceIterate(SpaceClosure blk, boolean usedOnly) {
    blk.doSpace(theSpace());
  }

  public void printOn(PrintStream tty) {
    tty.print("  old ");
    theSpace().printOn(tty);
  }
}
