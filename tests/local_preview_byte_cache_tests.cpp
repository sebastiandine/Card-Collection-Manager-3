#include <doctest/doctest.h>

// LocalPreviewByteCache is the only ccm_core test in this binary that
// exercises the real filesystem - by design. The adapter's reason for
// existing is to translate IPreviewByteCache calls into actual on-disk
// state (binary payloads, sidecar key files, mtime-driven LRU eviction
// via std::filesystem), and stubbing those primitives just to test the
// in-memory bookkeeping would defeat the whole point. We pin every test
// to a fresh, unique directory under the OS temp path and clean it up
// even on assertion failure.

#include "ccm/infra/LocalPreviewByteCache.hpp"
#include "ccm/infra/StdFileSystem.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <thread>

using namespace ccm;
namespace fs = std::filesystem;

namespace {

// Creates and removes a unique temp directory with strong cleanup
// guarantees. Doctest does not have RAII fixtures by default; this struct
// provides the same effect via destructor.
struct TempDir {
    fs::path path;

    TempDir() {
        std::random_device rd;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() /
               (std::string("ccm_preview_cache_test_") + std::to_string(stamp) +
                "_" + std::to_string(rd()));
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

// Touch helper: nudges a file's mtime backwards so eviction-by-oldest is
// deterministic regardless of FS timestamp resolution.
void backdate(const fs::path& p, int seconds) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    if (ec) return;
    fs::last_write_time(p, t - std::chrono::seconds(seconds), ec);
}

class FailingEnsureDirFs final : public IFileSystem {
public:
    explicit FailingEnsureDirFs(ccm::testing::InMemoryFileSystem& inner) : inner_(inner) {}

    [[nodiscard]] bool exists(const fs::path& p) const override { return inner_.exists(p); }
    [[nodiscard]] bool isDirectory(const fs::path& p) const override { return inner_.isDirectory(p); }
    Result<void> ensureDirectory(const fs::path& p) override {
        (void)p;
        return Result<void>::err("ensure failed");
    }
    Result<std::string> readText(const fs::path& p) override { return inner_.readText(p); }
    Result<void> writeText(const fs::path& p, std::string_view contents) override {
        return inner_.writeText(p, contents);
    }
    Result<void> copyFile(const fs::path& from, const fs::path& to, bool overwrite) override {
        return inner_.copyFile(from, to, overwrite);
    }
    Result<void> remove(const fs::path& p) override { return inner_.remove(p); }
    Result<std::vector<fs::path>> listDirectory(const fs::path& p) override {
        return inner_.listDirectory(p);
    }

private:
    ccm::testing::InMemoryFileSystem& inner_;
};

class FailingIndexWriteFs final : public IFileSystem {
public:
    explicit FailingIndexWriteFs(ccm::testing::InMemoryFileSystem& inner) : inner_(inner) {}

    [[nodiscard]] bool exists(const fs::path& p) const override { return inner_.exists(p); }
    [[nodiscard]] bool isDirectory(const fs::path& p) const override { return inner_.isDirectory(p); }
    Result<void> ensureDirectory(const fs::path& p) override { return inner_.ensureDirectory(p); }
    Result<std::string> readText(const fs::path& p) override { return inner_.readText(p); }
    Result<void> writeText(const fs::path& p, std::string_view contents) override {
        const auto path = p.generic_string();
        if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".idx") == 0) {
            return Result<void>::err("idx write failed");
        }
        return inner_.writeText(p, contents);
    }
    Result<void> copyFile(const fs::path& from, const fs::path& to, bool overwrite) override {
        return inner_.copyFile(from, to, overwrite);
    }
    Result<void> remove(const fs::path& p) override { return inner_.remove(p); }
    Result<std::vector<fs::path>> listDirectory(const fs::path& p) override {
        return inner_.listDirectory(p);
    }

private:
    ccm::testing::InMemoryFileSystem& inner_;
};

}  // namespace

