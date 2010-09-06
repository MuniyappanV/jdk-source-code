/*
 * @(#)PerfIntegerMonitor.java	1.1 04/02/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.jvmstat.perfdata.monitor;

import sun.jvmstat.monitor.*;
import sun.management.counter.Units;
import sun.management.counter.Variability;
import java.nio.IntBuffer;

/**
 * Class for monitoring a PerfData Integer instrument.
 *
 * @author Brian Doherty
 * @version 1.1, 02/23/04
 * @since 1.5
 */
public class PerfIntegerMonitor extends AbstractMonitor
                                implements IntegerMonitor {

    /**
     * The buffer containing the data for the integer instrument.
     */
    IntBuffer ib;

    /**
     * Constructor to create an IntegerMonitor object for the integer
     *  instrument represented by the data in the given buffer.
     *
     * @param name the name of the integer instrument
     * @param u the units of measure attribute
     * @param v the variability attribute
     * @param supported support level indicator
     * @param ib the buffer containing the integer instrument data.
     */
    public PerfIntegerMonitor(String name, Units u, Variability v,
                              boolean supported, IntBuffer ib) {
        super(name, u, v, supported);
        this.ib = ib;
    }

    /**
     * {@inheritDoc}
     * The object returned contains an Integer object containing the
     * current value of the IntegerInstrument.
     *
     * @return Object - the current value of the the IntegerInstrument. The
     *                   return type is guaranteed to be of type Integer.
     */
    public Object getValue() {
        return new Integer(ib.get(0));
    }

    /**
     * Return the current value of the IntegerInstrument as an int.
     *
     * @return int - a the current value of the IntegerInstrument.
     */
    public int intValue() {
        return ib.get(0);
    }
}
