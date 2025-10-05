#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace getrafty::rpc::io {

using Buffer = std::vector<uint8_t>;

enum class Status {
  OK,
  CLOSED,
  TIMEOUT,
  ERROR
};

class ISocket {
 public:
  virtual ~ISocket() = default;

  virtual void asyncRead(
      size_t max_bytes,
      std::move_only_function<void(Status, Buffer)> callback) = 0;

  virtual void asyncWrite(
      Buffer data,
      std::move_only_function<void(Status)> callback) = 0;

  virtual void close() = 0;
};

class IServerSocket {
 public:
  virtual ~IServerSocket() = default;

  virtual std::unique_ptr<ISocket> accept() = 0;
  virtual void close() = 0;
};

}  // namespace getrafty::rpc::io
