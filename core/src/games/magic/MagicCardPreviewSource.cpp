#include "ccm/games/magic/MagicCardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>
#include <string>

namespace ccm {

namespace {

// Percent-encode all bytes that are not unreserved per RFC 3986
// (A-Z / a-z / 0-9 / - . _ ~). Spaces become %20, quotes become %22, etc.
// Used to keep Scryfall's `q=...` parameter syntactically valid through cpr,
// which does not URL-encode the URL string we hand it.
std::string urlEncode(std::string_view in) {
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

// Apply the same name massaging as the legacy query path before sending.
std::string sanitizeName(std::string_view name) {
    std::string s(name);
    std::string::size_type pos = 0;
    while ((pos = s.find('&', pos)) != std::string::npos) {
        s.replace(pos, 1, "and");
        pos += 3;
    }
    return s;
}

}  // namespace

MagicCardPreviewSource::MagicCardPreviewSource(IHttpClient& http) : http_(http) {}

std::string MagicCardPreviewSource::buildSearchUrl(std::string_view name,
                                                   std::string_view setId) {
    // Build the unencoded query first so the output matches what Scryfall
    // would parse: name:"<sanitized>" AND set:<setId>
    const std::string sanitized = sanitizeName(name);
    std::string query = "name:\"";
    query += sanitized;
    query += "\" AND set:";
    query += std::string(setId);
    return std::string("https://api.scryfall.com/cards/search?q=") + urlEncode(query);
}

Result<std::string> MagicCardPreviewSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<std::string>::err("Scryfall response missing 'data' array.");
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return Result<std::string>::err("Scryfall returned no matching cards.");
        }
        const auto& first = data.at(0);
        if (!first.contains("image_uris") || !first.at("image_uris").is_object()) {
            // Double-faced cards expose image_uris on each face; there is no
            // fallback for this and surfaces it as "no preview".
            return Result<std::string>::err("Card has no top-level image_uris.");
        }
        const auto& uris = first.at("image_uris");
        if (!uris.contains("normal") || !uris.at("normal").is_string()) {
            return Result<std::string>::err("Card has no 'normal' image variant.");
        }
        return Result<std::string>::ok(uris.at("normal").get<std::string>());
    } catch (const std::exception& e) {
        return Result<std::string>::err(std::string("Scryfall JSON parse error: ") + e.what());
    }
}

Result<std::string> MagicCardPreviewSource::fetchImageUrl(std::string_view name,
                                                          std::string_view setId,
                                                          std::string_view /*setNo*/) {
    const std::string url = buildSearchUrl(name, setId);
    auto resp = http_.get(url);
    if (!resp) return Result<std::string>::err(resp.error());
    return parseResponse(resp.value());
}

}  // namespace ccm
