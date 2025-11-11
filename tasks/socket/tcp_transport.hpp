#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "transport.hpp"

namespace getrafty::rpc {

class TcpTransport : public ITransport {
 public:
  explicit TcpTransport(const Address& address);
  ~TcpTransport() override                     = default;
  TcpTransport(TcpTransport&&)                 = default;
  TcpTransport& operator=(TcpTransport&&)      = default;
  TcpTransport(const TcpTransport&)            = delete;
  TcpTransport& operator=(const TcpTransport&) = delete;

  // Lifecycle
  void attach(io::EventWatcher& ew, Fn<IOEvent&&> replay) override;
  void bind() override;
  void connect() override;
  void close() override;

  // I/O
  size_t resumeRead(Buffer& out_data, Peer& out_peer, IOStatus& out_status,
                    size_t offset, size_t max_len) noexcept override;
  void suspendRead() override;

  size_t resumeWrite(Buffer&& data, const Peer& peer,
                     IOStatus& out_status) noexcept override;

  void suspendWrite(const Peer& peer) override;

 private:
  struct Connection {
    int fd_{-1};
    std::string peer_;
    bool read_armed_{false};
    bool write_armed_{false};
  };

  // Low-level I/O handlers
  void onAcceptReady();
  void onConnectReady();
  void onReadReady(int fd);
  void onWriteReady(int fd);

  void watchAccept();
  void unwatchAccept();
  void watchConnect();
  void unwatchConnect();
  void watchRead(Connection& conn);
  void unwatchRead(Connection& conn);
  void watchWrite(Connection& conn);
  void unwatchWrite(Connection& conn);

  void releaseConnection(int fd, IOStatus status);

  std::string host_;
  uint16_t port_;
  int listen_fd_{-1};
  int client_fd_{-1};
  Peer hot_read_peer_;
  Peer hot_write_peer_;
  bool connected_{false};

  io::EventWatcher* ew_;
  Fn<IOEvent&&> replay_;

  std::unordered_map<Peer, int> fd_by_peer_;
  std::unordered_map<int, Connection> connection_by_fd_;
};

}  // namespace getrafty::rpc
