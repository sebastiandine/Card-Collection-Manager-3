#include "ccm/infra/CprHttpClient.hpp"

#include "ccm/util/HttpGetMapping.hpp"

#include <cpr/cpr.h>

#include <string>
#include <utility>

namespace ccm {

CprHttpClient::CprHttpClient(std::chrono::milliseconds timeout)
    : timeout_(timeout),
      session_(std::make_unique<cpr::Session>()) {
    // Configure session-wide options once; every Get() then only updates
    // the URL. libcurl's connection cache lives inside the easy handle, so
    // reusing one Session across calls is what gets us TLS keep-alive.
    session_->SetTimeout(cpr::Timeout{timeout_});
    // `Accept: application/json` breaks some CDNs that refuse non-JSON bodies
    // (preview pipeline also GETs raw JPG/PNG). Wildcard keeps JSON APIs happy.
    session_->SetHeader(cpr::Header{
        {"User-Agent", "card-collection-manager-3/0.1"},
        {"Accept",     "*/*"},
    });
    session_->SetRedirect(cpr::Redirect{/*max_redirects=*/10L,
                                        /*follow=*/true,
                                        /*cont_send_cred=*/false,
                                        cpr::PostRedirectFlags::POST_ALL});
    rawExecutor_ = [this](std::string_view url) -> RawResponse {
        session_->SetUrl(cpr::Url{std::string(url)});
        cpr::Response r = session_->Get();
        return RawResponse{
            .transportError = static_cast<bool>(r.error),
            .transportMessage = r.error.message,
            .statusCode = static_cast<int>(r.status_code),
            .body = std::move(r.text),
        };
    };
}

CprHttpClient::CprHttpClient(GetExecutor executor,
                             std::chrono::milliseconds timeout)
    : timeout_(timeout),
      session_(nullptr),
      executor_(std::move(executor)) {}

CprHttpClient::CprHttpClient(RawGetExecutor rawExecutor,
                             std::chrono::milliseconds timeout)
    : timeout_(timeout),
      session_(nullptr),
      rawExecutor_(std::move(rawExecutor)) {}

CprHttpClient::~CprHttpClient() = default;

Result<std::string> CprHttpClient::get(std::string_view url) {
    // libcurl easy handles (and therefore cpr::Session) are not thread-safe.
    // We serialize callers here; the preview path is single-flight already
    // (one fetch per BaseSelectedCardPanel selection change), so contention
    // is negligible.
    std::lock_guard<std::mutex> lock(sessionMutex_);
    if (executor_) {
        return executor_(url);
    }
    if (rawExecutor_) {
        RawResponse raw = rawExecutor_(url);
        return mapHttpGetResponse(raw.transportError,
                                  raw.transportMessage,
                                  raw.statusCode,
                                  std::move(raw.body),
                                  url);
    }
    return Result<std::string>::err("HTTP error: no executor configured");
}

}  // namespace ccm
