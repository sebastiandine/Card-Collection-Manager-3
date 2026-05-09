#pragma once

// CprHttpClient: cpr-based implementation of IHttpClient.
// All cpr/libcurl symbols stay confined to the .cpp file - any TU that just
// needs to make HTTP calls only depends on the IHttpClient port.

#include "ccm/ports/IHttpClient.hpp"

#include <chrono>

namespace ccm {

class CprHttpClient final : public IHttpClient {
public:
    explicit CprHttpClient(std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

    Result<std::string> get(std::string_view url) override;

private:
    std::chrono::milliseconds timeout_;
};

}  // namespace ccm
