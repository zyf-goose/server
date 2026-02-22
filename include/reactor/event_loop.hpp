#pragma once

#include <sys/epoll.h>

#include <cstdint>

#include "reactor/unique_fd.hpp"

namespace reactor {

class EventLoop {
public:
    explicit EventLoop(int max_events) noexcept;

    bool init() noexcept;

    bool add_fd(int fd, std::uint32_t events) noexcept;
    bool mod_fd(int fd, std::uint32_t events) noexcept;
    bool del_fd(int fd) noexcept;

    int wait(epoll_event* events, int max_events, int timeout_ms, int* err_out) noexcept;

    int epoll_fd() const noexcept { return epoll_fd_.get(); }

private:
    UniqueFd epoll_fd_;
    int max_events_;
};

}  // namespace reactor
