#pragma once

#include "error.hpp"

namespace getrafty::rpc::io {

class IMessage {
public:
  virtual ~IMessage() = default;

  virtual void setBody(const std::string& data) = 0;
  [[nodiscard]] virtual std::string& getBody() const = 0;

  virtual void setMethod(const std::string& method) = 0;
  [[nodiscard]] virtual std::string& getMethod() const = 0;

  virtual void setSequenceId(uint64_t value) = 0;
  [[nodiscard]] virtual uint64_t getSequenceId() const = 0;

  virtual void setProtocol(const std::string& protocol) = 0;
  [[nodiscard]] virtual std::string& getProtocol() const = 0;

  virtual void setErrorCode(RpcError::Code) = 0;
  [[nodiscard]] virtual RpcError::Code getErrorCode() const = 0;

  virtual std::shared_ptr<IMessage> constructFromCurrent() = 0;
};

using MessagePtr = std::shared_ptr<IMessage>;


class ISerializable {
public:
  virtual ~ISerializable() = default;
  virtual void serialize(io::IMessage&) const = 0;
  virtual void deserialize(io::IMessage&) = 0;
};

} // namespace getrafty::rpc::io