TEST_SUITE("LocalPreviewByteCache") {
    TEST_CASE("store then load round-trips bytes verbatim") {
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        const std::string key  = "magic|Lightning Bolt|lea|161";
        const std::string body = std::string("\x89PNG\r\n\x1a\n", 8) + std::string(2048, '\xab');

        cache.store(key, body);
        const auto loaded = cache.load(key);
        REQUIRE(loaded.kind == IPreviewByteCache::HitKind::Hit);
        CHECK(loaded.payload == body);
    }

    TEST_CASE("missing key is a clean miss, not an error") {
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        CHECK(cache.load("never-stored").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("empty payload is silently skipped") {
        // Empty would be an indistinguishable miss anyway, and writing it
        // would waste an inode. The contract is: we treat it as a no-op.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        cache.store("k", "");
        CHECK(cache.load("k").kind == IPreviewByteCache::HitKind::Miss);
        CHECK(cache.currentSizeBytes() == 0);
    }

    TEST_CASE("hash collision check via sidecar mismatch is treated as a miss") {
        // We fake a collision by writing the same hash file with a wrong
        // sidecar key. Even though the .bin is present, load() must
        // reject it - otherwise we'd serve the wrong card's bytes, which
        // is much worse than re-fetching.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        cache.store("real-key", "REAL");
        // Find the .idx and corrupt the key inside.
        for (const auto& entry : fs::directory_iterator(td.path)) {
            if (entry.path().extension() == ".idx") {
                std::error_code ec;
                fs::remove(entry.path(), ec);
                StdFileSystem io;
                (void)io.writeText(entry.path(), "different-key");
                break;
            }
        }

        CHECK(cache.load("real-key").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("survives an adapter restart over the same directory") {
        // The whole point of the disk tier is persistence. Exercise it by
        // dropping one cache instance and bringing up a new one against
        // the same path - the previously-stored entry must come back.
        TempDir td;
        StdFileSystem fs;
        const std::string key = "ygo|Dark Magician|SDY|6|1E";
        const std::string body(4096, 'D');

        {
            LocalPreviewByteCache writer(fs, td.path);
            writer.store(key, body);
        }
        {
            LocalPreviewByteCache reader(fs, td.path);
            const auto loaded = reader.load(key);
            REQUIRE(loaded.kind == IPreviewByteCache::HitKind::Hit);
            CHECK(loaded.payload == body);
        }
    }

    TEST_CASE("storeNegative round-trips as NegativeHit, not a miss and not a payload") {
        // Negative entries are the "we asked, the upstream said no image"
        // marker. They must round-trip as their own load kind so the
        // service can short-circuit without re-resolving the URL.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        const std::string key = "magic|Bogus Card|lea|";
        cache.storeNegative(key);

        const auto loaded = cache.load(key);
        CHECK(loaded.kind == IPreviewByteCache::HitKind::NegativeHit);
        CHECK(loaded.payload.empty());
        CHECK(cache.currentSizeBytes() == 0);  // negatives don't take real space
    }

    TEST_CASE("negative entries survive an adapter restart") {
        // The "warm restart, same negative answer instantly" property is
        // the whole reason negatives go to disk. Verify it explicitly so
        // it can never silently regress.
        TempDir td;
        StdFileSystem fs;
        const std::string key = "ygo|Truly Missing|SET|999||UE";

        {
            LocalPreviewByteCache writer(fs, td.path);
            writer.storeNegative(key);
        }
        {
            LocalPreviewByteCache reader(fs, td.path);
            CHECK(reader.load(key).kind == IPreviewByteCache::HitKind::NegativeHit);
        }
    }

    TEST_CASE("a later positive store overwrites an earlier negative entry") {
        // Upstream eventually grows a scan: the next positive store has
        // to flip the entry. If the .neg lingered we'd keep returning a
        // NegativeHit even after a successful network resolution, which
        // would defeat the recovery story.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        const std::string key  = "magic|Late|lea|";
        const std::string body = std::string(1024, 'L');

        cache.storeNegative(key);
        REQUIRE(cache.load(key).kind == IPreviewByteCache::HitKind::NegativeHit);

        cache.store(key, body);
        const auto loaded = cache.load(key);
        REQUIRE(loaded.kind == IPreviewByteCache::HitKind::Hit);
        CHECK(loaded.payload == body);
    }

    TEST_CASE("storeNegative for an existing positive entry replaces the bytes") {
        // Symmetric to the previous case: an upstream that *had* a scan
        // and now reports "no image" (e.g. file deleted) flips the entry
        // back to negative. The .bin must go away so it doesn't keep
        // serving stale bytes and doesn't keep wasting cap space.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        const std::string key = "magic|Recall|lea|";
        cache.store(key, std::string(2048, 'R'));
        REQUIRE(cache.currentSizeBytes() > 0);

        cache.storeNegative(key);
        const auto loaded = cache.load(key);
        CHECK(loaded.kind == IPreviewByteCache::HitKind::NegativeHit);
        CHECK(cache.currentSizeBytes() == 0);
    }

    TEST_CASE("collision check applies to negative entries too") {
        // A hash collision must not let a negative entry stored under
        // key A "leak" into a load of key B. We fake the collision by
        // rewriting the .idx sidecar with a different key.
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);

        cache.storeNegative("real-key");
        for (const auto& entry : fs::directory_iterator(td.path)) {
            if (entry.path().extension() == ".idx") {
                std::error_code ec;
                fs::remove(entry.path(), ec);
                StdFileSystem io;
                (void)io.writeText(entry.path(), "different-key");
                break;
            }
        }
        CHECK(cache.load("real-key").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("entry with payload but missing sidecar is treated as miss") {
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);
        cache.store("real-key", "REAL");

        for (const auto& entry : fs::directory_iterator(td.path)) {
            if (entry.path().extension() == ".idx") {
                std::error_code ec;
                fs::remove(entry.path(), ec);
            }
        }

        CHECK(cache.load("real-key").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("entry with negative marker but missing sidecar is treated as miss") {
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);
        cache.storeNegative("real-key");

        for (const auto& entry : fs::directory_iterator(td.path)) {
            if (entry.path().extension() == ".idx") {
                std::error_code ec;
                fs::remove(entry.path(), ec);
            }
        }

        CHECK(cache.load("real-key").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("entry with unreadable payload file is treated as miss") {
        TempDir td;
        StdFileSystem fs;
        LocalPreviewByteCache cache(fs, td.path);
        cache.store("real-key", "REAL");

        for (const auto& entry : fs::directory_iterator(td.path)) {
            if (entry.path().extension() == ".bin") {
                std::error_code ec;
                fs::remove(entry.path(), ec);
                break;
            }
        }

        CHECK(cache.load("real-key").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("evicts oldest entry when the size cap would be exceeded") {
        TempDir td;
        StdFileSystem fs;
        // Cap is just big enough to hold 2 of the 1 KiB payloads but not 3.
        constexpr std::size_t kCap = 2'400;
        LocalPreviewByteCache cache(fs, td.path, kCap);

        const std::string a(1024, 'a');
        const std::string b(1024, 'b');
        const std::string c(1024, 'c');

        cache.store("k-a", a);
        // Make 'a' look definitively older than 'b' regardless of the FS's
        // mtime resolution (some filesystems round to whole seconds, which
        // can otherwise make the test flaky).
        for (const auto& entry : fs::directory_iterator(td.path)) {
            backdate(entry.path(), /*seconds=*/10);
        }

        cache.store("k-b", b);
        cache.store("k-c", c);  // would push us to ~3 KiB; 'a' must go.

        CHECK(cache.load("k-a").kind == IPreviewByteCache::HitKind::Miss);
        CHECK(cache.load("k-b").kind == IPreviewByteCache::HitKind::Hit);
        CHECK(cache.load("k-c").kind == IPreviewByteCache::HitKind::Hit);
        CHECK(cache.currentSizeBytes() <= kCap);
    }

    TEST_CASE("a hit refreshes mtime so the entry is not the next eviction victim") {
        // Without the touch-on-load behavior, an entry that the user
        // re-views constantly would still age out simply because newer
        // entries piled on top. Verify we promote on hit.
        TempDir td;
        StdFileSystem fs;
        constexpr std::size_t kCap = 2'400;
        LocalPreviewByteCache cache(fs, td.path, kCap);

        const std::string a(1024, 'a');
        const std::string b(1024, 'b');
        const std::string c(1024, 'c');

        cache.store("k-a", a);
        cache.store("k-b", b);
        // Backdate both so the upcoming touch on 'a' is unambiguously newer.
        for (const auto& entry : fs::directory_iterator(td.path)) {
            backdate(entry.path(), /*seconds=*/10);
        }

        REQUIRE(cache.load("k-a").kind == IPreviewByteCache::HitKind::Hit);  // touches 'a' to "now".
        cache.store("k-c", c);                                                // forces an eviction.

        // 'a' was the youngest after the touch, so 'b' should be the victim.
        CHECK(cache.load("k-b").kind == IPreviewByteCache::HitKind::Miss);
        CHECK(cache.load("k-a").kind == IPreviewByteCache::HitKind::Hit);
        CHECK(cache.load("k-c").kind == IPreviewByteCache::HitKind::Hit);
    }
}

TEST_SUITE("LocalPreviewByteCache in-memory filesystem failures") {
    TEST_CASE("store is a silent no-op when ensureDirectory fails") {
        ccm::testing::InMemoryFileSystem inner;
        FailingEnsureDirFs fs{inner};
        LocalPreviewByteCache cache(fs, "/cache");

        cache.store("k", "payload");
        CHECK(cache.load("k").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("store rolls back payload when sidecar write fails") {
        ccm::testing::InMemoryFileSystem inner;
        FailingIndexWriteFs fs{inner};
        LocalPreviewByteCache cache(fs, "/cache");

        cache.store("k", "payload");
        CHECK(cache.load("k").kind == IPreviewByteCache::HitKind::Miss);
    }

    TEST_CASE("storeNegative rolls back marker when sidecar write fails") {
        ccm::testing::InMemoryFileSystem inner;
        FailingIndexWriteFs fs{inner};
        LocalPreviewByteCache cache(fs, "/cache");

        cache.storeNegative("k");
        CHECK(cache.load("k").kind == IPreviewByteCache::HitKind::Miss);
    }
}
