/*
 * @(#)byteorder_md.h	1.16 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

/*-
 * Win32 dependent machine byte ordering (actually intel ordering)
 */

#ifndef _JAVASOFT_WIN32_BYTEORDER_MD_H_
#define _JAVASOFT_WIN32_BYTEORDER_MD_H_

#ifdef	x86
#define ntohl(x)	((x << 24) | 				\
 			  ((x & 0x0000ff00) << 8) |		\
			  ((x & 0x00ff0000) >> 8) | 		\
			  (((unsigned long)(x & 0xff000000)) >> 24))
#define ntohs(x)	(((x & 0xff) << 8) | ((x >> 8) & (0xff)))
#define htonl(x)	ntohl(x)
#define htons(x)	ntohs(x)
#else	/* x86 */
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#define htonl(x)	(x)
#define htons(x)	(x)
#endif	/* x86 */

#endif /* !_JAVASOFT_WIN32_BYTEORDER_MD_H_ */
