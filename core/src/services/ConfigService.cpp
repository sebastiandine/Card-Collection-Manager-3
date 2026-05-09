#include "ccm/services/ConfigService.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

ConfigService::ConfigService(IFileSystem& fs,
                             std::filesystem::path configFilePath,
                             std::filesystem::path defaultDataStorage)
    : fs_(fs),
      path_(std::move(configFilePath)),
      defaultDataStorage_(std::move(defaultDataStorage)) {}

Configuration ConfigService::makeDefault() const {
    Configuration c;
    // generic_string() always uses '/' separators - keeps the value portable
    // across Windows / Unix and round-trip-friendly for tests and JSON.
    c.dataStorage = defaultDataStorage_.generic_string();
    c.defaultGame = Game::Magic;
    return c;
}

Result<void> ConfigService::initialize() {
    if (!fs_.exists(path_)) {
        current_ = makeDefault();
        return store(current_);
    }
    auto text = fs_.readText(path_);
    if (!text) return Result<void>::err(text.error());
    try {
        auto j = nlohmann::json::parse(text.value());
        current_ = j.get<Configuration>();
    } catch (const std::exception& e) {
        return Result<void>::err(std::string("config.json parse error: ") + e.what());
    }
    return Result<void>::ok();
}

Result<void> ConfigService::store(Configuration cfg) {
    current_ = std::move(cfg);
    const nlohmann::json j = current_;
    return fs_.writeText(path_, j.dump(2));
}

}  // namespace ccm
