/*
 * @(#)AliasFileParser.java	1.1 04/02/23
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.jvmstat.perfdata.monitor;

import java.net.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;

/**
 * Class for parsing alias files. File format is expected to follow
 * the following syntax:
 *
 *     alias name [alias]*
 *
 * Java style comments can occur anywhere within the file.
 * @author Brian Doherty
 * @version 1.1, 02/23/04
 * @since 1.5
 */
public class AliasFileParser {
    private static final String ALIAS = "alias";
    private static final boolean DEBUG = false;

    // other variables
    private URL inputfile;
    private StreamTokenizer st;
    private Token currentToken;

    AliasFileParser(URL inputfile) {
        this.inputfile = inputfile;
    }

    // value class to hold StreamTokenizer token values
    private class Token {
        public String sval;
        public int ttype;

        public Token(int ttype, String sval) {
            this.ttype = ttype;
            this.sval = sval;
        }
    }

    private void logln(String s) {
        if (DEBUG) {
            System.err.println(s);
        }
    }

    /**
     * method to get the next token as a Token type
     */
    private void nextToken() throws IOException {
        st.nextToken();
        currentToken = new Token(st.ttype, st.sval);

        logln("Read token: type = " + currentToken.ttype
              + " string = " + currentToken.sval);
    }

    /**
     * method to match the current Token to a specified token type and
     * value Throws a SyntaxException if token doesn't match.
     */
    private void match(int ttype, String token)
                 throws IOException, SyntaxException {

        if ((currentToken.ttype == ttype)
                && (currentToken.sval.compareTo(token) == 0)) {
            logln("matched type: " + ttype + " and token = "
                  + currentToken.sval);
            nextToken();
        } else {
            throw new SyntaxException(st.lineno());
        }
    }


    /*
     * method to match the current Token to a specified token type.
     * Throws a SyntaxException if token doesn't match.
     */
    private void match(int ttype) throws IOException, SyntaxException {
        if (currentToken.ttype == ttype) {
            logln("matched type: " + ttype + ", token = " + currentToken.sval);
            nextToken();
        } else {
            throw new SyntaxException(st.lineno());
        }
    }

    private void match(String token) throws IOException, SyntaxException {
        match(StreamTokenizer.TT_WORD, token);
    }

    /**
     * method to parse the given input file.
     */
    public void parse(Map map) throws SyntaxException, IOException {

        if (inputfile == null) {
            return;
        }

        BufferedReader r = new BufferedReader(
                new InputStreamReader(inputfile.openStream()));
        st = new StreamTokenizer(r);

        // allow both forms of commenting styles
        st.slashSlashComments(true);
        st.slashStarComments(true);
        st.wordChars('_','_');

        nextToken();

        while (currentToken.ttype != StreamTokenizer.TT_EOF) {
            // look for the start symbol
            if ((currentToken.ttype != StreamTokenizer.TT_WORD)
                    || (currentToken.sval.compareTo(ALIAS) != 0)) {
                nextToken();
                continue;
            }

            match(ALIAS);
            String name = currentToken.sval;
            match(StreamTokenizer.TT_WORD);

            ArrayList aliases = new ArrayList();

            do {
                aliases.add(currentToken.sval);
                match(StreamTokenizer.TT_WORD);

            } while ((currentToken.ttype != StreamTokenizer.TT_EOF)
                     && (currentToken.sval.compareTo(ALIAS) != 0));

            logln("adding map entry for " + name + " values = " + aliases);

            map.put(name, aliases);
        }
    }
}
