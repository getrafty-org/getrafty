#pragma once

#include "socket.hpp"
#include "event_watcher.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace getrafty::rpc::io {

class TcpSocket : public ISocket {
 public:
  TcpSocket(int fd, EventWatcher& watcher);
  ~TcpSocket() override;

  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;

  static std::unique_ptr<TcpSocket> connect(const std::string& host,
                                             uint16_t port,
                                             EventWatcher& watcher);

  void asyncRead(size_t max_bytes,
                std::move_only_function<void(Status, Buffer&&)> callback) override;

  void asyncWrite(Buffer data,
                 std::move_only_function<void(Status)> callback) override;

  void close() override;

 private:
  class ReadWatchCallbackQueue;
  class WriteWatchCallbackQueue;

  int fd_;
  EventWatcher& watcher_;
  std::atomic<bool> closed_{false};
  std::shared_ptr<ReadWatchCallbackQueue> read_queue_;
  std::shared_ptr<WriteWatchCallbackQueue> write_queue_;
};

class TcpServerSocket : public IServerSocket {
 public:
  ~TcpServerSocket() override;

  TcpServerSocket(const TcpServerSocket&) = delete;
  TcpServerSocket& operator=(const TcpServerSocket&) = delete;

  static std::unique_ptr<TcpServerSocket> listen(const std::string& host,
                                                  uint16_t port,
                                                  EventWatcher& watcher);

  std::unique_ptr<ISocket> accept() override;
  void close() override;
  uint16_t port() const;

 private:
  TcpServerSocket(int fd, EventWatcher& watcher);

  int fd_;
  EventWatcher& watcher_;
  std::atomic<bool> closed_{false};
};

}  // namespace getrafty::rpc::io
