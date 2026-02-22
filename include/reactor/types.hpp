#pragma once

#include <cstdint>

namespace reactor {

struct StopState {
    bool requested = false;
    int err = 0;
    const char* reason = nullptr;
};

struct ReactorStats {
    std::uint64_t accept_total = 0;
    std::uint64_t accept_eagain = 0;
    std::uint64_t close_total = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
};

}  // namespace reactor
