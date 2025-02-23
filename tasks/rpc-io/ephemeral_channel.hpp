#pragma once

#include <chrono>
#include <random>

#include "channel.hpp"
#include "folly/Synchronized.h"
#include "thread_pool.hpp"

namespace getrafty::rpc::io {

using wheels::concurrent::ThreadPool;
using rpc::RpcError;

namespace detail {
struct EphemeralMessage final : IMessage {
  EphemeralMessage() = default;

  void setBody(const std::string& data) override { body_ = data; }

  std::string& getBody() const override { return body_; }

  void setMethod(const std::string& method) override { method_ = method; }

  std::string& getMethod() const override { return method_; }

  void setSequenceId(uint64_t value) override { sequenceId_ = value; }

  uint64_t getSequenceId() const override { return sequenceId_; }

  void setProtocol(const std::string& protocol) override {
    protocol_ = protocol;
  }

  std::string& getProtocol() const override { return protocol_; }

  void setErrorCode(const RpcError::Code code) override { errorCode_ = code; }

  RpcError::Code getErrorCode() const override { return errorCode_; }

  std::shared_ptr<IMessage> constructFromCurrent() override {
    auto msg = std::make_shared<EphemeralMessage>();
    msg->setSequenceId(sequenceId_);
    return msg;
  }

 private:
  mutable std::string body_;
  mutable std::string method_;
  mutable std::string protocol_;
  uint64_t sequenceId_{0};
  RpcError::Code errorCode_{RpcError::Code::OK};
};
}  // namespace detail

class EphemeralChannel final
    : public IAsyncChannel,
      public std::enable_shared_from_this<EphemeralChannel> {
 public:
  ~EphemeralChannel() override;

  MessagePtr createMessage() override {
    return std::make_shared<detail::EphemeralMessage>();
  }

  void sendMessage(AsyncCallback&& cob, MessagePtr message,
                   std::chrono::milliseconds /*timeout*/) override;


  void recvMessage(AsyncCallback&& cob,
                   std::chrono::milliseconds /*timeout*/) override;

  static std::shared_ptr<IAsyncChannel> create(uint16_t address,
                   const std::shared_ptr<ThreadPool>& pool);
 private:
  struct ChannelPair {
    std::weak_ptr<EphemeralChannel> first;
    std::weak_ptr<EphemeralChannel> second;
  };

  inline static std::mutex registryMutex_;
  inline static std::unordered_map<uint16_t, ChannelPair> registry_;

  explicit EphemeralChannel(uint16_t address,
                   std::shared_ptr<ThreadPool> pool);

  void attachChannel();
  void detachChannel() const;

  std::shared_ptr<EphemeralChannel> findPeer() const;

  void deliver(MessagePtr msg);

  MessagePtr pickMessage();

  void scheduleCallback(std::function<void()>&& fn) const;

  uint16_t address_;
  std::shared_ptr<ThreadPool> tp_;

  std::mutex channelMutex_;
  std::vector<MessagePtr> inbox_;
  std::vector<AsyncCallback> callbacks_;
};

}  // namespace getrafty::rpc::io