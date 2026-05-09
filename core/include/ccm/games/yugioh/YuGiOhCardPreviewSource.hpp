#pragma once

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

// YuGiOhCardPreviewSource - resolves preview images for Yu-Gi-Oh! cards.
//
// The image-preview path is backed by Yugipedia's MediaWiki API
// (https://yugipedia.com/api.php). Yugipedia hosts actual per-printing card
// scans, with deterministic file names of the shape
// `<Slug>-<SET>-<REGION>-<RARITY>-<EDITION>.<ext>` (e.g.
// `BlueEyesWhiteDragon-LOB-EN-UR-UE.png` vs `BlueEyesWhiteDragon-SDK-NA-UR-UE.png`),
// which lets us return the right artwork for printings that share a passcode
// but have visibly different art - a case YGOPRODeck cannot disambiguate (its
// card_images array is keyed by art-treatment passcode, not by physical
// printing).
//
// The auto-detect-first-print path keeps using YGOPRODeck (`cardinfo.php`):
// that endpoint returns a richer set listing (with rarities and release
// dates) than Yugipedia, and we don't need image data for it.
//
// Region policy: always English (EN/NA/EU/AU) regardless of the card's
// stored Language. Localized scans are intentionally not queried so the user
// sees a consistent, well-stocked gallery (EN scans are the most complete).
class YuGiOhCardPreviewSource final : public ICardPreviewSource {
public:
    explicit YuGiOhCardPreviewSource(IHttpClient& http);

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;
    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override;
    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setId) override;

    // ---- Yugipedia helpers (image preview path) ----------------------------

    // Build the list of candidate Yugipedia file names to try, in priority
    // order (most likely first). Always uses English regions; the caller may
    // pass an empty rarityCode when the rarity is unknown, in which case the
    // returned list will skip rarity in the filename.
    static std::vector<std::string> buildCandidateFilenames(
        std::string_view name,
        std::string_view setCode,
        std::string_view rarityCode,
        bool firstEdition);

    // Build a single MediaWiki batch query URL that asks for imageinfo.url
    // for every filename. MediaWiki's `titles=` parameter joins page titles
    // with `|`, so we issue exactly one HTTP call per preview lookup.
    static std::string buildYugipediaQueryUrl(
        const std::vector<std::string>& filenames);

    // Parse a MediaWiki `query.pages` response and return the resolved URL of
    // the first filename in `filenameOrder` that exists. Missing pages have
    // the `missing` marker (no `imageinfo`); existing pages carry an
    // `imageinfo[0].url` we forward verbatim. Errors are classified:
    //   - JSON parse failure or schema deviation => Transient.
    //   - Every candidate came back missing => NotFound.
    static Result<std::string, PreviewLookupError> parseYugipediaResponse(
        const std::string& body,
        const std::vector<std::string>& filenameOrder);

    // Strip a card name down to Yugipedia's image-slug shape: alphanumerics
    // (and parentheses) only, no whitespace, no policy-banned punctuation.
    static std::string normalizeName(std::string_view name);

    // Map a CCM3 rarity name (e.g. "Ultra Rare") to the Yugipedia rarity
    // code used in image filenames (e.g. "UR"). Returns an empty string when
    // the rarity is unknown; the caller treats that as "skip rarity".
    static std::string rarityCodeFor(std::string_view rarityName);

    // Pull the set abbreviation out of a CCM3 setNo such as "LOB-005" or
    // "LOB-DE005" - in both cases we want "LOB". Returns the trimmed input
    // unchanged if no dash is present.
    static std::string extractSetCode(std::string_view setNo);

    // ---- YGOPRODeck helpers (auto-detect path + fallback) ------------------

    // Build a fuzzy-name `cardinfo.php` URL. `setName` may be empty for an
    // unfiltered fuzzy lookup. Used by detectFirstPrint and by the
    // standard-art fallback when Yugipedia has no scan for this printing.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setName);

    // Pick the standard artwork (card_images[0]) from a YGOPRODeck response,
    // preferring the exact-name match. Used only as a last-resort fallback
    // when Yugipedia returns nothing for any of our candidate filenames.
    // Errors are classified:
    //   - JSON parse failure or schema deviation => Transient.
    //   - Empty `data` array, or matched cards without a usable image
    //     variant => NotFound.
    static Result<std::string, PreviewLookupError>
        parseFallbackImageUrl(const std::string& body, std::string_view name);

    // Pick the first printing for `preferredSetName` from a YGOPRODeck
    // response. Drives the "Auto detect" button in the YGO edit dialog.
    static Result<AutoDetectedPrint> parseFirstPrint(const std::string& body,
                                                     std::string_view preferredSetName);

    // Every `(set_code, set_rarity)` pair for cards whose name matches
    // `wantedCardName` (case-insensitive). When `wantedCardName` is empty,
    // scans every row in `data[]` like `parseFirstPrint` did historically.
    static Result<std::vector<AutoDetectedPrint>>
        parsePrintVariants(const std::string& body,
                           std::string_view preferredSetName,
                           std::string_view wantedCardName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
