#pragma once

#include <vector>
#include <functional>
#include <memory>

#include "reactor/acceptor.hpp"
#include "reactor/config.hpp"
#include "reactor/connection_table.hpp"
#include "reactor/event_loop.hpp"
#include "reactor/types.hpp"

#include "utils/event_handler.hpp"

namespace reactor {

class Reactor {
public:
    using HandlerFactory = std::function<std::unique_ptr<EventHandler>()>;
    explicit Reactor(const ReactorConfig& config, HandlerFactory factory);

    int run() noexcept;

    const ReactorStats& stats() const noexcept { return stats_; }

private:
    int run_main_loop() noexcept;

    void request_stop(const char* reason, int err) noexcept;
    void close_connection(int fd) noexcept;
    void close_all_connections() noexcept;

    ReactorConfig config_;
    EventLoop loop_;
    Acceptor acceptor_;
    ConnectionTable connections_;
    StopState stop_;
    ReactorStats stats_;
    std::vector<epoll_event> events_;
};

}  // namespace reactor
