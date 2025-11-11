#include "tcp_transport.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "bits/ttl/logger.hpp"
#include "bits/util.hpp"
#include "event_watcher.hpp"
#include "transport.hpp"

namespace getrafty::rpc {

TcpTransport::TcpTransport(const Address& address) {
  auto parsed = bits::parseAddress(address);
  if (!parsed) {
    throw std::invalid_argument("Invalid address format");
  }
  host_ = std::move(parsed->first);
  port_ = parsed->second;
}

void TcpTransport::attach(io::EventWatcher& ew, Fn<IOEvent&&> replay) {
  ew_     = &ew;
  replay_ = std::move(replay);
  TTL_LOG(bits::ttl::Trace) << "(attach) Attached " << host_ << ":" << port_;
}

void TcpTransport::watchAccept() {
  ew_->watch(listen_fd_, io::WatchFlag::RDONLY, [this]() { onAcceptReady(); });
}

void TcpTransport::unwatchAccept() {
  if (listen_fd_ >= 0) {
    ew_->unwatch(listen_fd_, io::WatchFlag::RDONLY);
  }
}

void TcpTransport::watchConnect() {
  ew_->watch(client_fd_, io::WatchFlag::WRONLY, [this]() { onConnectReady(); });
}

void TcpTransport::unwatchConnect() {
  if (client_fd_ >= 0) {
    ew_->unwatch(client_fd_, io::WatchFlag::WRONLY);
  }
}

void TcpTransport::bind() {
  listen_fd_ = bits::makeSockTcp();
  if (listen_fd_ < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(bind) Failed to create socket errno=" << errno;
    replay_(BindRep{.status = IOStatus::Fatal, .endpoint = {}});
    return;
  }

  if (bits::setSockOptNonBlocking(listen_fd_) < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(bind) Failed to set non-blocking errno=" << errno;
    ::close(listen_fd_);
    listen_fd_ = -1;
    replay_(BindRep{.status = IOStatus::Fatal, .endpoint = {}});
    return;
  }

  if (bits::setSockOptShared(listen_fd_) < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(bind) Failed to set SO_REUSEADDR/SO_REUSEPORT errno=" << errno;
  }

  if (bits::sockBind(listen_fd_, port_, host_) < 0) {
    TTL_LOG(bits::ttl::Error) << "(bind) Socket bind failed errno=" << errno;
    ::close(listen_fd_);
    listen_fd_ = -1;
    replay_(BindRep{.status = IOStatus::Fatal, .endpoint = {}});
    return;
  }

  if (bits::sockListen(listen_fd_) < 0) {
    TTL_LOG(bits::ttl::Error) << "(bind) Listen failed errno=" << errno;
    ::close(listen_fd_);
    listen_fd_ = -1;
    replay_(BindRep{.status = IOStatus::Fatal, .endpoint = {}});
    return;
  }

  auto hostPort = bits::getSockOptHostPort(listen_fd_);
  if (!hostPort) {
    TTL_LOG(bits::ttl::Error) << "(bind) Failed to get bound address";
    ::close(listen_fd_);
    listen_fd_ = -1;
    replay_(BindRep{.status = IOStatus::Fatal, .endpoint = {}});
    return;
  }

  TTL_LOG(bits::ttl::Trace) << "(bind) Bound " << *hostPort;
  watchAccept();
  replay_(BindRep{.status = IOStatus::Ok, .endpoint = *hostPort});
}

void TcpTransport::connect() {
  client_fd_ = bits::makeSockTcp();
  if (client_fd_ < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(connect) Failed to create socket errno=" << errno;
    return;
  }

  if (bits::setSockOptNonBlocking(client_fd_) < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(connect) Failed to set non-blocking errno=" << errno;
    ::close(client_fd_);
    client_fd_ = -1;
    return;
  }

  if (bits::setSockOptTcpNoDelay(client_fd_) < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(connect) Failed to set TCP_NODELAY errno=" << errno;
  }

  if (bits::setSockOptTcpKeepAlive(client_fd_) < 0) {
    TTL_LOG(bits::ttl::Error)
        << "(connect)  Failed to set TCP_KEEPALIVE errno=" << errno;
  }

  int rc = bits::sockConnect(client_fd_, port_, host_);
  if (rc < 0 && errno != EINPROGRESS) {
    TTL_LOG(bits::ttl::Error) << "(connect) Connect failed errno=" << errno;
    ::close(client_fd_);
    client_fd_ = -1;
    return;
  }

  TTL_LOG(bits::ttl::Trace) << "(connect) Connected " << host_ << ":" << port_;
  watchConnect();
}

void TcpTransport::close() {
  if (listen_fd_ >= 0) {
    unwatchAccept();
    ::close(listen_fd_);
    listen_fd_ = -1;
  }

  if (client_fd_ >= 0) {
    unwatchConnect();
    client_fd_ = -1;
  }

  for (auto& [fd, conn] : connection_by_fd_) {
    unwatchRead(conn);
    unwatchWrite(conn);
    ::close(fd);
  }

  connection_by_fd_.clear();
  fd_by_peer_.clear();
  hot_read_peer_.clear();
  hot_write_peer_.clear();
}

void TcpTransport::onAcceptReady() {
  // Accept a bunch of connection while on the core
  // Limit to avoid event loop starvation
  for (auto i = 0; i < 10; ++i) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int fd = ::accept4(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len,
                       SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      TTL_LOG(bits::ttl::Error) << "(accept) Accept failed errno=" << errno;
      break;
    }

    if (bits::setSockOptTcpNoDelay(fd) < 0) {
      TTL_LOG(bits::ttl::Error)
          << "(accept) Failed to set TCP_NODELAY errno=" << errno
          << ", fd=" << fd;
    }

    if (bits::setSockOptTcpKeepAlive(fd) < 0) {
      TTL_LOG(bits::ttl::Error)
          << "(accept) Failed to set TCP_KEEPALIVE errno=" << errno;
    }

    // FIXME
    char ip[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    uint16_t peer_port = bits::fromNetwork(addr.sin_port);
    Peer peer          = std::string(ip) + ":" + std::to_string(peer_port);

    connection_by_fd_[fd] = Connection{
        .fd_          = fd,
        .peer_        = peer,
        .read_armed_  = false,
        .write_armed_ = false,
    };
    fd_by_peer_[peer] = fd;

    TTL_LOG(bits::ttl::Trace) << "(accept) Accepted connection from " << peer;

    // FIXME: double lookup
    watchRead(connection_by_fd_[fd]);
  }
}

void TcpTransport::onConnectReady() {
  unwatchConnect();

  int error     = 0;
  socklen_t len = sizeof(error);
  if (::getsockopt(client_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
    TTL_LOG(bits::ttl::Error) << "(connect) getsockopt failed errno=" << errno;
    ::close(client_fd_);
    client_fd_ = -1;
    replay_(ConnectRep{.status = IOStatus::Fatal});
    return;
  }

  if (error != 0) {
    TTL_LOG(bits::ttl::Error) << "(connect) Connect failed errno=" << error;
    ::close(client_fd_);
    client_fd_ = -1;
    replay_(ConnectRep{.status = IOStatus::Fatal});
    return;
  }

  auto peer_addr = bits::getSockOptHostPort(client_fd_);
  if (!peer_addr) {
    TTL_LOG(bits::ttl::Error) << "Failed to get peer address";
    ::close(client_fd_);
    client_fd_ = -1;
    replay_(ConnectRep{.status = IOStatus::Fatal});
    return;
  }

  Peer peer                     = *peer_addr;
  connection_by_fd_[client_fd_] = Connection{
      .fd_          = client_fd_,
      .peer_        = peer,
      .read_armed_  = false,
      .write_armed_ = false,
  };
  fd_by_peer_[peer] = client_fd_;
  connected_        = true;

  TTL_LOG(bits::ttl::Trace)
      << "(connect) Connected to " << host_ << ":" << port_;

  replay_(ConnectRep{.status = IOStatus::Ok});
}

void TcpTransport::onReadReady(int fd) {
  const auto& it = connection_by_fd_.find(fd);
  if (it != connection_by_fd_.end()) {
    hot_read_peer_ = it->second.peer_;
    TTL_LOG(bits::ttl::Trace)
        << "(read) Ready fd=" << fd << ", peer=" << hot_read_peer_;
    replay_(ReadReadyRep{});
  }
}

void TcpTransport::onWriteReady(int fd) {
  const auto& it = connection_by_fd_.find(fd);
  if (it != connection_by_fd_.end()) {
    hot_write_peer_ = it->second.peer_;
    TTL_LOG(bits::ttl::Trace)
        << "(write) Ready fd=" << fd << ", peer=" << hot_write_peer_;
    replay_(WriteReadyRep{.peer = hot_write_peer_});
  }
}

void TcpTransport::watchRead(Connection& conn) {
  if (!conn.read_armed_) {
    TTL_LOG(bits::ttl::Trace)
        << "(read) watchRead before ew->watch fd=" << conn.fd_;
    ew_->watch(conn.fd_, io::WatchFlag::RDONLY,
               [this, fd = conn.fd_]() { onReadReady(fd); });
    TTL_LOG(bits::ttl::Trace)
        << "(read) watchRead after ew->watch fd=" << conn.fd_;
    conn.read_armed_ = true;
    TTL_LOG(bits::ttl::Trace) << "(read) Watching fd=" << conn.fd_;
  }
}

void TcpTransport::unwatchRead(Connection& conn) {
  if (conn.read_armed_) {
    TTL_LOG(bits::ttl::Trace)
        << "(read) unwatchRead before ew->unwatch fd=" << conn.fd_;
    ew_->unwatch(conn.fd_, io::WatchFlag::RDONLY);
    TTL_LOG(bits::ttl::Trace)
        << "(read) unwatchRead after ew->unwatch fd=" << conn.fd_;
    conn.read_armed_ = false;
    TTL_LOG(bits::ttl::Trace) << "(read) Unwatch fd=" << conn.fd_;
  }
}

void TcpTransport::watchWrite(Connection& conn) {
  if (!conn.write_armed_) {
    ew_->watch(conn.fd_, io::WatchFlag::WRONLY,
               [this, fd = conn.fd_]() { onWriteReady(fd); });
    conn.write_armed_ = true;
    TTL_LOG(bits::ttl::Trace) << "(write) Watching fd=" << conn.fd_;
  }
}

void TcpTransport::unwatchWrite(Connection& conn) {
  if (conn.write_armed_) {
    ew_->unwatch(conn.fd_, io::WatchFlag::WRONLY);
    conn.write_armed_ = false;
    TTL_LOG(bits::ttl::Trace) << "(write) Unwatch fd=" << conn.fd_;
  }
}

void TcpTransport::releaseConnection(int fd, IOStatus) {
  auto it = connection_by_fd_.find(fd);
  if (it == connection_by_fd_.end()) {
    return;
  }

  auto& conn = it->second;
  Peer peer  = conn.peer_;

  unwatchRead(conn);
  unwatchWrite(conn);
  ::close(fd);

  connection_by_fd_.erase(it);
  fd_by_peer_.erase(peer);

  if (hot_read_peer_ == peer) {
    hot_read_peer_.clear();
  }
  if (hot_write_peer_ == peer) {
    hot_write_peer_.clear();
  }

  TTL_LOG(bits::ttl::Trace) << "Connection closed " << peer;
}

size_t TcpTransport::resumeRead(Buffer& out_data, Peer& out_peer,
                                IOStatus& out_status, size_t offset,
                                size_t max_len) noexcept {
  constexpr size_t kMinSize = 1 << 10;  // 1KB

  auto capacity = max_len > 0 ? max_len : kMinSize;

  if (out_data.size() < offset + capacity) {
    out_data.resize(offset + capacity);
  }

  // Fast path, read is called right after 'hot_read_peer_' transitioned to ready
  if (!hot_read_peer_.empty()) {
    TTL_LOG(bits::ttl::Trace) << "(read) Fast path";
    const auto& peer_it = fd_by_peer_.find(hot_read_peer_);
    if (peer_it != fd_by_peer_.end()) {
      int fd              = peer_it->second;
      const auto& conn_it = connection_by_fd_.find(fd);
      if (conn_it != connection_by_fd_.end()) {
        auto& conn = conn_it->second;
        ssize_t n =
            ::recv(fd, out_data.data() + offset, capacity, MSG_DONTWAIT);

        if (n > 0) {
          out_peer   = conn.peer_;
          out_status = IOStatus::Ok;
          if (static_cast<size_t>(n) < capacity) {
            hot_read_peer_.clear();
          }
          TTL_LOG(bits::ttl::Trace)
              << "(read) Read " << n << " bytes from peer=" << conn.peer_;
          return n;
        }

        if (n == 0) {
          TTL_LOG(bits::ttl::Trace)
              << "(read) Peer disconnected, peer=" << conn.peer_;
          hot_read_peer_.clear();
          releaseConnection(fd, IOStatus::Eof);
          out_status = IOStatus::Eof;
          return 0;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          watchRead(conn);
          hot_read_peer_.clear();
          out_status = IOStatus::WouldBlock;
          TTL_LOG(bits::ttl::Trace)
              << "(read) Read queued, peer=" << conn.peer_;
          return 0;
        }

        TTL_LOG(bits::ttl::Trace)
            << "(read) Read failed, halting connection for peer=" << conn.peer_;
        hot_read_peer_.clear();
        releaseConnection(fd, IOStatus::Fatal);
        out_status = IOStatus::Fatal;
        return -1;
      }
    }
    hot_read_peer_.clear();
  }

  // Fallback, spurious read
  TTL_LOG(bits::ttl::Trace) << "(read) Spurious read, scanning "
                            << connection_by_fd_.size() << " connections";
  for (auto& [fd, conn] : connection_by_fd_) {
    if (!conn.read_armed_) {
      watchRead(conn);
    }

    ssize_t n = ::recv(fd, out_data.data() + offset, capacity, MSG_DONTWAIT);

    if (n > 0) {
      out_peer   = conn.peer_;
      out_status = IOStatus::Ok;
      TTL_LOG(bits::ttl::Trace)
          << "(read) Read " << n << " bytes from peer=" << conn.peer_;
      return n;
    }

    if (n == 0) {
      TTL_LOG(bits::ttl::Trace)
          << "(read) Peer disconnected, peer=" << conn.peer_;
      releaseConnection(fd, IOStatus::Eof);
      out_status = IOStatus::Eof;
      return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      continue;
    }

    TTL_LOG(bits::ttl::Trace)
        << "(read) Read failed, halting connection for peer=" << conn.peer_;
    releaseConnection(fd, IOStatus::Fatal);
    out_status = IOStatus::Fatal;
    return -1;
  }

  out_status = IOStatus::WouldBlock;
  return 0;
}

size_t TcpTransport::resumeWrite(Buffer&& data, const Peer& peer,
                                 IOStatus& out_status) noexcept {
  Connection* conn = nullptr;

  if (!peer.empty()) {
    const auto peer_it = fd_by_peer_.find(peer);
    if (peer_it == fd_by_peer_.end()) {
      out_status = IOStatus::Fatal;
      return -1;
    }

    const auto conn_it = connection_by_fd_.find(peer_it->second);
    if (conn_it == connection_by_fd_.end()) {
      out_status = IOStatus::Fatal;
      return -1;
    }
    conn = &conn_it->second;
  } else {
    if (connected_) {
      const auto conn_it = connection_by_fd_.find(client_fd_);
      if (conn_it != connection_by_fd_.end()) {
        conn = &conn_it->second;
      }
    }

    if (!conn && connection_by_fd_.size() == 1) {
      conn = &connection_by_fd_.begin()->second;
    }

    if (!conn) {
      out_status = IOStatus::Error;
      return -1;
    }
  }

  auto& resolved_conn = *conn;
  const int fd        = resolved_conn.fd_;
  // MSG_NOSIGNAL: https://man7.org/linux/man-pages/man2/send.2.html
  ssize_t n = ::send(fd, data.data(), data.size(), MSG_DONTWAIT | MSG_NOSIGNAL);

  if (n > 0) {
    TTL_LOG(bits::ttl::Trace)
        << "(write) Wrote " << n << " bytes to peer=" << resolved_conn.peer_;
    out_status = IOStatus::Ok;
    return n;
  }

  if (n == 0) {
    TTL_LOG(bits::ttl::Trace)
        << "(read) Peer disconnected, peer=" << resolved_conn.peer_;
    releaseConnection(fd, IOStatus::Eof);
    out_status = IOStatus::Eof;
    return 0;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    watchWrite(resolved_conn);
    out_status = IOStatus::WouldBlock;
    TTL_LOG(bits::ttl::Trace)
        << "(write) Write queued, peer=" << resolved_conn.peer_;
    return 0;
  }

  TTL_LOG(bits::ttl::Trace)
      << "(write) Write failed, halting connection for peer="
      << resolved_conn.peer_;
  releaseConnection(fd, IOStatus::Fatal);
  out_status = IOStatus::Fatal;
  return -1;
}

void TcpTransport::suspendRead() {
  TTL_LOG(bits::ttl::Trace) << "(read) Suspend for all "
                            << connection_by_fd_.size() << " connections";
  for (auto& [fd, conn] : connection_by_fd_) {
    unwatchRead(conn);
  }
}

void TcpTransport::suspendWrite(const Peer& peer) {
  const auto& peer_it = fd_by_peer_.find(peer);
  if (peer_it == fd_by_peer_.end()) {
    return;
  }

  int fd              = peer_it->second;
  const auto& conn_it = connection_by_fd_.find(fd);
  if (conn_it == connection_by_fd_.end()) {
    return;
  }

  TTL_LOG(bits::ttl::Trace) << "(write) Suspend for per=" << peer;
  unwatchWrite(conn_it->second);
}

}  // namespace getrafty::rpc
