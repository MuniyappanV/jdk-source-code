/*
 * @(#)WinNTFileSystem_md.c	1.18 04/02/17
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <direct.h>
#include <windows.h>
#include <io.h>

#include "jvm.h"
#include "jni.h"
#include "jni_util.h"
#include "io_util.h"
#include "io_util_md.h"
#include "dirent_md.h"
#include "java_io_FileSystem.h"

#define WITH_UNICODE_PATH(env, object, id, var)                               \
    WITH_UNICODE_STRING(env,                                                  \
             ((object == NULL)                                                \
              ? NULL                                                          \
              : (*(env))->GetObjectField((env), (object), (id))),             \
             var)

#define END_UNICODE_PATH(env, var) END_UNICODE_STRING(env, var)

#define MAX_PATH_LENGTH 1024

#define SHORT_DIR_LIMIT 248

static struct {
    jfieldID path;
} ids;

JNIEXPORT void JNICALL 
Java_java_io_WinNTFileSystem_initIDs(JNIEnv *env, jclass cls)
{
    jclass fileClass = (*env)->FindClass(env, "java/io/File");
    if (!fileClass) return;
    ids.path = 
             (*env)->GetFieldID(env, fileClass, "path", "Ljava/lang/String;");
}

/* -- Path operations -- */

extern int wcanonicalize(const WCHAR *path, WCHAR *out, int len);
extern int wcanonicalizeWithPrefix(const WCHAR *canonicalPrefix, const WCHAR *pathWithCanonicalPrefix, WCHAR *out, int len);

JNIEXPORT jstring JNICALL 
Java_java_io_WinNTFileSystem_canonicalize0(JNIEnv *env, jobject this,
                                           jstring pathname)
{
    jstring rv = NULL;
    WCHAR canonicalPath[MAX_PATH_LENGTH];

    WITH_UNICODE_STRING(env, pathname, path) {
        if (wcanonicalize(path, canonicalPath, MAX_PATH_LENGTH) >= 0) {
            rv = (*env)->NewString(env, canonicalPath, wcslen(canonicalPath));
        }
    } END_UNICODE_STRING(env, path); 
    if (rv == NULL) {
     	JNU_ThrowIOExceptionWithLastError(env, "Bad pathname");
    }
    return rv;
}


JNIEXPORT jstring JNICALL 
Java_java_io_WinNTFileSystem_canonicalizeWithPrefix0(JNIEnv *env, jobject this,
                                                     jstring canonicalPrefixString,
                                                     jstring pathWithCanonicalPrefixString)
{
    jstring rv = NULL;
    WCHAR canonicalPath[MAX_PATH_LENGTH];

    WITH_UNICODE_STRING(env, canonicalPrefixString, canonicalPrefix) {
        WITH_UNICODE_STRING(env, pathWithCanonicalPrefixString, pathWithCanonicalPrefix) {
            if (wcanonicalizeWithPrefix(canonicalPrefix,
                                        pathWithCanonicalPrefix,
                                        canonicalPath, MAX_PATH_LENGTH) >= 0) {
                rv = (*env)->NewString(env, canonicalPath, wcslen(canonicalPath));
            }
        } END_UNICODE_STRING(env, pathWithCanonicalPrefix);
    } END_UNICODE_STRING(env, canonicalPrefix);
    if (rv == NULL) {
     	JNU_ThrowIOExceptionWithLastError(env, "Bad pathname");
    }
    return rv;
}


/* -- Attribute accessors -- */

