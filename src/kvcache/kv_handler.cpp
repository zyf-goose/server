#include <utils/kv_handler.hpp>
#include <string>
#include <string_view>
#include <cstring>
#include <algorithm>


size_t KvHandler::on_bytes(const char* source, size_t size){
    //assuming no buffer overflow or wrap in local and connection
    std::string res(protocol_.process_bytes(source, size));
    std::memmove(buffer_.data(), res.data(), res.size());
    cursor_ = res.size();
    return cursor_;
}    
size_t KvHandler::append_output(char* target, size_t size){
    size_t n = std::min(size, cursor_);
    std::memmove(target, buffer_.data(), n);
    cursor_ = 0;
    return n;
}