#pragma once

#include "transport.hpp"
#include "thread_pool.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace getrafty::rpc {

class Channel {
 public:
  explicit Channel(std::unique_ptr<io::Transport> transport,
                   wheels::concurrent::ThreadPool& pool);
  ~Channel();

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  void call(io::Buffer request,
           std::move_only_function<void(io::Status, io::Buffer)> callback);

  void close();

 private:
  std::unique_ptr<io::Transport> transport_;
  wheels::concurrent::ThreadPool& pool_;

  std::atomic<uint64_t> next_id_{0};
  std::mutex mutex_;
  std::unordered_map<uint64_t, std::move_only_function<void(io::Status, io::Buffer)>> pending_;
  bool receiving_ = false;

  void startReceiving();
  void handleResponse(io::Status status, io::Buffer message);
};

}  // namespace getrafty::rpc
