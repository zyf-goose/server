#include <utils/kv_handler.hpp>
#include <algorithm>
#include <cstring>
#include <string_view>

namespace {

constexpr std::string_view kProtocolError = "ERROR\r\n";

}  // namespace

bool KvHandler::enqueue_output(std::string_view response) noexcept {
    const std::size_t free_space = output_buffer_.size() - output_size_;
    if (response.size() > free_space) {
        return false;
    }

    std::memcpy(output_buffer_.data() + output_size_, response.data(), response.size());
    output_size_ += response.size();
    return true;
}

void KvHandler::consume_input(std::size_t bytes) noexcept {
    if (bytes >= input_size_) {
        input_size_ = 0;
        return;
    }

    input_size_ -= bytes;
    std::memmove(input_buffer_.data(), input_buffer_.data() + bytes, input_size_);
}

void KvHandler::consume_output(std::size_t bytes) noexcept {
    if (bytes >= output_size_) {
        output_size_ = 0;
        return;
    }

    output_size_ -= bytes;
    std::memmove(output_buffer_.data(), output_buffer_.data() + bytes, output_size_);
}

std::size_t KvHandler::on_bytes(const char* source, std::size_t size) {
    if (source == nullptr || size == 0U) {
        return 0U;
    }

    const std::size_t free_space = input_buffer_.size() - input_size_;
    const std::size_t copied = std::min(size, free_space);
    if (copied != 0U) {
        std::memcpy(input_buffer_.data() + input_size_, source, copied);
        input_size_ += copied;
    }

    for (;;) {
        const char* frame_end = nullptr;
        for (std::size_t i = 1; i < input_size_; ++i) {
            if (input_buffer_[i - 1] == '\r' && input_buffer_[i] == '\n') {
                frame_end = input_buffer_.data() + (i - 1U);
                break;
            }
        }

        if (frame_end == nullptr) {
            break;
        }

        const std::size_t frame_len = static_cast<std::size_t>(frame_end - input_buffer_.data());
        const std::string response =
            protocol_.process_message(std::string_view(input_buffer_.data(), frame_len));
        if (!enqueue_output(response)) {
            output_size_ = 0;
            enqueue_output(kProtocolError);
            input_size_ = 0;
            break;
        }

        consume_input(frame_len + Protocol::kFrameTerminator.size());
    }

    if (copied < size || input_size_ > Protocol::kMaxMessageLength + Protocol::kFrameTerminator.size()) {
        input_size_ = 0;
        output_size_ = 0;
        enqueue_output(kProtocolError);
    }

    return copied;
}

std::size_t KvHandler::append_output(char* target, std::size_t size) {
    if (target == nullptr || size == 0U || output_size_ == 0U) {
        return 0U;
    }

    const std::size_t copied = std::min(size, output_size_);
    std::memcpy(target, output_buffer_.data(), copied);
    consume_output(copied);
    return copied;
}    
