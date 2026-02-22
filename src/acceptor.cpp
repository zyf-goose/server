#include "reactor/acceptor.hpp"

#include <arpa/inet.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "reactor/sys_utils.hpp"

namespace reactor {

namespace {

int create_listener_socket() noexcept {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd >= 0) {
        return fd;
    }

    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (!set_non_blocking(fd) || !set_close_on_exec(fd)) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        return -1;
    }

    return fd;
}

}  // namespace

bool Acceptor::init(const ReactorConfig& cfg, EventLoop& loop, StopState& stop) noexcept {
    int server_fd = create_listener_socket();
    if (server_fd < 0) {
        stop.requested = true;
        stop.reason = "listener socket creation failed";
        stop.err = errno;
        return false;
    }

    UniqueFd listener(server_fd);

    if (!set_reuse_addr(listener.get())) {
        stop.requested = true;
        stop.reason = "SO_REUSEADDR setup failed";
        stop.err = errno;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(cfg.port));

    if (::bind(listener.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        stop.requested = true;
        stop.reason = "bind failed";
        stop.err = errno;
        return false;
    }

    if (::listen(listener.get(), cfg.backlog) < 0) {
        stop.requested = true;
        stop.reason = "listen failed";
        stop.err = errno;
        return false;
    }

    if (!loop.add_fd(listener.get(), EPOLLIN)) {
        stop.requested = true;
        stop.reason = "epoll add listener failed";
        stop.err = errno;
        return false;
    }

    std::cout << "listening on port " << cfg.port << " with epoll\n";
    listener_fd_ = std::move(listener);
    return true;
}

int Acceptor::on_readable(EventLoop& loop,
                          ConnectionTable& connections,
                          ReactorStats& stats,
                          StopState& stop) noexcept {
    int accepted = 0;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd =
            ::accept4(listener_fd_.get(),
                      reinterpret_cast<sockaddr*>(&client_addr),
                      &client_len,
                      SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (client_fd < 0) {
            const int err = errno;
            if (is_would_block_errno(err)) {
                ++stats.accept_eagain;
                return accepted;
            }
            if (err == EINTR) {
                continue;
            }

            if (is_resource_limit_errno(err)) {
                stop.requested = true;
                stop.reason = "accept hit OS resource limit";
                stop.err = err;
            }

            std::cerr << "accept failed errno=" << err << " (" << std::strerror(err) << ")\n";
            return accepted;
        }

        if (!connections.can_track(client_fd)) {
            ::close(client_fd);
            stop.requested = true;
            stop.reason = "application connection table exhausted";
            stop.err = 0;
            return accepted;
        }

        if (!loop.add_fd(client_fd, EPOLLIN | EPOLLRDHUP)) {
            const int err = errno;
            ::close(client_fd);

            if (is_epoll_limit_errno(err)) {
                stop.requested = true;
                stop.reason = "epoll watch/resource limit reached";
                stop.err = err;
            }

            std::cerr << "epoll_ctl(ADD client) failed errno=" << err << " (" << std::strerror(err)
                      << ")\n";
            return accepted;
        }

        if (!connections.activate(client_fd)) {
            loop.del_fd(client_fd);
            ::close(client_fd);
            stop.requested = true;
            stop.reason = "connection activation failed";
            stop.err = 0;
            return accepted;
        }

        ++accepted;
        ++stats.accept_total;
        if ((stats.accept_total % 10000U) == 0U) {
            std::cout << "accepted_total=" << stats.accept_total << '\n';
        }
    }
}

void Acceptor::shutdown(EventLoop& loop) noexcept {
    if (!listener_fd_.valid()) {
        return;
    }

    loop.del_fd(listener_fd_.get());
    listener_fd_.reset();
}

}  // namespace reactor
