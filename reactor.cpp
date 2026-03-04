#include "reactor/config.hpp"
#include "reactor/reactor.hpp"
#include "utils/echo_handler.hpp"

int main() {
    const reactor::ReactorConfig config{};
    reactor::Reactor server(config, make_echo_handler);
    return server.run();
}
