#pragma once

// IFileSystem - filesystem operations the services need, expressed as a
// narrow port. Real implementation is `StdFileSystem` (over <filesystem>).
// In-memory implementation can be plugged in for tests.

#include "ccm/util/Result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ccm {

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual bool exists(const std::filesystem::path& p) const = 0;
    [[nodiscard]] virtual bool isDirectory(const std::filesystem::path& p) const = 0;

    virtual Result<void> ensureDirectory(const std::filesystem::path& p) = 0;
    virtual Result<std::string> readText(const std::filesystem::path& p) = 0;
    virtual Result<void> writeText(const std::filesystem::path& p, std::string_view contents) = 0;
    virtual Result<void> copyFile(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  bool overwrite) = 0;
    virtual Result<void> remove(const std::filesystem::path& p) = 0;
    virtual Result<std::vector<std::filesystem::path>> listDirectory(
        const std::filesystem::path& p) = 0;
};

}  // namespace ccm
