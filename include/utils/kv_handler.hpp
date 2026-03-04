#pragma once

#include <memory>

#include "kvcache/protocol.hpp"
#include "utils/event_handler.hpp"


class KvHandler : public EventHandler{
public:
    KvHandler(Protocol& protocol):
        protocol_(protocol){}
    size_t on_bytes(const char* source, size_t size) override;    
    size_t append_output(char* target, size_t size) override;

private:
    static constexpr size_t kMaxBuffer = 1024;
    Protocol& protocol_;
    std::array<char, kMaxBuffer> buffer_;
    size_t cursor_ = 0;
};

inline std::unique_ptr<EventHandler> make_kv_handler(Protocol& protocol){
    return std::make_unique<KvHandler>(protocol);
}
