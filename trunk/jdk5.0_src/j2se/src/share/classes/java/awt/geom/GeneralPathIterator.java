/*
 * @(#)GeneralPathIterator.java	1.22 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package java.awt.geom;

/**
 * This class represents the iterator for General Paths.
 * It can be used to retrieve all of the elements in a GeneralPath.
 * The {@link GeneralPath#getPathIterator}
 *  method is used to create a
 * GeneralPathIterator for a particular GeneralPath.
 * The iterator can be used to iterator the path only once.
 * Subsequent iterations require a new iterator.
 *
 * @see GeneralPath
 *
 * @version 10 Feb 1997
 * @author	Jim Graham
 */
class GeneralPathIterator implements PathIterator {
    int typeIdx = 0;
    int pointIdx   = 0;
    GeneralPath path;
    AffineTransform affine;

    private static final int curvesize[] = {2, 2, 4, 6, 0};

    /**
     * Constructs an iterator given a GeneralPath.
     * @see GeneralPath#getPathIterator
     */
    GeneralPathIterator(GeneralPath path) {
	this(path, null);
    }

    /**
     * Constructs an iterator given a GeneralPath and an optional
     * AffineTransform.
     * @see GeneralPath#getPathIterator
     */
    GeneralPathIterator(GeneralPath path, AffineTransform at) {
        this.path = path;
	this.affine = at;
    }

    /**
     * Return the winding rule for determining the interior of the
     * path.
     * @see PathIterator#WIND_EVEN_ODD
     * @see PathIterator#WIND_NON_ZERO
     */
    public int getWindingRule() {
	return path.getWindingRule();
    }

    /**
     * Tests if there are more points to read.
     * @return true if there are more points to read
     */
    public boolean isDone() {
        return (typeIdx >= path.numTypes);
    }

    /**
     * Moves the iterator to the next segment of the path forwards
     * along the primary direction of traversal as long as there are
     * more points in that direction.
     */
    public void next() {
	int type = path.pointTypes[typeIdx++];
	pointIdx += curvesize[type];
    }

    /**
     * Returns the coordinates and type of the current path segment in
     * the iteration.
     * The return value is the path segment type:
     * SEG_MOVETO, SEG_LINETO, SEG_QUADTO, SEG_CUBICTO, or SEG_CLOSE.
     * A float array of length 6 must be passed in and may be used to
     * store the coordinates of the point(s).
     * Each point is stored as a pair of float x,y coordinates.
     * SEG_MOVETO and SEG_LINETO types will return one point,
     * SEG_QUADTO will return two points,
     * SEG_CUBICTO will return 3 points
     * and SEG_CLOSE will not return any points.
     * @see PathIterator#SEG_MOVETO
     * @see PathIterator#SEG_LINETO
     * @see PathIterator#SEG_QUADTO
     * @see PathIterator#SEG_CUBICTO
     * @see PathIterator#SEG_CLOSE
     */
    public int currentSegment(float[] coords) {
	int type = path.pointTypes[typeIdx];
	int numCoords = curvesize[type];
	if (numCoords > 0 && affine != null) {
	    affine.transform(path.pointCoords, pointIdx,
			     coords, 0,
			     numCoords / 2);
	} else {
	    System.arraycopy(path.pointCoords, pointIdx, coords, 0, numCoords);
	}
        return type;
    }

    /**
     * Returns the coordinates and type of the current path segment in
     * the iteration.
     * The return value is the path segment type:
     * SEG_MOVETO, SEG_LINETO, SEG_QUADTO, SEG_CUBICTO, or SEG_CLOSE.
     * A double array of length 6 must be passed in and may be used to
     * store the coordinates of the point(s).
     * Each point is stored as a pair of double x,y coordinates.
     * SEG_MOVETO and SEG_LINETO types will return one point,
     * SEG_QUADTO will return two points,
     * SEG_CUBICTO will return 3 points
     * and SEG_CLOSE will not return any points.
     * @see PathIterator#SEG_MOVETO
     * @see PathIterator#SEG_LINETO
     * @see PathIterator#SEG_QUADTO
     * @see PathIterator#SEG_CUBICTO
     * @see PathIterator#SEG_CLOSE
     */
    public int currentSegment(double[] coords) {
	int type = path.pointTypes[typeIdx];
	int numCoords = curvesize[type];
	if (numCoords > 0 && affine != null) {
	    affine.transform(path.pointCoords, pointIdx,
			     coords, 0,
			     numCoords / 2);
	} else {
	    for (int i=0; i < numCoords; i++) {
		coords[i] = path.pointCoords[pointIdx + i];
	    }
	}
        return type;
    }
}
