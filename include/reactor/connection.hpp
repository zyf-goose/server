#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "reactor/config.hpp"
#include "utils/event_handler.hpp"

namespace reactor {

struct IoResult {
    bool should_close = false;
    std::size_t read_bytes = 0;
    std::size_t write_bytes = 0;
};

class Connection {
public:
    void attach(int fd) noexcept;
    void reset() noexcept;
    void set_event_handler(std::unique_ptr<EventHandler> handler);

    bool is_active() const noexcept { return active_; }
    int fd() const noexcept { return fd_; }
    bool has_pending_write() const noexcept { return write_buffer_size_ != 0U; }
    bool is_epollout_registered() const noexcept { return epollout_registered_; }
    void set_epollout_registered(bool registered) noexcept { epollout_registered_ = registered; }

    IoResult on_readable() noexcept;
    IoResult on_writable() noexcept;

private:
    bool enqueue_write(const char* data, std::size_t len) noexcept;
    bool flush_write(IoResult& result) noexcept;

    int fd_ = -1;
    bool active_ = false;
    bool epollout_registered_ = false;
    std::array<char, kConnectionBufferSize> read_buffer_{};
    std::array<char, kConnectionBufferSize> write_buffer_{};
    std::size_t write_buffer_begin_ = 0;
    std::size_t write_buffer_size_ = 0;
    std::unique_ptr<EventHandler> handler_;
};

}  // namespace reactor
