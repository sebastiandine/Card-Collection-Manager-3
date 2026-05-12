#include <doctest/doctest.h>

#include "ccm/ports/ICardPreviewSource.hpp"

using namespace ccm;

namespace {

class MinimalPreviewSource final : public ICardPreviewSource {
public:
    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view,
                      std::string_view,
                      std::string_view) override {
        return Result<std::string, PreviewLookupError>::err(
            PreviewLookupError{PreviewLookupError::Kind::NotFound, "not found"});
    }
};

class AutoDetectPreviewSource final : public ICardPreviewSource {
public:
    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override {
        return true;
    }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view,
                      std::string_view,
                      std::string_view) override {
        return Result<std::string, PreviewLookupError>::ok("https://example.test/card.png");
    }
};

}  // namespace

TEST_SUITE("ICardPreviewSource defaults") {
    TEST_CASE("auto-detect is disabled by default") {
        MinimalPreviewSource src;
        CHECK_FALSE(src.supportsAutoDetectPrint());
    }

    TEST_CASE("implementations may override supportsAutoDetectPrint") {
        AutoDetectPreviewSource src;
        CHECK(src.supportsAutoDetectPrint());
    }

    TEST_CASE("default detectFirstPrint returns explicit unsupported error") {
        MinimalPreviewSource src;
        const auto out = src.detectFirstPrint("Card", "Set");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Auto-detect not supported by this game.");
    }

    TEST_CASE("default detectPrintVariants returns explicit unsupported error") {
        MinimalPreviewSource src;
        const auto out = src.detectPrintVariants("Card", "Set");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Print variant listing not supported by this game.");
    }
}
