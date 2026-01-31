// include/tell/error.hpp
// Simplified error handling — single class with kind enum.

#pragma once

#include <string>
#include <stdexcept>

namespace tell {

enum class ErrorKind {
    Configuration,  // Invalid config at construction
    Validation,     // Bad input (dropped, reported via callback)
    Network,        // Transport failure (retried)
    Serialization,  // Encoding failure (dropped)
    Closed,         // Client already closed
    Io              // System I/O error
};

class TellError : public std::exception {
public:
    TellError(ErrorKind kind, std::string message)
        : kind_(kind), message_(std::move(message)) {}

    TellError(ErrorKind kind, const std::string& field, const std::string& reason)
        : kind_(kind), message_("validation error: " + field + " " + reason),
          field_(field), reason_(reason) {}

    const char* what() const noexcept override { return message_.c_str(); }
    ErrorKind kind() const noexcept { return kind_; }
    const std::string& message() const noexcept { return message_; }
    const std::string& field() const noexcept { return field_; }
    const std::string& reason() const noexcept { return reason_; }

    static TellError configuration(std::string msg) {
        return TellError(ErrorKind::Configuration, "configuration error: " + msg);
    }

    static TellError validation(std::string field, std::string reason) {
        return TellError(ErrorKind::Validation, field, reason);
    }

    static TellError network(std::string msg) {
        return TellError(ErrorKind::Network, "network error: " + msg);
    }

    static TellError serialization(std::string msg) {
        return TellError(ErrorKind::Serialization, "serialization error: " + msg);
    }

    static TellError closed() {
        return TellError(ErrorKind::Closed, "client is closed");
    }

    static TellError io(std::string msg) {
        return TellError(ErrorKind::Io, "io error: " + msg);
    }

private:
    ErrorKind kind_;
    std::string message_;
    std::string field_;
    std::string reason_;
};

} // namespace tell
