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

    TEST_CASE("returns an explicit error when executor is empty") {
        CprHttpClient::GetExecutor empty;
        CprHttpClient client{empty};

        const auto out = client.get("https://example.com");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP error: no executor configured");
    }
}

TEST_SUITE("CprHttpClient injected raw executor") {
    TEST_CASE("maps 2xx raw response to success body") {
        CprHttpClient client{
            [](std::string_view) -> CprHttpClient::RawResponse {
                return CprHttpClient::RawResponse{
                    .transportError = false,
                    .transportMessage = "",
                    .statusCode = 200,
                    .body = "ok-body",
                };
            }
        };

        const auto out = client.get("https://example.com/success");
        REQUIRE(out.isOk());
        CHECK(out.value() == "ok-body");
    }

    TEST_CASE("maps transport error via shared http mapping") {
        CprHttpClient client{
            [](std::string_view) -> CprHttpClient::RawResponse {
                return CprHttpClient::RawResponse{
                    .transportError = true,
                    .transportMessage = "timeout",
                    .statusCode = 0,
                    .body = "",
                };
            }
        };

        const auto out = client.get("https://example.com/timeout");
        REQUIRE(out.isErr());
        CHECK(out.error().find("timeout") != std::string::npos);
    }
}

TEST_SUITE("CprHttpClient real session") {
    TEST_CASE("default constructor handles malformed URL without crashing") {
        // Exercise the real cpr::Session-backed constructor/lambda path
        // without depending on external network availability.
        CprHttpClient client{};
        const auto out = client.get("://not-a-valid-url");
        REQUIRE(out.isErr());
        CHECK(out.error().find("HTTP") != std::string::npos);
    }
}
