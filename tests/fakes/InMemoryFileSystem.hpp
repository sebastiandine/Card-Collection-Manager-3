#pragma once

// In-memory IFileSystem fake used to drive the service tests with no real I/O.
// Tracks regular files (string contents) and directories (just by presence).

#include "ccm/ports/IFileSystem.hpp"

#include <map>
#include <set>
#include <string>

namespace ccm::testing {

class InMemoryFileSystem final : public IFileSystem {
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

    // Test helpers.
    [[nodiscard]] const std::map<std::string, std::string>& files() const noexcept { return files_; }
    [[nodiscard]] const std::set<std::string>&  dirs()  const noexcept { return dirs_; }

private:
    static std::string norm(const std::filesystem::path& p);

    std::map<std::string, std::string> files_;
    std::set<std::string>              dirs_;
};

}  // namespace ccm::testing
