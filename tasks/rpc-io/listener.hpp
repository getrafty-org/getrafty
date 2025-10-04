#pragma once

#include <memory>
#include <folly/coro/Task.h>
#include "channel.hpp"

namespace getrafty::rpc::io {

class IListener {
public:
  virtual ~IListener() = default;
  virtual folly::coro::Task<std::shared_ptr<IAsyncChannel>> accept() = 0;
};

using IListenerFactory = std::function<std::shared_ptr<IListener>(const std::string&)>;

} // namespace getrafty::rpc::io