JNIEXPORT jint JNICALL
Java_java_io_WinNTFileSystem_getBooleanAttributes(JNIEnv *env, jobject this,
                                                  jobject file) 
{

    jint rv = 0;
    jint pathlen;

#define PAGEFILE_NAMELEN 12 

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        DWORD a = GetFileAttributesW(path);
        if (a != ((DWORD)-1)) {
            rv = (java_io_FileSystem_BA_EXISTS
              | ((a & FILE_ATTRIBUTE_DIRECTORY)
                 ? java_io_FileSystem_BA_DIRECTORY
                : java_io_FileSystem_BA_REGULAR)
              | ((a & FILE_ATTRIBUTE_HIDDEN)
                 ? java_io_FileSystem_BA_HIDDEN : 0));
        } else { /* pagefile.sys is a special case */
            if (GetLastError() == ERROR_SHARING_VIOLATION)
               if ((pathlen = wcslen(path)) >= PAGEFILE_NAMELEN && 
                    _wcsicmp(path + pathlen - PAGEFILE_NAMELEN, 
                             L"pagefile.sys") == 0) 
                    rv = java_io_FileSystem_BA_EXISTS | 
                         java_io_FileSystem_BA_REGULAR;
        }
    } END_UNICODE_PATH(env, path);
    return rv;
}


JNIEXPORT jboolean 
JNICALL Java_java_io_WinNTFileSystem_checkAccess(JNIEnv *env, jobject this, 
                                                 jobject file, jboolean write) 
{
    jboolean rv = JNI_FALSE;

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        if (_waccess(path, (write ? 2 : 4)) == 0) {
            rv = JNI_TRUE;
        }
    } END_UNICODE_PATH(env, path);
    return rv;
}

JNIEXPORT jlong JNICALL
Java_java_io_WinNTFileSystem_getLastModifiedTime(JNIEnv *env, jobject this,
                                                 jobject file) 
{
    jlong rv = 0;
    WITH_UNICODE_PATH(env, file, ids.path, path) {
        LARGE_INTEGER modTime;
        FILETIME t;
        HANDLE h = CreateFileW(
            path,
            /* Device query access */
            0,
            /* Share it */
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            /* No security attributes */
            NULL,
            /* Open existing or fail */
            OPEN_EXISTING,
            /* Backup semantics for directories */
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
            /* No template file */
            NULL);
        if (h != INVALID_HANDLE_VALUE) {
            GetFileTime(h, NULL, NULL, &t);
            CloseHandle(h);
            modTime.LowPart = (DWORD) t.dwLowDateTime;
            modTime.HighPart = (LONG) t.dwHighDateTime;
            rv = modTime.QuadPart / 10000;
            rv -= 11644473600000;
        } 
    } END_UNICODE_PATH(env, path);
    return rv;
}

JNIEXPORT jlong JNICALL
Java_java_io_WinNTFileSystem_getLength(JNIEnv *env, jobject this, jobject file)
{
    jlong rv = 0;

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        struct _stati64 sb;
        if (_wstati64(path, &sb) == 0) {
            rv = sb.st_size;
        }
    } END_UNICODE_PATH(env, path);
    return rv;
}

/* -- File operations -- */

JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_createFileExclusively(JNIEnv *env, jclass cls,
                                                   jstring path)
{
    HANDLE h = NULL;
    WCHAR *pathbuf = pathToNTPath(env, path, JNI_FALSE);
    if (pathbuf == NULL) {
        return JNI_FALSE;
    }

    h = CreateFileW(
        pathbuf,                             /* Wide char path name */
        GENERIC_READ | GENERIC_WRITE,  /* Read and write permission */
        FILE_SHARE_READ | FILE_SHARE_WRITE,   /* File sharing flags */
        NULL,                                /* Security attributes */
        CREATE_NEW,                         /* creation disposition */
        FILE_ATTRIBUTE_NORMAL,              /* flags and attributes */
        NULL);

    free(pathbuf);

    if (h == INVALID_HANDLE_VALUE) {
        int error = GetLastError();
        if ((error == ERROR_FILE_EXISTS)||(error == ERROR_ALREADY_EXISTS)) {
            return JNI_FALSE;
        }
        JNU_ThrowIOExceptionWithLastError(env, "Could not open file");
        return JNI_FALSE;
    }
    CloseHandle(h);
    return JNI_TRUE;
}

static int 
removeFileOrDirectory(const jchar *path) 
{ 
    /* Returns 0 on success */ 
    DWORD a;

    SetFileAttributesW(path, 0);
    a = GetFileAttributesW(path);
    if (a == ((DWORD)-1)) {
        return 1;
    } else if (a & FILE_ATTRIBUTE_DIRECTORY) {
        return !RemoveDirectoryW(path);
    } else {
        return !DeleteFileW(path);
    }
}

JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_delete0(JNIEnv *env, jobject this, jobject file)
{
    jboolean rv = JNI_FALSE;
    WCHAR *pathbuf = fileToNTPath(env, file, ids.path);
    if (pathbuf == NULL) {
        return JNI_FALSE;
    }
    if (removeFileOrDirectory(pathbuf) == 0) {
        rv = JNI_TRUE;
    }
    free(pathbuf);
    return rv;
}

typedef int (*WDELETEPROC)(const jchar *path);

static struct wdlEntry {
    struct wdlEntry *next;
    WDELETEPROC wdeleteProc;
    jchar name[JVM_MAXPATHLEN + 1];
} *wdeletionList = NULL;

static void
wdeleteOnExitHook(void)		/* Called by the VM on exit */
{
    struct wdlEntry *e, *next;
    for (e = wdeletionList; e; e = next) {
	    next = e->next;
	    e->wdeleteProc(e->name);
	    free(e);
    }
}

void
wdeleteOnExit(JNIEnv *env, const jchar *path, WDELETEPROC dp)
{
    struct wdlEntry *dl = wdeletionList;
    struct wdlEntry *e = (struct wdlEntry *)malloc(sizeof(struct wdlEntry));

    if (e == NULL) {
        JNU_ThrowOutOfMemoryError(env, 0);
        return;
    }

    wcscpy(e->name, path);
    e->wdeleteProc = dp;

    if (dl == NULL) {
        JVM_OnExit(wdeleteOnExitHook);
    }

    e->next = wdeletionList;
    wdeletionList = e;
}

JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_deleteOnExit(JNIEnv *env, jobject this, 
                                          jobject file) 
{
    WITH_UNICODE_PATH(env, file, ids.path, path) {
        wdeleteOnExit(env, path, removeFileOrDirectory);
    } END_UNICODE_PATH(env, path);
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL 
Java_java_io_WinNTFileSystem_list(JNIEnv *env, jobject this, jobject file)
{
    WCHAR *search_path;
    HANDLE handle;
    WIN32_FIND_DATAW find_data;
    int len, maxlen;
    jobjectArray rv, old;
    DWORD fattr;
    jstring name;

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        search_path = (WCHAR*)malloc(2*wcslen(path) + 6);
        if (search_path == 0) {
            errno = ENOMEM;
            return NULL;
        }
        wcscpy(search_path, path);
    } END_UNICODE_PATH(env, path);
    fattr = GetFileAttributesW(search_path);
    if (fattr == ((DWORD)-1)) {
        free(search_path);
        return NULL;
    } else if ((fattr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        free(search_path);
        return NULL;
    }

    /* Remove trailing space chars from directory name */
    len = wcslen(search_path);
    while (search_path[len-1] == ' ') {
        len--;
    }
    search_path[len] = 0;
    
    /* Append "*", or possibly "\\*", to path */
    if ((search_path[0] == L'\\' && search_path[1] == L'\0') ||
        (search_path[1] == L':'
        && (search_path[2] == L'\0'
        || (search_path[2] == L'\\' && search_path[3] == L'\0')))) {
        /* No '\\' needed for cases like "\" or "Z:" or "Z:\" */
        wcscat(search_path, L"*");
    } else {
        wcscat(search_path, L"\\*");
    }

    /* Open handle to the first file */
    handle = FindFirstFileW(search_path, &find_data);
    free(search_path);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            // error
            return NULL;
        } else {
            // No files found - return an empty array
            rv = (*env)->NewObjectArray(env, 0, JNU_ClassString(env), NULL);
            return rv;
        }
    }

    /* Allocate an initial String array */
    len = 0;
    maxlen = 16;
    rv = (*env)->NewObjectArray(env, maxlen, JNU_ClassString(env), NULL);
    if (rv == NULL) // Couldn't allocate an array
        return NULL;
    /* Scan the directory */
    do {
        if (!wcscmp(find_data.cFileName, L".") 
                                || !wcscmp(find_data.cFileName, L".."))
           continue;
        name = (*env)->NewString(env, find_data.cFileName, 
                                 wcslen(find_data.cFileName));
        if (name == NULL)
            return NULL; // error;
        if (len == maxlen) {
            old = rv;
            rv = (*env)->NewObjectArray(env, maxlen <<= 1,
                                            JNU_ClassString(env), NULL);
            if ( rv == NULL 
                         || JNU_CopyObjectArray(env, rv, old, len) < 0)
                return NULL; // error
            (*env)->DeleteLocalRef(env, old);
        }
        (*env)->SetObjectArrayElement(env, rv, len++, name);
        (*env)->DeleteLocalRef(env, name);
        
    } while (FindNextFileW(handle, &find_data));

    if (GetLastError() != ERROR_NO_MORE_FILES)
        return NULL; // error
    FindClose(handle);

    /* Copy the final results into an appropriately-sized array */    
    old = rv;
    rv = (*env)->NewObjectArray(env, len, JNU_ClassString(env), NULL);
    if (rv == NULL)
        return NULL; /* error */
    if (JNU_CopyObjectArray(env, rv, old, len) < 0)
        return NULL; /* error */    
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_createDirectory(JNIEnv *env, jobject this, 
                                             jobject file) 
{
    BOOL h = FALSE;
    WCHAR *pathbuf = fileToNTPath(env, file, ids.path);
    if (pathbuf == NULL) {
        /* Exception is pending */
        return JNI_FALSE;
    }

    h = CreateDirectoryW(pathbuf, NULL);

    free(pathbuf);
    
    if (h == 0) {
        return JNI_FALSE;
    }
        
    return JNI_TRUE;
}


JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_rename0(JNIEnv *env, jobject this, jobject from, 
                                     jobject to) 
{

    jboolean rv = JNI_FALSE;

    WITH_UNICODE_PATH(env, from, ids.path, fromPath) {
        WITH_UNICODE_PATH(env, to, ids.path, toPath) {
            if (_wrename(fromPath, toPath) == 0) {
                rv = JNI_TRUE;
            }
        } END_UNICODE_PATH(env, toPath);
    } END_UNICODE_PATH(env, fromPath);
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_setLastModifiedTime(JNIEnv *env, jobject this,
                                                 jobject file, jlong time) 
{
    jboolean rv = JNI_FALSE;

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        HANDLE h;
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
        if (h != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER modTime;
            FILETIME t;
            modTime.QuadPart = (time + 11644473600000L) * 10000L;
            t.dwLowDateTime = (DWORD)modTime.LowPart;
            t.dwHighDateTime = (DWORD)modTime.HighPart;
            if (SetFileTime(h, NULL, NULL, &t)) {
                rv = JNI_TRUE;
            }
            CloseHandle(h);
        }
    } END_UNICODE_PATH(env, path);

    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_WinNTFileSystem_setReadOnly(JNIEnv *env, jobject this, 
                                         jobject file) 
{
    jboolean rv = JNI_FALSE;

    WITH_UNICODE_PATH(env, file, ids.path, path) {
        DWORD a;
        a = GetFileAttributesW(path);
        if (a != ((DWORD)-1)) {
            if (SetFileAttributesW(path, a | FILE_ATTRIBUTE_READONLY))
            rv = JNI_TRUE;
        }
    } END_UNICODE_PATH(env, path);
    return rv;
}

/* -- Filesystem interface -- */


JNIEXPORT jobject JNICALL
Java_java_io_WinNTFileSystem_getDriveDirectory(JNIEnv *env, jobject this, 
                                               jint drive) 
{
    jchar buf[_MAX_PATH];
    jchar *p = _wgetdcwd(drive, buf, sizeof(buf));
    if (p == NULL) return NULL;
    if (iswalpha(*p) && (p[1] == L':')) p += 2;
    return (*env)->NewString(env, p, wcslen(p));
}
