#pragma once

#include "socket.hpp"
#include "event_watcher.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
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
                std::move_only_function<void(Status, Buffer)> callback) override;

  void asyncWrite(Buffer data,
                 std::move_only_function<void(Status)> callback) override;

  void close() override;

 private:
  int fd_;
  EventWatcher& watcher_;
  std::atomic<bool> closed_{false};
};

class TcpServerSocket : public IServerSocket {
 public:
  TcpServerSocket(const std::string& host, uint16_t port, EventWatcher& watcher);
  ~TcpServerSocket() override;

  TcpServerSocket(const TcpServerSocket&) = delete;
  TcpServerSocket& operator=(const TcpServerSocket&) = delete;

  std::unique_ptr<ISocket> accept() override;
  void close() override;

 private:
  int fd_;
  EventWatcher& watcher_;
  std::atomic<bool> closed_{false};
};

}  // namespace getrafty::rpc::io
