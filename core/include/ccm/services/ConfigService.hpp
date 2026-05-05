#pragma once

// ConfigService: owns the live `Configuration`, persists it to `config.json`
// next to the executable. Mirrors `util/config.rs` behavior - a missing file
// is auto-created with sensible defaults on first launch.

#include "ccm/domain/Configuration.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/util/Result.hpp"

#include <filesystem>

namespace ccm {

class ConfigService {
public:
    // `configFilePath` is the absolute path to `config.json`. The defaults
    // applied to a freshly created config use `defaultDataStorage` for the
    // dataStorage field.
    ConfigService(IFileSystem& fs,
                  std::filesystem::path configFilePath,
                  std::filesystem::path defaultDataStorage);

    // Load (or create) the configuration. Must be called once at startup.
    Result<void> initialize();

    [[nodiscard]] const Configuration& current() const noexcept { return current_; }

    // Replace the live configuration and persist immediately.
    Result<void> store(Configuration cfg);

private:
    IFileSystem& fs_;
    std::filesystem::path path_;
    std::filesystem::path defaultDataStorage_;
    Configuration current_{};

    [[nodiscard]] Configuration makeDefault() const;
};

}  // namespace ccm
