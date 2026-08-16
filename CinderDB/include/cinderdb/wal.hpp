#pragma once
#include <mutex>
#include <functional>
#include <string>
#include <string_view>
namespace cinderdb {
enum class WalOperation : unsigned char { put = 1, erase = 2 };
class WriteAheadLog {
public:
 explicit WriteAheadLog(std::string path);
 ~WriteAheadLog();
 WriteAheadLog(const WriteAheadLog&) = delete; WriteAheadLog& operator=(const WriteAheadLog&) = delete;
 void append(WalOperation op, std::string_view key, std::string_view value = {});
 void replay(const std::function<void(WalOperation, std::string, std::string)>& apply);
 [[nodiscard]] const std::string& path() const noexcept { return path_; }
private:
 std::string path_; int fd_{-1}; std::mutex mutex_;
};
}  // namespace cinderdb
