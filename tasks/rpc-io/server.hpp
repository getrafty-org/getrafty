#pragma once

#include "socket.hpp"
#include "thread_pool.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace getrafty::rpc {

class Server {
 public:
  using Handler = std::function<io::Buffer(const io::Buffer&)>;

  Server(std::unique_ptr<io::IServerSocket> server_socket,
         wheels::concurrent::ThreadPool& pool);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void on(const std::string& method, Handler handler);

  void start();
  void stop();

 private:
  std::unique_ptr<io::IServerSocket> server_socket_;
  wheels::concurrent::ThreadPool& pool_;

  std::unordered_map<std::string, Handler> handlers_;
  std::atomic<bool> running_{false};
  std::unique_ptr<std::thread> acceptor_thread_;

  void acceptLoop();
  void handleConnection(std::unique_ptr<io::ISocket> socket);
};

}  // namespace getrafty::rpc
