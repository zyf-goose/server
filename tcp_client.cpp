#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

struct ClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;
    std::uint32_t connections = 1;
    std::uint32_t queries_per_connection = 1;
    std::uint32_t query_bytes = 16;
    std::uint32_t threads = 1;
    int timeout_ms = 2000;
    bool verify_echo = true;
    bool tcp_nodelay = true;
};

struct Endpoint {
    sockaddr_storage addr{};
    socklen_t addr_len = 0;
    int family = AF_UNSPEC;
};

struct WorkerRange {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

struct WorkerStats {
    std::uint64_t connections_opened = 0;
    std::uint64_t queries_completed = 0;
    std::uint64_t bytes_tx = 0;
    std::uint64_t bytes_rx = 0;
    std::uint64_t failures = 0;
    std::uint64_t rtt_ns_total = 0;
    std::uint64_t rtt_ns_min = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t rtt_ns_max = 0;
};

enum class ParseResult {
    kOk,
    kHelp,
    kError,
};

void print_usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "  --host <ip-or-hostname>       default: 127.0.0.1\n"
        << "  --port <1-65535>              default: 8080\n"
        << "  --connections <count>         default: 1\n"
        << "  --queries <count>             default: 1\n"
        << "  --query-bytes <count>         default: 16\n"
        << "  --threads <count>             default: 1\n"
        << "  --timeout-ms <ms>             default: 2000\n"
        << "  --no-verify                   disable echo verification\n"
        << "  --no-nodelay                  disable TCP_NODELAY\n"
        << "  --help                        print this message\n";
}

bool parse_u32(const char* s, std::uint32_t* out) {
    if (s == nullptr || *s == '\0' || out == nullptr) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' ||
        v > static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }

    *out = static_cast<std::uint32_t>(v);
    return true;
}

bool parse_port(const char* s, std::uint16_t* out) {
    std::uint32_t tmp = 0;
    if (!parse_u32(s, &tmp) || tmp == 0 || tmp > 65535U) {
        return false;
    }
    *out = static_cast<std::uint16_t>(tmp);
    return true;
}

bool parse_i32(const char* s, int* out) {
    if (s == nullptr || *s == '\0' || out == nullptr) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < std::numeric_limits<int>::min() ||
        v > std::numeric_limits<int>::max()) {
        return false;
    }

    *out = static_cast<int>(v);
    return true;
}

ParseResult parse_args(int argc, char** argv, ClientConfig* cfg) {
    if (cfg == nullptr) {
        return ParseResult::kError;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return ParseResult::kHelp;
        }

        if (arg == "--no-verify") {
            cfg->verify_echo = false;
            continue;
        }

        if (arg == "--no-nodelay") {
            cfg->tcp_nodelay = false;
            continue;
        }

        if (i + 1 >= argc) {
            std::cerr << "missing value for " << arg << '\n';
            return ParseResult::kError;
        }

        const char* value = argv[++i];
        if (arg == "--host") {
            cfg->host = value;
        } else if (arg == "--port") {
            if (!parse_port(value, &cfg->port)) {
                std::cerr << "invalid --port: " << value << '\n';
                return ParseResult::kError;
            }
        } else if (arg == "--connections") {
            if (!parse_u32(value, &cfg->connections) || cfg->connections == 0U) {
                std::cerr << "invalid --connections: " << value << '\n';
                return ParseResult::kError;
            }
        } else if (arg == "--queries") {
            if (!parse_u32(value, &cfg->queries_per_connection)) {
                std::cerr << "invalid --queries: " << value << '\n';
                return ParseResult::kError;
            }
        } else if (arg == "--query-bytes") {
            if (!parse_u32(value, &cfg->query_bytes) || cfg->query_bytes == 0U) {
                std::cerr << "invalid --query-bytes: " << value << '\n';
                return ParseResult::kError;
            }
        } else if (arg == "--threads") {
            if (!parse_u32(value, &cfg->threads) || cfg->threads == 0U) {
                std::cerr << "invalid --threads: " << value << '\n';
                return ParseResult::kError;
            }
        } else if (arg == "--timeout-ms") {
            if (!parse_i32(value, &cfg->timeout_ms) || cfg->timeout_ms <= 0) {
                std::cerr << "invalid --timeout-ms: " << value << '\n';
                return ParseResult::kError;
            }
        } else {
            std::cerr << "unknown argument: " << arg << '\n';
            return ParseResult::kError;
        }
    }

    return ParseResult::kOk;
}

