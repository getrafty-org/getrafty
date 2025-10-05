#pragma once

#include "socket.hpp"
#include <functional>
#include <memory>

namespace getrafty::rpc::io {

class Transport {
 public:
  explicit Transport(std::unique_ptr<ISocket> socket);
  ~Transport();

  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;

  void asyncRead(std::move_only_function<void(Status, Buffer)> callback);

  void asyncWrite(Buffer message, std::move_only_function<void(Status)> callback);

  void close();

 private:
  std::unique_ptr<ISocket> socket_;
  Buffer read_buffer_;
};

}  // namespace getrafty::rpc::io
