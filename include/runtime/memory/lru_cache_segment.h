#pragma once

#include "runtime/base/noncopyable.h"

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace runtime::memory {

// 单个分段 LRU 
// front() -> 最久未使用
// back() -> 最近使用
template<typename Key, typename Value>
class LRUCacheSegment : public runtime::base::NonCopyable {
public:
    explicit LRUCacheSegment(std::size_t capacity) : capacity_(capacity) {}

    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lk{mutex_};

        auto it = mp_.find(key);
        if(it == mp_.end()) return false;

        // 移到链表尾部
        lru_list_.splice(lru_list_.end(), lru_list_, it->second);
        value = it->second->second;
        return true;
    }

    std::optional<Value> get(const Key &key) {
        std::lock_guard<std::mutex> lk{mutex_};

        auto it = mp_.find(key);
        if(it == mp_.end()) return std::nullopt;

        lru_list_.splice(lru_list_.end(), lru_list_, it->second);
        return it->second->second;
    }
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lk{mutex_};

        if(capacity_ == 0) return ;
        
        auto it = mp_.find(key);

        if(it != mp_.end()) {
            it->second->second = value;
            lru_list_.splice(lru_list_.end(), lru_list_, it->second);
            return ;
        }
        if(mp_.size() >= capacity_) {
            auto &old = lru_list_.front();
            mp_.erase(old.first);
            lru_list_.pop_front();
        }
        lru_list_.emplace_back(key, value);
        mp_[key] = std::prev(lru_list_.end());
    }

    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lk{mutex_};

        auto it = mp_.find(key);
        if(it == mp_.end()) return false;
        
        lru_list_.erase(it->second);
        mp_.erase(it);
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk{mutex_};
        return mp_.size();
    }
    
    bool contains(const Key &key) const {
        std::lock_guard<std::mutex> lk{mutex_};
        return mp_.find(key) != mp_.end();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk{mutex_};
        return mp_.empty();
    }
    std::size_t capacity() const {
        return capacity_;
    }

    void clear() {
        std::lock_guard<std::mutex> lk{mutex_};
        mp_.clear();
        lru_list_.clear();
    }

private:
    using ListIt = typename std::list<std::pair<Key, Value>>::iterator;

    mutable std::mutex mutex_;
    std::list<std::pair<Key, Value>> lru_list_;
    std::unordered_map<Key, ListIt> mp_;
    std::size_t capacity_;
};

}   // namespace runtime::memory
