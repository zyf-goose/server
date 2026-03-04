#include <string>
#include <string_view>
#include <array>
#include <optional>
#include <iostream>

#include <kvcache/cache.hpp>

// assumptions:
// each read consists of a single complete query
// nodes are separted by #, # doesn't appear in value fields
// message finishes with /r/n
// eg. OP#KEY[#VAL]/r/n

class Protocol{
public:
    enum class Operations{
        SET = 0,
        GET,
        DEL,
        MOD,
        EXIST,
        INVALID
    };

    static constexpr size_t kMaxMessageLength = 255;

    using Tokens = std::array<std::string_view, 3>;
    std::pair<Tokens, int> parse(const char* message, size_t length){
        Tokens tokens;

        if (!message || length <= 0 || length > kMaxMessageLength){
            return {tokens, 0};
        }

        std::string_view sv (message, length);
/*
        if (sv.back() != '\n'){
            return tokens;
        }
        sv.remove_suffix(1);
        if (sv.back() != '\r'){
            return tokens;
        }
        sv.remove_suffix(1);

        if (sv.empty()){
            return tokens;
        }
*/
        size_t start = 0;
        size_t token_count = 0;
        while(start <= sv.size()){
            size_t pos = sv.find('#', start);
            if (pos == std::string_view::npos){
                pos = sv.size();
            }
            std::string_view token = sv.substr(start, pos - start);
            tokens[token_count] = token;
            token_count++;

            start = pos + 1;
        }
        return {tokens, token_count};
    }

    Operations get_op(std::string_view token){
        if (token == "GET") return Operations::GET;
        if (token == "SET") return Operations::SET;
        if (token == "DEL") return Operations::DEL;
        if (token == "MOD") return Operations::MOD;
        if (token == "EXIST") return Operations::EXIST;
        return Operations::INVALID;
    }

    std::string process_op(const Tokens& tokens, size_t token_count){
        std::string response;
        switch(get_op(tokens[0])){
            case Operations::GET:{
                if (token_count != 2){
                    response = "ERROR\r\n";
                    break;
                }
                auto res = cache_.get(std::string(tokens[1]));
                if (res == std::nullopt){
                    response = "NOT FOUND\r\n";
                    break;
                }
                response = res.value();
                response += "\r\n";
                break;
            }
            case Operations::SET:{
                if (token_count != 3){
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.set(std::string(tokens[1]), std::string(tokens[2]))){
                    // exists
                    response = "UPDATED\r\n";
                    break;
                }
                response = "ADDED\r\n";
                break;
            }
            case Operations::DEL:{               
                if (token_count != 2){
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.del(std::string(tokens[1]))){
                    response = "DELETED\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::MOD:{               
                if (token_count != 3){
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.mod(std::string(tokens[1]), std::string(tokens[2]))){
                    response = "UPDATED\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::EXIST:{
                if (token_count != 2){
                    response = "ERROR\r\n";
                    break;
                }
                if (cache_.exist(std::string(tokens[1]))){
                    response = "FOUND\r\n";
                    break;
                }
                response = "NOT FOUND\r\n";
                break;
            }
            case Operations::INVALID:
            default:{
                response = "INVALID";
                break;
            }
        }
        return response;
    }

    std::string process_bytes(const char* source, size_t size){
        //std::cout<<"handler receives: "<<std::string(source, size)<<'\n';
        auto [tokens, count] = parse(source, size);
        //std::cout<<"token 0: "<<tokens[0]<<" token 1: "<<tokens[1]<<'\n';
        return process_op(tokens, count);
    }
private:
    Cache cache_;
};