#pragma once

#include <deque>
#include <memory>
#include <unordered_map>
#include "transport.hpp"

namespace getrafty::rpc {

class FramedTransport : public ITransport {
 public:
  explicit FramedTransport(std::unique_ptr<ITransport> transport);
  ~FramedTransport() override                        = default;
  FramedTransport(FramedTransport&&)                 = default;
  FramedTransport& operator=(FramedTransport&&)      = default;
  FramedTransport(const FramedTransport&)            = delete;
  FramedTransport& operator=(const FramedTransport&) = delete;

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
  std::unique_ptr<ITransport> transport_;

  // ==== YOUR CODE: @62e6 ====
  // 
  // ==== END YOUR CODE ====
};

}  // namespace getrafty::rpc
