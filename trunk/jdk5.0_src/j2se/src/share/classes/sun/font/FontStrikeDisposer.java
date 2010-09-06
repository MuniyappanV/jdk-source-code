/*
 * @(#)FontStrikeDisposer.java	1.3 03/21/04
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.font;

import sun.java2d.DisposerRecord;

/*
 * This keeps track of data that needs to be cleaned up once a
 * strike is freed.
 * a) The native memory that is the glyph image cache.
 * b) removing the "desc" key from the strike's map.
 * This is safe to do because this disposer is invoked only when the
 * reference object has been cleared, which means the value indexed by
 * this key is just an empty reference object.
 * It is possible that a new FontStrike has been created that would
 * be referenced by the same (equals) key. If it is placed in the map
 * before this disposer is executed, then we do not want to remove that
 * object. We should only remove an object where the value is null.
 * So we first verify that the key still points to a cleared reference.
 * Updates to the map thus need to be synchronized.
 *
 * A WeakHashmap will automatically clean up, but we might maintain a
 * reference to the "desc" key in the FontStrike (value) which would
 * prevent the keys from being discarded. And since the strike is the only
 * place is likely we would maintain such a strong reference, then the map
 * entries would be removed much more promptly than we need.
 */

class FontStrikeDisposer extends DisposerRecord {

    Font2D font2D;
    FontStrikeDesc desc;
    long[] longGlyphImages;
    int [] intGlyphImages;
    long pScalerContext = 0L;
    boolean disposed = false;
    boolean comp = false;

    public FontStrikeDisposer(Font2D font2D, FontStrikeDesc desc,
			      long pContext, int[] images) {
	this.font2D = font2D;
	this.desc = desc;
	this.pScalerContext = pContext;
	this.intGlyphImages = images;
    }

    public FontStrikeDisposer(Font2D font2D, FontStrikeDesc desc,
			      long pContext, long[] images) {
	this.font2D = font2D;
	this.desc = desc;
	this.pScalerContext = pContext;
	this.longGlyphImages = images;
    }

    public FontStrikeDisposer(Font2D font2D, FontStrikeDesc desc,
			      long pContext) {
	this.font2D = font2D;
	this.desc = desc;
	this.pScalerContext = pContext;
    }

    public FontStrikeDisposer(Font2D font2D, FontStrikeDesc desc) {
	this.font2D = font2D;
	this.desc = desc;
	this.comp = true;
    }

    public synchronized void dispose() {
	if (!disposed) {
	    font2D.removeFromCache(desc);
	    StrikeCache.disposeStrike(this);
	    disposed = true;
	}
    }
}
