#pragma once

#include <sys/types.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "bits/ring_buffer.hpp"
#include "event_watcher.hpp"
#include "transport.hpp"

//
// Socket is a simple abstraction over low-level network
// transports (TCP, UDP, etc.) for async I/O.
// It operates in either connected (client) or bound mode (server).
//
// Socket state is modeled as (semi-)deterministic state machine.
// State transition happens on "tick". Read and write states are implicit.
// Socket can be in either read or write implicit state but not in both
// at the same time.
//
//                         +-------------+
//                   +-----|    Idle     |-----+
//                   |     +-------------+     |
//               {BindReq}                {ConnectReq}
//                   |                         |
//                   v                         v
//            +-------------+           +-------------+
//            |  BindWait   |---->+<----| ConnectWait |
//            +-------------+     |     +-------------+
//                   |            |           |
//             {BindRep,Ok}       |     {ConnectRep,Ok}
//                   |         {*,!OK}        |
//                   v            |           v
//            +-------------+     |     +-------------+
//            |    Bound    |     |     |  Connected  |
//            +-------------+     v     +-------------+
//                   |     +-------------+     |
//                   |     |   Closed    |     |
//                   |     +-------------+     |
//               {CloseReq}       ^       {CloseReq} & EOF
//                   |            |            |
//                   |     +-------------+     |
//                   +---> |  CloseWait  | <---+
//                         +-------------+
//
// FSM step function, tick, has overrides for each state .
// This step function is called by socket itself for housekeeping as well
// as downstream dependency (e.g. transport).
//

namespace getrafty::rpc {

class Socket : public std::enable_shared_from_this<Socket> {
 public:
  explicit Socket(io::EventWatcher& ew, std::unique_ptr<ITransport> transport);
  ~Socket();

  Socket(const Socket&)            = delete;
  Socket& operator=(const Socket&) = delete;

  void close(Fn<> callback);
  void bind(Fn<IOStatus, Address> callback);
  void connect(Fn<IOStatus> callback);
  void read(Fn<IOStatus, Buffer&&, Peer> callback);
  void write(Buffer data, Peer peer, Fn<IOStatus> callback);

 private:
  enum SocketState : uint8_t {
    Idle        = 0,
    BindWait    = 1 << 0,
    Bound       = 1 << 1,
    ConnectWait = 1 << 2,
    Connected   = 1 << 3,
    CloseWait   = 1 << 4,
    Closed      = 1 << 5,
  };

  // FSM / Socket requests
  void tick(ConnectReq&& ev);
  void tick(BindReq&& ev);
  void tick(CloseReq&& ev);
  void tick(ReadReq&& ev);
  void tick(WriteReq&& ev);
  // FSM / Transport responses
  void tick(BindRep&& ev);
  void tick(ConnectRep&& ev);
  void tick(ReadReadyRep&& ev);
  void tick(WriteReadyRep&& ev);

  // Internal functions called to complete I/O operation
  void transportRead();
  void transportWrite(const Peer& peer);

  uint8_t state_;
  io::EventWatcher& ew_;
  std::unique_ptr<ITransport> transport_;

  bits::RingBuffer<BindReq, 1> bind_queue_;
  bits::RingBuffer<ConnectReq, 1> connect_queue_;
  bits::RingBuffer<ReadReq, 1024> read_queue_;
  std::unordered_map<Peer, WriteReq> write_queue_;
};

}  // namespace getrafty::rpc
