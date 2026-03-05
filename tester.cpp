#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>

#include "kvcache/protocol.hpp"

namespace {

bool expect_eq(const std::string& name, const std::string& got, const std::string& want) {
    if (got == want) {
        std::cout << "[PASS] " << name << '\n';
        return true;
    }
    std::cerr << "[FAIL] " << name << " got='" << got << "' want='" << want << "'\n";
    return false;
}

bool expect_true(const std::string& name, bool value) {
    if (value) {
        std::cout << "[PASS] " << name << '\n';
        return true;
    }
    std::cerr << "[FAIL] " << name << " expected true\n";
    return false;
}

bool expect_false(const std::string& name, bool value) {
    if (!value) {
        std::cout << "[PASS] " << name << '\n';
        return true;
    }
    std::cerr << "[FAIL] " << name << " expected false\n";
    return false;
}

bool expect_opt_eq(const std::string& name,
                   const std::optional<std::string>& value,
                   const std::optional<std::string>& want) {
    if (value == want) {
        std::cout << "[PASS] " << name << '\n';
        return true;
    }
    const std::string got_s = value.has_value() ? *value : "<null>";
    const std::string want_s = want.has_value() ? *want : "<null>";
    std::cerr << "[FAIL] " << name << " got='" << got_s << "' want='" << want_s << "'\n";
    return false;
}

bool run_cache_tests() {
    bool ok = true;
    Cache cache(2);

    ok &= expect_opt_eq("cache.get missing", cache.get("k0"), std::nullopt);
    ok &= expect_false("cache.set insert returns false", cache.set("k1", "v1"));
    ok &= expect_opt_eq("cache.get k1", cache.get("k1"), std::optional<std::string>("v1"));
    ok &= expect_true("cache.set update returns true", cache.set("k1", "v1b"));
    ok &= expect_opt_eq("cache.get k1 updated", cache.get("k1"), std::optional<std::string>("v1b"));

    ok &= expect_false("cache.set k2 insert", cache.set("k2", "v2"));
    ok &= expect_false("cache.set k3 insert + evict", cache.set("k3", "v3"));
    ok &= expect_false("cache.exist evicted k1", cache.exist("k1"));
    ok &= expect_true("cache.exist k2", cache.exist("k2"));
    ok &= expect_true("cache.exist k3", cache.exist("k3"));
    ok &= expect_true("cache.del k2", cache.del("k2"));
    ok &= expect_false("cache.del missing", cache.del("k2"));

    return ok;
}

bool run_protocol_tests() {
    bool ok = true;
    Protocol protocol;

    ok &= expect_eq("proto SET", protocol.process_bytes("SET#pk1#pv1", 11), "ADDED\r\n");
    ok &= expect_eq("proto GET hit", protocol.process_bytes("GET#pk1", 7), "pv1\r\n");
    ok &= expect_eq("proto EXIST hit", protocol.process_bytes("EXIST#pk1", 9), "FOUND\r\n");
    ok &= expect_eq("proto DEL hit", protocol.process_bytes("DEL#pk1", 7), "DELETED\r\n");
    ok &= expect_eq("proto GET miss", protocol.process_bytes("GET#pk1", 7), "NOT FOUND\r\n");

    return ok;
}

int connect_server() {
    const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (::inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        ::close(sock);
        return -1;
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock);
        return -1;
    }

    return sock;
}

bool send_all(int sock, const std::string& msg) {
    std::size_t sent_total = 0;
    while (sent_total < msg.size()) {
        const ssize_t sent = ::send(sock, msg.data() + sent_total, msg.size() - sent_total, 0);
        if (sent <= 0) {
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

bool recv_once(int sock, std::string* out) {
    char buffer[1024] = {};
    const ssize_t bytes = ::recv(sock, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        return false;
    }
    out->assign(buffer, static_cast<std::size_t>(bytes));
    return true;
}

bool check_round_trip(int sock,
                      const std::string& request,
                      const std::string& expected_response,
                      const std::string& name) {
    if (!send_all(sock, request)) {
        std::cerr << "[FAIL] " << name << " send failed\n";
        return false;
    }

    std::string response;
    if (!recv_once(sock, &response)) {
        std::cerr << "[FAIL] " << name << " recv failed\n";
        return false;
    }
    return expect_eq(name, response, expected_response);
}

bool run_connection_tests() {
    int sock = connect_server();
    if (sock < 0) {
        std::cerr << "[SKIP] connection tests (start reactor on 127.0.0.1:8080)\n";
        return true;
    }

    bool ok = true;
    ok &= check_round_trip(sock, "DEL#nk1", "NOT FOUND\r\n", "net DEL warmup");
    ok &= check_round_trip(sock, "SET#nk1#nv1", "ADDED\r\n", "net SET");
    ok &= check_round_trip(sock, "GET#nk1", "nv1\r\n", "net GET");
    ok &= check_round_trip(sock, "MOD#nk1#nv2", "UPDATED\r\n", "net MOD");
    ok &= check_round_trip(sock, "GET#nk1", "nv2\r\n", "net GET updated");
    ok &= check_round_trip(sock, "EXIST#nk1", "FOUND\r\n", "net EXIST");
    ok &= check_round_trip(sock, "DEL#nk1", "DELETED\r\n", "net DEL");
    ok &= check_round_trip(sock, "GET#nk1", "NOT FOUND\r\n", "net GET miss");

    ::close(sock);
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= run_cache_tests();
    ok &= run_protocol_tests();
    ok &= run_connection_tests();
    return ok ? 0 : 1;
}
