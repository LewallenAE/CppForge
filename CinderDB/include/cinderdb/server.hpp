#pragma once
#include "cinderdb/bounded_queue.hpp"
#include "cinderdb/fd.hpp"
#include "cinderdb/store.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>
namespace cinderdb {
class Server {
public:
 Server(unsigned short port, std::string wal_path, std::size_t workers, std::size_t queue_capacity = 64);
 ~Server(); Server(const Server&)=delete; Server& operator=(const Server&)=delete;
 void start(); void stop() noexcept; [[nodiscard]] unsigned short port() const noexcept;
private:
 void accept_loop() noexcept; void worker_loop() noexcept; void handle_client(UniqueFd client) noexcept;
 unsigned short requested_port_; unsigned short bound_port_{0}; std::size_t workers_count_; UniqueFd listener_; KeyValueStore store_; BoundedQueue<UniqueFd> connections_; std::vector<std::thread> workers_; std::thread accept_thread_; std::atomic<bool> stopping_{false};
};
}  // namespace cinderdb
