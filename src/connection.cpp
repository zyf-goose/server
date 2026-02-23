#include "reactor/connection.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <sys/socket.h>
#include <unistd.h>

#include "reactor/sys_utils.hpp"

namespace reactor {

void Connection::attach(int fd) noexcept {
    fd_ = fd;
    active_ = true;
    epollout_registered_ = false;
    write_buffer_begin_ = 0;
    write_buffer_size_ = 0;
}

void Connection::reset() noexcept {
    fd_ = -1;
    active_ = false;
    epollout_registered_ = false;
    write_buffer_begin_ = 0;
    write_buffer_size_ = 0;
}

bool Connection::enqueue_write(const char* data, std::size_t len) noexcept {
    if (len == 0U) {
        return true;
    }

    const std::size_t capacity = write_buffer_.size();
    const std::size_t free_space = capacity - write_buffer_size_;
    if (len > free_space) {
        return false;
    }

    const std::size_t tail = (write_buffer_begin_ + write_buffer_size_) % capacity;
    const std::size_t first = std::min(len, capacity - tail);
    std::memcpy(write_buffer_.data() + tail, data, first);
    const std::size_t second = len - first;
    if (second != 0U) {
        std::memcpy(write_buffer_.data(), data + first, second);
    }

    write_buffer_size_ += len;
    return true;
}

bool Connection::flush_write(IoResult& result) noexcept {
    const std::size_t capacity = write_buffer_.size();
    while (write_buffer_size_ != 0U) {
        const std::size_t chunk = std::min(write_buffer_size_, capacity - write_buffer_begin_);
        const ssize_t sent =
            ::send(fd_, write_buffer_.data() + write_buffer_begin_, chunk, MSG_NOSIGNAL);
        if (sent > 0) {
            const std::size_t written = static_cast<std::size_t>(sent);
            write_buffer_begin_ = (write_buffer_begin_ + written) % capacity;
            write_buffer_size_ -= written;
            result.write_bytes += written;
            continue;
        }

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        if (sent < 0 && is_would_block_errno(errno)) {
            return true;
        }

        return false;
    }

    write_buffer_begin_ = 0;
    return true;
}

IoResult Connection::on_writable() noexcept {
    IoResult result{};
    if (!active_) {
        return result;
    }

    if (!flush_write(result)) {
        result.should_close = true;
    }
    return result;
}

IoResult Connection::on_readable() noexcept {
    IoResult result{};
    if (!active_) {
        return result;
    }

    if (!flush_write(result)) {
        result.should_close = true;
        return result;
    }

    while (true) {
        const ssize_t bytes_received = ::recv(fd_, read_buffer_.data(), read_buffer_.size(), 0);
        if (bytes_received == 0) {
            result.should_close = true;
            return result;
        }

        if (bytes_received < 0) {
            const int err = errno;
            if (err == EINTR) {
                continue;
            }
            if (is_would_block_errno(err)) {
                return result;
            }
            result.should_close = true;
            return result;
        }

        result.read_bytes += static_cast<std::size_t>(bytes_received);

        std::size_t offset = 0;
        if (write_buffer_size_ == 0U) {
            while (offset < static_cast<std::size_t>(bytes_received)) {
                const ssize_t sent = ::send(fd_,
                                            read_buffer_.data() + offset,
                                            static_cast<std::size_t>(bytes_received) - offset,
                                            MSG_NOSIGNAL);
                if (sent > 0) {
                    const std::size_t written = static_cast<std::size_t>(sent);
                    offset += written;
                    result.write_bytes += written;
                    continue;
                }

                if (sent < 0 && errno == EINTR) {
                    continue;
                }

                if (sent < 0 && is_would_block_errno(errno)) {
                    break;
                }

                result.should_close = true;
                return result;
            }
        }

        if (offset < static_cast<std::size_t>(bytes_received)) {
            const std::size_t remaining = static_cast<std::size_t>(bytes_received) - offset;
            if (!enqueue_write(read_buffer_.data() + offset, remaining)) {
                result.should_close = true;
                return result;
            }

            if (!flush_write(result)) {
                result.should_close = true;
                return result;
            }
        }
    }

    return result;
}

}  // namespace reactor