bool resolve_endpoint(const ClientConfig& cfg, Endpoint* endpoint) {
    if (endpoint == nullptr) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string service = std::to_string(cfg.port);
    const int rc = ::getaddrinfo(cfg.host.c_str(), service.c_str(), &hints, &result);
    if (rc != 0) {
        std::cerr << "getaddrinfo failed: " << ::gai_strerror(rc) << '\n';
        return false;
    }

    bool found = false;
    for (addrinfo* p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family != AF_INET && p->ai_family != AF_INET6) {
            continue;
        }
        if (static_cast<std::size_t>(p->ai_addrlen) > sizeof(endpoint->addr)) {
            continue;
        }

        std::memcpy(&endpoint->addr, p->ai_addr, static_cast<std::size_t>(p->ai_addrlen));
        endpoint->addr_len = static_cast<socklen_t>(p->ai_addrlen);
        endpoint->family = p->ai_family;
        found = true;
        break;
    }

    ::freeaddrinfo(result);

    if (!found) {
        std::cerr << "no usable endpoint found for " << cfg.host << ":" << cfg.port << '\n';
    }
    return found;
}

int open_client_socket(const ClientConfig& cfg, int family) {
    int fd = ::socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        fd = ::socket(family, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
    }

    const timeval timeout{cfg.timeout_ms / 1000, (cfg.timeout_ms % 1000) * 1000};
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        return -1;
    }

    if (cfg.tcp_nodelay) {
        int one = 1;
        if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0) {
            const int saved = errno;
            ::close(fd);
            errno = saved;
            return -1;
        }
    }

    return fd;
}

bool connect_with_timeout(int fd, const Endpoint& endpoint, int timeout_ms) {
    const int old_flags = ::fcntl(fd, F_GETFL, 0);
    if (old_flags < 0) {
        return false;
    }

    if (::fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) != 0) {
        return false;
    }

    const int rc = ::connect(fd, reinterpret_cast<const sockaddr*>(&endpoint.addr), endpoint.addr_len);
    if (rc == 0) {
        (void)::fcntl(fd, F_SETFL, old_flags);
        return true;
    }

    if (errno != EINPROGRESS) {
        (void)::fcntl(fd, F_SETFL, old_flags);
        return false;
    }

    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLOUT;
    const int poll_rc = ::poll(&pfd, 1, timeout_ms);
    if (poll_rc <= 0) {
        if (poll_rc == 0) {
            errno = ETIMEDOUT;
        }
        (void)::fcntl(fd, F_SETFL, old_flags);
        return false;
    }

    int so_error = 0;
    socklen_t opt_len = sizeof(so_error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &opt_len) != 0) {
        (void)::fcntl(fd, F_SETFL, old_flags);
        return false;
    }

    (void)::fcntl(fd, F_SETFL, old_flags);
    if (so_error != 0) {
        errno = so_error;
        return false;
    }

    return true;
}

