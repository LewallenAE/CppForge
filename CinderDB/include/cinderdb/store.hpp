#pragma once
#include "cinderdb/wal.hpp"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
namespace cinderdb {
class KeyValueStore {
public:
 explicit KeyValueStore(const std::string& wal_path);
 void put(std::string key, std::string value);
 [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
 void erase(std::string key);
 [[nodiscard]] std::size_t size() const;
private:
 mutable std::mutex mutex_; std::unordered_map<std::string,std::string> data_; WriteAheadLog wal_;
};
}  // namespace cinderdb
