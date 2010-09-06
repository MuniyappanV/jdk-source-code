/*
 * @(#)sqltypes.h	1.9 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
/*********************************************************************
** SQLTYPES.H - This file defines the types used in ODBC
**
** Copyright: 1992-1999 MERANT, Inc.	
** This software contains confidential and proprietary
** information of MERANT, Inc.
**
** (C) Copyright 1995-1997 By Microsoft Corp.
**
**		Created 04/10/95 for 2.50 specification
**		Updated 12/11/95 for 3.00 specification
*********************************************************************/

#ifndef __SQLTYPES
#define __SQLTYPES

/* if ODBCVER is not defined, assume version 3.51 */
#ifndef ODBCVER
#define ODBCVER	0x0351
#endif  /* ODBCVER */

#ifdef __cplusplus
extern "C" { 			/* Assume C declarations for C++   */
#endif  /* __cplusplus */

/* environment specific definitions */
#ifndef EXPORT
#define EXPORT   
#endif

#define SQL_API

#ifndef RC_INVOKED

/* API declaration data types */
typedef unsigned char   SQLCHAR;
#if (ODBCVER >= 0x0300)
/*typedef signed char     SQLSCHAR;*/
typedef unsigned char   SQLDATE;
typedef unsigned char   SQLDECIMAL;
typedef double          SQLDOUBLE;
typedef double          SQLFLOAT;
#endif
#ifndef _LP64
typedef long            SQLINTEGER;
/*typedef unsigned long   SQLUINTEGER;*/
#else
typedef int           	SQLINTEGER;
/*typedef unsigned int	SQLUINTEGER;*/
#endif
#if (ODBCVER >= 0x0300)
typedef unsigned char   SQLNUMERIC;
#endif
typedef void *          SQLPOINTER;
#if (ODBCVER >= 0x0300)
typedef float           SQLREAL;
#endif
typedef short           SQLSMALLINT;
typedef unsigned short  SQLUSMALLINT;
#if (ODBCVER >= 0x0300)
typedef unsigned char   SQLTIME;
typedef unsigned char   SQLTIMESTAMP;
typedef unsigned char   SQLVARCHAR;
#endif

/* function return type */
typedef SQLSMALLINT     SQLRETURN;

/* generic data structures */
#if (ODBCVER >= 0x0300)
typedef void*			SQLHANDLE;
typedef SQLHANDLE               SQLHENV;
typedef SQLHANDLE               SQLHDBC;
typedef SQLHANDLE               SQLHSTMT;
typedef SQLHANDLE               SQLHDESC;
#else
typedef void*			SQLHENV;
typedef void*			SQLHDBC;
typedef void*			SQLHSTMT;
#endif /* ODBCVER >= 0x0300 */

/* SQL portable types for C */
typedef unsigned char           UCHAR;
typedef signed char             SCHAR;
typedef SCHAR                   SQLSCHAR;
#ifndef _LP64
typedef long int                SDWORD;
#else
typedef int                	SDWORD;
#endif
typedef short int               SWORD;
#ifndef _LP64
typedef unsigned long int       UDWORD;
#else
typedef unsigned int       	UDWORD;
#endif
typedef unsigned short int      UWORD;
typedef UDWORD                  SQLUINTEGER;

#ifndef _LP64
typedef signed long             SLONG;
#else
typedef signed int             	SLONG;
#endif
typedef signed short            SSHORT;
#ifndef _LP64
typedef unsigned long           ULONG;
#else
typedef unsigned int           	ULONG;
#endif
typedef unsigned short          USHORT;
typedef double                  SDOUBLE;
typedef double            		LDOUBLE; 
typedef float                   SFLOAT;

typedef void*              		PTR;

typedef void*              		HENV;
typedef void*              		HDBC;
typedef void*              		HSTMT;

typedef signed short            RETCODE;

typedef SQLPOINTER              SQLHWND;

#ifndef	__SQLDATE
#define	__SQLDATE
/* transfer types for DATE, TIME, TIMESTAMP */
typedef struct tagDATE_STRUCT
{
        SQLSMALLINT    year;
        SQLUSMALLINT   month;
        SQLUSMALLINT   day;
} DATE_STRUCT;

#if (ODBCVER >= 0x0300)
typedef DATE_STRUCT	SQL_DATE_STRUCT;
#endif  /* ODBCVER >= 0x0300 */

typedef struct tagTIME_STRUCT
{
        SQLUSMALLINT   hour;
        SQLUSMALLINT   minute;
        SQLUSMALLINT   second;
} TIME_STRUCT;

#if (ODBCVER >= 0x0300)
typedef TIME_STRUCT	SQL_TIME_STRUCT;
#endif /* ODBCVER >= 0x0300 */

typedef struct tagTIMESTAMP_STRUCT
{
        SQLSMALLINT    year;
        SQLUSMALLINT   month;
        SQLUSMALLINT   day;
        SQLUSMALLINT   hour;
        SQLUSMALLINT   minute;
        SQLUSMALLINT   second;
        SQLUINTEGER    fraction;
} TIMESTAMP_STRUCT;

#if (ODBCVER >= 0x0300)
typedef TIMESTAMP_STRUCT	SQL_TIMESTAMP_STRUCT;
#endif  /* ODBCVER >= 0x0300 */


/*
 * enumerations for DATETIME_INTERVAL_SUBCODE values for interval data types
 * these values are from SQL-92
 */

#if (ODBCVER >= 0x0300)
typedef enum 
{
	SQL_IS_YEAR						= 1,
	SQL_IS_MONTH					= 2,
	SQL_IS_DAY						= 3,
	SQL_IS_HOUR						= 4,
	SQL_IS_MINUTE					= 5,
	SQL_IS_SECOND					= 6,
	SQL_IS_YEAR_TO_MONTH			= 7,
	SQL_IS_DAY_TO_HOUR				= 8,
	SQL_IS_DAY_TO_MINUTE			= 9,
	SQL_IS_DAY_TO_SECOND			= 10,
	SQL_IS_HOUR_TO_MINUTE			= 11,
	SQL_IS_HOUR_TO_SECOND			= 12,
	SQL_IS_MINUTE_TO_SECOND			= 13
} SQLINTERVAL;

#endif  /* ODBCVER >= 0x0300 */

#if (ODBCVER >= 0x0300)
typedef struct tagSQL_YEAR_MONTH
{
		SQLUINTEGER		year;
		SQLUINTEGER		month;
} SQL_YEAR_MONTH_STRUCT;

typedef struct tagSQL_DAY_SECOND
{
		SQLUINTEGER		day;
		SQLUINTEGER		hour;
		SQLUINTEGER		minute;
		SQLUINTEGER		second;
		SQLUINTEGER		fraction;
} SQL_DAY_SECOND_STRUCT;

typedef struct tagSQL_INTERVAL_STRUCT
{
	SQLINTERVAL		interval_type;
	SQLSMALLINT		interval_sign;
	union {
		SQL_YEAR_MONTH_STRUCT		year_month;
		SQL_DAY_SECOND_STRUCT		day_second;
	} intval;

} SQL_INTERVAL_STRUCT;

#endif  /* ODBCVER >= 0x0300 */

#endif	/* __SQLDATE	*/

/* the ODBC C types for SQL_C_SBIGINT and SQL_C_UBIGINT */
#if (ODBCVER >= 0x0300)
#if (_MSC_VER >= 900)
#define ODBCINT64	__int64
#endif  

/* define ODBCINT64 for Unix */
#ifndef ODBCINT64
#ifndef _LP64
#define ODBCINT64	long long
#else
#define ODBCINT64	long
#endif
#endif

/* If using other compilers, define ODBCINT64 to the 
	approriate 64 bit integer type */
#ifdef ODBCINT64
typedef ODBCINT64	SQLBIGINT;
typedef unsigned ODBCINT64	SQLUBIGINT;
#endif
#endif  /* ODBCVER >= 0x0300 */

/* internal representation of numeric data type */
#if (ODBCVER >= 0x0300)
#define SQL_MAX_NUMERIC_LEN		16
typedef struct tagSQL_NUMERIC_STRUCT
{
	SQLCHAR		precision;
	SQLSCHAR	scale;
	SQLCHAR		sign;	/* 1 if positive, 0 if negative */
	SQLCHAR		val[SQL_MAX_NUMERIC_LEN];
} SQL_NUMERIC_STRUCT;
#endif  /* ODBCVER >= 0x0300 */

/* This is actually from 0x0350 */
#if (ODBCVER >= 0x0300)
typedef struct  tagSQLGUID
{
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[ 8 ];
} SQLGUID;
#endif  /* ODBCVER >= 0x0300 */

#ifndef _LP64
typedef unsigned long int       BOOKMARK;
#else
typedef unsigned int       BOOKMARK;
#endif

typedef WCHAR		SQLWCHAR;

#ifdef UNICODE
typedef SQLWCHAR        SQLTCHAR;
#else
typedef SQLCHAR         SQLTCHAR;
#endif  /* UNICODE */



#endif     /* RC_INVOKED */


#ifdef __cplusplus
}                                    /* End of extern "C" { */
#endif  /* __cplusplus */

#endif /* #ifndef __SQLTYPES */
