#pragma once

// std::filesystem-backed implementation of IFileSystem.

#include "ccm/ports/IFileSystem.hpp"

namespace ccm {

class StdFileSystem final : public IFileSystem {
public:
    [[nodiscard]] bool exists(const std::filesystem::path& p) const override;
    [[nodiscard]] bool isDirectory(const std::filesystem::path& p) const override;

    Result<void> ensureDirectory(const std::filesystem::path& p) override;
    Result<std::string> readText(const std::filesystem::path& p) override;
    Result<void> writeText(const std::filesystem::path& p, std::string_view contents) override;
    Result<void> copyFile(const std::filesystem::path& from,
                          const std::filesystem::path& to,
                          bool overwrite) override;
    Result<void> remove(const std::filesystem::path& p) override;
    Result<std::vector<std::filesystem::path>> listDirectory(
        const std::filesystem::path& p) override;
};

}  // namespace ccm
