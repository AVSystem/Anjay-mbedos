/*
 * Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANJAY_MBEDOS_POSIX_COMPAT_H
#define ANJAY_MBEDOS_POSIX_COMPAT_H

#include <avsystem/commons/avs_socket.h>

// HACK: Anjay's event loop implementation depends on either poll() or select()
// with a roughly POSIX-compatible API. We cannot model that directly in Mbed
// OS, as sockets are C++ classes and there is no concept of "file descriptor".
//
// This file is a hack that makes anjay_event_loop.c compile nevertheless.

#ifndef AVS_COMMONS_NET_POSIX_AVS_SOCKET_HAVE_POLL
#define AVS_COMMONS_NET_POSIX_AVS_SOCKET_HAVE_POLL
#endif // AVS_COMMONS_NET_POSIX_AVS_SOCKET_HAVE_POLL

#ifdef __cplusplus
extern "C" {
#endif //  __cplusplus

struct avs_mbedos_pollfd {
    avs_net_socket_t *fd;
    short events;
    short revents;
};

int _anjay_mbedos_poll(struct avs_mbedos_pollfd *fds,
                       size_t nfds,
                       int timeout_ms);

#define AVS_MBEDOS_POLLIN 1

#ifdef __cplusplus
} // extern "C"
#endif //  __cplusplus

// HACK: The hacks below are excluded in C++, so that they do not interfere with
// the implementation in avs_socket_impl.cpp.
#ifndef __cplusplus

// HACK: Normally, anjay-mbedos sockets return AvsSocket * via get_system().
// That cannot be meaningfully dereferenced, and anjay_event_loop.c does
// that to retrieve the file descriptor.
//
// So we redefine avs_net_socket_get_system(socket) to return a pointer to a
// temporary variable holding the socket pointer itself, which means that
// *avs_net_socket_get_system(socket) == socket. That way we can use
// avs_net_socket_t * as the "file descriptor" type.
#define avs_net_socket_get_system(Socket) (&(avs_net_socket_t *) { (Socket) })

typedef avs_net_socket_t *sockfd_t;
#define INVALID_SOCKET NULL

#ifdef pollfd
#undef pollfd
#endif // pollfd
#define pollfd avs_mbedos_pollfd

#ifdef POLLIN
#undef POLLIN
#endif // POLLIN
#define POLLIN AVS_MBEDOS_POLLIN

#ifdef poll
#undef poll
#endif // poll
#define poll _anjay_mbedos_poll

#endif // __cplusplus

#endif /* ANJAY_MBEDOS_POSIX_COMPAT_H */
