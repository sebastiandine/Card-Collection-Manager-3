#pragma once

// IHttpClient - tiny abstraction over HTTP GET so the rest of the codebase
// never sees libcurl/cpr directly. Tests can substitute a fake client.

#include "ccm/util/Result.hpp"

#include <string>
#include <string_view>

namespace ccm {

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // Issue a blocking HTTPS GET. Returns the response body on success, an
    // error string on failure (no exceptions across the port boundary).
    //
    // The returned `std::string` is a raw byte buffer - it is NOT decoded as
    // text. Binary payloads (e.g. PNG/JPEG image bytes for the card preview
    // feature) round-trip through this method intact.
    virtual Result<std::string> get(std::string_view url) = 0;
};

}  // namespace ccm
