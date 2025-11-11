#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

namespace getrafty::io {
class EventWatcher;
}

namespace getrafty::rpc {

template <class... Args>
using Fn = std::move_only_function<void(Args...)>;

enum class IOStatus : uint8_t {
  Ok,          // Completed normally
  WouldBlock,  // Can not be completed right now
  Fatal,       // Completed abnormally (unexpected), e.g. device failure
  Error,       // Completed abnormally (expected), e.g. I/O on closed socket
  Eof,         // Other end is offline
};

using Address = std::string;

using Buffer = std::vector<uint8_t>;

using Peer = std::string;

struct BindReq {
  Fn<IOStatus, Address> callback;
};

struct ConnectReq {
  Fn<IOStatus> callback;
};

struct ReadReq {
  Fn<IOStatus, Buffer&&, Peer> callback;
};

struct WriteReq {
  Buffer data;
  Peer peer;
  Fn<IOStatus> callback;
};

struct CloseReq {
  Fn<> callback;
};

struct BindRep {
  IOStatus status;
  std::string endpoint;
};

struct ConnectRep {
  IOStatus status;
};

struct ReadReadyRep {};

struct WriteReadyRep {
  Peer peer;
};


using IOEvent = std::variant<BindReq, BindRep, ConnectReq, ConnectRep, ReadReq,
                             ReadReadyRep, WriteReq, WriteReadyRep,
                             CloseReq>;

struct ITransport {
  virtual ~ITransport() = default;

  // Lifecycle
  virtual void attach(io::EventWatcher& ew, Fn<IOEvent&&> replay) = 0;
  virtual void bind()                                             = 0;
  virtual void connect()                                          = 0;
  virtual void close()                                            = 0;

  // I/O
  virtual size_t resumeRead(Buffer& out_data, Peer& out_peer,
                            IOStatus& out_status, size_t offset = 0,
                            size_t max_len = 0) noexcept = 0;
  virtual void suspendRead()                             = 0;

  virtual size_t resumeWrite(Buffer&& data, const Peer& peer,
                             IOStatus& out_status) noexcept = 0;
  virtual void suspendWrite(const Peer& peer)               = 0;
};

}  // namespace getrafty::rpc
