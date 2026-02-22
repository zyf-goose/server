#include "reactor/config.hpp"
#include "reactor/reactor.hpp"

int main() {
    const reactor::ReactorConfig config{};
    reactor::Reactor server(config);
    return server.run();
}
