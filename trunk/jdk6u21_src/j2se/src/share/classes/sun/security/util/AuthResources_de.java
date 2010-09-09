/*
 * @(#)AuthResources_de.java	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.util;

/**
 * <p> This class represents the <code>ResourceBundle</code>
 * for the following packages:
 *
 * <ol>
 * <li> com.sun.security.auth
 * <li> com.sun.security.auth.login
 * </ol>
 * 
 * @version 1.6, 01/23/01
 */
public class AuthResources_de extends java.util.ListResourceBundle {

    private static final Object[][] contents = {

	// NT principals
	{"invalid null input: value", "Ung\u00fcltige Nulleingabe: {0}"},
	{"NTDomainPrincipal: name", "NT-Dom\u00e4nen-Principal: {0}"},
	{"NTNumericCredential: name", "NT numerische Authentisierung: {0}"},
	{"Invalid NTSid value", "Ung\u00fcltiger NTSid-Wert"},
	{"NTSid: name", "NTSid: {0}"},
	{"NTSidDomainPrincipal: name", "NT-Sid-Dom\u00e4nen-Principal: {0}"},
	{"NTSidGroupPrincipal: name", "NT-Sid-Gruppen-Principal: {0}"},
	{"NTSidPrimaryGroupPrincipal: name", "NT-Sid-Prim\u00e4rgruppen-Principal: {0}"},
	{"NTSidUserPrincipal: name", "NT-Sid-Benutzer-Principal: {0}"},
	{"NTUserPrincipal: name", "NT-Benutzer-Principal: {0}"},

	// UnixPrincipals
	{"UnixNumericGroupPrincipal [Primary Group]: name",
        "Unix numerischer Gruppen-Principal [Prim\u00e4rgruppe]: {0}"},
	{"UnixNumericGroupPrincipal [Supplementary Group]: name",
        "Unix numerische Gruppen-Principal [Zusatzgruppe]: {0}"},
	{"UnixNumericUserPrincipal: name", "Unix numerischer Benutzer-Principal: {0}"},
	{"UnixPrincipal: name", "Unix-Principal: {0}"},

	// com.sun.security.auth.login.ConfigFile
	{"Unable to properly expand config", "{0} kann nicht ordnungsgem\u00e4\u00df erweitert werden."},
	{"extra_config (No such file or directory)",
        "{0} (Datei oder Verzeichnis existiert nicht.)"},
	{"Unable to locate a login configuration",
        "Anmeldekonfiguration kann nicht gefunden werden."},
	{"Configuration Error:\n\tInvalid control flag, flag",
        "Konfigurationsfehler:\n\tUng\u00fcltiges Steuerflag, {0}"},
	{"Configuration Error:\n\tCan not specify multiple entries for appName",
        "Konfigurationsfehler:\n\tEs k\u00f6nnen nicht mehrere Angaben f\u00fcr {0} gemacht werden."},
	{"Configuration Error:\n\texpected [expect], read [end of file]",
        "Konfigurationsfehler:\n\terwartet [{0}], gelesen [Dateiende]"},
	{"Configuration Error:\n\tLine line: expected [expect], found [value]",
        "Konfigurationsfehler:\n\tZeile {0}: erwartet [{1}], gefunden [{2}]"},
	{"Configuration Error:\n\tLine line: expected [expect]",
        "Konfigurationsfehler:\n\tZeile {0}: erwartet [{1}]"},
	{"Configuration Error:\n\tLine line: system property [value] expanded to empty value",
        "Konfigurationsfehler:\n\tZeile {0}: Systemeigenschaft [{1}] auf leeren Wert erweitert"},

	// com.sun.security.auth.module.JndiLoginModule
	{"username: ","Benutzername: "},
	{"password: ","Passwort: "},
	
	// com.sun.security.auth.module.KeyStoreLoginModule
	{"Please enter keystore information",
		"Bitte geben Sie die Keystore-Informationen ein"},
	{"Keystore alias: ","Keystore-Alias: "},
	{"Keystore password: ","Keystore-Passwort: "},
	{"Private key password (optional): ",
        "Privates Schl\u00fcsselpasswort (optional): "},

	// com.sun.security.auth.module.Krb5LoginModule
	{"Kerberos username [[defUsername]]: ",
        "Kerberos-Benutzername [{0}]: "}, 
	{"Kerberos password for [username]: ",
            "Kerberos-Passwort f\u00fcr {0}: "},

	/***	EVERYTHING BELOW IS DEPRECATED	***/

	// com.sun.security.auth.PolicyFile
	{": error parsing ", ": Parser-Fehler "},
	{": ", ": "},
	{": error adding Permission ", ": Fehler beim Hinzuf\u00fcgen der Berechtigung "},
	{" ", " "},
	{": error adding Entry ", ": Fehler beim Hinzuf\u00fcgen des Eintrags "},
	{"(", "("},
	{")", ")"},
	{"attempt to add a Permission to a readonly PermissionCollection",
        "Es wurde versucht, eine Berechtigung zu einer schreibgesch\u00fctzten Berechtigungssammlung hinzuzuf\u00fcgen."},

	// com.sun.security.auth.PolicyParser
	{"expected keystore type", "erwarteter Keystore-Typ"},
	{"can not specify Principal with a ",
        "Principal kann nicht mit einer "},
	{"wildcard class without a wildcard name",
        "Wildcard-Klasse ohne Wildcard-Namen angegeben werden."},
	{"expected codeBase or SignedBy", "codeBase oder SignedBy erwartet"},
	{"only Principal-based grant entries permitted",
        "Nur Principal-basierte Berechtigungseintr\u00e4ge erlaubt"},
	{"expected permission entry", "Berechtigungseintrag erwartet"},
	{"number ", "Nummer "},
	{"expected ", "erwartet "},
	{", read end of file", ", Dateiende lesen"},
	{"expected ';', read end of file", "';' erwartet, Dateiende lesen"},
	{"line ", "Zeile "},
	{": expected '", ": erwartet '"},
	{"', found '", "', gefunden '"},
	{"'", "'"},

	// SolarisPrincipals
	{"SolarisNumericGroupPrincipal [Primary Group]: ",
        "Solaris numerischer Gruppen-Principal [Prim\u00e4rgruppe]: "},
	{"SolarisNumericGroupPrincipal [Supplementary Group]: ",
        "Solaris numerischer Gruppen-Principal [Zusatzgruppe]: "},
	{"SolarisNumericUserPrincipal: ",
        "Solaris numerischer Benutzer-Principal: "},
	{"SolarisPrincipal: ", "Solaris-Principal: "},
	{"provided null name", "enthielt leeren Namen"}

    };

    /**
     * Returns the contents of this <code>ResourceBundle</code>.
     *
     * <p>
     *
     * @return the contents of this <code>ResourceBundle</code>.
     */
    public Object[][] getContents() {
	return contents;
    }
}
