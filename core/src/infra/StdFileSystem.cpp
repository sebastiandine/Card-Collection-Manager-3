#include "ccm/infra/StdFileSystem.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace ccm {

namespace fs = std::filesystem;

bool StdFileSystem::exists(const fs::path& p) const {
    std::error_code ec;
    return fs::exists(p, ec);
}

bool StdFileSystem::isDirectory(const fs::path& p) const {
    std::error_code ec;
    return fs::is_directory(p, ec);
}

Result<void> StdFileSystem::ensureDirectory(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec)) {
        if (fs::is_directory(p, ec)) return Result<void>::ok();
        return Result<void>::err("Path exists but is not a directory: " + p.string());
    }
    fs::create_directories(p, ec);
    if (ec) return Result<void>::err("create_directories failed: " + ec.message());
    return Result<void>::ok();
}

Result<std::string> StdFileSystem::readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return Result<std::string>::err("Unable to open file: " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in && !in.eof()) return Result<std::string>::err("Read error on: " + p.string());
    return Result<std::string>::ok(ss.str());
}

Result<void> StdFileSystem::writeText(const fs::path& p, std::string_view contents) {
    std::error_code ec;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
        if (ec) return Result<void>::err("create_directories failed: " + ec.message());
    }
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return Result<void>::err("Unable to create file: " + p.string());
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!out) return Result<void>::err("Write error on: " + p.string());
    return Result<void>::ok();
}

Result<void> StdFileSystem::copyFile(const fs::path& from, const fs::path& to, bool overwrite) {
    std::error_code ec;
    if (to.has_parent_path()) {
        fs::create_directories(to.parent_path(), ec);
        if (ec) return Result<void>::err("create_directories failed: " + ec.message());
        ec.clear();
    }
    const auto opt = overwrite ? fs::copy_options::overwrite_existing
                               : fs::copy_options::none;
    fs::copy_file(from, to, opt, ec);
    if (ec) return Result<void>::err("copy_file failed: " + ec.message());
    return Result<void>::ok();
}

Result<void> StdFileSystem::remove(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
    if (ec) return Result<void>::err("remove failed: " + ec.message());
    return Result<void>::ok();
}

Result<std::vector<fs::path>> StdFileSystem::listDirectory(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_directory(p, ec)) {
        return Result<std::vector<fs::path>>::err("Not a directory: " + p.string());
    }
    std::vector<fs::path> out;
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        out.push_back(entry.path());
    }
    if (ec) return Result<std::vector<fs::path>>::err("directory_iterator: " + ec.message());
    return Result<std::vector<fs::path>>::ok(std::move(out));
}

}  // namespace ccm
