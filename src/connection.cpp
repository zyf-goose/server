#include "reactor/connection.hpp"

#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>

#include "reactor/sys_utils.hpp"

namespace reactor {

void Connection::attach(int fd) noexcept {
    fd_ = fd;
    active_ = true;
}

void Connection::reset() noexcept {
    fd_ = -1;
    active_ = false;
}

IoResult Connection::on_readable() noexcept {
    IoResult result{};

    if (!active_) {
        return result;
    }

    const ssize_t bytes_received = ::recv(fd_, rx_buffer_.data(), rx_buffer_.size(), 0);
    if (bytes_received == 0) {
        result.should_close = true;
        return result;
    }

    if (bytes_received < 0) {
        const int err = errno;
        if (err == EINTR || is_would_block_errno(err)) {
            return result;
        }
        result.should_close = true;
        return result;
    }

    result.read_bytes = static_cast<std::size_t>(bytes_received);

    ssize_t total_sent = 0;
    while (total_sent < bytes_received) {
        const ssize_t sent = ::send(fd_,
                                    rx_buffer_.data() + total_sent,
                                    static_cast<std::size_t>(bytes_received - total_sent),
                                    0);
        if (sent > 0) {
            total_sent += sent;
            continue;
        }

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        result.should_close = true;
        return result;
    }

    result.write_bytes = static_cast<std::size_t>(total_sent);
    return result;
}

}  // namespace reactor
