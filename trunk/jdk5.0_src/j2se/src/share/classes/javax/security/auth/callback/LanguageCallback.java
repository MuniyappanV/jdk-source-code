/*
 * @(#)LanguageCallback.java	1.14 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package javax.security.auth.callback;

import java.util.Locale;

/**
 * <p> Underlying security services instantiate and pass a
 * <code>LanguageCallback</code> to the <code>handle</code>
 * method of a <code>CallbackHandler</code> to retrieve the <code>Locale</code>
 * used for localizing text.
 *
 * @version 1.14, 12/19/03
 * @see javax.security.auth.callback.CallbackHandler
 */
public class LanguageCallback implements Callback, java.io.Serializable {

    private static final long serialVersionUID = 2019050433478903213L;

    /**
     * @serial
     * @since 1.4
     */
    private Locale locale;

    /**
     * Construct a <code>LanguageCallback</code>.
     */
    public LanguageCallback() { }

    /**
     * Set the retrieved <code>Locale</code>.
     *
     * <p>
     *
     * @param locale the retrieved <code>Locale</code>.
     *
     * @see #getLocale
     */
    public void setLocale(Locale locale) {
	this.locale = locale;
    }
 
    /**
     * Get the retrieved <code>Locale</code>.
     *
     * <p>
     *
     * @return the retrieved <code>Locale</code>, or null
     *		if no <code>Locale</code> could be retrieved.
     *
     * @see #setLocale
     */
    public Locale getLocale() {
	return locale;
    }
}
