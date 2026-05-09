#pragma once

// CprHttpClient: cpr-based implementation of IHttpClient.
// All cpr/libcurl symbols stay confined to the .cpp file - any TU that just
// needs to make HTTP calls only depends on the IHttpClient port.

#include "ccm/ports/IHttpClient.hpp"

#include <chrono>
#include <memory>
#include <mutex>

namespace cpr { class Session; }

namespace ccm {

// Concrete IHttpClient backed by libcpr/libcurl. The single owned
// `cpr::Session` keeps libcurl's connection pool alive across calls, so
// repeat HTTPS requests to the same host (api.scryfall.com, yugipedia.com,
// ms.yugipedia.com, …) reuse the existing TLS connection instead of paying
// for a fresh handshake every time. Concurrent calls are serialized through
// a mutex - libcurl easy handles are not thread-safe, and the preview path
// only fires one outbound request at a time anyway.
class CprHttpClient final : public IHttpClient {
public:
    explicit CprHttpClient(std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});
    ~CprHttpClient() override;

    Result<std::string> get(std::string_view url) override;

private:
    std::chrono::milliseconds timeout_;
    std::unique_ptr<cpr::Session> session_;
    std::mutex sessionMutex_;
};

}  // namespace ccm
