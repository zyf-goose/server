#include "reactor/sys_utils.hpp"

#include <cerrno>

#include <fcntl.h>
#include <sys/socket.h>

namespace reactor {

bool set_non_blocking(int fd) noexcept {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool set_close_on_exec(int fd) noexcept {
    const int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

bool set_reuse_addr(int fd) noexcept {
    int one = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0;
}

bool is_would_block_errno(int err) noexcept {
    return err == EAGAIN || err == EWOULDBLOCK;
}

bool is_resource_limit_errno(int err) noexcept {
    return err == EMFILE || err == ENFILE || err == ENOMEM || err == ENOBUFS;
}

bool is_epoll_limit_errno(int err) noexcept {
    return err == EMFILE || err == ENFILE || err == ENOSPC || err == ENOMEM;
}

}  // namespace reactor
