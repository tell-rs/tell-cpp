// src/transport.cpp
// TCP transport with auto-reconnect.

#include "transport.hpp"

#include <cstring>
#include <stdexcept>

// POSIX sockets
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

namespace tell {

TcpTransport::TcpTransport(std::string endpoint, std::chrono::milliseconds timeout)
    : endpoint_(std::move(endpoint)), timeout_(timeout) {
    // Parse host:port
    auto colon = endpoint_.rfind(':');
    if (colon == std::string::npos) {
        throw TellError::configuration("endpoint must be host:port, got: " + endpoint_);
    }
    host_ = endpoint_.substr(0, colon);
    int port_int = 0;
    try {
        port_int = std::stoi(endpoint_.substr(colon + 1));
    } catch (...) {
        throw TellError::configuration("endpoint port is not a valid number: " + endpoint_);
    }
    if (port_int <= 0 || port_int > 65535) {
        throw TellError::configuration("endpoint port must be 1-65535, got: " + std::to_string(port_int));
    }
    port_ = static_cast<uint16_t>(port_int);
}

TcpTransport::~TcpTransport() {
    close_connection();
}

void TcpTransport::close_connection() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

void TcpTransport::ensure_connected() {
    if (socket_fd_ >= 0) return;
    connect();
}

void TcpTransport::connect() {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    auto port_str = std::to_string(port_);
    int err = ::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res);
    if (err != 0 || res == nullptr) {
        throw TellError::network("DNS resolution failed for " + host_);
    }

    // Try each resolved address (IPv6/IPv4) until one connects.
    // Matches Rust tokio behavior which iterates all addresses.
    int fd = -1;
    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int ret = ::connect(fd, rp->ai_addr, rp->ai_addrlen);

        if (ret == 0) {
            // Connected immediately
            ::fcntl(fd, F_SETFL, flags);
            ::freeaddrinfo(res);
            configure_socket(fd);
            socket_fd_ = fd;
            return;
        }

        if (errno != EINPROGRESS) {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Wait for connection with timeout
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLOUT;

        int poll_ret = ::poll(&pfd, 1, static_cast<int>(timeout_.count()));
        if (poll_ret <= 0) {
            ::close(fd);
            fd = -1;
            continue;
        }

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Connected — restore blocking mode
        ::fcntl(fd, F_SETFL, flags);
        ::freeaddrinfo(res);
        configure_socket(fd);
        socket_fd_ = fd;
        return;
    }

    ::freeaddrinfo(res);
    throw TellError::network("connect failed to " + endpoint_);
}

void TcpTransport::configure_socket(int fd) {
    int nodelay = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    int keepalive = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout_.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout_.count() % 1000) * 1000);
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool TcpTransport::write_all(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(socket_fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            close_connection();
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TcpTransport::send_frame(const uint8_t* data, size_t len) {
    try {
        ensure_connected();
    } catch (...) {
        return false;
    }

    // Length prefix (4 bytes, big-endian)
    if (len > UINT32_MAX) return false;
    uint32_t frame_len = static_cast<uint32_t>(len);
    uint8_t header[4] = {
        static_cast<uint8_t>(frame_len >> 24),
        static_cast<uint8_t>(frame_len >> 16),
        static_cast<uint8_t>(frame_len >> 8),
        static_cast<uint8_t>(frame_len),
    };

    if (!write_all(header, 4)) return false;
    if (!write_all(data, len)) return false;
    return true;
}

} // namespace tell
