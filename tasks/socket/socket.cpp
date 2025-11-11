#include "socket.hpp"
#include <array>
#include <bits/ttl/logger.hpp>
#include <cassert>
#include <latch>
#include <utility>
#include "event_watcher.hpp"
#include "transport.hpp"

namespace getrafty::rpc {

Socket::Socket(io::EventWatcher& ew, std::unique_ptr<ITransport> transport)
    : state_(Idle), ew_(ew), transport_(std::move(transport)) {
  // All events are dispatched through single threaded EventWatcher loop
  transport_->attach(ew_, [this](IOEvent&& ev) {
    ew_.runInEventWatcherLoop([self  = shared_from_this(),
                               event = std::move(ev)]() mutable {
      std::visit([&](auto&& e) { self->tick(std::forward<decltype(e)>(e)); },
                 std::move(event));
    });
  });
}

Socket::~Socket() {
  std::latch done{1};
  ew_.runInEventWatcherLoop([this, &done]() {
    transport_->close();
    done.count_down();
  });
  done.wait();
}

void Socket::tick(BindReq&& ev) {
  // Called by user on Idle socket to initiate bind operation (Idle->BindWait).
  // Operation result dispatched by transport via BindRep (BindWait->Bound|Closed).
  TTL_LOG(bits::ttl::Trace) << "BindReq state=" << state_;

  if (state_ != Idle) {
    TTL_LOG(bits::ttl::Error) << "BindReq rejected: invalid state=" << state_;
    ev.callback(IOStatus::Fatal, {});
    return;
  }
  state_ = BindWait;
  bind_queue_.push_back(std::move(ev));
  transport_->bind();
}

void Socket::tick(BindRep&& ev) {
  // Called by transport on BindWait socket to complete bind operation (BindWait->Bound|Closed).
  // Invokes user callback with bind result and endpoint address on success.
  TTL_LOG(bits::ttl::Trace) << "BindRep state=" << state_;

  if (state_ != BindWait) {
    TTL_LOG(bits::ttl::Trace) << "BindRep ignored: unexpected state=" << state_;
    return;
  }
  assert(!bind_queue_.empty());

  auto req = std::move(bind_queue_.front());
  bind_queue_.pop_front();

  if (ev.status == IOStatus::Ok) {
    TTL_LOG(bits::ttl::Trace) << "Bind successful endpoint=" << ev.endpoint;
    state_ = Bound;
    req.callback(ev.status, ev.endpoint);
  } else {
    TTL_LOG(bits::ttl::Error)
        << "Bind failed status=" << static_cast<int>(ev.status);
    state_ = Closed;
    req.callback(ev.status, {});
  }
}

void Socket::tick(ConnectReq&& ev) {
  // Called by user on Idle socket to initiate connect operation (Idle->ConnectWait).
  // Operation result dispatched by transport via ConnectRep (ConnectWait->Connected|Closed).
  TTL_LOG(bits::ttl::Trace) << "ConnectReq state=" << state_;

  if (state_ != Idle) {
    TTL_LOG(bits::ttl::Error)
        << "ConnectReq rejected: invalid state=" << state_;
    ev.callback(IOStatus::Fatal);
    return;
  }
  state_ = ConnectWait;
  connect_queue_.push_back(std::move(ev));
  transport_->connect();
}

void Socket::tick(ConnectRep&& ev) {
  // Called by transport on ConnectWait socket to complete connect operation (ConnectWait->Connected|Closed).
  // Invokes user callback with connection result.
  TTL_LOG(bits::ttl::Trace) << "ConnectRep state=" << state_;

  if (state_ != ConnectWait) {
    TTL_LOG(bits::ttl::Trace)
        << "ConnectRep ignored: unexpected state=" << state_;
    return;
  }
  assert(!connect_queue_.empty());

  auto req = std::move(connect_queue_.front());
  connect_queue_.pop_front();

  if (ev.status == IOStatus::Ok) {
    TTL_LOG(bits::ttl::Trace) << "Connect successful";
    state_ = Connected;
    req.callback(ev.status);
  } else {
    TTL_LOG(bits::ttl::Error)
        << "Connect failed status=" << static_cast<int>(ev.status);
    state_ = Closed;
    req.callback(ev.status);
  }
}

void Socket::tick(ReadReq&& ev) {
  // Called by user on Bound|Connected socket to queue read request.
  // Invokes user callback immediately if data available, otherwise data delivered via transport ReadReadyRep.
  TTL_LOG(bits::ttl::Trace)
      << "ReadReq state=" << state_ << " queue_size=" << read_queue_.size();

  if (state_ & (CloseWait | Closed)) {
    TTL_LOG(bits::ttl::Trace) << "ReadReq rejected: socket closed";
    ev.callback(IOStatus::Error, {}, {});
    return;
  }

  if (read_queue_.full()) {
    TTL_LOG(bits::ttl::Trace) << "ReadReq rejected: queue full";
    ev.callback(IOStatus::WouldBlock, {}, {});
    return;
  }
  read_queue_.push_back(std::move(ev));
  transportRead();
}

void Socket::tick(WriteReq&& ev) {
  // Called by user on Bound|Connected socket to queue write request.
  // Invokes user callback immediately if transport ready, otherwise write completed via transport WriteReadyRep.
  TTL_LOG(bits::ttl::Trace) << "WriteReq peer=" << ev.peer
                            << " len=" << ev.data.size() << " state=" << state_;

  if (state_ & (Closed | CloseWait)) {
    TTL_LOG(bits::ttl::Trace) << "WriteReq rejected: socket closed";
    ev.callback(IOStatus::Error);
    return;
  }
  // Server mode (Bound) requires explicit peer for writes
  if ((state_ & Bound) && ev.peer.empty()) {
    TTL_LOG(bits::ttl::Error)
        << "WriteReq rejected: server write with empty peer";
    ev.callback(IOStatus::Error);
    return;
  }

  if (ev.data.empty()) {
    TTL_LOG(bits::ttl::Error) << "WriteReq rejected: empty data";
    ev.callback(IOStatus::Fatal);
    return;
  }

  // Try to insert write request into queue (keyed by peer)
  // If peer already has pending write, reject with WouldBlock
  const auto& [it, inserted] = write_queue_.emplace(ev.peer, std::move(ev));
  if (!inserted) {
    TTL_LOG(bits::ttl::Trace)
        << "WriteReq rejected: peer busy peer=" << ev.peer;
    ev.callback(IOStatus::WouldBlock);
    return;
  }

  transportWrite(it->second.peer);
}

void Socket::tick(CloseReq&& ev) {
  // Called by user on any socket state to close socket (any->CloseWait->Closed).
  // Closes transport, fails all pending operations, and invokes user callback. Idempotent.
  TTL_LOG(bits::ttl::Trace) << "CloseReq state=" << state_;

  // Idempotent close: multiple close calls are safe and invoke callback immediately
  if (state_ & (Closed | CloseWait)) {
    TTL_LOG(bits::ttl::Trace) << "CloseReq: already closed";
    if (ev.callback) {
      ev.callback();
    }
    return;
  }
  state_ = CloseWait;
  TTL_LOG(bits::ttl::Trace) << "CloseReq: calling transport->close()";
  transport_->close();
  TTL_LOG(bits::ttl::Trace) << "CloseReq: transport->close() returned";

  // Fail pending requests to prevent hanging callbacks after socket closes
  if (!bind_queue_.empty()) {
    auto req = std::move(bind_queue_.front());
    req.callback(IOStatus::Error, {});
  }

  if (!connect_queue_.empty()) {
    auto req = std::move(connect_queue_.front());
    req.callback(IOStatus::Error);
  }

  while (!read_queue_.empty()) {
    auto req = std::move(read_queue_.front());
    read_queue_.pop_front();
    if (req.callback) {
      req.callback(IOStatus::Error, {}, {});
    }
  }

  for (auto& [peer, req] : write_queue_) {
    if (req.callback) {
      req.callback(IOStatus::Error);
    }
  }

  state_ = Closed;
  ev.callback();
}

void Socket::tick(ReadReadyRep&&) {
  // Called by transport when socket readable.
  // Delivers queued data to user callbacks.
  TTL_LOG(bits::ttl::Trace) << "ReadReadyRep state=" << state_
                            << ", queue_size=" << read_queue_.size();
  transportRead();
}

void Socket::tick(WriteReadyRep&& ev) {
  // Called by transport when socket writable.
  // Completes queued write operation and invokes user callback.
  TTL_LOG(bits::ttl::Trace) << "WriteReadyRep state=" << state_;
  transportWrite(ev.peer);
}

void Socket::transportRead() {
  while (!read_queue_.empty()) {
    Buffer data;
    Peer peer;
    IOStatus status;

    TTL_LOG(bits::ttl::Trace) << "processReads: calling transport->read()";
    auto n = transport_->resumeRead(data, peer, status);
    TTL_LOG(bits::ttl::Trace)
        << "processReads: transport->read() returned n=" << n
        << " status=" << static_cast<int>(status);

    if (n <= 0) {
      if (status == IOStatus::WouldBlock) {
        TTL_LOG(bits::ttl::Trace) << "processReads: no data available";
        // Transport doesn't have available data, stop read for any pending requests
        break;
      }

      // Handle IO error or EOF
      auto req = std::move(read_queue_.front());
      read_queue_.pop_front();

      TTL_LOG(bits::ttl::Trace) << "Read completed peer=" << peer
                                << " status=" << static_cast<int>(status);

      // EOF handling differs by socket mode:
      // For connected, EOF means remote closed connection -> close local end.
      // For bound, EOF propagated to caller who manages per-peer state
      if (status == IOStatus::Eof && (state_ & Connected) != 0) {
        TTL_LOG(bits::ttl::Debug)
            << "Received EOF on connected socket: clossing socket, peer="
            << peer;
        tick(CloseReq{});
        return;
      }
      req.callback(status, {}, std::move(peer));
      continue;
    }

    auto req = std::move(read_queue_.front());
    read_queue_.pop_front();

    TTL_LOG(bits::ttl::Trace)
        << "Read successful peer=" << peer << " len=" << data.size();
    req.callback(IOStatus::Ok, std::move(data), std::move(peer));
  }

  if (read_queue_.empty()) {
    // No more pending requests so hint a transport to 'suspend'.
    // Suspend is a hint and not necessary do anything but some transport
    // implementations  can use it to optimize IO scheduling
    TTL_LOG(bits::ttl::Trace) << "processReads: suspending read";
    transport_->suspendRead();
  }
  TTL_LOG(bits::ttl::Trace)
      << "processReads: completed, queue_size=" << read_queue_.size();
}

void Socket::transportWrite(const Peer& peer) {
  TTL_LOG(bits::ttl::Trace) << "processWrite peer=" << peer;

  // Lookup write request based on socket mode:
  // Connected: use empty peer key {} (single peer connection)
  // Bound:     use specific peer (multi-peer server)
  std::unordered_map<Peer, WriteReq>::iterator it;
  if ((state_ & Connected) != 0) {
    it = write_queue_.find({});
  } else {
    it = write_queue_.find(peer);
  }

  if (it == write_queue_.end()) {
    TTL_LOG(bits::ttl::Trace)
        << "processWrite: peer not in queue peer=" << peer;
    return;
  }

  auto& req                  = it->second;
  const Peer& queued_peer    = it->first;
  const Peer& transport_peer = queued_peer;
  const Peer& log_peer       = queued_peer.empty() ? peer : queued_peer;

  TTL_LOG(bits::ttl::Trace)
      << "write peer=" << log_peer << " len=" << req.data.size();
  IOStatus status;
  auto n = transport_->resumeWrite(std::move(req.data), transport_peer, status);

  auto finalize = [&](IOStatus result) {
    auto request = std::move(it->second);
    write_queue_.erase(it);
    if (request.callback) {
      request.callback(result);
    }
  };

  if (n < 0) {
    TTL_LOG(bits::ttl::Error) << "Write failed peer=" << log_peer
                              << " status=" << static_cast<int>(status);
    // TODO: Handle write to a disconnected peer
    finalize(status);
    return;
  }

  if (status == IOStatus::WouldBlock) {
    TTL_LOG(bits::ttl::Trace) << "Write would block peer=" << log_peer;
    return;
  }

  if (status == IOStatus::Ok) {
    TTL_LOG(bits::ttl::Trace)
        << "Write successful peer=" << log_peer << " len=" << n;
    finalize(IOStatus::Ok);
  } else {
    TTL_LOG(bits::ttl::Error) << "Write completed with error peer=" << log_peer
                              << " status=" << static_cast<int>(status);
    finalize(status);
  }
}

void Socket::bind(Fn<IOStatus, Address> callback) {
  ew_.runInEventWatcherLoop(
      [self = shared_from_this(), cb = std::move(callback)]() mutable {
        self->tick(BindReq{std::move(cb)});
      });
}

void Socket::connect(Fn<IOStatus> callback) {
  ew_.runInEventWatcherLoop(
      [self = shared_from_this(), cb = std::move(callback)]() mutable {
        self->tick(ConnectReq{std::move(cb)});
      });
}

void Socket::read(Fn<IOStatus, Buffer&&, Peer> callback) {
  ew_.runInEventWatcherLoop(
      [self = shared_from_this(), cb = std::move(callback)]() mutable {
        self->tick(ReadReq{std::move(cb)});
      });
}

void Socket::write(Buffer data, Peer peer, Fn<IOStatus> callback) {
  ew_.runInEventWatcherLoop([self = shared_from_this(), d = std::move(data),
                             p  = std::move(peer),
                             cb = std::move(callback)]() mutable {
    self->tick(WriteReq{
        .data = std::move(d), .peer = std::move(p), .callback = std::move(cb)});
  });
}

void Socket::close(Fn<> callback) {
  ew_.runInEventWatcherLoop(
      [self = shared_from_this(), cb = std::move(callback)]() mutable {
        self->tick(CloseReq{std::move(cb)});
      });
}

}  // namespace getrafty::rpc
