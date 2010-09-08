/*
 * @(#)Blit.c	1.10 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "GraphicsPrimitiveMgr.h"
#include "Region.h"

#include "sun_java2d_loops_Blit.h"

/*
 * Class:     sun_java2d_loops_Blit
 * Method:    Blit
 * Signature: (Lsun/java2d/SurfaceData;Lsun/java2d/SurfaceData;Ljava/awt/Composite;IIIIII)V
 */
JNIEXPORT void JNICALL
Java_sun_java2d_loops_Blit_Blit
    (JNIEnv *env, jobject self,
     jobject srcData, jobject dstData, jobject comp, jobject clip,
     jint srcx, jint srcy, jint dstx, jint dsty, jint width, jint height)
{
    SurfaceDataOps *srcOps;
    SurfaceDataOps *dstOps;
    SurfaceDataRasInfo srcInfo;
    SurfaceDataRasInfo dstInfo;
    NativePrimitive *pPrim;
    CompositeInfo compInfo;
    RegionData clipInfo;
    jint dstFlags;

    pPrim = GetNativePrim(env, self);
    if (pPrim == NULL) {
	return;
    }
    if (pPrim->pCompType->getCompInfo != NULL) {
	(*pPrim->pCompType->getCompInfo)(env, &compInfo, comp);
    }
    if (Region_GetInfo(env, clip, &clipInfo)) {
	return;
    }

    srcOps = SurfaceData_GetOps(env, srcData);
    dstOps = SurfaceData_GetOps(env, dstData);
    if (srcOps == 0 || dstOps == 0) {
	return;
    }

    srcInfo.bounds.x1 = srcx;
    srcInfo.bounds.y1 = srcy;
    srcInfo.bounds.x2 = srcx + width;
    srcInfo.bounds.y2 = srcy + height;
    dstInfo.bounds.x1 = dstx;
    dstInfo.bounds.y1 = dsty;
    dstInfo.bounds.x2 = dstx + width;
    dstInfo.bounds.y2 = dsty + height;
    srcx -= dstx;
    srcy -= dsty;
    SurfaceData_IntersectBounds(&dstInfo.bounds, &clipInfo.bounds);
    if (srcOps->Lock(env, srcOps, &srcInfo, pPrim->srcflags) != SD_SUCCESS) {
	return;
    }

    dstFlags = pPrim->dstflags;
    if (!Region_IsRectangular(&clipInfo)) {
	dstFlags |= SD_LOCK_PARTIAL_WRITE;
    }
    if (dstOps->Lock(env, dstOps, &dstInfo, dstFlags) != SD_SUCCESS) {
	SurfaceData_InvokeUnlock(env, srcOps, &srcInfo);
	return;
    }
    SurfaceData_IntersectBlitBounds(&dstInfo.bounds, &srcInfo.bounds,
				    srcx, srcy);
    Region_IntersectBounds(&clipInfo, &dstInfo.bounds);

    if (!Region_IsEmpty(&clipInfo)) {
	srcOps->GetRasInfo(env, srcOps, &srcInfo);
	dstOps->GetRasInfo(env, dstOps, &dstInfo);
	if (srcInfo.rasBase && dstInfo.rasBase) {
	    SurfaceDataBounds span;
	    jint savesx = srcInfo.bounds.x1;
	    jint savedx = dstInfo.bounds.x1;
	    Region_StartIteration(env, &clipInfo);
	    while (Region_NextIteration(&clipInfo, &span)) {
		void *pSrc = PtrCoord(srcInfo.rasBase,
				      srcx + span.x1, srcInfo.pixelStride,
				      srcy + span.y1, srcInfo.scanStride);
		void *pDst = PtrCoord(dstInfo.rasBase,
				      span.x1, dstInfo.pixelStride,
				      span.y1, dstInfo.scanStride);
		/*
		 * Fix for 4804375
		 * REMIND: There should probably be a better
		 * way to give the span coordinates to the
		 * inner loop.  This is only really needed
		 * for the 1, 2, and 4 bit loops.
		 */
		srcInfo.bounds.x1 = srcx + span.x1;
		dstInfo.bounds.x1 = span.x1;
		(*pPrim->funcs.blit)(pSrc, pDst,
				     span.x2 - span.x1, span.y2 - span.y1,
				     &srcInfo, &dstInfo, pPrim, &compInfo);
	    }
	    Region_EndIteration(env, &clipInfo);
	    srcInfo.bounds.x1 = savesx;
	    dstInfo.bounds.x1 = savedx;
	}
	SurfaceData_InvokeRelease(env, dstOps, &dstInfo);
	SurfaceData_InvokeRelease(env, srcOps, &srcInfo);
    }
    SurfaceData_InvokeUnlock(env, dstOps, &dstInfo);
    SurfaceData_InvokeUnlock(env, srcOps, &srcInfo);
}
