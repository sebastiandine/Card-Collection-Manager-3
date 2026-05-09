#include "fakes/InMemoryFileSystem.hpp"

namespace ccm::testing {

namespace fs = std::filesystem;

std::string InMemoryFileSystem::norm(const fs::path& p) {
    // Use forward slashes regardless of host so tests are portable.
    return p.lexically_normal().generic_string();
}

bool InMemoryFileSystem::exists(const fs::path& p) const {
    const auto k = norm(p);
    return files_.count(k) || dirs_.count(k);
}

bool InMemoryFileSystem::isDirectory(const fs::path& p) const {
    return dirs_.count(norm(p)) > 0;
}

Result<void> InMemoryFileSystem::ensureDirectory(const fs::path& p) {
    auto k = norm(p);
    if (files_.count(k)) {
        return Result<void>::err("Path is a file, cannot become a directory: " + k);
    }
    // Walk parent chain so listDirectory() acts naturally.
    fs::path acc;
    for (const auto& part : p) {
        acc /= part;
        dirs_.insert(norm(acc));
    }
    return Result<void>::ok();
}

Result<std::string> InMemoryFileSystem::readText(const fs::path& p) {
    auto it = files_.find(norm(p));
    if (it == files_.end()) {
        return Result<std::string>::err("Not found: " + norm(p));
    }
    return Result<std::string>::ok(it->second);
}

Result<void> InMemoryFileSystem::writeText(const fs::path& p, std::string_view contents) {
    if (p.has_parent_path()) {
        auto r = ensureDirectory(p.parent_path());
        if (!r) return r;
    }
    files_[norm(p)] = std::string(contents);
    return Result<void>::ok();
}

Result<void> InMemoryFileSystem::copyFile(const fs::path& from, const fs::path& to, bool overwrite) {
    auto src = files_.find(norm(from));
    if (src == files_.end()) {
        return Result<void>::err("copy_file source missing: " + norm(from));
    }
    if (!overwrite && files_.count(norm(to))) {
        return Result<void>::err("copy_file destination exists: " + norm(to));
    }
    if (to.has_parent_path()) {
        auto r = ensureDirectory(to.parent_path());
        if (!r) return r;
    }
    files_[norm(to)] = src->second;
    return Result<void>::ok();
}

Result<void> InMemoryFileSystem::remove(const fs::path& p) {
    files_.erase(norm(p));
    return Result<void>::ok();
}

Result<std::vector<fs::path>> InMemoryFileSystem::listDirectory(const fs::path& p) {
    const auto prefix = norm(p) + "/";
    std::vector<fs::path> out;
    for (const auto& [path, _] : files_) {
        if (path.rfind(prefix, 0) == 0 &&
            path.find('/', prefix.size()) == std::string::npos) {
            out.emplace_back(path);
        }
    }
    return Result<std::vector<fs::path>>::ok(std::move(out));
}

}  // namespace ccm::testing
