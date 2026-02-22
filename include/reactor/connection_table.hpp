#pragma once

#include <cstddef>
#include <vector>

#include "reactor/connection.hpp"

namespace reactor {

class ConnectionTable {
public:
    explicit ConnectionTable(std::size_t max_connections);

    bool can_track(int fd) const noexcept;

    Connection* get(int fd) noexcept;
    const Connection* get(int fd) const noexcept;

    bool activate(int fd) noexcept;
    void deactivate(int fd) noexcept;

    std::size_t size() const noexcept { return slots_.size(); }

private:
    std::vector<Connection> slots_;
};

}  // namespace reactor
