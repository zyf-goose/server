#include "utils/event_handler.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <memory>

class EchoHandler : public EventHandler{
public:
    EchoHandler() = default;

    // return number of bytes consumed by handler
    size_t on_bytes(const char* source, size_t size) override{
        if (source == nullptr || size == 0){
            return 0;
        }
        const size_t nBytes = std::min(size, kBufferSize - cursor_);
        std::memcpy(buffer_.begin() + cursor_, source, nBytes);
        cursor_ += nBytes;

        return nBytes;
    }
    // return number of bytes consumed by connection
    size_t append_output(char* target, size_t size) override{
        if (target == nullptr || size == 0){
            return 0;
        }

        const size_t nBytes = std::min(size, cursor_);
        std::memcpy(target, buffer_.begin(), nBytes);
        cursor_ -= nBytes;
        if (cursor_ != 0){
            std::memmove(buffer_.data(), buffer_.data() + nBytes, cursor_);
        }

        return nBytes;
    }

private:
    static constexpr size_t kBufferSize = 1024;
    std::array<char, kBufferSize> buffer_;
    size_t cursor_ = 0;
};

std::unique_ptr<EventHandler> make_echo_handler()
{
    return std::make_unique<EchoHandler>();
}