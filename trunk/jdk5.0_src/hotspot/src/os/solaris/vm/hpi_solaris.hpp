#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)hpi_solaris.hpp	1.17 03/12/23 16:37:42 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

//
// Parts of the HPI interface for which the HotSparc does not use the
// HPI (because the interruptible IO mechanims used are different).
//

#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/filio.h>
#include <unistd.h>
#include <netdb.h>

// HPI_FileInterface

// Many system calls can be interrupted by signals and must be restarted.
// Restart support was added without disturbing the extent of thread 
// interruption support.

inline int    hpi::close(int fd) {
  RESTARTABLE_RETURN_INT(::close(fd)); 
}

inline size_t hpi::read(int fd, void *buf, unsigned int nBytes) {
  INTERRUPTIBLE_RETURN_INT(::read(fd, buf, nBytes), os::Solaris::clear_interrupted);
}

inline size_t hpi::write(int fd, const void *buf, unsigned int nBytes) {
  INTERRUPTIBLE_RETURN_INT(::write(fd, buf, nBytes), os::Solaris::clear_interrupted);
}


// HPI_SocketInterface

inline int    hpi::socket_close(int fd) {
  RESTARTABLE_RETURN_INT(::close(fd)); 
}

inline int    hpi::socket(int domain, int type, int protocol) {
  return ::socket(domain, type, protocol);
}

inline int    hpi::recv(int fd, char *buf, int nBytes, int flags) {
  INTERRUPTIBLE_RETURN_INT(::recv(fd, buf, nBytes, flags), os::Solaris::clear_interrupted);
}

inline int    hpi::send(int fd, char *buf, int nBytes, int flags) {
  INTERRUPTIBLE_RETURN_INT(::send(fd, buf, nBytes, flags), os::Solaris::clear_interrupted);
}

// As both poll and select can be interrupted by signals, we have to be 
// prepared to restart the system call after updating the timeout, unless
// a poll() is done with timeout == -1, in which case we repeat with this
// "wait forever" value. 

inline int    hpi::timeout(int fd, long timeout) {
  int res;
  struct timeval t;
  julong prevtime, newtime;
  static const char* aNull = 0;

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;

  gettimeofday(&t, &aNull);
  prevtime = ((julong)t.tv_sec * 1000)  +  t.tv_usec / 1000;

  for(;;) {
  
    INTERRUPTIBLE_NORESTART(::poll(&pfd, 1, timeout), res, os::Solaris::clear_interrupted);
    if(res == OS_ERR && errno == EINTR) {
	gettimeofday(&t, &aNull);
	newtime = ((julong)t.tv_sec * 1000)  +  t.tv_usec /1000;
	timeout -= newtime - prevtime;
	if(timeout <= 0)
	  return OS_OK;
	prevtime = newtime;
    } else
      return res;
  }
}

inline int    hpi::listen(int fd, int count) {
  if (fd < 0) 
    return OS_ERR;
  
  return ::listen(fd, count);
}

inline int    hpi::connect(int fd, struct sockaddr *him, int len) {
  INTERRUPTIBLE_RETURN_INT(::connect(fd, him, len), os::Solaris::clear_interrupted);
}

inline int    hpi::accept(int fd, struct sockaddr *him, int *len) {
  if (fd < 0)
    return OS_ERR;
  INTERRUPTIBLE_RETURN_INT((int)::accept(fd, him, (int*)len), os::Solaris::clear_interrupted);
}

inline int    hpi::recvfrom(int fd, char *buf, int nBytes, int flags,
			    sockaddr *from, int *fromlen) {
  //%%note jvm_r11
  INTERRUPTIBLE_RETURN_INT((int)::recvfrom(fd, buf, nBytes, flags, from, fromlen), os::Solaris::clear_interrupted);
}

inline int    hpi::sendto(int fd, char *buf, int len, int flags,
			  struct sockaddr *to, int tolen) {
  //%%note jvm_r11
  INTERRUPTIBLE_RETURN_INT((int)::sendto(fd, buf, len, flags, to, tolen), os::Solaris::clear_interrupted);
}

inline int    hpi::socket_available(int fd, long *pbytes) {
  if (fd < 0)
    return OS_OK;

  int ret;

  RESTARTABLE(::ioctl(fd, FIONREAD, pbytes), ret);

  //%% note ioctl can return 0 when successful, JVM_SocketAvailable
  // is expected to return 0 on failure and 1 on success to the jdk.

  return (ret == OS_ERR) ? 0 : 1;
}


/*
HPIDECL(socket_shutdown, "socket_shutdown", _socket, SocketShutdown, 
        int, "%d",
        (int fd, int howto),
        ("fd = %d, howto = %d", fd, howto),
        (fd, howto));
	*/
inline int hpi::socket_shutdown(int fd, int howto){
  return ::shutdown(fd, howto);
}

/*
HPIDECL(bind, "bind", _socket, Bind, 
        int, "%d",
        (int fd, struct sockaddr *him, int len),
        ("fd = %d, him = %p, len = %d",
         fd, him, len),
        (fd, him, len));
*/
inline int hpi::bind(int fd, struct sockaddr *him, int len){
  INTERRUPTIBLE_RETURN_INT_NORESTART(::bind(fd, him, len),os::Solaris::clear_interrupted);
}

/*
HPIDECL(get_sock_name, "get_sock_name", _socket, GetSocketName, 
        int, "%d",
        (int fd, struct sockaddr *him, int *len),
        ("fd = %d, him = %p, len = %p",
         fd, him, len),
        (fd, him, len));
	*/
inline int hpi::get_sock_name(int fd, struct sockaddr *him, int *len){
  return ::getsockname(fd, him, len);
}

/*
HPIDECL(get_host_name, "get_host_name", _socket, GetHostName, int, "%d",
        (char *hostname, int namelen),
        ("hostname = %p, namelen = %d",
         hostname, namelen),
        (hostname, namelen));
	*/
inline int hpi::get_host_name(char* name, int namelen){
  return ::gethostname(name, namelen);
}

/*
HPIDECL(get_sock_opt, "get_sock_opt", _socket, SocketGetOption, int, "%d",
        (int fd, int level, int optname, char *optval, int* optlen),
        ("fd = %d, level = %d, optname = %d, optval = %p, optlen = %p",
         fd, level, optname, optval, optlen),
        (fd, level, optname, optval, optlen));
	*/
inline int hpi::get_sock_opt(int fd, int level, int optname, 
			     char *optval, int* optlen){
  return ::getsockopt(fd, level, optname, optval, optlen);
}

/*
HPIDECL(set_sock_opt, "set_sock_opt", _socket, SocketSetOption, int, "%d",
        (int fd, int level, int optname, const char *optval, int optlen),
        ("fd = %d, level = %d, optname = %d, optval = %p, optlen = %d",
         fd, level, optname, optval, optlen),
        (fd, level, optname, optval, optlen));
	*/
inline int hpi::set_sock_opt(int fd, int level, int optname, 
			     const char *optval, int optlen){
  return ::setsockopt(fd, level, optname, optval, optlen);
}

//Reconciliation History
// 1.3 98/10/21 18:17:14 hpi_win32.hpp
// 1.6 99/06/28 11:01:36 hpi_win32.hpp
//End
