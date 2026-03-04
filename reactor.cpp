#include "reactor/config.hpp"
#include "reactor/reactor.hpp"
#include "utils/echo_handler.hpp"
#include "utils/kv_handler.hpp"

#include <memory>

int main() {
    const reactor::ReactorConfig config{};
    Protocol protocol;
    reactor::Reactor server(config, [&protocol](){
        return make_kv_handler(protocol);
    });

    //reactor::Reactor server(config, make_echo_handler);
    return server.run();
}
