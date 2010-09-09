/*
 * @(#)StandardTextSource.java	1.17 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*
 * @(#)StandardTextSource.java	1.8 00/03/15
 *
 * (C) Copyright IBM Corp. 1998-2003 - All Rights Reserved
 */

package sun.font;

import java.awt.Font;
import java.awt.font.FontRenderContext;
import java.awt.font.LineMetrics;

public class StandardTextSource extends TextSource {
  char[] chars;
  int start;
  int len;
  int cstart;
  int clen;
  int level; // assumed all uniform
  int flags; // see GlyphVector.java
  Font font;
  FontRenderContext frc;
  CoreMetrics cm;

  /**
   * Create a simple implementation of a TextSource.
   *
   * Chars is an array containing clen chars in the context, in
   * logical order, contiguously starting at cstart.  Start and len
   * represent that portion of the context representing the true
   * source; start, like cstart, is relative to the start of the
   * character array.
   *
   * Level is the bidi level (0-63 for the entire context. Flags is
   * the layout flags. Font is the font, frc is the render context,
   * and lm is the line metrics for the entire source text, but not
   * necessarily the context.  
   */
  public StandardTextSource(char[] chars,
			    int start,
			    int len,
			    int cstart,
			    int clen,
			    int level,
			    int flags,
			    Font font,
			    FontRenderContext frc,
			    CoreMetrics cm) {
    if (chars == null) {
      throw new IllegalArgumentException("bad chars: null");
    }
    if (cstart < 0) {
      throw new IllegalArgumentException("bad cstart: " + cstart);
    }
    if (start < cstart) {
      throw new IllegalArgumentException("bad start: " + start + " for cstart: " + cstart);
    }
    if (clen < 0) {
      throw new IllegalArgumentException("bad clen: " + clen);
    }
    if (cstart + clen > chars.length) {
      throw new IllegalArgumentException("bad clen: " + clen + " cstart: " + cstart + " for array len: " + chars.length);
    }
    if (len < 0) {
      throw new IllegalArgumentException("bad len: " + len);
    }
    if ((start + len) > (cstart + clen)) {
      throw new IllegalArgumentException("bad len: " + len + " start: " + start + " for cstart: " + cstart + " clen: " + clen);
    }
    if (font == null) {
      throw new IllegalArgumentException("bad font: null");
    }
    if (frc == null) {
      throw new IllegalArgumentException("bad frc: null");
    }

    this.chars = chars;
    this.start = start;
    this.len = len;
    this.cstart = cstart;
    this.clen = clen;
    this.level = level;
    this.flags = flags;
    this.font = font;
    this.frc = frc;

    if (cm != null) {
        this.cm = cm;
    } else {
        LineMetrics metrics = font.getLineMetrics(chars, cstart, clen, frc);
        this.cm = ((FontLineMetrics)metrics).cm;
    }
  }

  /** Create a StandardTextSource whose context is coextensive with the source. */
  public StandardTextSource(char[] chars,
			    int start,
			    int len,
			    int level,
			    int flags,
			    Font font,
			    FontRenderContext frc,
			    CoreMetrics cm) {
    this(chars, start, len, start, len, level, flags, font, frc, cm);
  }

  /** Create a StandardTextSource whose context and source are coextensive with the entire char array. */
  public StandardTextSource(char[] chars,
			    int level,
			    int flags,
			    Font font,
			    FontRenderContext frc) {
    this(chars, 0, chars.length, 0, chars.length, level, flags, font, frc, null);
  }

  /** Create a StandardTextSource whose context and source are all the text in the String. */
  public StandardTextSource(String str,
			    int level,
			    int flags,
			    Font font,
			    FontRenderContext frc) {
    this(str.toCharArray(), 0, str.length(), 0, str.length(), level, flags, font, frc, null);
  }

  // TextSource API

  public char[] getChars() {
    return chars;
  }

  public int getStart() {
    return start;
  }

  public int getLength() {
    return len;
  }

  public int getContextStart() {
    return cstart;
  }

  public int getContextLength() {
    return clen;
  }

  public int getLayoutFlags() {
    return flags;
  }

  public int getBidiLevel() {
    return level;
  }

  public Font getFont() {
    return font;
  }

  public FontRenderContext getFRC() {
    return frc;
  }

  public CoreMetrics getCoreMetrics() {
    return cm;
  }

  public TextSource getSubSource(int start, int length, int dir) {
    if (start < 0 || length < 0 || (start + length) > len) {
      throw new IllegalArgumentException("bad start (" + start + ") or length (" + length + ")");
    }

    int level = this.level;
    if (dir != TextLineComponent.UNCHANGED) {
        boolean ltr = (flags & 0x8) == 0;
        if (!(dir == TextLineComponent.LEFT_TO_RIGHT && ltr) &&
                !(dir == TextLineComponent.RIGHT_TO_LEFT && !ltr)) {
            throw new IllegalArgumentException("direction flag is invalid");
        }
        level = ltr? 0 : 1;
    }

    return new StandardTextSource(chars, this.start + start, length, cstart, clen, level, flags, font, frc, cm);
  }

  public String toString() {
    return toString(WITH_CONTEXT);
  }

  public String toString(boolean withContext) {
    StringBuffer buf = new StringBuffer(super.toString());
    buf.append("[start:");
    buf.append(start);
    buf.append(", len:" );
    buf.append(len);
    buf.append(", cstart:");
    buf.append(cstart);
    buf.append(", clen:" );
    buf.append(clen);
    buf.append(", chars:\"");
    int chStart, chLimit;
    if (withContext == WITH_CONTEXT) {
        chStart = cstart;
        chLimit = cstart + clen;
    }
    else {
        chStart = start;
        chLimit = start + len;
    }
    for (int i = chStart; i < chLimit; ++i) {
      if (i > chStart) {
	buf.append(" ");
      }
      buf.append(Integer.toHexString(chars[i]));
    }
    buf.append("\"");
    buf.append(", level:");
    buf.append(level);
    buf.append(", flags:");
    buf.append(flags);
    buf.append(", font:");
    buf.append(font);
    buf.append(", frc:");
    buf.append(frc);
    buf.append(", cm:");
    buf.append(cm);
    buf.append("]");

    return buf.toString();
  }
}
