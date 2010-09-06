/*
 * @(#)MonitorStatus.java	1.1 04/02/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.jvmstat.perfdata.monitor;

import java.util.List;

/**
 * Immutable class containing the list of inserted and deleted
 * monitors over an arbitrary time period.
 *
 * @author Brian Doherty
 * @version 1.1, 02/23/04
 * @since 1.5
 */
public class MonitorStatus {

    /**
     * The list of Monitors inserted since the last query.
     */
    protected List inserted;

    /**
     * The list of Monitors removed since the last query.
     */
    protected List removed;

    /**
     * Create a MonitorStatus instance.
     *
     * @param inserted the list of Monitors inserted
     * @param removed the list of Monitors removed
     */
    public MonitorStatus(List inserted, List removed) {
        this.inserted = inserted;
        this.removed = removed;
    }

    /**
     * Get the list of Monitors inserted since the last query.
     *
     * @return List - the List of Monitor objects inserted or an empty List.
     */
    public List getInserted() {
        return inserted;
    }

    /**
     * Get the list of Monitors removed since the last query.
     *
     * @return List - the List of Monitor objects removed or an empty List.
     */
    public List getRemoved() {
        return removed;
    }
}
