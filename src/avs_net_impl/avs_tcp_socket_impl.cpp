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

#include <TCPServer.h>
#include <TCPSocket.h>

#include <avsystem/commons/avs_commons_config.h>
#include <avsystem/commons/avs_errno.h>

#include "avs_mbed_hacks.h"
#include "avs_socket_impl.h"

using namespace avs_mbed_hacks;
using namespace avs_mbed_impl;
using namespace mbed;
using namespace std;

namespace avs_mbed_impl {

avs_error_t AvsTcpSocket::configure_socket() {
    // configuration not really supported...
    if (configuration_.interface_name[0] || configuration_.priority
        || configuration_.dscp || configuration_.transparent) {
        return avs_errno(AVS_EINVAL);
    }
    return AVS_OK;
}

/**
 * This function is intended to be a drop-in replacement for TCPSocket::recv(),
 * but make sure that recv(NULL, 0) always returns success if there is data
 * available for reading.
 *
 * We call recv() like that in our implementation of poll() - this is necessary,
 * because there is no native poll() in Mbed OS, and the closest mechanism there
 * is, InternetSocket::sigio(), is somewhat unreliable, as there is no
 * information on *what* event happened - the callback may be called both when
 * there is data available, or when the socket is closed, and perhaps even when
 * data can be written.
 *
 * That's why we make sure that TCPSocket::recv() is always called with buffer
 * bigger than 0. We can then buffer that initial part of data, and return
 * success from recv(NULL, 0) properly, until the buffer is actually consumed.
 */
nsapi_size_or_error_t AvsTcpSocket::recv_with_buffer_hack(void *data,
                                                          nsapi_size_t size) {
    MBED_ASSERT(socket_.get());
    TCPSocket *tcp_socket = static_cast<TCPSocket *>(socket_.get());
    uint8_t *u8data = reinterpret_cast<uint8_t *>(data);
    if (size > 0) {
        if (has_buffered_byte_) {
            *u8data++ = buffered_byte_;
            --size;
            if (size == 0) {
                has_buffered_byte_ = false;
                return 1;
            }
        }
        nsapi_size_or_error_t result = tcp_socket->recv(u8data, size);
        if (has_buffered_byte_ && result >= 0) {
            has_buffered_byte_ = false;
            ++result;
        }
        return result;
    } else {
        if (has_buffered_byte_) {
            return 0;
        } else {
            nsapi_size_or_error_t result = tcp_socket->recv(&buffered_byte_, 1);
            if (result > 0) {
                has_buffered_byte_ = true;
                result = 0;
            }
            return result;
        }
    }
}

avs_error_t AvsTcpSocket::try_connect(const SocketAddress &address) {
    if (state_ != AVS_NET_SOCKET_STATE_CLOSED) {
        LOG(ERROR, "socket is already bound");
        return avs_errno(AVS_EISCONN);
    }
    MBED_ASSERT(!socket_.get());
    auto_ptr<TCPSocket> new_socket(new (nothrow) TCPSocket());
    if (!new_socket.get()) {
        LOG(ERROR, "cannot create socket");
        return avs_errno(AVS_ENOMEM);
    }
    nsapi_error_t nserr = new_socket->open(&AvsSocketGlobal::get_interface());
    if (nserr) {
        LOG(ERROR, "cannot open socket");
        return avs_errno(nsapi_error_to_errno(nserr));
    }
    avs_error_t err = configure_socket();
    if (avs_is_err(err)) {
        LOG(WARNING, "socket configuration problem");
        return err;
    }
    new_socket->sigio(callback(trigger_poll_flag));
    new_socket->set_timeout(NET_CONNECT_TIMEOUT_MS);
    nserr = new_socket->connect(address);
    if (nserr) {
        return avs_errno(nsapi_error_to_errno(nserr));
    }

    // check if connection is really usable
    nserr = new_socket->send(NULL, 0);
    if (nserr) {
        return avs_errno(nsapi_error_to_errno(nserr));
    }

    socket_ = new_socket;
    return AVS_OK;
}

avs_error_t AvsTcpSocket::connect(const char *host, const char *port) {
    if (socket_.get()) {
        LOG(ERROR, "socket is already connected or bound");
        return avs_errno(AVS_EISCONN);
    }
    return AvsSocket::connect(host, port);
}

bool AvsTcpSocket::ready_to_receive() const {
    if (state_ == AVS_NET_SOCKET_STATE_ACCEPTED
        || state_ == AVS_NET_SOCKET_STATE_CONNECTED) {
        MBED_ASSERT(socket_.get());
        socket_->set_blocking(false);
        nsapi_size_or_error_t result =
                const_cast<AvsTcpSocket *>(this)->recv_with_buffer_hack(NULL,
                                                                        0);
        LOG(DEBUG, "result == %d", (int) result);
        return result == NSAPI_ERROR_OK;
    }
    // TODO: support TCPServer
    return false;
}

avs_error_t AvsTcpSocket::send(const void *buffer, size_t buffer_length) {
    if (state_ != AVS_NET_SOCKET_STATE_ACCEPTED
        && state_ != AVS_NET_SOCKET_STATE_CONNECTED) {
        LOG(ERROR, "attempted send() on a socket not created");
        return avs_errno(AVS_EBADF);
    }
    MBED_ASSERT(socket_.get());
    socket_->set_timeout(NET_SEND_TIMEOUT_MS);

    // only a client socket can be in ACCEPTED or CONNECTED state,
    // so socket_ must be a TCPSocket
    nsapi_size_or_error_t result = static_cast<TCPSocket *>(socket_.get())
                                           ->send(buffer, buffer_length);
    if (result < 0) {
        return avs_errno(nsapi_error_to_errno(result));
    } else if ((size_t) result < buffer_length) {
        LOG(ERROR, "sending fail (%lu/%lu)", (unsigned long) result,
            (unsigned long) buffer_length);
        return avs_errno(AVS_EIO);
    } else {
        // SUCCESS
        return AVS_OK;
    }
}

avs_error_t AvsTcpSocket::receive_from(size_t *out_size,
                                       void *buffer,
                                       size_t buffer_length,
                                       char *host,
                                       size_t host_size,
                                       char *port_str,
                                       size_t port_str_size) {
    if (state_ != AVS_NET_SOCKET_STATE_ACCEPTED
        && state_ != AVS_NET_SOCKET_STATE_CONNECTED) {
        LOG(ERROR, "attempted receive_from() on a socket not created");
        return avs_errno(AVS_EBADF);
    }
    MBED_ASSERT(socket_.get());
    if (host_size) {
        avs_error_t err = remote_host(host, host_size);
        if (avs_is_err(err)) {
            return err;
        }
    }
    if (port_str_size) {
        avs_error_t err = remote_port(port_str, port_str_size);
        if (avs_is_err(err)) {
            return err;
        }
    }
    avs_time_monotonic_t deadline =
            avs_time_monotonic_add(avs_time_monotonic_now(), recv_timeout_);
    static_cast<TCPSocket *>(socket_.get())
            ->set_blocking(!avs_time_monotonic_valid(deadline));
    reset_poll_flag();
    nsapi_size_or_error_t result = recv_with_buffer_hack(buffer, buffer_length);
    while (result == NSAPI_ERROR_WOULD_BLOCK
           && avs_time_monotonic_before(avs_time_monotonic_now(), deadline)) {
        wait_on_poll_flag(deadline);
        result = recv_with_buffer_hack(buffer, buffer_length);
        reset_poll_flag();
    }
    if (result < 0) {
        return avs_errno(nsapi_error_to_errno(result));
    } else {
        *out_size = (size_t) result;
        return AVS_OK;
    }
}

avs_error_t AvsTcpSocket::try_bind(const SocketAddress &localaddr) {
    if (socket_.get()) {
        LOG(ERROR, "socket is already connected or bound");
        return avs_errno(AVS_EISCONN);
    }

    int reuse_addr = configuration_.reuse_addr;
    if (reuse_addr != 0 && reuse_addr != 1) {
        return avs_errno(AVS_EINVAL);
    }

#if PREREQ_MBED_OS(5, 10, 0)
    auto_ptr<TCPSocket> socket(new (nothrow) TCPSocket());
#else // mbed OS < 5.10
    auto_ptr<TCPServer> socket(new (nothrow) TCPServer());
#endif
    if (!socket.get()) {
        LOG(ERROR, "cannot create TCPServer");
        return avs_errno(AVS_ENOMEM);
    }
    nsapi_error_t nserr = socket->open(&AvsSocketGlobal::get_interface());
    if (nserr) {
        LOG(ERROR, "cannot open socket");
        return avs_errno(nsapi_error_to_errno(nserr));
    }

    socket->sigio(callback(trigger_poll_flag));
    if ((nserr = socket->setsockopt(NSAPI_SOCKET, NSAPI_REUSEADDR, &reuse_addr,
                                    sizeof(reuse_addr)))) {
        LOG(ERROR, "can't set socket opt: %d", (int) nserr);
        return avs_errno(nsapi_error_to_errno(nserr));
    }

    avs_error_t err = configure_socket();
    if (avs_is_err(err)) {
        return err;
    }
    if ((nserr = socket->bind(localaddr))) {
        LOG(ERROR, "bind error: %d", (int) nserr);
        return avs_errno(nsapi_error_to_errno(nserr));
    }
    if ((nserr = socket->listen(NET_LISTEN_BACKLOG))) {
        LOG(ERROR, "listen error: %d", (int) nserr);
        return avs_errno(nsapi_error_to_errno(nserr));
    }
    socket_ = socket;
    state_ = AVS_NET_SOCKET_STATE_BOUND;
    local_address_ = localaddr;
    if (local_address_.get_port() == 0) {
        int32_t port = get_local_port(socket_.get());
        if (port < 0) {
            LOG(ERROR, "socket was bound to an invalid port");
            return avs_errno(AVS_EINVAL);
        }
        local_address_.set_port(port);
    }
    return AVS_OK;
}

avs_error_t AvsTcpSocket::accept(AvsSocket *new_socket_) {
    if (!new_socket_ || !socket_types_match(this, new_socket_)) {
        LOG(ERROR, "accept() called with a non-TCP socket");
        return avs_errno(AVS_EINVAL);
    }

    AvsTcpSocket *new_socket = static_cast<AvsTcpSocket *>(new_socket_);
    if (new_socket->socket_.get()) {
        LOG(ERROR, "socket is already connected or bound");
        return avs_errno(AVS_EISCONN);
    }

    if (state_ != AVS_NET_SOCKET_STATE_BOUND) {
        LOG(ERROR, "attempted accept() on a socket not in listening state");
        return avs_errno(AVS_EBADF);
    }
    MBED_ASSERT(socket_.get());

    SocketAddress addr;
    socket_->set_timeout(NET_ACCEPT_TIMEOUT_MS);
    nsapi_error_t err = 0;
#if PREREQ_MBED_OS(5, 10, 0)
    auto_ptr<TCPSocket> new_mbed_socket(
            static_cast<TCPSocket *>(socket_.get())->accept(&err));
#else // mbed OS < 5.10
    auto_ptr<TCPSocket> new_mbed_socket(new (nothrow) TCPSocket());
    if (new_mbed_socket.get()) {
        err = new_mbed_socket->open(&AvsSocketGlobal::get_interface());
        if (!err) {
            err = static_cast<TCPServer *>(socket_.get())
                          ->accept(new_mbed_socket.get(), &addr);
        }
    }
#endif
    if (!new_mbed_socket.get()) {
        return avs_errno(AVS_ENOMEM);
    }
    if (err) {
        return avs_errno(nsapi_error_to_errno(err));
    }
    new_mbed_socket->sigio(callback(trigger_poll_flag));
    new_socket->socket_ = new_mbed_socket;
    new_socket->state_ = AVS_NET_SOCKET_STATE_ACCEPTED;
    new_socket->update_remote_endpoint(addr.get_ip_address(), addr);
    new_socket->local_address_ = local_address_;
    return AVS_OK;
}

void AvsTcpSocket::close() {
    socket_.reset();
    state_ = AVS_NET_SOCKET_STATE_CLOSED;
    local_address_ = SocketAddress();
    // avs_commons' contract requires that the remote port is not reset when
    // closing the socket; some software (e.g. Anjay) relies on that;
    // so we reset only the address but not the port
    remote_address_.set_addr(local_address_.get_addr());
}

} // namespace avs_mbed_impl
