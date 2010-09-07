/*
 * Copyright (c) 2002, 2006, Oracle and/or its affiliates. All rights reserved.
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

package sun.jvm.hotspot.debugger.linux;

import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.debugger.linux.amd64.*;
import sun.jvm.hotspot.debugger.linux.ia64.*;
import sun.jvm.hotspot.debugger.linux.x86.*;
import sun.jvm.hotspot.debugger.linux.sparc.*;

class LinuxThreadContextFactory {
   static ThreadContext createThreadContext(LinuxDebugger dbg) {
      String cpu = dbg.getCPU();
      if (cpu.equals("x86")) {
         return new LinuxX86ThreadContext(dbg);
      } else if (cpu.equals("amd64")) {
         return new LinuxAMD64ThreadContext(dbg);
      } else if (cpu.equals("ia64")) {
         return new LinuxIA64ThreadContext(dbg);
      } else if (cpu.equals("sparc")) {
         return new LinuxSPARCThreadContext(dbg);
      } else {
         throw new RuntimeException("cpu " + cpu + " is not yet supported");
      }
   }
}
