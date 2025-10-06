#include "tcp_socket.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace getrafty::rpc::io {

TcpServerSocket::TcpServerSocket(int fd, EventWatcher& watcher)
    : fd_(fd), watcher_(watcher) {}

TcpServerSocket::~TcpServerSocket() {
  close();
}

std::unique_ptr<TcpServerSocket> TcpServerSocket::listen(
    const std::string& host, uint16_t port, EventWatcher& watcher) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("Failed to create socket: " +
                             std::string(std::strerror(errno)));
  }

  constexpr int kTrue = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &kTrue, sizeof(kTrue)) < 0) {
    ::close(fd);
    throw std::runtime_error("Failed to set to <SO_REUSEADDR>:" +
                             std::string(std::strerror(errno)));
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    ::close(fd);
    throw std::runtime_error("Wrong addr:" + std::string(std::strerror(errno)));
  }

  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    throw std::runtime_error("Failed to bind:" +
                             std::string(std::strerror(errno)));
  }

  if (::listen(fd, SOMAXCONN) < 0) {
    ::close(fd);
    throw std::runtime_error("Failed to listen:" +
                             std::string(std::strerror(errno)));
  }

  return std::unique_ptr<TcpServerSocket>(new TcpServerSocket(fd, watcher));
}

std::unique_ptr<ISocket> TcpServerSocket::accept() {
  if (closed_.load()) {
    throw std::runtime_error("Server socket is closed");
  }

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);

  int client_fd =
      ::accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
  if (client_fd < 0) {
    throw std::runtime_error("Accept failed: " +
                             std::string(std::strerror(errno)));
  }

  int flags = ::fcntl(client_fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    ::close(client_fd);
    throw std::runtime_error("Failed to set <O_NONBLOCK>: " +
                             std::string(std::strerror(errno)));
  }

  return std::make_unique<TcpSocket>(client_fd, watcher_);
}

void TcpServerSocket::close() {
  bool expected = false;
  if (closed_.compare_exchange_strong(expected, true)) {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }
}

uint16_t TcpServerSocket::port() const {
  sockaddr_in addr{};
  socklen_t addr_len = sizeof(addr);
  if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
    throw std::runtime_error("Failed to get socket name: " +
                             std::string(std::strerror(errno)));
  }
  return ntohs(addr.sin_port);
}

TcpSocket::TcpSocket(int fd, EventWatcher& watcher)
    : fd_(fd),
      watcher_(watcher),
      read_queue_(std::make_shared<ReadWatchCallbackQueue>(fd_)),
      write_queue_(std::make_shared<WriteWatchCallbackQueue>(fd_, watcher)) {}

TcpSocket::~TcpSocket() {
  close();
}

std::unique_ptr<TcpSocket> TcpSocket::connect(const std::string& host,
                                              uint16_t port,
                                              EventWatcher& watcher) {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Failed to create socket: " +
                             std::string(std::strerror(errno)));
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    ::close(sock);
    throw std::runtime_error("Invalid address: " + host);
  }

  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(sock);
    throw std::runtime_error("Failed to connect to " + host + ":" +
                             std::to_string(port) + ": " +
                             std::string(std::strerror(errno)));
  }

  int flags = ::fcntl(sock, F_GETFL, 0);
  if (flags < 0 || ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
    ::close(sock);
    throw std::runtime_error("Failed to set non-blocking: " +
                             std::string(std::strerror(errno)));
  }

  return std::make_unique<TcpSocket>(sock, watcher);
}

class TcpSocket::WriteWatchCallbackQueue : public IWatchCallback {
 public:
  explicit WriteWatchCallbackQueue(int fd, EventWatcher& watcher)
      : fd_(fd), watcher_(watcher) {}

  bool enqueue(Buffer data, std::move_only_function<void(Status)> cb) {
    std::move_only_function<void(Status)>* prev_cb = nullptr;
    auto* curr_cb = new std::move_only_function<void(Status)>(std::move(cb));

    if (!callback_.compare_exchange_strong(prev_cb, curr_cb)) {
      (*curr_cb)(Status::BUSY);
      delete curr_cb;
      return false;
    }

    data_ = std::move(data);
    written_ = 0;
    idle_count_.store(0);  // Reset idle count when new work arrives
    return true;
  }

  std::move_only_function<void(Status)>* pop() {
    return callback_.exchange(nullptr);
  }

