/*
 * @(#)Resources_de.java	1.9 04/02/27
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.security.util;

/**
 * <p> This class represents the <code>ResourceBundle</code>
 * for KeyTool,PolicyTool and javax.security.auth.*.
 *
 * @version 1.11, 01/30/01
 */
public class Resources_de extends java.util.ListResourceBundle {

    private static final Object[][] contents = {

	// shared (from jarsigner)
	{" ", " "},
	{"  ", "  "},
	{"      ", "      "},
	{", ", ", "},
	// shared (from keytool)
	{"\n", "\n"},
	{"*******************************************",
		"*******************************************"},
	{"*******************************************\n\n",
		"*******************************************\n\n"},

	// keytool
	{"keytool error: ", "Keytool-Fehler: "},
	{"Illegal option:  ", "Unzul\u00e4ssige Option:  "},
	{"-keystore must be NONE if -storetype is PKCS11",
		"-keystore muss NONE sein, wenn -storetype auf PKCS11 gesetzt ist"},
	{"-storepasswd and -keypasswd commands not supported if -storetype is PKCS11",
		"Die Befehle -storepasswd und -keypasswd werden nicht unterst\u00fctzt, wenn -storetype auf PKCS11 gesetzt ist"},
        {"-keypass and -new can not be specified if -storetype is PKCS11",
		"-keypass und -new k\u00f6nnen nicht angegeben werden, wenn -storetype auf PKCS11 gesetzt ist"},
        {"if -protected is specified, then -storepass, -keypass, and -new must not be specified",
		"Wenn -protected angegeben ist, d\u00fcrfen -storepass, -keypass und -new nicht angegeben werden"},

	{"Validity must be greater than zero",
		"G\u00fcltigkeit muss gr\u00f6\u00dfer als Null sein"},
	{"provName not a provider", "{0} kein Provider"},
	{"Must not specify both -v and -rfc with 'list' command",
		"-v und -rfc d\u00fcrfen bei Befehl 'list' nicht beide angegeben werden"},
	{"Key password must be at least 6 characters",
		"Schl\u00fcsselpasswort muss mindestens 6 Zeichen lang sein"},
	{"New password must be at least 6 characters",
		"Neues Passwort muss mindest 6 Zeichen lang sein"},
	{"Keystore file exists, but is empty: ",
		"Keystore-Datei vorhanden, aber leer: "},
	{"Keystore file does not exist: ",
		"Keystore-Datei nicht vorhanden: "},
	{"Must specify destination alias", "Zielalias muss angegeben werden."},
	{"Must specify alias", "Alias muss angegeben werden."},
	{"Keystore password must be at least 6 characters",
		"Keystore-Passwort muss mindestens 6 Zeichen lang sein."},
	{"Enter keystore password:  ", "Geben Sie das Keystore-Passwort ein:  "},
	{"Keystore password is too short - must be at least 6 characters",
	 "Keystore-Passwort zu kurz - muss mindestens 6 Zeichen lang sein."},
	{"Too many failures - try later", "Zu viele Fehler - versuchen Sie es sp\u00e4ter noch einmal."},
	{"Certification request stored in file <filename>",
		"Zertifizierungsanforderung in Datei <{0}> gespeichert."},
	{"Submit this to your CA", "Reichen Sie dies bei Ihrem CA ein."},
	{"Certificate stored in file <filename>",
		"Zertifikat in Datei <{0}> gespeichert."},
	{"Certificate reply was installed in keystore",
		"Zertifikatantwort wurde in Keystore installiert."},
	{"Certificate reply was not installed in keystore",
		"Zertifikatantwort wurde nicht in Keystore installiert."},
	{"Certificate was added to keystore",
		"Zertifikat wurde zu Keystore hinzugef\u00fcgt."},
	{"Certificate was not added to keystore",
		"Zertifikat wurde nicht zu Keystore hinzugef\u00fcgt."},
	{"[Storing ksfname]", "[{0} wird gesichert.]"},
	{"alias has no public key (certificate)",
		"{0} hat keinen \u00f6ffentlichen Schl\u00fcssel (Zertifikat)."},
	{"Cannot derive signature algorithm",
		"Signaturalgorithmus kann nicht abgeleitet werden."},
	{"Alias <alias> does not exist",
		"Alias <{0}> existiert nicht."},
	{"Alias <alias> has no certificate",
		"Alias <{0}> hat kein Zertifikat."},
	{"Key pair not generated, alias <alias> already exists",
		"Schl\u00fcsselpaar wurde nicht erzeugt, Alias <{0}> ist bereits vorhanden."},
	{"Cannot derive signature algorithm",
		"Signaturalgorithmus kann nicht abgeleitet werden."},
	{"Generating keysize bit keyAlgName key pair and self-signed certificate (sigAlgName)\n\tfor: x500Name",
		"Erzeuge {0}-Bit {1} Schl\u00fcsselpaar und selbstsigniertes Zertifikat  ({2})\n\tf\u00fcr: {3}"},
	{"Enter key password for <alias>", "Geben Sie das Passwort f\u00fcr <{0}> ein."},
	{"\t(RETURN if same as keystore password):  ",
		"\t(EINGABETASTE, wenn Passwort dasselbe wie f\u00fcr Keystore):  "},
	{"Key password is too short - must be at least 6 characters",
		"Schl\u00fcsselpasswort zu kurz - muss mindestens 6 Zeichen lang sein."},
	{"Too many failures - key not added to keystore",
		"Zu viele Fehler - Schl\u00fcssel wurde nicht zu Keystore hinzugef\u00fcgt."},
	{"Destination alias <dest> already exists",
		"Zielalias <{0}> bereits vorhanden"},
	{"Password is too short - must be at least 6 characters",
		"Passwort zu kurz - muss mindestens 6 Zeichen lang sein"},
	{"Too many failures. Key entry not cloned",
		"Zu viele Fehler. Schl\u00fcsseleingabe wurde nicht dupliziert."},
	{"key password for <alias>", "Schl\u00fcsselpasswort f\u00fcr <{0}>"},
	{"Keystore entry for <id.getName()> already exists",
		"Keystore-Eintrag f\u00fcr <{0}> bereits vorhanden"},
	{"Creating keystore entry for <id.getName()> ...",
		"Keystore-Eintrag f\u00fcr <{0}> wird erstellt ..."},
	{"No entries from identity database added",
		"Keine Eintr\u00e4ge von Identit\u00e4tsdatenbank hinzugef\u00fcgt"},
	{"Alias name: alias", "Aliasname: {0}"},
	{"Creation date: keyStore.getCreationDate(alias)",
		"Erstellungsdatum: {0,date}"},
	{"alias, keyStore.getCreationDate(alias), ",
		"{0}, {1,date}, "},
	{"alias, ", "{0}, "},
	{"Entry type: keyEntry", "Eintragstyp: keyEntry"},
	{"keyEntry,", "keyEntry,"},
	{"Certificate chain length: ", "Zertifikatskettenl\u00e4nge: "},
	{"Certificate[(i + 1)]:", "Zertifikat[{0,number,integer}]:"},
	{"Certificate fingerprint (MD5): ", "Zertifikatsfingerabdruck (MD5): "},
	{"Entry type: trustedCertEntry\n", "Eintragstyp: trustedCertEntry\n"},
	{"trustedCertEntry,", "trustedCertEntry,"},
	{"Keystore type: ", "Keystore-Typ: "},
	{"Keystore provider: ", "Keystore-Provider: "},
	{"Your keystore contains keyStore.size() entry",
		"Ihr Keystore enth\u00e4lt {0,number,integer} Eintrag/-\u00e4ge."},
	{"Your keystore contains keyStore.size() entries",
		"Ihr Keystore enth\u00e4lt {0,number,integer} Eintr\u00e4ge."},
	{"Failed to parse input", "Eingabe konnte nicht analysiert werden."},
	{"Empty input", "Leere Eingabe"},
	{"Not X.509 certificate", "Kein X.509-Zertifikat"},
	{"Cannot derive signature algorithm",
		"Signaturalgorithmus kann nicht abgeleitet werden."},
	{"alias has no public key", "{0} hat keinen \u00f6ffentlichen Schl\u00fcssel."},
	{"alias has no X.509 certificate", "{0} hat kein X.509-Zertifikat."},
	{"New certificate (self-signed):", "Neues Zertifikat (selbstsigniert):"},
	{"Reply has no certificates", "Antwort hat keine Zertifikate."},
	{"Certificate not imported, alias <alias> already exists",
		"Zertifikat nicht importiert, Alias <{0}> bereits vorhanden"},
	{"Input not an X.509 certificate", "Eingabe kein X.509-Zertifikat"},
	{"Certificate already exists in keystore under alias <trustalias>",
		"Zertifikat in Keystore bereits unter Alias <{0}> vorhanden"},
	{"Do you still want to add it? [no]:  ",
		"M\u00f6chten Sie es trotzdem hinzuf\u00fcgen? [Nein]:  "},
	{"Certificate already exists in system-wide CA keystore under alias <trustalias>",
		"Zertifikat in systemweiten CA-Keystore bereits unter Alias <{0}> vorhanden."},
	{"Do you still want to add it to your own keystore? [no]:  ",
		"M\u00f6chten Sie es trotzdem zu Ihrem eigenen Keystore hinzuf\u00fcgen? [Nein]:  "},
	{"Trust this certificate? [no]:  ", "Diesem Zertifikat vertrauen? [Nein]:  "},
	{"YES", "JA"},
	{"New prompt: ", "Neues {0}: "},
	{"Passwords must differ", "Passw\u00f6rter m\u00fcssen sich unterscheiden"},
	{"Re-enter new prompt: ", "Neues {0} nochmals eingeben: "},
	{"They don't match; try again", "Sie stimmen nicht \u00fcberein, versuchen Sie es noch einmal."},
	{"Enter prompt alias name:  ", "Geben Sie den Aliasnamen von {0} ein:  "},
	{"Enter alias name:  ", "Geben Sie den Aliasnamen ein:  "},
	{"\t(RETURN if same as for <otherAlias>)",
		"\t(EINGABETASTE, wenn selber Name wie f\u00fcr <{0}>)"},
	{"*PATTERN* printX509Cert",
		"Eigent\u00fcmer: {0}\nAussteller: {1}\nSeriennummer {2}\nG\u00fcltig ab: {3} bis: {4}\nZertifikatfingerabdr\u00fccke:\n\t MD5: {5}\n\t SHA1: {6}"},
	{"What is your first and last name?",
		"Wie lautet Ihr Vor- und Nachname?"},
	{"What is the name of your organizational unit?",
		"Wie lautet der Name Ihrer organisatorischen Einheit?"},
	{"What is the name of your organization?",
		"Wie lautet der Name Ihrer Organisation?"},
	{"What is the name of your City or Locality?",
		"Wie lautet der Name Ihrer Stadt oder Gemeinde?"},
	{"What is the name of your State or Province?",
		"Wie lautet der Name Ihres Bundeslandes oder Ihrer Provinz?"},
	{"What is the two-letter country code for this unit?",
		"Wie lautet der Landescode (zwei Buchstaben) f\u00fcr diese Einheit?"},
	{"Is <name> correct?", "Ist {0} richtig?"},
	{"no", "Nein"},
	{"yes", "Ja"},
	{"y", "J"},
	{"  [defaultValue]:  ", " [{0}]:  "},
	{"Alias <alias> has no (private) key",
		"Alias <{0}> hat keinen (privaten) Schl\u00fcssel."},
	{"Recovered key is not a private key",
		"Der wiederhergestellte Schl\u00fcssel ist kein privater Schl\u00fcssel."},
	{"*****************  WARNING WARNING WARNING  *****************",
	    "*****************  WARNUNG WARNUNG WARNUNG  *****************"},
	{"* The integrity of the information stored in your keystore  *",
	    "* Die Integrit\u00e4t der in Ihrem Keystore gespeicherten Informationen  *"},
	{"* has NOT been verified!  In order to verify its integrity, *",
	    "* ist NICHT verifiziert worden! Damit die Integrit\u00e4t verifiziert werden kann, *"},
	{"* you must provide your keystore password.                  *",
	    "* m\u00fcssen Sie Ihr Keystore-Passwort eingeben. *"},
	{"Certificate reply does not contain public key for <alias>",
		"Zertifikatantwort enth\u00e4lt keinen \u00f6ffentlichen Schl\u00fcssel f\u00fcr <{0}>."},
	{"Incomplete certificate chain in reply",
		"Unvollst\u00e4ndige Zertifikatskette in Antwort"},
	{"Certificate chain in reply does not verify: ",
		"Zertifikatskette in Antwort verifiziert nicht: "},
	{"Top-level certificate in reply:\n",
		"Toplevel-Zertifikat in Antwort:\n"},
	{"... is not trusted. ", "... wird nicht vertraut. "},
	{"Install reply anyway? [no]:  ", "Antwort trotzdem installieren? [Nein]:  "},
	{"NO", "NEIN"},
	{"Public keys in reply and keystore don't match",
		"\u00d6ffentliche Schl\u00fcssel in Antwort und Keystore stimmen nicht \u00fcberein."},
	{"Certificate reply and certificate in keystore are identical",
		"Zertifikatantwort und Zertifikat in Keystore sind identisch."},
	{"Failed to establish chain from reply",
		"Kette konnte nicht aus Antwort entnommen werden."},
	{"n", "N"},
	{"Wrong answer, try again", "Falsche Antwort, versuchen Sie es noch einmal."},
	{"keytool usage:\n", "Keytool-Syntax:\n"},
	{"-certreq     [-v] [-protected]",
		"-certreq     [-v] [-protected]"},
	{"\t     [-alias <alias>] [-sigalg <sigalg>]",
		"\t     [-alias <Alias>] [-sigalg <Sigalg>]"},
	{"\t     [-file <csr_file>] [-keypass <keypass>]",
		"\t     [-file <csr_Datei>] [-keypass <Keypass>]"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"\t     [-storetype <storetype>] [-providerName <name>]",
                "\t     [-storetype <Storetyp>] [-providerName <Name>]"},
	{"\t     [-providerClass <provider_class_name> [-providerArg <arg>]] ...",
                "\t     [-providerClass <Provider-Klassenname> [-providerArg <Arg>]] ..."},
	{"-delete      [-v] [-protected] -alias <alias>",
		"-delete      [-v] [-protected] -alias <Alias>"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"-export      [-v] [-rfc] [-protected]",
			"-export      [-v] [-rfc] [-protected]"},
	{"\t     [-alias <alias>] [-file <cert_file>]",
		"\t     [-alias <Alias>] [-file <Zert_datei>]"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"-genkey      [-v] [-protected]",
			"-genkey      [-v] [-protected]"},
	{"\t     [-alias <alias>]", "\t     [-alias <Alias>]"},
        {"\t     [-keyalg <keyalg>] [-keysize <keysize>]",
                "\t     [-keyalg <Schl\u00fcssel-Alg>] [-keysize <Schl\u00fcsselgr\u00f6\u00dfe>]"},
        {"\t     [-sigalg <sigalg>] [-dname <dname>]",
                "\t     [-sigalg <Signal-Alg>] [-dname <Dname>]"},
        {"\t     [-validity <valDays>] [-keypass <keypass>]",
                "\t     [-validity <G\u00fcltigkeitTage>] [-keypass <Schl\u00fcsselpass>]"},
	{"-help", "-help"},
	{"-identitydb  [-v] [-protected]",
		"-identitydb  [-v] [-protected]"},
	{"\t     [-file <idb_file>]", "\t     [-file <idb-Datei>]"},
	{"-import      [-v] [-noprompt] [-trustcacerts] [-protected]",
			"-import      [-v] [-noprompt] [-trustcacerts] [-protected]"},
	{"\t     [-file <cert_file>] [-keypass <keypass>]",
		"\t     [-file <Zert_Datei>] [-keypass <Schl\u00fcsselpass>]"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"-keyclone    [-v] [-protected]",
			"-keyclone    [-v] [-protected]"},
	{"\t     [-alias <alias>] -dest <dest_alias>",
		"\t     [-alias <alias>] -dest <Ziel_Alias>"},
	{"\t     [-keypass <keypass>] [-new <new_keypass>]",
		"\t     [-keypass <Schl\u00fcsselpass>] [-new <neu_Schl\u00fcsselpass>]"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"-keypasswd   [-v] [-alias <alias>]",
		"-keypasswd   [-v] [-alias <Alias>]"},
	{"\t     [-keypass <old_keypass>] [-new <new_keypass>]",
		"\t     [-keypass <alt_Schl\u00fcsselpass>] [-new <neu_Schl\u00fcsselpass>]"},
	{"\t     [-keystore <keystore>] [-storepass <storepass>]",
		"\t     [-keystore <Keystore>] [-storepass <Storepass>]"},
	{"-list        [-v | -rfc] [-protected]",
			"-list        [-v | -rfc] [-protected]"},
	{"-printcert   [-v] [-file <cert_file>]",
		"-printcert   [-v] [-file <Zert_Datei>]"},
	{"-selfcert    [-v] [-protected]",
			"-selfcert    [-v] [-protected]"},

	{"\t     [-dname <dname>] [-validity <valDays>]",
		"\t     [-dname <dname>] [-validity <G\u00fcltigTage>]"},
	{"\t     [-keypass <keypass>] [-sigalg <sigalg>]",
               "\t     [-keypass <keypass>] [-sigalg <sigalg>]"},
	{"-storepasswd [-v] [-new <new_storepass>]",
		"-storepasswd [-v] [-new <neu_Storepass>]"},

	// policytool
	{"Warning: A public key for alias 'signers[i]' does not exist.",
		"Warnung: F\u00fcr Alias {0} ist kein \u00f6ffentlicher Schl\u00fcssel vorhanden."},
	{"Warning: Class not found: ",
		"Warnung: Klasse nicht gefunden: "},
	{"Policy File opened successfully",
		"Richtliniendatei erfolgreich ge\u00f6ffnet"},
	{"null Keystore name", "kein Keystore-Name"},
	{"Warning: Unable to open Keystore: ",
		"Warnung: Keystore kann nicht ge\u00f6ffnet werden: "},
	{"Illegal option: ", "Unzul\u00e4ssige Option: "},
	{"Usage: policytool [options]", "Syntax: policytool [Optionen]"},
	{"  [-file <file>]    policy file location",
		" [-file <Datei>]    Verzeichnis der Richtliniendatei"},
	{"New", "Neu"},
	{"Open", "\u00d6ffnen"},
	{"Save", "Speichern"},
	{"Save As", "Speichern unter"},
	{"View Warning Log", "Warnungsprotokoll anzeigen"},
	{"Exit", "Beenden"},
	{"Add Policy Entry", "Richtlinieneintrag hinzuf\u00fcgen"},
	{"Edit Policy Entry", "Richtlinieneintrag bearbeiten"},
	{"Remove Policy Entry", "Richtlinieneintrag l\u00f6schen"},
	{"Change KeyStore", "KeyStore \u00e4ndern"},
	{"Add Public Key Alias", "Alias f\u00fcr \u00f6ffentlichen Schl\u00fcssel hinzuf\u00fcgen"},
	{"Remove Public Key Alias", "Alias f\u00fcr \u00f6ffentlichen Schl\u00fcssel l\u00f6schen"},
	{"File", "Datei"},
	{"Edit", "Bearbeiten"},
	{"Policy File:", "Richtliniendatei:"},
	{"Keystore:", "Keystore:"},
	{"Error parsing policy file policyFile: pppe.getMessage()",
		"Fehler beim Analysieren der Richtliniendatei {0}: {1}"},
	{"Could not find Policy File: ", "Richtliniendatei konnte nicht gefunden werden: "},
	{"Policy Tool", "Richtlinientool"},
	{"Errors have occurred while opening the policy configuration.  View the Warning Log for more information.",
		"Beim \u00d6ffnen der Richtlinienkonfiguration sind Fehler aufgetreten. Weitere Informationen finden Sie im Warnungsprotokoll."},
	{"Error", "Fehler"},
	{"OK", "OK"},
	{"Status", "Status"},
	{"Warning", "Warnung"},
	{"Permission:                                                       ",
		"Berechtigung:                                                       "},
	{"Target Name:                                                    ",
		"Zielname:                                                    "},
	{"library name", "Bibliotheksname"},
	{"package name", "Paketname"},
	{"property name", "Eigenschaftsname"},
	{"provider name", "Providername"},
	{"Actions:                                                             ",
		"Aktionen:                                                             "},
	{"OK to overwrite existing file filename?",
		"Vorhandene Datei {0} \u00fcberschreiben?"},
	{"Cancel", "Abbrechen"},
	{"CodeBase:", "Code-Basis:"},
	{"SignedBy:", "Signiert von:"},
	{"  Add Permission", " Berechtigung hinzuf\u00fcgen"},
	{"  Edit Permission", " Berechtigung \u00e4ndern"},
	{"Remove Permission", "Berechtigung l\u00f6schen"},
	{"Done", "Fertig"},
	{"New KeyStore URL:", "Neue KeyStore-URL:"},
	{"New KeyStore Type:", "Neuer KeyStore-Typ:"},
	{"Permissions", "Berechtigungen"},
	{"  Edit Permission:", " Berechtigung \u00e4ndern:"},
	{"  Add New Permission:", " Neue Berechtigung hinzuf\u00fcgen:"},
	{"Signed By:", "Signiert von:"},
	{"Permission and Target Name must have a value",
		"Berechtigung und Zielname m\u00fcssen einen Wert haben"},
	{"Remove this Policy Entry?", "Diesen Richtlinieneintrag l\u00f6schen?"},
	{"Overwrite File", "Datei \u00fcberschreiben"},
	{"Policy successfully written to filename",
		"Richtlinien erfolgreich in {0} geschrieben"},
	{"null filename", "Null als Dateiname"},
	{"filename not found", "{0} nicht gefunden"},
	{"     Save changes?", " \u00c4nderungen speichern?"},
	{"Yes", "Ja"},
	{"No", "Nein"},
	{"Error: Could not open policy file, filename, because of parsing error: pppe.getMessage()",
		"Fehler: Richtliniendatei {0} konnte wegen Analysefehler nicht ge\u00f6ffnet werden: {1}"},
	{"Permission could not be mapped to an appropriate class",
		"Berechtigung konnte keiner entsprechenden Klasse zugeordnet werden"},
	{"Policy Entry", "Richtlinieneintrag"},
	{"Save Changes", "\u00c4nderungen speichern"},
	{"No Policy Entry selected", "Kein Richtlinieneintrag ausgew\u00e4hlt"},
	{"Keystore", "Keystore"},
	{"KeyStore URL must have a valid value",
		"KeyStore-URL muss einen g\u00fcltigen Wert haben"},
	{"Invalid value for Actions", "Ung\u00fcltiger Wert f\u00fcr Aktionen"},
	{"No permission selected", "Keine Berechtigung ausgew\u00e4hlt"},
	{"Warning: Invalid argument(s) for constructor: ",
		"Warnung: Ung\u00fcltige(s) Argument(e) f\u00fcr Konstruktor: "},
	{"Add Principal", "Principal hinzuf\u00fcgen"},
	{"Edit Principal", "Principal bearbeiten"},
	{"Remove Principal", "Principal l\u00f6schen"},
	{"Principal Type:", "Principal-Typ:"},
        {"Principal Name:", "Principal-Name:"},
	{"Illegal Principal Type", "Ung\u00fcltiger Principal-Typ"},
	{"No principal selected", "Kein Principal ausgew\u00e4hlt"},
	{"Principals:", "Principals:"},
	{"Principals", "Principals"},
	{"  Add New Principal:", " Neuen Principal hinzuf\u00fcgen:"},
	{"  Edit Principal:", " Principal bearbeiten:"},
	{"name", "Name"},
	{"Cannot Specify Principal with a Wildcard Class without a Wildcard Name",
	    "Principal kann nicht mit einer Wildcard-Klasse ohne Wildcard-Namen angegeben werden"},
	{"Cannot Specify Principal without a Class",
	    "Principal kann nicht ohne eine Klasse angegeben werden"},

        {"Cannot Specify Principal without a Name",
            "Principal kann nicht ohne einen Namen angegeben werden"},

	// javax.security.auth.PrivateCredentialPermission
	{"invalid null input(s)", "Ung\u00fcltige Null-Eingabe(n)"},
	{"actions can only be 'read'", "Aktionen k\u00f6nnen nur 'gelesen' werden"},
	{"permission name [name] syntax invalid: ",
		"Syntax f\u00fcr Berechtigungsnamen [{0}] ung\u00fcltig: "},
	{"Credential Class not followed by a Principal Class and Name",
		"Nach Authentisierungsklasse folgt keine Principal-Klasse und kein Name."},
	{"Principal Class not followed by a Principal Name",
		"Nach Principal-Klasse folgt kein Principal-Name"},
	{"Principal Name must be surrounded by quotes",
		"Principal-Name muss vorn und hinten mit Anf\u00fchrungsstrichen versehen sein"},
	{"Principal Name missing end quote",
		"Abschlie\u00dfendes Anf\u00fchrungszeichen f\u00fcr Principal-Name fehlt"},
	{"PrivateCredentialPermission Principal Class can not be a wildcard (*) value if Principal Name is not a wildcard (*) value",
		"Private Authentisierungsberechtigung Principal-Klasse kann kein Wildcardwert (*) sein, wenn der Principal-Name kein Wildcardwert (*) ist."},
	{"CredOwner:\n\tPrincipal Class = class\n\tPrincipal Name = name",
		"Authentisierungsbesitzer:\n\tPrincipal-Klasse = {0}\n\tPrincipal-Name = {1}"},

	// javax.security.auth.x500
	{"provided null name", "hat Null als Namen geliefert"},

	// javax.security.auth.Subject
	{"invalid null AccessControlContext provided",
		"Ung\u00fcltige Null als Zugangskontrollkontext geliefert"},
	{"invalid null action provided", "Ung\u00fcltige Null als Aktion geliefert"},
	{"invalid null Class provided", "Ung\u00fcltige Null als Klasse geliefert"},
	{"Subject:\n", "Betreff:\n"},
	{"\tPrincipal: ", "\tPrincipal: "},
	{"\tPublic Credential: ", "\t\u00d6ffentliche Authentisierung: "},
	{"\tPrivate Credentials inaccessible\n",
		"\tKein Zugriff auf private Authentisierungen m\u00f6glich\n"},
	{"\tPrivate Credential: ", "\tPrivate Authentisierung: "},
	{"\tPrivate Credential inaccessible\n",
		"\tKein Zugriff auf private Authentisierung m\u00f6glich\n"},
	{"Subject is read-only", "Betreff ist schreibgesch\u00fctzt"},
	{"attempting to add an object which is not an instance of java.security.Principal to a Subject's Principal Set",
		"Es wird versucht, ein Objekt hinzuzuf\u00fcgen, das keine Instanz von java.security.Principal f\u00fcr eine Principal-Gruppe eines Betreffs ist."},
	{"attempting to add an object which is not an instance of class",
		"Es wird versucht, ein Objekt hinzuzuf\u00fcgen, das keine Instanz von {0} ist."},

	// javax.security.auth.login.AppConfigurationEntry
	{"LoginModuleControlFlag: ", "Anmeldemodul-Steuerflag: "},

	// javax.security.auth.login.LoginContext
	{"Invalid null input: name", "Ung\u00fcltige Nulleingabe: Name"},
	{"No LoginModules configured for name",
	 "F\u00fcr {0} sind keine Anmeldemodule konfiguriert."},
	{"invalid null Subject provided", "Ung\u00fcltige Null als Betreff geliefert"},
	{"invalid null CallbackHandler provided",
		"Ung\u00fcltige Null als Callback-Handler geliefert"},
	{"null subject - logout called before login",
		"Null-Betreff - Abmeldung vor Anmeldung aufgerufen"},
	{"unable to instantiate LoginModule, module, because it does not provide a no-argument constructor",
		"Es kann keine Instanz des Anmeldemoduls {0} erstellt werden, weil es keinen argumentlosen Konstruktor liefert."},
	{"unable to instantiate LoginModule",
		"Es kann keine Instanz des Anmeldemoduls erstellt werden."},
	{"unable to find LoginModule class: ",
		"Die Anmeldemodulklasse kann nicht gefunden werden: "},
	{"unable to access LoginModule: ",
		"Kein Zugriff auf Anmeldemodul m\u00f6glich: "},
	{"Login Failure: all modules ignored",
		"Anmeldefehler: Alle Module werden ignoriert"},

	// sun.security.provider.PolicyFile

	{"java.security.policy: error parsing policy:\n\tmessage",
		"java.security.policy: Fehler bei Analyse {0}:\n\t{1}"},
	{"java.security.policy: error adding Permission, perm:\n\tmessage",
		"java.security.policy: Fehler beim Hinzuf\u00fcgen der Genehmigung, {0}:\n\t{1}"},
	{"java.security.policy: error adding Entry:\n\tmessage",
		"java.security.policy: Fehler beim Hinzuf\u00fcgen des Eintrags:\n\t{0}"},
	{"alias name not provided (pe.name)", "Aliasname nicht bereitgestellt ({0})"},
	{"unable to perform substitution on alias, suffix",
		"kann Substition von Alias nicht durchf\u00fchren, {0}"},
	{"substitution value, prefix, unsupported",
		"Substitutionswert, {0}, nicht unterst\u00fctzt"},
	{"(", "("},
	{")", ")"},
	{"type can't be null","Typ kann nicht Null sein"},

	// sun.security.provider.PolicyParser
	{"keystorePasswordURL can not be specified without also specifying keystore",
		"keystorePasswordURL kann nicht ohne Keystore angegeben werden"},
	{"expected keystore type", "erwarteter Keystore-Typ"},
	{"expected keystore provider", "erwarteter Keystore-Provider"},
	{"multiple Codebase expressions",
	        "mehrere Codebase-Ausdr\u00fccke"},
        {"multiple SignedBy expressions","mehrere SignedBy-Ausdr\u00fccke"},
	{"SignedBy has empty alias","Leerer Alias in SignedBy"},
	{"can not specify Principal with a wildcard class without a wildcard name",
		"Kann Principal nicht mit einer Wildcard-Klasse ohne Wildcard-Namen angeben."},
	{"expected codeBase or SignedBy or Principal",
		"CodeBase oder SignedBy oder Principal erwartet"},
	{"expected permission entry", "Berechtigungseintrag erwartet"},
	{"number ", "Nummer "},
	{"expected [expect], read [end of file]",
		"erwartet [{0}], gelesen [Dateiende]"},
	{"expected [;], read [end of file]",
		"erwartet [;], gelesen [Dateiende]"},
	{"line number: msg", "Zeile {0}: {1}"},
	{"line number: expected [expect], found [actual]",
		"Zeile {0}: erwartet [{1}], gefunden [{2}]"},
	{"null principalClass or principalName",
		"Principal-Klasse oder Principal-Name Null"},

	// sun.security.pkcs11.SunPKCS11
		{"PKCS11 Token [providerName] Password: ",
			"Passwort f\u00fcr PKCS11-Token [{0}]: "},

	/* --- DEPRECATED --- */
	// javax.security.auth.Policy
	{"unable to instantiate Subject-based policy",
		"Es kann keine Instanz f\u00fcr Betreff-basierte Richtlinien erstellt werden."}
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


