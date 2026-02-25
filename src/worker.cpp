// src/worker.cpp
// Background worker thread implementation.

#include "worker.hpp"

#include <cmath>
#include <cstring>
#include <functional>
#include <random>

namespace tell {

Worker::Worker(TellConfig config)
    : config_(std::move(config)),
      transport_(config_.endpoint(), config_.network_timeout()) {
    event_queue_.reserve(config_.batch_size());
    log_queue_.reserve(config_.batch_size());
    data_buf_.reserve(64 * 1024);
    batch_buf_.reserve(64 * 1024);

    thread_ = std::thread(&Worker::run, this);
}

Worker::~Worker() {
    if (running_.load()) {
        (void)send_close();  // future ignored; join() ensures completion
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    // Join all outstanding retry threads
    std::lock_guard<std::mutex> lock(retry_mutex_);
    for (auto& t : retry_threads_) {
        if (t.joinable()) t.join();
    }
    retry_threads_.clear();
}

void Worker::enqueue(WorkerMessage msg) {
    bool was_empty;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        was_empty = queue_.empty();
        if (queue_.size() >= MAX_QUEUE_SIZE) {
            // Drop oldest
            queue_.pop();
        }
        queue_.push(std::move(msg));
    }
    // Only wake the worker if it's likely sleeping (queue was empty)
    if (was_empty) {
        cv_.notify_one();
    }
}

void Worker::send_event(QueuedEvent event) {
    enqueue(std::move(event));
}

void Worker::send_log(QueuedLog log) {
    enqueue(std::move(log));
}

std::future<void> Worker::send_flush() {
    auto p = std::make_shared<std::promise<void>>();
    auto f = p->get_future();
    enqueue(FlushSignal{std::move(p)});
    return f;
}

std::future<void> Worker::send_close() {
    auto p = std::make_shared<std::promise<void>>();
    auto f = p->get_future();
    enqueue(CloseSignal{std::move(p)});
    return f;
}

void Worker::run() {
    auto flush_interval = config_.flush_interval();
    auto batch_size = config_.batch_size();
    auto next_flush = std::chrono::steady_clock::now() + flush_interval;

    while (running_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto wait_until = next_flush;
        cv_.wait_until(lock, wait_until, [this] { return !queue_.empty() || !running_.load(); });

        // Drain all pending messages
        std::queue<WorkerMessage> local_queue;
        std::swap(local_queue, queue_);
        lock.unlock();

        bool should_flush = false;
        bool should_close = false;
        std::vector<std::shared_ptr<std::promise<void>>> completions;

        while (!local_queue.empty()) {
            auto& msg = local_queue.front();
            if (auto* ev = std::get_if<QueuedEvent>(&msg)) {
                event_queue_.push_back(std::move(*ev));
                if (event_queue_.size() >= batch_size) {
                    flush_events();
                }
            } else if (auto* lg = std::get_if<QueuedLog>(&msg)) {
                log_queue_.push_back(std::move(*lg));
                if (log_queue_.size() >= batch_size) {
                    flush_logs();
                }
            } else if (auto* fs = std::get_if<FlushSignal>(&msg)) {
                should_flush = true;
                if (fs->completion) completions.push_back(std::move(fs->completion));
            } else if (auto* cs = std::get_if<CloseSignal>(&msg)) {
                should_close = true;
                if (cs->completion) completions.push_back(std::move(cs->completion));
            }
            local_queue.pop();
        }

        // Timer-based flush
        auto now = std::chrono::steady_clock::now();
        if (now >= next_flush) {
            should_flush = true;
            next_flush = now + flush_interval;
        }

        if (should_flush || should_close) {
            flush_events();
            flush_logs();

            for (auto& p : completions) {
                p->set_value();
            }
            completions.clear();
        }

        if (should_close) {
            transport_.close_connection();
            running_.store(false);
            return;
        }
    }
}

void Worker::flush_events() {
    if (event_queue_.empty()) return;

    std::vector<QueuedEvent> events;
    std::swap(events, event_queue_);

    // Build encoding params
    std::vector<encoding::EventParams> params;
    params.reserve(events.size());

    const auto& svc = config_.service();
    static const std::string default_service = "app";
    const auto& resolved_svc = svc.empty() ? default_service : svc;
    for (const auto& e : events) {
        encoding::EventParams p;
        p.event_type = e.event_type;
        p.timestamp = e.timestamp;
        p.service = resolved_svc.c_str();
        p.service_len = resolved_svc.size();
        p.device_id = e.device_id;
        p.session_id = e.session_id;
        if (!e.event_name.empty()) {
            p.event_name = e.event_name.c_str();
            p.event_name_len = e.event_name.size();
        }
        if (!e.payload.empty()) {
            p.payload = e.payload.data();
            p.payload_len = e.payload.size();
        }
        params.push_back(p);
    }

    // Encode EventData
    data_buf_.clear();
    size_t data_start = encoding::encode_event_data_into(data_buf_, params);

    // Encode Batch
    batch_buf_.clear();
    encoding::BatchParams bp;
    bp.api_key = config_.api_key_bytes().data();
    bp.schema_type = SchemaType::Event;
    bp.version = encoding::DEFAULT_VERSION;
    bp.batch_id = batch_counter_.fetch_add(1, std::memory_order_relaxed);
    bp.data = data_buf_.data() + data_start;
    bp.data_len = data_buf_.size() - data_start;
    encoding::encode_batch_into(batch_buf_, bp);

    send_or_retry(batch_buf_.data(), batch_buf_.size());
}

void Worker::flush_logs() {
    if (log_queue_.empty()) return;

    std::vector<QueuedLog> logs;
    std::swap(logs, log_queue_);

    std::vector<encoding::LogEntryParams> params;
    params.reserve(logs.size());

    for (const auto& l : logs) {
        encoding::LogEntryParams p;
        p.event_type = LogEventType::Log;
        p.session_id = l.session_id;
        p.level = l.level;
        p.timestamp = l.timestamp;
        if (!l.source.empty()) {
            p.source = l.source.c_str();
            p.source_len = l.source.size();
        }
        if (!l.service.empty()) {
            p.service = l.service.c_str();
            p.service_len = l.service.size();
        }
        if (!l.payload.empty()) {
            p.payload = l.payload.data();
            p.payload_len = l.payload.size();
        }
        params.push_back(p);
    }

    data_buf_.clear();
    size_t data_start = encoding::encode_log_data_into(data_buf_, params);

    batch_buf_.clear();
    encoding::BatchParams bp;
    bp.api_key = config_.api_key_bytes().data();
    bp.schema_type = SchemaType::Log;
    bp.version = encoding::DEFAULT_VERSION;
    bp.batch_id = batch_counter_.fetch_add(1, std::memory_order_relaxed);
    bp.data = data_buf_.data() + data_start;
    bp.data_len = data_buf_.size() - data_start;
    encoding::encode_batch_into(batch_buf_, bp);

    send_or_retry(batch_buf_.data(), batch_buf_.size());
}

void Worker::send_or_retry(const uint8_t* data, size_t len) {
    if (transport_.send_frame(data, len)) {
        return; // Fast path: sent on first try
    }

    if (config_.max_retries() > 0) {
        std::vector<uint8_t> owned(data, data + len);
        std::lock_guard<std::mutex> lock(retry_mutex_);
        // Reap finished threads before checking limit
        retry_threads_.erase(
            std::remove_if(retry_threads_.begin(), retry_threads_.end(),
                [](std::thread& t) {
                    if (t.joinable()) {
                        // Can't non-blocking check in C++; keep all until destructor.
                        return false;
                    }
                    return true;
                }),
            retry_threads_.end());
        if (retry_threads_.size() >= MAX_RETRY_THREADS) {
            if (config_.on_error()) {
                config_.on_error()(TellError::network("send failed, retry pool full"));
            }
        } else {
            retry_threads_.emplace_back(&Worker::retry_send, this, std::move(owned));
        }
    } else if (config_.on_error()) {
        config_.on_error()(TellError::network("send failed, no retries configured"));
    }
}

void Worker::retry_send(std::vector<uint8_t> data) {
    TcpTransport retry_transport(config_.endpoint(), config_.network_timeout());

    for (uint32_t attempt = 1; attempt <= config_.max_retries(); attempt++) {
        // Exponential backoff: 1s * 1.5^(attempt-1), 20% jitter, cap 30s
        double base = 1000.0 * std::pow(1.5, static_cast<double>(attempt - 1));

        // Simple jitter using random_device
        std::random_device rd;
        double jitter = base * 0.2 * (static_cast<double>(rd()) / static_cast<double>(rd.max()));
        double delay = std::min(base + jitter, 30000.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(delay)));

        if (retry_transport.send_frame(data.data(), data.size())) {
            return; // Success
        }
    }

    // All retries exhausted
    if (config_.on_error()) {
        config_.on_error()(TellError::network("send failed after " +
            std::to_string(config_.max_retries()) + " retries"));
    }
}

} // namespace tell
