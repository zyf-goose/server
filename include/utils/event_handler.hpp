#pragma once
#include <cstddef>

class EventHandler{
public:
    virtual size_t on_bytes(const char* source, size_t size) = 0;    
    virtual size_t append_output(char* target, size_t size) = 0;

    virtual ~EventHandler(){};
};