#pragma once

#include <array>
#include <string>
#include <string_view>

#include <kvcache/cache.hpp>

// Wire format:
// OP#KEY[#VAL]\r\n
// '#' is the field delimiter and values must not contain '#'.
class Protocol {
public:
    enum class Operations {
        SET = 0,
        GET,
        DEL,
        MOD,
        EXIST,
        INVALID
    };

    static constexpr std::size_t kMaxMessageLength = 255;
    static constexpr std::string_view kFrameTerminator = "\r\n";

    using Tokens = std::array<std::string_view, 3>;

    std::pair<Tokens, std::size_t> parse(std::string_view message) const noexcept {
        Tokens tokens{};
        if (message.empty() || message.size() > kMaxMessageLength) {
            return {tokens, 0U};
        }

        std::size_t start = 0;
        std::size_t token_count = 0;
        while (start <= message.size()) {
            if (token_count == tokens.size()) {
                return {Tokens{}, 0U};
            }

            std::size_t pos = message.find('#', start);
            if (pos == std::string_view::npos) {
                pos = message.size();
            }

            tokens[token_count] = message.substr(start, pos - start);
            ++token_count;
            start = pos + 1U;
        }

        return {tokens, token_count};
    }

    Operations get_op(std::string_view token) const noexcept {
        if (token == "GET") return Operations::GET;
        if (token == "SET") return Operations::SET;
        if (token == "DEL") return Operations::DEL;
        if (token == "MOD") return Operations::MOD;
        if (token == "EXIST") return Operations::EXIST;
        return Operations::INVALID;
    }

    std::string process_op(const Tokens& tokens, std::size_t token_count) {
        std::string response;
        switch (get_op(tokens[0])) {
            case Operations::GET: {
                if (token_count != 2U) {
                    response = "ERROR\r\n";
                    break;
                }
                auto res = cache_.get(std::string(tokens[1]));
                if (res == std::nullopt) {
                    response = "NOT FOUND\r\n";
                    break;
                }
                response = res.value();
                response += "\r\n";
                break;
            }
            case Operations::SET: {
                if (token_count != 3U) {
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.set(std::string(tokens[1]), std::string(tokens[2]))) {
                    response = "UPDATED\r\n";
                    break;
                }
                response = "ADDED\r\n";
                break;
            }
            case Operations::DEL: {
                if (token_count != 2U) {
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.del(std::string(tokens[1]))) {
                    response = "DELETED\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::MOD: {
                if (token_count != 3U) {
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.mod(std::string(tokens[1]), std::string(tokens[2]))) {
                    response = "UPDATED\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::EXIST: {
                if (token_count != 2U) {
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.exist(std::string(tokens[1]))) {
                    response = "FOUND\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::INVALID:
            default: {
                response = "ERROR\r\n";
                break;
            }
        }
        return response;
    }

    std::string process_message(std::string_view message) {
        if (message.ends_with(kFrameTerminator)) {
            message.remove_suffix(kFrameTerminator.size());
        }

        auto [tokens, count] = parse(message);
        if (count == 0U) {
            return "ERROR\r\n";
        }
        return process_op(tokens, count);
    }

    std::string process_bytes(const char* source, std::size_t size) {
        if (source == nullptr) {
            return "ERROR\r\n";
        }
        return process_message(std::string_view(source, size));
    }

private:
    Cache cache_;
};
