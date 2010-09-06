/*
 * @(#)AbstractSelectionKey.java	1.11 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.nio.channels.spi;

import java.nio.channels.*;


/**
 * Base implementation class for selection keys.
 *
 * <p> This class tracks the validity of the key and implements cancellation.
 *
 * @author Mark Reinhold
 * @author JSR-51 Expert Group
 * @version 1.11, 03/12/19
 * @since 1.4
 */

public abstract class AbstractSelectionKey
    extends SelectionKey
{

    /**
     * Initializes a new instance of this class.  </p>
     */
    protected AbstractSelectionKey() { }

    private volatile boolean valid = true;

    public final boolean isValid() {
	return valid;
    }

    void invalidate() {					// package-private
	valid = false;
    }

    /**
     * Cancels this key.
     *
     * <p> If this key has not yet been cancelled then it is added to its
     * selector's cancelled-key set while synchronized on that set.  </p>
     */
    public final void cancel() {
	if (valid) {
            valid = false;
            ((AbstractSelector)selector()).cancel(this);
        }
    }
}
