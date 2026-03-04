#include "kvcache/cache.hpp"

using Value = Cache::Value;
using Key = Cache::Key;
std::optional<Value> Cache::get(Key key){    
    auto mapIter = map_.find(key); 
    if (mapIter == map_.end()){
        return std::nullopt;
    }
    touch(mapIter->second);
    return mapIter->second->second;
}

// always successful, return true if the value exists, false if not
bool Cache::set(Key key, Value val){    
    auto mapIter = map_.find(key); 
    if (mapIter != map_.end()){
        mapIter->second->second = val;        
        touch(mapIter->second);
        return true;
    }
    else{
        if (cache_.size() >= capacity_){
            std::string toDel = cache_.back().first;
            del(toDel);
        }
        cache_.push_front({key, val});
        map_[key] = cache_.begin();
    }
    return false;
}

bool Cache::del(Key key){
    auto mapIter = map_.find(key);
    if (mapIter != map_.end()){
        cache_.erase(mapIter->second);
        map_.erase(mapIter);
        return true;
    }
    return false;
}

bool Cache::mod(Key key, Value val){
    auto mapIter = map_.find(key);
    if (mapIter != map_.end()){
        mapIter->second->second = val;
        touch(mapIter->second);
        return true;
    }
    return false;
}

bool Cache::exist(Key key){    
    auto mapIter = map_.find(key);
    if (mapIter != map_.end()){
        touch(mapIter->second);
        return true;
    }
    return false;
    
}

void Cache::touch(NodeIter iter){
    cache_.splice(cache_.begin(), cache_, iter);
}