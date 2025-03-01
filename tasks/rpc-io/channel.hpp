#pragma once

#include "folly/Function.h"
#include "message.hpp"

namespace getrafty::rpc::io {

enum IOStatus : uint16_t {
  OK = 0x0,
  SOCK_CLOSED=0x1,
  IO_TIMEOUT=0x2,
  ERR_UNKNOWN=0xffff,
};

struct Result {
  IOStatus status;
  MessagePtr message;
};

using AsyncCallback = folly::Function<void(Result)>;

class IAsyncChannel {
 public:

  virtual ~IAsyncChannel() = default;

  virtual MessagePtr createMessage() = 0;

  virtual void open() = 0;
  virtual void close() = 0;
  virtual bool isOpen() = 0;
  virtual void sendMessage(AsyncCallback&& cob, MessagePtr message, std::chrono::milliseconds) = 0;
  virtual void recvMessage(AsyncCallback&& cob, std::chrono::milliseconds) = 0;
};

}  // namespace getrafty::rpc::io