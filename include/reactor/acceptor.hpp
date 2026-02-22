#pragma once

#include "reactor/config.hpp"
#include "reactor/connection_table.hpp"
#include "reactor/event_loop.hpp"
#include "reactor/types.hpp"
#include "reactor/unique_fd.hpp"

namespace reactor {

class Acceptor {
public:
    bool init(const ReactorConfig& cfg, EventLoop& loop, StopState& stop) noexcept;

    int listener_fd() const noexcept { return listener_fd_.get(); }

    int on_readable(EventLoop& loop,
                    ConnectionTable& connections,
                    ReactorStats& stats,
                    StopState& stop) noexcept;

    void shutdown(EventLoop& loop) noexcept;

private:
    UniqueFd listener_fd_;
};

}  // namespace reactor