bool send_all(int fd, const char* data, std::size_t len) {
    std::size_t sent_total = 0;
    while (sent_total < len) {
        const ssize_t sent = ::send(fd, data + sent_total, len - sent_total, 0);
        if (sent > 0) {
            sent_total += static_cast<std::size_t>(sent);
            continue;
        }

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

bool recv_all(int fd, char* data, std::size_t len) {
    std::size_t read_total = 0;
    while (read_total < len) {
        const ssize_t n = ::recv(fd, data + read_total, len - read_total, 0);
        if (n > 0) {
            read_total += static_cast<std::size_t>(n);
            continue;
        }

        if (n == 0) {
            errno = ECONNRESET;
            return false;
        }

        if (errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

void fill_query(std::vector<char>& buf, std::uint32_t conn_id, std::uint32_t query_id) noexcept {
    std::uint64_t state = (static_cast<std::uint64_t>(conn_id) << 32U) | query_id;
    for (std::size_t i = 0; i < buf.size(); ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = static_cast<char>('A' + ((state >> 58U) % 26U));
    }
}

void run_worker(const ClientConfig& cfg,
                const Endpoint& endpoint,
                WorkerRange range,
                std::atomic<bool>* stop,
                WorkerStats* out) {
    if (out == nullptr || stop == nullptr) {
        return;
    }

    WorkerStats stats{};
    std::vector<char> tx(static_cast<std::size_t>(cfg.query_bytes));
    std::vector<char> rx(static_cast<std::size_t>(cfg.query_bytes));

    for (std::uint32_t conn_id = range.begin; conn_id < range.end; ++conn_id) {
        // Acquire pairs with release store on failure so threads observe the stop request.
        if (stop->load(std::memory_order_acquire)) {
            break;
        }

        const int fd = open_client_socket(cfg, endpoint.family);
        if (fd < 0) {
            ++stats.failures;
            stop->store(true, std::memory_order_release);
            break;
        }

        if (!connect_with_timeout(fd, endpoint, cfg.timeout_ms)) {
            ++stats.failures;
            stop->store(true, std::memory_order_release);
            ::close(fd);
            break;
        }

        ++stats.connections_opened;

        for (std::uint32_t q = 0; q < cfg.queries_per_connection; ++q) {
            if (stop->load(std::memory_order_acquire)) {
                break;
            }

            fill_query(tx, conn_id, q);
            const auto start = std::chrono::steady_clock::now();

            if (!send_all(fd, tx.data(), tx.size())) {
                ++stats.failures;
                stop->store(true, std::memory_order_release);
                break;
            }

            if (!recv_all(fd, rx.data(), rx.size())) {
                ++stats.failures;
                stop->store(true, std::memory_order_release);
                break;
            }

            const auto end = std::chrono::steady_clock::now();
            const auto rtt_ns =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               end - start)
                                               .count());

            if (cfg.verify_echo && std::memcmp(tx.data(), rx.data(), tx.size()) != 0) {
                ++stats.failures;
                stop->store(true, std::memory_order_release);
                break;
            }

            ++stats.queries_completed;
            stats.bytes_tx += static_cast<std::uint64_t>(tx.size());
            stats.bytes_rx += static_cast<std::uint64_t>(rx.size());
            stats.rtt_ns_total += rtt_ns;
            if (rtt_ns < stats.rtt_ns_min) {
                stats.rtt_ns_min = rtt_ns;
            }
            if (rtt_ns > stats.rtt_ns_max) {
                stats.rtt_ns_max = rtt_ns;
            }
        }

        ::close(fd);
    }

    *out = stats;
}

std::vector<WorkerRange> split_ranges(std::uint32_t connections, std::uint32_t threads) {
    std::vector<WorkerRange> ranges(static_cast<std::size_t>(threads));
    const std::uint32_t base = connections / threads;
    const std::uint32_t remainder = connections % threads;

    std::uint32_t cursor = 0;
    for (std::uint32_t i = 0; i < threads; ++i) {
        const std::uint32_t span = base + (i < remainder ? 1U : 0U);
        ranges[static_cast<std::size_t>(i)] = WorkerRange{cursor, static_cast<std::uint32_t>(cursor + span)};
        cursor += span;
    }

    return ranges;
}

}  // namespace

int main(int argc, char** argv) {
    ClientConfig cfg{};
    const ParseResult parse_result = parse_args(argc, argv, &cfg);
    if (parse_result == ParseResult::kHelp) {
        return 0;
    }
    if (parse_result != ParseResult::kOk) {
        return 1;
    }

    if (cfg.threads > cfg.connections) {
        cfg.threads = cfg.connections;
    }

    Endpoint endpoint{};
    if (!resolve_endpoint(cfg, &endpoint)) {
        return 1;
    }

    const auto ranges = split_ranges(cfg.connections, cfg.threads);

    std::atomic<bool> stop{false};
    std::vector<WorkerStats> stats(cfg.threads);
    std::vector<std::thread> workers;
    workers.reserve(cfg.threads);

    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i < cfg.threads; ++i) {
        workers.emplace_back(run_worker,
                             std::cref(cfg),
                             std::cref(endpoint),
                             ranges[static_cast<std::size_t>(i)],
                             &stop,
                             &stats[static_cast<std::size_t>(i)]);
    }

    for (auto& worker : workers) {
        worker.join();
    }
    const auto end = std::chrono::steady_clock::now();

    std::uint64_t total_connections_opened = 0;
    std::uint64_t total_queries_completed = 0;
    std::uint64_t total_failures = 0;
    std::uint64_t total_bytes_tx = 0;
    std::uint64_t total_bytes_rx = 0;
    std::uint64_t total_rtt_ns = 0;
    std::uint64_t min_rtt_ns = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t max_rtt_ns = 0;

    for (const WorkerStats& ws : stats) {
        total_connections_opened += ws.connections_opened;
        total_queries_completed += ws.queries_completed;
        total_failures += ws.failures;
        total_bytes_tx += ws.bytes_tx;
        total_bytes_rx += ws.bytes_rx;
        total_rtt_ns += ws.rtt_ns_total;
        if (ws.rtt_ns_min < min_rtt_ns) {
            min_rtt_ns = ws.rtt_ns_min;
        }
        if (ws.rtt_ns_max > max_rtt_ns) {
            max_rtt_ns = ws.rtt_ns_max;
        }
    }

    const std::uint64_t target_queries =
        static_cast<std::uint64_t>(cfg.connections) * static_cast<std::uint64_t>(cfg.queries_per_connection);
    const auto elapsed_ns =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    const double elapsed_s = static_cast<double>(elapsed_ns) / 1e9;
    const double qps = elapsed_s > 0.0 ? static_cast<double>(total_queries_completed) / elapsed_s : 0.0;
    const double avg_rtt_us = total_queries_completed > 0
                                  ? static_cast<double>(total_rtt_ns) / static_cast<double>(total_queries_completed) /
                                        1000.0
                                  : 0.0;
    const double min_rtt_us =
        total_queries_completed > 0 ? static_cast<double>(min_rtt_ns) / 1000.0 : 0.0;
    const double max_rtt_us =
        total_queries_completed > 0 ? static_cast<double>(max_rtt_ns) / 1000.0 : 0.0;

    std::cout << "host=" << cfg.host << " port=" << cfg.port << " threads=" << cfg.threads
              << " connections_target=" << cfg.connections << " queries_per_connection=" << cfg.queries_per_connection
              << " query_bytes=" << cfg.query_bytes << " verify=" << (cfg.verify_echo ? "on" : "off") << '\n';

    std::cout << "connections_opened=" << total_connections_opened << " queries_target=" << target_queries
              << " queries_completed=" << total_queries_completed << " failures=" << total_failures
              << " bytes_tx=" << total_bytes_tx << " bytes_rx=" << total_bytes_rx << '\n';

    std::cout << "elapsed_ms=" << (static_cast<double>(elapsed_ns) / 1e6) << " qps=" << qps
              << " avg_rtt_us=" << avg_rtt_us << " min_rtt_us=" << min_rtt_us
              << " max_rtt_us=" << max_rtt_us << '\n';

    return total_failures == 0 && total_queries_completed == target_queries ? 0 : 1;
}
