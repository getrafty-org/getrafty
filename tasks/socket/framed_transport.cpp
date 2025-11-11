#include "framed_transport.hpp"

#include <cstring>

#include "bits/ttl/logger.hpp"
#include "bits/util.hpp"

namespace getrafty::rpc {

namespace {
constexpr size_t kFrameHeaderSize = 1 << 2;
}  // namespace

FramedTransport::FramedTransport(std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport)) {}

void FramedTransport::attach(io::EventWatcher& ew, Fn<IOEvent&&> replay) {
  // ==== YOUR CODE: @148d ====

  // ==== END YOUR CODE ====
}

void FramedTransport::bind() {
  // ==== YOUR CODE: @024e ====

  // ==== END YOUR CODE ====
}

void FramedTransport::connect() {
  // ==== YOUR CODE: @2657 ====

  // ==== END YOUR CODE ====
}

void FramedTransport::close() {
  // ==== YOUR CODE: @0481 ====

  // ==== END YOUR CODE ====
}

size_t FramedTransport::resumeRead(Buffer& out_data, Peer& out_peer,
                                   IOStatus& out_status, size_t /*offset*/,
                                   size_t /*max_len*/) noexcept {

  // ==== YOUR CODE: @9af3 ====

  // ==== END YOUR CODE ====
}

size_t FramedTransport::resumeWrite(Buffer&& data, const Peer& peer,
                                    IOStatus& out_status) noexcept {
  // ==== YOUR CODE: @3c29 ====

  // ==== END YOUR CODE ====
}

void FramedTransport::suspendRead() {
  // ==== YOUR CODE: @c2ac ====

  // ==== END YOUR CODE ====
}

void FramedTransport::suspendWrite(const Peer& peer) {
  // ==== YOUR CODE: @5a48 ====

  // ==== END YOUR CODE ====
}

// ==== YOUR CODE: @c261 ====

// ==== END YOUR CODE ====

}  // namespace getrafty::rpc