#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

class Cache{
public:
    using Value = std::string;
    using Key = std::string;
    using ListNode = std::pair<Key, Value>;
    using List = std::list<ListNode>;
    using NodeIter = List::iterator;

    Cache() = default;
    Cache(size_t capacity):
        capacity_(capacity) {};

    std::optional<Value> get(Key key);
    bool set(Key Key, Value val);
    bool del(Key key);
    bool mod(Key key, Value val);
    bool exist(Key key);

private:
    void touch(NodeIter iter);
    size_t capacity_ = 5;
    List cache_;
    std::unordered_map<Key, NodeIter> map_;


};