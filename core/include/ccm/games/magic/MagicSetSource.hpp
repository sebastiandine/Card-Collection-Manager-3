#pragma once

// MagicSetSource: ISetSource implementation for Magic the Gathering.
// Calls the Scryfall API at https://api.scryfall.com/sets, drops digital-only
// sets, maps the response into our `Set` domain type, and sorts by release date
// ascending. Behavior matches `magic/set_services.rs::update_sets`.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

namespace ccm {

class MagicSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://api.scryfall.com/sets";

    explicit MagicSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // Pure parser exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
