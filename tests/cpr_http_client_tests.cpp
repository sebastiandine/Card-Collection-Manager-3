#include <doctest/doctest.h>

#include "ccm/infra/CprHttpClient.hpp"

#include <string>
#include <string_view>

using namespace ccm;

TEST_SUITE("CprHttpClient injected executor") {
    TEST_CASE("forwards URL to injected executor and returns payload") {
        std::string seenUrl;
        CprHttpClient client{
            [&seenUrl](std::string_view url) -> Result<std::string> {
                seenUrl = std::string(url);
                return Result<std::string>::ok("body");
            }
        };

        const auto out = client.get("https://example.com/api?q=1");
        REQUIRE(out.isOk());
        CHECK(out.value() == "body");
        CHECK(seenUrl == "https://example.com/api?q=1");
    }

    TEST_CASE("propagates injected executor error as-is") {
        CprHttpClient client{
            [](std::string_view) -> Result<std::string> {
                return Result<std::string>::err("HTTP 503 from https://example.com");
            }
        };

        const auto out = client.get("https://example.com");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP 503 from https://example.com");
    }
}
