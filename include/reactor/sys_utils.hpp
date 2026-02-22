#pragma once

namespace reactor {

bool set_non_blocking(int fd) noexcept;
bool set_close_on_exec(int fd) noexcept;
bool set_reuse_addr(int fd) noexcept;

bool is_would_block_errno(int err) noexcept;
bool is_resource_limit_errno(int err) noexcept;
bool is_epoll_limit_errno(int err) noexcept;

}  // namespace reactor
