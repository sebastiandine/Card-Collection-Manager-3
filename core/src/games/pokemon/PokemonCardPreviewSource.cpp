#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>
#include <string>

namespace ccm {

namespace {

// RFC 3986 percent-encoder for the search-query payload. Same rules as the
// Magic implementation; kept private so the two can drift independently if a
// future API requires it.
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

// Strip everything after the first '/' in a Pokemon collector number.
// The Pokemon TCG API expects `number:"4"`, but cards are commonly stored as
// `4/102`. Without this, no API match is found.
std::string normalizeNumber(std::string_view setNo) {
    std::string s(setNo);
    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        s = s.substr(0, slash);
    }
    return s;
}

}  // namespace

PokemonCardPreviewSource::PokemonCardPreviewSource(IHttpClient& http) : http_(http) {}

std::string PokemonCardPreviewSource::buildSearchUrl(std::string_view name,
                                                     std::string_view setId,
                                                     std::string_view setNo) {
    // Build the unencoded query first so the output matches what the Pokemon
    // TCG search syntax expects: name:"<name>" set.id:<setId> number:<num>.
    std::string query = "name:\"";
    query += std::string(name);
    query += "\"";
    if (!setId.empty()) {
        query += " set.id:";
        query += std::string(setId);
    }
    const std::string num = normalizeNumber(setNo);
    if (!num.empty()) {
        query += " number:";
        query += num;
    }
    return std::string("https://api.pokemontcg.io/v2/cards?q=") + urlEncode(query);
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::parseResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return R::err({K::Transient, "Pokemon TCG response missing 'data' array."});
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return R::err({K::NotFound, "Pokemon TCG returned no matching cards."});
        }
        const auto& first = data.at(0);
        if (!first.contains("images") || !first.at("images").is_object()) {
            return R::err({K::NotFound, "Card has no 'images' object."});
        }
        const auto& images = first.at("images");
        if (images.contains("large") && images.at("large").is_string()) {
            return R::ok(images.at("large").get<std::string>());
        }
        if (images.contains("small") && images.at("small").is_string()) {
            return R::ok(images.at("small").get<std::string>());
        }
        return R::err({K::NotFound, "Card has no 'large' or 'small' image variant."});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
            std::string("Pokemon TCG JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::fetchImageUrl(std::string_view name,
                                        std::string_view setId,
                                        std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    const std::string url = buildSearchUrl(name, setId, setNo);
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parseResponse(resp.value());
}

}  // namespace ccm
