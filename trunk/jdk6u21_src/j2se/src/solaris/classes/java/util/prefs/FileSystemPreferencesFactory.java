/*
 * @(#)FileSystemPreferencesFactory.java	1.7 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.util.prefs;

/**
 * Factory for FileSystemPreferences.  This class allows FileSystemPreferences
 * to be installed as the Preferences implementations via the
 * java.util.prefs.PreferencesFactory system property.
 *
 * @author  Josh Bloch
 * @version 1.7, 03/23/10
 * @see     FileSystemPreferences
 * @see     Preferences
 * @since   1.4
 */

class FileSystemPreferencesFactory implements PreferencesFactory {
    public Preferences userRoot() {
        return FileSystemPreferences.getUserRoot();
    }

    public Preferences systemRoot() {
        return FileSystemPreferences.getSystemRoot();
    }
}
