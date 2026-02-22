#pragma once

#include <cstddef>

namespace reactor {

struct ReactorConfig {
    int port = 8080;
    int backlog = 1024;
    int max_events = 1024;
    std::size_t max_connections = 1024U * 1024U;
};

constexpr std::size_t kConnectionBufferSize = 16;

}  // namespace reactor
