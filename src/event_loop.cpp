#include "reactor/event_loop.hpp"

#include <cerrno>
#include <cstdint>

#include <sys/epoll.h>

namespace reactor {

EventLoop::EventLoop(int max_events) noexcept : max_events_(max_events) {}

bool EventLoop::init() noexcept {
    int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    epoll_fd_.reset(fd);
    return true;
}

bool EventLoop::add_fd(int fd, std::uint32_t events) noexcept {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EventLoop::mod_fd(int fd, std::uint32_t events) noexcept {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EventLoop::del_fd(int fd) noexcept {
    return ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int EventLoop::wait(epoll_event* events, int max_events, int timeout_ms, int* err_out) noexcept {
    const int used_max_events = max_events > 0 ? max_events : max_events_;
    const int ready = ::epoll_wait(epoll_fd_.get(), events, used_max_events, timeout_ms);
    if (ready < 0 && err_out != nullptr) {
        *err_out = errno;
    }
    return ready;
}

}  // namespace reactor
