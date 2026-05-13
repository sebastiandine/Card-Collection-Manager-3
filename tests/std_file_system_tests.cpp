#include <doctest/doctest.h>

// StdFileSystem translates IFileSystem onto std::filesystem. Like
// LocalPreviewByteCache tests, these exercise real disk under a dedicated
// temp directory so permission errors and path normalization behave like production.

#include "ccm/infra/StdFileSystem.hpp"

#include <chrono>
#include <filesystem>
#include <random>
#include <string>

using namespace ccm;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;

    TempDir() {
        std::random_device rd;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() /
               (std::string("ccm_std_fs_test_") + std::to_string(stamp) + "_" +
                std::to_string(rd()));
        std::error_code ec;
        fs::create_directories(path, ec);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
};

}  // namespace

TEST_SUITE("StdFileSystem") {
    TEST_CASE("exists and isDirectory reflect real paths") {
        TempDir td;
        StdFileSystem fs;

        const auto nested = td.path / "a" / "b";
        CHECK_FALSE(fs.exists(nested));

        REQUIRE(fs.ensureDirectory(nested).isOk());
        CHECK(fs.exists(nested));
        CHECK(fs.isDirectory(nested));

        const auto filePath = td.path / "file.bin";
        REQUIRE(fs.writeText(filePath, "x").isOk());
        CHECK(fs.exists(filePath));
        CHECK_FALSE(fs.isDirectory(filePath));
    }

    TEST_CASE("ensureDirectory succeeds when directory already exists") {
        TempDir td;
        StdFileSystem fs;
        const auto dir = td.path / "existing";
        REQUIRE(fs.ensureDirectory(dir).isOk());
        REQUIRE(fs.ensureDirectory(dir).isOk());
        CHECK(fs.isDirectory(dir));
    }

    TEST_CASE("ensureDirectory errors when path is a regular file") {
        TempDir td;
        StdFileSystem fs;
        const auto clash = td.path / "notadir";
        REQUIRE(fs.writeText(clash, "block").isOk());

        const auto r = fs.ensureDirectory(clash);
        REQUIRE(r.isErr());
        CHECK(r.error().find("not a directory") != std::string::npos);
    }

    TEST_CASE("readText round-trips bytes written by writeText") {
        TempDir td;
        StdFileSystem fs;
        const auto p = td.path / "sub" / "cfg.json";
        const std::string payload(std::string("{\"x\":") + std::string(4, '\0') + "}");

        REQUIRE(fs.writeText(p, payload).isOk());
        const auto readBack = fs.readText(p);
        REQUIRE(readBack.isOk());
        CHECK(readBack.value() == payload);
    }

    TEST_CASE("readText errors when file does not exist") {
        TempDir td;
        StdFileSystem fs;
        const auto missing = td.path / "missing.txt";

        const auto r = fs.readText(missing);
        REQUIRE(r.isErr());
        CHECK(r.error().find("Unable to open") != std::string::npos);
    }

    TEST_CASE("writeText truncates an existing file") {
        TempDir td;
        StdFileSystem fs;
        const auto p = td.path / "t.txt";
        REQUIRE(fs.writeText(p, "aaaaaaaaaa").isOk());
        REQUIRE(fs.writeText(p, "hi").isOk());

        const auto r = fs.readText(p);
        REQUIRE(r.isOk());
        CHECK(r.value() == "hi");
    }

    TEST_CASE("writeText/readText work for top-level relative files") {
        TempDir td;
        StdFileSystem fs;
        const auto oldCwd = fs::current_path();
        fs::current_path(td.path);

        const fs::path topLevel = "top-level.txt";
        REQUIRE(fs.writeText(topLevel, "hello").isOk());
        const auto r = fs.readText(topLevel);
        REQUIRE(r.isOk());
        CHECK(r.value() == "hello");

        std::error_code ec;
        fs::current_path(oldCwd, ec);
    }

    TEST_CASE("copyFile copies bytes and respects overwrite flag") {
        TempDir td;
        StdFileSystem fs;
        const auto src = td.path / "src.bin";
        const auto dst = td.path / "nested" / "dst.bin";

        REQUIRE(fs.writeText(src, "alpha").isOk());
        REQUIRE(fs.copyFile(src, dst, /*overwrite=*/false).isOk());

        auto rd = fs.readText(dst);
        REQUIRE(rd.isOk());
        CHECK(rd.value() == "alpha");

        REQUIRE(fs.writeText(src, "beta").isOk());
        const auto noOverwrite = fs.copyFile(src, dst, /*overwrite=*/false);
        REQUIRE(noOverwrite.isErr());

        REQUIRE(fs.copyFile(src, dst, /*overwrite=*/true).isOk());
        rd = fs.readText(dst);
        REQUIRE(rd.isOk());
        CHECK(rd.value() == "beta");
    }

    TEST_CASE("copyFile fails cleanly when source is missing") {
        TempDir td;
        StdFileSystem fs;
        const auto src = td.path / "ghost.dat";
        const auto dst = td.path / "out.dat";

        const auto r = fs.copyFile(src, dst, /*overwrite=*/false);
        REQUIRE(r.isErr());
        CHECK(r.error().find("copy_file failed") != std::string::npos);
    }

    TEST_CASE("remove deletes a file and tolerates repeated removes") {
        TempDir td;
        StdFileSystem fs;
        const auto p = td.path / "gone.txt";
        REQUIRE(fs.writeText(p, "body").isOk());

        REQUIRE(fs.remove(p).isOk());
        CHECK_FALSE(fs.exists(p));

        REQUIRE(fs.remove(p).isOk());
    }

    TEST_CASE("listDirectory errors when path is not a directory") {
        TempDir td;
        StdFileSystem fs;
        const auto p = td.path / "single.dat";
        REQUIRE(fs.writeText(p, "").isOk());

        const auto r = fs.listDirectory(p);
        REQUIRE(r.isErr());
        CHECK(r.error().find("Not a directory") != std::string::npos);
    }

    TEST_CASE("listDirectory returns entries for an empty and populated folder") {
        TempDir td;
        StdFileSystem fs;
        const auto dir = td.path / "list_me";

        REQUIRE(fs.ensureDirectory(dir).isOk());
        auto empty = fs.listDirectory(dir);
        REQUIRE(empty.isOk());
        CHECK(empty.value().empty());

        REQUIRE(fs.writeText(dir / "a.txt", "a").isOk());
        REQUIRE(fs.writeText(dir / "b.txt", "b").isOk());

        auto filled = fs.listDirectory(dir);
        REQUIRE(filled.isOk());
        CHECK(filled.value().size() == 2u);
    }

    TEST_CASE("writeText fails when the path names an existing directory") {
        TempDir td;
        StdFileSystem fs;
        const auto dir = td.path / "is_dir";
        REQUIRE(fs.ensureDirectory(dir).isOk());

        const auto r = fs.writeText(dir, "cannot-write-here");
        REQUIRE(r.isErr());
        CHECK(r.error().find("Unable to create file") != std::string::npos);
    }
}
