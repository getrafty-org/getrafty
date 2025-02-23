#pragma once

namespace getrafty::rpc {



class RpcError final : public std::runtime_error {
public:
  enum class Code : uint16_t {
    OK,
    SEND_TIMEOUT,
    RECV_TIMEOUT,
    PROC_TIMEOUT,
    OVERALL_TIMEOUT,
    APP_ERROR,
    FAILURE,
    CANCELLED,
  };
  explicit RpcError(const Code code) : RpcError(code, "err"){};
  explicit RpcError(const Code code, const std::string& m)
      : runtime_error(m), code_(code){};

  [[nodiscard]] Code code() const { return code_; }

private:
  const Code code_;
};

}  // namespace getrafty::rpc