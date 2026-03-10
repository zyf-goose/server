#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "kvcache/protocol.hpp"
#include "utils/event_handler.hpp"


class KvHandler : public EventHandler {
public:
    explicit KvHandler(Protocol& protocol) noexcept : protocol_(protocol) {}

    std::size_t on_bytes(const char* source, std::size_t size) override;
    std::size_t append_output(char* target, std::size_t size) override;

private:
    static constexpr std::size_t kInputBufferSize = 4096;
    static constexpr std::size_t kOutputBufferSize = 8192;

    bool enqueue_output(std::string_view response) noexcept;
    void consume_input(std::size_t bytes) noexcept;
    void consume_output(std::size_t bytes) noexcept;

    Protocol& protocol_;
    std::array<char, kInputBufferSize> input_buffer_{};
    std::array<char, kOutputBufferSize> output_buffer_{};
    std::size_t input_size_ = 0;
    std::size_t output_size_ = 0;
};

inline std::unique_ptr<EventHandler> make_kv_handler(Protocol& protocol) {
    return std::make_unique<KvHandler>(protocol);
}
