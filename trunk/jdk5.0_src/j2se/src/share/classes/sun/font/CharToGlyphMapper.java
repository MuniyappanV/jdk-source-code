/* @(#)CharToGlyphMapper.java	1.3 12/19/03
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.font;

/* 
 * NB the versions that take a char as an int are used by the opentype
 * layout engine. If that remains in native these methods may not be
 * needed in the Java class.
 */
public abstract class CharToGlyphMapper {

    public static final int HI_SURROGATE_START = 0xD800;
    public static final int HI_SURROGATE_END = 0xDBFF;
    public static final int LO_SURROGATE_START = 0xDC00;
    public static final int LO_SURROGATE_END = 0xDFFF;

    public static final int UNINITIALIZED_GLYPH = -1;
    public static final int INVISIBLE_GLYPH_ID = 0xffff;

    protected int missingGlyph = CharToGlyphMapper.UNINITIALIZED_GLYPH;

    public int getMissingGlyphCode() {
	return missingGlyph;
    }

    /* Default implementations of these methods may be overridden by
     * subclasses which have special requirements or optimisations
     */

    public boolean canDisplay(char ch) {
	int glyph = charToGlyph(ch);
	return glyph != missingGlyph;
    }

    public boolean canDisplay(int cp) {
	int glyph = charToGlyph(cp);
	return glyph != missingGlyph;
    }

    public int charToGlyph(char unicode) {
	char[] chars = new char[1];
	int[] glyphs = new int[1];
	chars[0] = unicode;
        charsToGlyphs(1, chars, glyphs);
        return glyphs[0];
    }

    public int charToGlyph(int unicode) {
	int[] chars = new int[1];
	int [] glyphs = new int[1];
	chars[0] = unicode;
	charsToGlyphs(1, chars, glyphs);
        return glyphs[0];
    }

    public abstract int getNumGlyphs();

    public abstract void charsToGlyphs(int count,
				       char[] unicodes, int[] glyphs);

    public abstract boolean charsToGlyphsNS(int count,
					    char[] unicodes, int[] glyphs);

    public abstract void charsToGlyphs(int count,
				       int[] unicodes, int[] glyphs);

}
