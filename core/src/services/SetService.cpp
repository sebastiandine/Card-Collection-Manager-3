#include "ccm/services/SetService.hpp"

namespace ccm {

SetService::SetService(ISetRepository& repo) : repo_(repo) {}

void SetService::registerModule(IGameModule* module) {
    if (module) modules_[module->id()] = module;
}

Result<std::vector<Set>> SetService::updateSets(Game game) {
    auto it = modules_.find(game);
    if (it == modules_.end() || it->second == nullptr) {
        return Result<std::vector<Set>>::err("No game module registered for this game.");
    }
    auto fetched = it->second->setSource().fetchAll();
    if (!fetched) return fetched;
    auto saved = repo_.save(game, fetched.value());
    if (!saved) return Result<std::vector<Set>>::err(saved.error());
    return fetched;
}

Result<std::vector<Set>> SetService::getSets(Game game) {
    return repo_.load(game);
}

}  // namespace ccm
