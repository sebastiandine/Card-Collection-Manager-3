#include "ccm/services/CardPreviewService.hpp"

namespace ccm {

CardPreviewService::CardPreviewService(IHttpClient& http) : http_(http) {}

void CardPreviewService::registerModule(IGameModule& module) {
    if (auto* src = module.cardPreviewSource(); src != nullptr) {
        sources_[module.id()] = src;
    }
}

Result<std::string> CardPreviewService::fetchPreviewBytes(Game game,
                                                          std::string_view name,
                                                          std::string_view setId,
                                                          std::string_view setNo) {
    auto it = sources_.find(game);
    if (it == sources_.end() || it->second == nullptr) {
        return Result<std::string>::err("No preview source registered for this game.");
    }
    auto url = it->second->fetchImageUrl(name, setId, setNo);
    if (!url) return Result<std::string>::err(url.error());
    auto bytes = http_.get(url.value());
    if (!bytes) return Result<std::string>::err(bytes.error());
    return Result<std::string>::ok(std::move(bytes).value());
}

Result<std::string> CardPreviewService::fetchImageBytesByUrl(std::string_view url) {
    auto bytes = http_.get(url);
    if (!bytes) return Result<std::string>::err(bytes.error());
    return Result<std::string>::ok(std::move(bytes).value());
}

}  // namespace ccm
