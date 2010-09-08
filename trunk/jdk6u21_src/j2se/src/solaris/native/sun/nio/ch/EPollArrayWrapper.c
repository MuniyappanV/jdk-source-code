/*
 * @(#)EPollArrayWrapper.c	1.8 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "jlong.h"

#include "sun_nio_ch_EPollArrayWrapper.h"

#include <dlfcn.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* epoll_wait(2) man page */

typedef union epoll_data {
    void *ptr;
    int fd;
    __uint32_t u32;
    __uint64_t u64;
} epoll_data_t;

struct epoll_event {
    __uint32_t events;  /* Epoll events */
    epoll_data_t data;  /* User data variable */
} __attribute__ ((__packed__));

#ifdef	__cplusplus
}
#endif

#define RESTARTABLE(_cmd, _result) do { \
  do { \
    _result = _cmd; \
  } while((_result == -1) && (errno == EINTR)); \
} while(0)

/*
 * epoll event notification is new in 2.6 kernel. As the offical build
 * platform for the JDK is on a 2.4-based distribution then we must
 * obtain the addresses of the epoll functions dynamically.
 */
typedef int (*epoll_create_t)(int size);
typedef int (*epoll_ctl_t)   (int epfd, int op, int fd, struct epoll_event *event);
typedef int (*epoll_wait_t)  (int epfd, struct epoll_event *events, int maxevents, int timeout);

static epoll_create_t epoll_create_func;
static epoll_ctl_t    epoll_ctl_func;
static epoll_wait_t   epoll_wait_func;

static int
iepoll(int epfd, struct epoll_event *events, int numfds, jlong timeout) 
{
    jlong start, now;
    int remaining = timeout;
    struct timeval t;
    int diff;

    gettimeofday(&t, NULL);
    start = t.tv_sec * 1000 + t.tv_usec / 1000;

    for (;;) {
        int res = (*epoll_wait_func)(epfd, events, numfds, timeout);
        if (res < 0 && errno == EINTR) {
            if (remaining >= 0) {
                gettimeofday(&t, NULL);
                now = t.tv_sec * 1000 + t.tv_usec / 1000;
                diff = now - start;
                remaining -= diff;
                if (diff < 0 || remaining <= 0) {
                    return 0;
                }
                start = now;
            }
        } else {
            return res;
        }
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_EPollArrayWrapper_init(JNIEnv *env, jclass this) 
{
    epoll_create_func = (epoll_create_t) dlsym(RTLD_DEFAULT, "epoll_create");
    epoll_ctl_func    = (epoll_ctl_t)    dlsym(RTLD_DEFAULT, "epoll_ctl");
    epoll_wait_func   = (epoll_wait_t)   dlsym(RTLD_DEFAULT, "epoll_wait");
                                                                                                   
    if ((epoll_create_func == NULL) || (epoll_ctl_func == NULL) ||
        (epoll_wait_func == NULL)) {
        JNU_ThrowInternalError(env, "unable to get address of epoll functions, pre-2.6 kernel?");
    }
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPollArrayWrapper_epollCreate(JNIEnv *env, jobject this)
{
    /*
     * epoll_create expects a size as a hint to the kernel about how to
     * dimension internal structures. We can't predict the size in advance.
     */
    int epfd = (*epoll_create_func)(256);
    if (epfd < 0) {
       JNU_ThrowIOExceptionWithLastError(env, "epoll_create failed");
    }
    return epfd;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPollArrayWrapper_fdLimit(JNIEnv *env, jclass this)
{
    struct rlimit rlp;
    if (getrlimit(RLIMIT_NOFILE, &rlp) < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "getrlimit failed");
    }
    return (jint)rlp.rlim_max;
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_EPollArrayWrapper_epollCtl(JNIEnv *env, jobject this, jint epfd,
                                           jint opcode, jint fd, jint events)
{
    struct epoll_event event;
    int res;

    event.events = events;
    event.data.fd = fd;

    RESTARTABLE((*epoll_ctl_func)(epfd, (int)opcode, (int)fd, &event), res);

    /* 
     * A channel may be registered with several Selectors. When each Selector
     * is polled a EPOLL_CTL_DEL op will be inserted into its pending update
     * list to remove the file descriptor from epoll. The "last" Selector will
     * close the file descriptor which automatically unregisters it from each
     * epoll descriptor. To avoid costly synchronization between Selectors we
     * allow pending updates to be processed, ignoring errors. The errors are
     * harmless as the last update for the file descriptor is guaranteed to
     * be EPOLL_CTL_DEL.
     */
    if (res < 0 && errno != EBADF && errno != ENOENT && errno != EPERM) {
        JNU_ThrowIOExceptionWithLastError(env, "epoll_ctl failed");
    }
}

JNIEXPORT jint JNICALL 
Java_sun_nio_ch_EPollArrayWrapper_epollWait(JNIEnv *env, jobject this,
                                            jlong address, jint numfds,
                                            jlong timeout, jint epfd)
{
    struct epoll_event *events = jlong_to_ptr(address);
    int res;

    if (timeout <= 0) {           /* Indefinite or no wait */
        RESTARTABLE((*epoll_wait_func)(epfd, events, numfds, timeout), res);
    } else {                      /* Bounded wait; bounded restarts */
        res = iepoll(epfd, events, numfds, timeout);
    }
    
    if (res < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "epoll_wait failed");
    }
    return res;
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_EPollArrayWrapper_interrupt(JNIEnv *env, jobject this, jint fd)
{
    int fakebuf[1];
    fakebuf[0] = 1;
    if (write(fd, fakebuf, 1) < 0) {
        JNU_ThrowIOExceptionWithLastError(env,"write to interrupt fd failed");
    }
}

