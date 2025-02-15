#pragma once

#include "message.hpp"

namespace getrafty::rpc::io {

enum Status : uint16_t {
  OK = 0x0,
  SOCK_CLOSED=0x1,
  ERR_UNKNOWN=0xffff,
};

struct Result {
  Status status;
  MessagePtr message;
};

using AsyncCallback = std::function<void(Result)>;

class IAsyncChannel {
 public:

  virtual ~IAsyncChannel() = default;

  virtual MessagePtr createMessage() = 0;

  virtual void sendMessage(AsyncCallback&& cob, MessagePtr message) = 0;

  virtual void recvMessage(AsyncCallback&& cob) = 0;
};
}  // namespace getrafty::rpc::io