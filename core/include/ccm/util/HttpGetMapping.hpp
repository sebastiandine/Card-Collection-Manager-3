#pragma once

#include "ccm/util/Result.hpp"

#include <string>
#include <string_view>

namespace ccm {

// Shared classification for raw HTTP GET outcomes (transport vs status vs OK).
// `CprHttpClient::get` delegates here so doctest can exercise the branches
// without touching libcpr or the network stack.
[[nodiscard]] inline Result<std::string> mapHttpGetResponse(bool curlTransportError,
                                                             std::string_view curlErrorMessage,
                                                             long httpStatusCode,
                                                             std::string responseBody,
                                                             std::string_view requestUrl) {
    if (curlTransportError) {
        return Result<std::string>::err(std::string("HTTP error: ") +
                                         std::string(curlErrorMessage));
    }
    if (httpStatusCode < 200 || httpStatusCode >= 300) {
        return Result<std::string>::err(
            "HTTP " + std::to_string(httpStatusCode) + " from " +
            std::string(requestUrl));
    }
    return Result<std::string>::ok(std::move(responseBody));
}

}  // namespace ccm
