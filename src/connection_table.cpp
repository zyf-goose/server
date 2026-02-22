#include "reactor/connection_table.hpp"

namespace reactor {

ConnectionTable::ConnectionTable(std::size_t max_connections) : slots_(max_connections) {}

bool ConnectionTable::can_track(int fd) const noexcept {
    return fd >= 0 && static_cast<std::size_t>(fd) < slots_.size();
}

Connection* ConnectionTable::get(int fd) noexcept {
    if (!can_track(fd)) {
        return nullptr;
    }
    return &slots_[static_cast<std::size_t>(fd)];
}

const Connection* ConnectionTable::get(int fd) const noexcept {
    if (!can_track(fd)) {
        return nullptr;
    }
    return &slots_[static_cast<std::size_t>(fd)];
}

bool ConnectionTable::activate(int fd) noexcept {
    Connection* c = get(fd);
    if (c == nullptr) {
        return false;
    }
    c->attach(fd);
    return true;
}

void ConnectionTable::deactivate(int fd) noexcept {
    Connection* c = get(fd);
    if (c != nullptr) {
        c->reset();
    }
}

}  // namespace reactor
