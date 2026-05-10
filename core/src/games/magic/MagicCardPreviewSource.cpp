#include "ccm/games/magic/MagicCardPreviewSource.hpp"

#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <string>

namespace ccm {

namespace {

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
    return std::string("https://api.scryfall.com/cards/search?q=") +
           rfc3986PercentEncode(query);
}

Result<std::string, PreviewLookupError>
MagicCardPreviewSource::parseResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            // Treat schema deviation as transient: the API contract failed,
            // not the user's record. Scryfall returns a JSON error object
            // here on outage, which is rare but not stable.
            return R::err({K::Transient, "Scryfall response missing 'data' array."});
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return R::err({K::NotFound, "Scryfall returned no matching cards."});
        }
        const auto& first = data.at(0);
        if (!first.contains("image_uris") || !first.at("image_uris").is_object()) {
            // Double-faced cards expose image_uris on each face; there is no
            // fallback for this and surfaces it as "no preview".
            return R::err({K::NotFound, "Card has no top-level image_uris."});
        }
        const auto& uris = first.at("image_uris");
        if (!uris.contains("normal") || !uris.at("normal").is_string()) {
            return R::err({K::NotFound, "Card has no 'normal' image variant."});
        }
        return R::ok(uris.at("normal").get<std::string>());
    } catch (const std::exception& e) {
        return R::err({K::Transient, std::string("Scryfall JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
MagicCardPreviewSource::fetchImageUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view /*setNo*/) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    const std::string url = buildSearchUrl(name, setId);
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parseResponse(resp.value());
}

}  // namespace ccm
