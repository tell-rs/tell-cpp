// src/worker.hpp
// Background worker thread — channel-based producer-consumer pattern.

#pragma once

#include "encoding.hpp"
#include "transport.hpp"
#include "tell/config.hpp"
#include "tell/error.hpp"
#include "tell/types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace tell {

// Queued event ready to be encoded.
struct QueuedEvent {
    EventType event_type = EventType::Unknown;
    uint64_t timestamp = 0;
    uint8_t device_id[16] = {};
    uint8_t session_id[16] = {};
    std::string event_name;
    std::vector<uint8_t> payload;
    // Note: service is set at config level, not per-event.
    // The worker reads it from config_ when building EventParams.
};

// Queued log entry ready to be encoded.
struct QueuedLog {
    LogLevel level = LogLevel::Info;
    uint64_t timestamp = 0;
    uint8_t session_id[16] = {};
    std::string source;
    std::string service;
    std::vector<uint8_t> payload;
};

// Signals carry a per-request completion promise (like Rust's oneshot channel).
struct FlushSignal {
    std::shared_ptr<std::promise<void>> completion;
};
struct CloseSignal {
    std::shared_ptr<std::promise<void>> completion;
};

// Worker message: exactly one variant active at a time.
using WorkerMessage = std::variant<QueuedEvent, QueuedLog, FlushSignal, CloseSignal>;

class Worker {
public:
    Worker(TellConfig config);
    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Send a message to the worker (non-blocking).
    void send_event(QueuedEvent event);
    void send_log(QueuedLog log);
    std::future<void> send_flush();
    std::future<void> send_close();

    // Access config (for service resolution in client).
    const TellConfig& config() const noexcept { return config_; }

private:
    void run();
    void flush_events();
    void flush_logs();
    void send_or_retry(const uint8_t* data, size_t len);
    void retry_send(std::vector<uint8_t> data);

    void enqueue(WorkerMessage msg);

    TellConfig config_;
    TcpTransport transport_;
    std::thread thread_;

    // Channel
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<WorkerMessage> queue_;
    static constexpr size_t MAX_QUEUE_SIZE = 10000;

    // Queues
    std::vector<QueuedEvent> event_queue_;
    std::vector<QueuedLog> log_queue_;

    // Reusable encoding buffers
    std::vector<uint8_t> data_buf_;
    std::vector<uint8_t> batch_buf_;

    std::atomic<uint64_t> batch_counter_{1};
    std::atomic<bool> running_{true};

    // Managed retry threads (joined on shutdown instead of detached)
    std::mutex retry_mutex_;
    std::vector<std::thread> retry_threads_;
    static constexpr size_t MAX_RETRY_THREADS = 8;
};

} // namespace tell
