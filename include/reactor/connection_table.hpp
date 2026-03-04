#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "reactor/connection.hpp"

namespace reactor {

class ConnectionTable {
public:
    using HandlerFactory = std::function<std::unique_ptr<EventHandler>()>;
    explicit ConnectionTable(std::size_t max_connections, HandlerFactory factory);

    bool can_track(int fd) const noexcept;

    Connection* get(int fd) noexcept;
    const Connection* get(int fd) const noexcept;

    bool activate(int fd) noexcept;
    void deactivate(int fd) noexcept;

    std::size_t size() const noexcept { return slots_.size(); }

private:
    std::vector<Connection> slots_;
    HandlerFactory factory_;
};

}  // namespace reactor
