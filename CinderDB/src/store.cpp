#include "cinderdb/store.hpp"
namespace cinderdb {
KeyValueStore::KeyValueStore(const std::string& path) : wal_{path} { wal_.replay([this](WalOperation op,std::string key,std::string value) { if(op==WalOperation::put) data_[std::move(key)]=std::move(value); else data_.erase(key); }); }
void KeyValueStore::put(std::string key,std::string value) { std::lock_guard lock{mutex_}; wal_.append(WalOperation::put,key,value); data_[std::move(key)]=std::move(value); }
std::optional<std::string> KeyValueStore::get(std::string_view key) const { std::lock_guard lock{mutex_}; const auto it=data_.find(std::string{key}); if(it==data_.end()) return std::nullopt; return it->second; }
void KeyValueStore::erase(std::string key) { std::lock_guard lock{mutex_}; wal_.append(WalOperation::erase,key); data_.erase(key); }
std::size_t KeyValueStore::size() const { std::lock_guard lock{mutex_}; return data_.size(); }
}  // namespace cinderdb
