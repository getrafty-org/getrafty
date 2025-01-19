#pragma once

namespace getrafty::rpc::io {

class IChannel {
 public:
  class IMessage {
   public:
    virtual ~IMessage() = default;

    virtual void writeInt32(std::int32_t value) = 0;
    virtual void writeInt64(std::int64_t value) = 0;
    virtual void writeBytes(const std::vector<std::uint8_t>& bytes,
                            uint32_t length) = 0;
    virtual void writeString(const std::string& str) = 0;

    virtual std::int32_t readInt32() = 0;
    virtual std::int64_t readInt64() = 0;
    virtual std::vector<std::uint8_t> readBytes(uint32_t length) = 0;
    virtual std::string readString() = 0;

    virtual std::optional<std::string> getHeader(const std::string& key) = 0;
    virtual void setHeader(const std::string& key,
                           const std::string& value) = 0;
  };

  using MessagePtr = std::shared_ptr<IMessage>;

  virtual ~IChannel() = default;

  virtual MessagePtr createMessage() = 0;

  virtual void sendAsync(MessagePtr msg, std::function<void()> callback,
                         std::optional<uint32_t> timeout_ms) = 0;

  virtual void receiveAsync(std::function<void(MessagePtr)>,
                            std::optional<uint32_t> timeout_ms) = 0;
};
}  // namespace getrafty::rpc::io