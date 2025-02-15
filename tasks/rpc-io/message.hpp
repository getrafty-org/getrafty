#pragma once

namespace getrafty::rpc::io {

class IMessage {
public:
  virtual ~IMessage() = default;

  virtual void writeXID(std::uint64_t value) = 0;
  virtual void writeInt32(std::int32_t value) = 0;
  virtual void writeInt64(std::int64_t value) = 0;
  virtual void writeBytes(const std::vector<std::uint8_t>& bytes, uint32_t length) = 0;
  virtual void writeString(const std::string& str) = 0;
  virtual std::uint64_t consumeXID() = 0;
  virtual std::int32_t consumeInt32() = 0;
  virtual std::int64_t consumeInt64() = 0;
  virtual std::vector<std::uint8_t> consumeBytes(uint32_t length) = 0;
  virtual std::string consumeString() = 0;

};

using MessagePtr = std::shared_ptr<IMessage>;


class ISerializable {
public:
  virtual ~ISerializable() = default;
  virtual void serialize(io::IMessage&) const = 0;
  virtual void deserialize(io::IMessage&) = 0;
};

} // namespace getrafty::rpc::io