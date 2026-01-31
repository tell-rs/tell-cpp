// src/transport.hpp
// TCP transport with auto-reconnect — single persistent connection.

#pragma once

#include "tell/error.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace tell {

class TcpTransport {
public:
    TcpTransport(std::string endpoint, std::chrono::milliseconds timeout);
    ~TcpTransport();

    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    // Send length-prefixed frame: [4 bytes BE length][payload].
    // Auto-reconnects on failure.
    bool send_frame(const uint8_t* data, size_t len);

    void close_connection();

    const std::string& endpoint() const noexcept { return endpoint_; }
    std::chrono::milliseconds timeout() const noexcept { return timeout_; }

private:
    void ensure_connected();
    void connect();
    void configure_socket(int fd);
    bool write_all(const uint8_t* data, size_t len);

    std::string endpoint_;
    std::string host_;
    uint16_t port_ = 0;
    std::chrono::milliseconds timeout_;
    int socket_fd_ = -1;
};

} // namespace tell
