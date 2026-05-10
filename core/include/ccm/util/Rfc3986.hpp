#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace ccm {

// Percent-encode all bytes that are not unreserved per RFC 3986
// (A-Z / a-z / 0-9 / - . _ ~). Needed because cpr does not encode the URL
// string passed to IHttpClient::get.
[[nodiscard]] inline std::string rfc3986PercentEncode(std::string_view in) {
    std::ostringstream out;
    out.fill('0');
    out << std::hex << std::uppercase;
    for (unsigned char c : in) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) {
            out << static_cast<char>(c);
        } else {
            out << '%';
            out.width(2);
            out << static_cast<unsigned int>(c);
        }
    }
    return out.str();
}

}  // namespace ccm