  void run(int /*fd*/) override {
    // Spurious wakeup guard: skip if no operation pending
    auto* cb = callback_.load();
    if (!cb) {
      // No callback - increment idle count
      // After several idle wakeups, unwatch to stop spinning
      constexpr int kMaxIdleWakeups =
          10;  // Allow ~10 spurious wakeups before unwatching
      if (idle_count_.fetch_add(1) >= kMaxIdleWakeups) {
        watcher_.unwatch(fd_, WatchFlag::WRONLY);
        idle_count_.store(0);
      }
      return;
    }

    if (written_ >= data_.size()) {
      cb = callback_.exchange(nullptr);
      if (cb) {
        std::unique_ptr<std::move_only_function<void(Status)>> guard(cb);
        (*guard)(Status::OK);
        // Don't unwatch immediately - another write might come soon
        // The idle counter will handle unwatching if no new work arrives
      }
      return;
    }

    ssize_t n = ::send(fd_, data_.data() + written_, data_.size() - written_,
                       MSG_NOSIGNAL);

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Socket not ready: event loop will call us again
        return;
      }
      Status status = (errno == EPIPE || errno == ECONNRESET)
                          ? Status::BROKEN_PIPE
                          : Status::ERROR;

      cb = callback_.exchange(nullptr);
      if (cb) {
        std::unique_ptr<std::move_only_function<void(Status)>> guard(cb);
        (*guard)(status);
        // Unwatch immediately on error since no retry is expected
        watcher_.unwatch(fd_, WatchFlag::WRONLY);
        idle_count_.store(0);
      }
      return;
    }

    // Track partial progress: event loop continues on next wakeup
    written_ += static_cast<size_t>(n);
  }

 private:
  int fd_;
  EventWatcher& watcher_;
  Buffer data_;
  size_t written_ = 0;
  std::atomic<int> idle_count_{0};
  std::atomic<std::move_only_function<void(Status)>*> callback_;
};

class TcpSocket::ReadWatchCallbackQueue : public IWatchCallback {
 public:
  explicit ReadWatchCallbackQueue(int fd) : fd_(fd) {}

  bool enqueue(size_t max_bytes,
               std::move_only_function<void(Status, Buffer&&)> cb) {
    std::move_only_function<void(Status, Buffer&&)>* prev_cb = nullptr;
    auto* curr_cb =
        new std::move_only_function<void(Status, Buffer&&)>(std::move(cb));
    if (!callback_.compare_exchange_strong(prev_cb, curr_cb)) {
      (*curr_cb)(Status::BUSY, {});
      delete curr_cb;
      return false;
    }
    max_bytes_ = max_bytes;
    return true;
  }

  std::move_only_function<void(Status, Buffer&&)>* pop() {
    return callback_.exchange(nullptr);
  }

  void run(int /*fd*/) override {
    auto* cb = callback_.load();
    if (!cb) {
      return;
    }

    Buffer buf(max_bytes_);
    ssize_t n = ::recv(fd_, buf.data(), buf.size(), 0);

    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    cb = callback_.exchange(nullptr);
    if (!cb) {
      return;
    }

    std::unique_ptr<std::move_only_function<void(Status, Buffer&&)>> guard(cb);
    if (n < 0) {
      (*guard)(Status::ERROR, {});
    } else if (n == 0) {
      (*guard)(Status::PEER_CLOSED, {});
    } else {
      buf.resize(static_cast<size_t>(n));
      (*guard)(Status::OK, std::move(buf));
    }
  }

 private:
  int fd_;
  size_t max_bytes_;
  std::atomic<std::move_only_function<void(Status, Buffer&&)>*> callback_;
};

void TcpSocket::asyncRead(
    size_t max_bytes,
    std::move_only_function<void(Status, Buffer&&)> callback) {
  if (closed_.load()) {
    callback(Status::CLOSED, {});
    return;
  }

  if (!read_queue_->enqueue(max_bytes, std::move(callback))) {
    return;
  }

  watcher_.watch(fd_, WatchFlag::RDONLY,
                 std::static_pointer_cast<IWatchCallback>(read_queue_));
}

void TcpSocket::asyncWrite(Buffer data,
                           std::move_only_function<void(Status)> callback) {
  if (closed_.load()) {
    callback(Status::CLOSED);
    return;
  }

  if (!write_queue_->enqueue(std::move(data), std::move(callback))) {
    return;
  }

  watcher_.watch(fd_, WatchFlag::WRONLY,
                 std::static_pointer_cast<IWatchCallback>(write_queue_));
}

void TcpSocket::close() {
  if (!closed_.exchange(true)) {
    if (fd_ >= 0) {
      watcher_.unwatch(fd_, WatchFlag::RDONLY);
      watcher_.unwatch(fd_, WatchFlag::WRONLY);

      if (read_queue_) {
        auto* cb = read_queue_->pop();
        if (cb) {
          std::unique_ptr<std::move_only_function<void(Status, Buffer&&)>>
              guard(cb);
          (*guard)(Status::CLOSED, {});
        }
        read_queue_.reset();
      }
      if (write_queue_) {
        auto* cb = write_queue_->pop();
        if (cb) {
          std::unique_ptr<std::move_only_function<void(Status)>> guard(cb);
          (*guard)(Status::CLOSED);
        }
        write_queue_.reset();
      }

      ::close(fd_);
      fd_ = -1;
    }
  }
}

}  // namespace getrafty::rpc::io
