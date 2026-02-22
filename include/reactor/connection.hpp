#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "reactor/config.hpp"

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

    bool is_active() const noexcept { return active_; }
    int fd() const noexcept { return fd_; }

    IoResult on_readable() noexcept;

private:
    int fd_ = -1;
    bool active_ = false;
    std::array<char, kConnectionBufferSize> rx_buffer_{};
};

}  // namespace reactor
