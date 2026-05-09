#include "ccm/infra/CprHttpClient.hpp"

#include <cpr/cpr.h>

#include <string>

namespace ccm {

CprHttpClient::CprHttpClient(std::chrono::milliseconds timeout) : timeout_(timeout) {}

Result<std::string> CprHttpClient::get(std::string_view url) {
    cpr::Response r = cpr::Get(
        cpr::Url{std::string(url)},
        cpr::Timeout{timeout_},
        // Identify ourselves; some APIs rate-limit unknown agents harshly.
        cpr::Header{{"User-Agent", "card-collection-manager-3/0.1"},
                    {"Accept",     "application/json"}}
    );

    if (r.error) {
        return Result<std::string>::err("HTTP error: " + r.error.message);
    }
    if (r.status_code < 200 || r.status_code >= 300) {
        return Result<std::string>::err(
            "HTTP " + std::to_string(r.status_code) + " from " + std::string(url));
    }
    return Result<std::string>::ok(std::move(r.text));
}

}  // namespace ccm
