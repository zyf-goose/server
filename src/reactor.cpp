#include "reactor/reactor.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <sys/epoll.h>
#include <unistd.h>

namespace reactor {

namespace {

constexpr std::uint32_t kClientBaseEvents = EPOLLIN | EPOLLRDHUP | EPOLLET;

}  // namespace

Reactor::Reactor(const ReactorConfig& config)
    : config_(config),
      loop_(config.max_events),
      connections_(config.max_connections),
      events_(static_cast<std::size_t>(config.max_events)) {}

int Reactor::run() noexcept {
    if (!loop_.init()) {
        std::cerr << "epoll_create1 failed errno=" << errno << " (" << std::strerror(errno)
                  << ")\n";
        return 1;
    }

    if (!acceptor_.init(config_, loop_, stop_)) {
        if (stop_.reason != nullptr) {
            std::cerr << "reactor init failed: " << stop_.reason;
            if (stop_.err != 0) {
                std::cerr << ", errno=" << stop_.err << " (" << std::strerror(stop_.err) << ")";
            }
            std::cerr << '\n';
        }
        return 1;
    }

    const int loop_rc = run_main_loop();

    if (stop_.reason != nullptr) {
        std::cerr << "reactor stopping: " << stop_.reason;
        if (stop_.err != 0) {
            std::cerr << ", errno=" << stop_.err << " (" << std::strerror(stop_.err) << ")";
        }
        std::cerr << '\n';
    }

    close_all_connections();
    acceptor_.shutdown(loop_);

    const std::uint64_t active_now = stats_.accept_total - stats_.close_total;
    std::cout << "accept_total=" << stats_.accept_total << " close_total=" << stats_.close_total
              << " active_now=" << active_now << " read_bytes=" << stats_.read_bytes
              << " write_bytes=" << stats_.write_bytes << '\n';

    return loop_rc;
}

int Reactor::run_main_loop() noexcept {
    while (!stop_.requested) {
        int wait_err = 0;
        const int ready =
            loop_.wait(events_.data(), static_cast<int>(events_.size()), -1, &wait_err);
        if (ready < 0) {
            if (wait_err == EINTR) {
                continue;
            }
            request_stop("epoll_wait failed", wait_err);
            break;
        }

        for (int i = 0; i < ready; ++i) {
            const int fd = events_[static_cast<std::size_t>(i)].data.fd;
            const std::uint32_t ev = events_[static_cast<std::size_t>(i)].events;

            if (fd == acceptor_.listener_fd()) {
                acceptor_.on_readable(loop_, connections_, stats_, stop_);
                if (stop_.requested) {
                    break;
                }
                continue;
            }

            if ((ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) {
                close_connection(fd);
                continue;
            }

            if ((ev & (EPOLLIN | EPOLLOUT)) == 0U) {
                continue;
            }

            Connection* conn = connections_.get(fd);
            if (conn == nullptr || !conn->is_active()) {
                continue;
            }

            IoResult io{};
            if ((ev & EPOLLOUT) != 0U) {
                // Writable handler only contributes write progress; read_bytes remains 0 by contract.
                const IoResult writable = conn->on_writable();
                io.read_bytes += writable.read_bytes;
                io.write_bytes += writable.write_bytes;
                io.should_close = io.should_close || writable.should_close;
            }

            if (!io.should_close && (ev & EPOLLIN) != 0U) {
                // Readable handler may also write (echo path + opportunistic flush), so merge totals.
                const IoResult readable = conn->on_readable();
                io.read_bytes += readable.read_bytes;
                io.write_bytes += readable.write_bytes;
                io.should_close = io.should_close || readable.should_close;
            }

            stats_.read_bytes += static_cast<std::uint64_t>(io.read_bytes);
            stats_.write_bytes += static_cast<std::uint64_t>(io.write_bytes);

            if (io.should_close) {
                close_connection(fd);
                continue;
            }

            const bool want_epollout = conn->has_pending_write();
            if (want_epollout != conn->is_epollout_registered()) {
                // Keep kernel registration aligned with pending output bytes.
                const std::uint32_t events = kClientBaseEvents | (want_epollout ? EPOLLOUT : 0U);
                if (!loop_.mod_fd(fd, events)) {
                    request_stop("epoll mod client failed", errno);
                    break;
                }
                // Update cached state only after successful epoll_ctl(MOD).
                conn->set_epollout_registered(want_epollout);
            }
        }
    }

    return stop_.err == 0 ? 0 : 1;
}

void Reactor::request_stop(const char* reason, int err) noexcept {
    stop_.requested = true;
    stop_.reason = reason;
    stop_.err = err;
}

void Reactor::close_connection(int fd) noexcept {
    Connection* conn = connections_.get(fd);
    if (conn == nullptr || !conn->is_active()) {
        return;
    }

    loop_.del_fd(fd);
    ::close(fd);
    connections_.deactivate(fd);
    ++stats_.close_total;
}

void Reactor::close_all_connections() noexcept {
    for (std::size_t fd = 0; fd < connections_.size(); ++fd) {
        Connection* conn = connections_.get(static_cast<int>(fd));
        if (conn != nullptr && conn->is_active()) {
            close_connection(static_cast<int>(fd));
        }
    }
}

}  // namespace reactor
