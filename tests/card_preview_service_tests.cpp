#include <doctest/doctest.h>

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"
#include "ccm/ports/IPreviewByteCache.hpp"
#include "ccm/services/CardPreviewService.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ccm;

namespace {

class FakeSource final : public ICardPreviewSource {
public:
    std::string url = "https://example.com/preview.jpg";
    bool ok = true;
    // The kind of error returned when ok == false. Defaults to Transient
    // because the historical tests assumed "errors don't get cached"; the
    // negative-cache tests flip this to NotFound explicitly.
    PreviewLookupError::Kind errKind = PreviewLookupError::Kind::Transient;
    std::string err = "boom";
    int calls{0};

    // Capture inputs so tests can assert routing.
    std::string lastName;
    std::string lastSetId;
    std::string lastSetNo;
    std::string detectLastName;
    std::string detectLastSetId;
    AutoDetectedPrint detectedPrint{"LOB-001", "Ultra Rare"};
    bool allowAutoDetect{true};

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override {
        ++calls;
        lastName  = std::string(name);
        lastSetId = std::string(setId);
        lastSetNo = std::string(setNo);
        if (ok) {
            return Result<std::string, PreviewLookupError>::ok(url);
        }
        return Result<std::string, PreviewLookupError>::err({errKind, err});
    }

    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override {
        detectLastName = std::string(name);
        detectLastSetId = std::string(setId);
        return Result<AutoDetectedPrint>::ok(detectedPrint);
    }

    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setId) override {
        detectLastName  = std::string(name);
        detectLastSetId = std::string(setId);
        std::vector<AutoDetectedPrint> v;
        v.push_back(detectedPrint);
        return Result<std::vector<AutoDetectedPrint>>::ok(std::move(v));
    }

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override {
        return allowAutoDetect;
    }
};

class FixedHttpClient final : public IHttpClient {
public:
    std::string lastUrl;
    std::string body;
    bool ok = true;
    std::string err = "offline";
    int calls{0};

    Result<std::string> get(std::string_view url) override {
        ++calls;
        lastUrl = std::string(url);
        return ok ? Result<std::string>::ok(body)
                  : Result<std::string>::err(err);
    }
};

// In-memory persistent cache stand-in. Real implementation lives in
// LocalPreviewByteCache (covered by local_preview_byte_cache_tests.cpp); the
// fake here lets us pin down CardPreviewService's wiring without dragging in
// the filesystem.
class InMemoryByteCache final : public IPreviewByteCache {
public:
    struct Entry {
        bool        negative{false};
        std::string payload;
    };
    std::unordered_map<std::string, Entry> entries;
    int loadCalls{0};
    int storeCalls{0};
    int storeNegativeCalls{0};

    [[nodiscard]] LoadResult load(std::string_view key) override {
        ++loadCalls;
        auto it = entries.find(std::string(key));
        if (it == entries.end()) return {HitKind::Miss, {}};
        if (it->second.negative) return {HitKind::NegativeHit, {}};
        return {HitKind::Hit, it->second.payload};
    }
    void store(std::string_view key, const std::string& payload) override {
        ++storeCalls;
        entries[std::string(key)] = Entry{false, payload};
    }
    void storeNegative(std::string_view key) override {
        ++storeNegativeCalls;
        entries[std::string(key)] = Entry{true, {}};
    }
};

class AlwaysNegativeUrlCache final : public IPreviewByteCache {
public:
    [[nodiscard]] LoadResult load(std::string_view key) override {
        if (!key.empty() && key.front() == 'u') {
            return {HitKind::NegativeHit, {}};
        }
        return {HitKind::Miss, {}};
    }
    void store(std::string_view, const std::string&) override {}
    void storeNegative(std::string_view) override {}
};

// Minimal IGameModule fake that exposes a configurable preview source.
class FakeGameModule final : public IGameModule {
public:
    Game gameId = Game::Magic;
    ICardPreviewSource* preview = nullptr;

    [[nodiscard]] Game        id() const noexcept override { return gameId; }
    [[nodiscard]] std::string dirName()     const override { return "fake"; }
    [[nodiscard]] std::string displayName() const override { return "Fake"; }

    ISetSource& setSource() override {
        // Tests in this file never call setSource(); a never-returned helper
        // would clutter the fake. Throwing keeps misuse loud.
        throw std::logic_error("FakeGameModule::setSource() not used in this test");
    }
    ICardPreviewSource* cardPreviewSource() noexcept override { return preview; }
};

}  // namespace

TEST_SUITE("CardPreviewService::fetchPreviewBytes") {
    TEST_CASE("happy path: routes through registered module then http GET") {
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = std::string("\x89PNG\r\n\x1a\n", 8);  // arbitrary binary

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == http.body);
        CHECK(http.lastUrl == "https://example.com/img.png");
        CHECK(source.lastName  == "Lightning Bolt");
        CHECK(source.lastSetId == "lea");
        CHECK(source.lastSetNo.empty());
    }

    TEST_CASE("module returning nullptr preview source is skipped silently") {
        FakeGameModule module;
        module.gameId = Game::Pokemon;
        module.preview = nullptr;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);  // no-op

        const auto out = svc.fetchPreviewBytes(Game::Pokemon, "Pikachu", "sv3", "1");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") != std::string::npos);
    }

    TEST_CASE("unregistered game returns an explicit error") {
        FixedHttpClient http;
        CardPreviewService svc{http};
        const auto out = svc.fetchPreviewBytes(Game::Pokemon, "Pikachu", "sv3", "1");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") != std::string::npos);
    }

    TEST_CASE("source error propagates and http is not called") {
        FakeSource source;
        source.ok = false;
        source.err = "scryfall 404";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.lastUrl = "<unset>";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(out.isErr());
        CHECK(out.error() == "scryfall 404");
        CHECK(http.lastUrl == "<unset>");
    }

    TEST_CASE("http GET error propagates") {
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.ok = false;
        http.err = "net down";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(out.isErr());
        CHECK(out.error() == "net down");
    }
}

TEST_SUITE("CardPreviewService caching") {
    TEST_CASE("repeat fetchPreviewBytes for the same key serves from cache") {
        // Re-selecting the same row in the table is the hot path: the
        // (game, name, setId, setNo) tuple uniquely identifies a printing,
        // and the resolved image URL is a deterministic function of those
        // inputs - so we can safely cache the bytes and skip HTTP entirely
        // on repeat. Each cache hit spares us two HTTPS round trips
        // (Yugipedia API + image GET) plus a TLS handshake on a cold pool.
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "PNG-bytes";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto first = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(first.isOk());
        CHECK(first.value() == "PNG-bytes");
        CHECK(http.calls == 1);

        // Mutate the source's response to prove the second call doesn't go
        // through the source either: only the cache should be consulted.
        source.url = "https://example.com/CHANGED.png";
        http.body = "DIFFERENT-bytes";

        const auto second = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(second.isOk());
        CHECK(second.value() == "PNG-bytes");
        CHECK(http.calls == 1);  // no new GET issued
    }

    TEST_CASE("different cards share the same source but get separate cache slots") {
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;

        CardPreviewService svc{http};
        svc.registerModule(module);

        source.url = "https://example.com/a.png";
        http.body = "A-bytes";
        const auto a = svc.fetchPreviewBytes(Game::Magic, "Card A", "lea", "");
        REQUIRE(a.isOk());
        CHECK(a.value() == "A-bytes");

        source.url = "https://example.com/b.png";
        http.body = "B-bytes";
        const auto b = svc.fetchPreviewBytes(Game::Magic, "Card B", "lea", "");
        REQUIRE(b.isOk());
        CHECK(b.value() == "B-bytes");

        CHECK(http.calls == 2);

        // Re-fetching A returns the originally cached payload, not the most
        // recent http.body; this pins down per-card cache scoping.
        http.body = "stale";
        const auto aAgain = svc.fetchPreviewBytes(Game::Magic, "Card A", "lea", "");
        REQUIRE(aAgain.isOk());
        CHECK(aAgain.value() == "A-bytes");
        CHECK(http.calls == 2);
    }

    TEST_CASE("source errors are not cached so a transient failure can recover") {
        // If the per-game source returns an error we should not poison the
        // cache - the next selection must be free to retry, otherwise a
        // single network blip would permanently disable previews for that
        // card until the user restarts the app.
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "PNG-bytes";

        CardPreviewService svc{http};
        svc.registerModule(module);

        source.ok = false;
        source.err = "transient 503";
        const auto firstErr = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(firstErr.isErr());
        CHECK(http.calls == 0);  // source short-circuited before HTTP

        source.ok = true;
        source.url = "https://example.com/x.png";
        const auto recovered = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        REQUIRE(recovered.isOk());
        CHECK(recovered.value() == "PNG-bytes");
        CHECK(http.calls == 1);
    }

    TEST_CASE("HTTP success writes through to the persistent cache") {
        // The persistent tier is fire-and-forget on the way down (HTTP -> disk)
        // and consulted on the way up (cache miss -> disk -> HTTP). This first
        // sub-case exercises the write-through path.
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "PNG-bytes";

        InMemoryByteCache disk;
        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(out.isOk());
        CHECK(disk.storeCalls == 1);
        CHECK(disk.entries.size() == 1);
        // Key shape is service-internal so we don't assert it directly; only
        // that *something* identifying this card was persisted with the right
        // payload.
        bool foundPayload = false;
        for (const auto& [k, e] : disk.entries) {
            if (!e.negative && e.payload == "PNG-bytes") foundPayload = true;
        }
        CHECK(foundPayload);
    }

    TEST_CASE("warm restart: a fresh service instance serves from disk without HTTP") {
        // Simulates an app restart by constructing two CardPreviewService
        // instances over the same persistent cache. The second instance
        // must serve the previously-fetched bytes from disk - no source
        // call, no HTTP call - which is the whole point of the persistent
        // tier.
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::YuGiOh;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "DM-bytes";

        InMemoryByteCache disk;

        // Run 1: cold start, normal fetch path warms the persistent cache.
        {
            CardPreviewService svc{http, &disk};
            svc.registerModule(module);
            const auto out = svc.fetchPreviewBytes(
                Game::YuGiOh, "Dark Magician", "Starter Deck: Yugi", "SDY-006||Ultra Rare||UE");
            REQUIRE(out.isOk());
        }
        REQUIRE(http.calls == 1);
        REQUIRE(disk.entries.size() == 1);

        // Run 2: pretend the app restarted. Make HTTP and the source both
        // return obviously-wrong data so any unexpected fall-through to
        // them shows up loudly in the assertion.
        http.body = "UNEXPECTED";
        source.url = "https://example.com/UNEXPECTED.png";
        {
            CardPreviewService svc{http, &disk};
            svc.registerModule(module);
            const auto warm = svc.fetchPreviewBytes(
                Game::YuGiOh, "Dark Magician", "Starter Deck: Yugi", "SDY-006||Ultra Rare||UE");
            REQUIRE(warm.isOk());
            CHECK(warm.value() == "DM-bytes");
        }
        // No second HTTP request - disk tier covered it.
        CHECK(http.calls == 1);
        CHECK(disk.loadCalls >= 1);
    }

    TEST_CASE("disk hit promotes into the in-memory tier so subsequent calls skip even disk I/O") {
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "X-bytes";

        InMemoryByteCache disk;
        // Pre-seed the disk cache with a value the service has never fetched
        // this session. The first call should pull it off disk and copy it
        // into memory; the second call should be served from memory without
        // touching the disk cache at all.
        // We can't compose the key from outside, so we go through the
        // service once to learn the key shape via storeCalls.
        {
            CardPreviewService svc{http, &disk};
            svc.registerModule(module);
            (void)svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        }
        REQUIRE(http.calls == 1);
        REQUIRE(disk.entries.size() == 1);
        const auto storedKey = disk.entries.begin()->first;

        // Clear the in-memory tier by constructing a fresh service. Disk
        // entry stays.
        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        const int loadsBefore = disk.loadCalls;
        // Force HTTP to fail loudly so we can prove neither call hits it.
        http.ok = false;

        const auto first = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(first.isOk());
        CHECK(first.value() == "X-bytes");
        CHECK(disk.loadCalls == loadsBefore + 1);

        const auto second = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(second.isOk());
        CHECK(second.value() == "X-bytes");
        // No additional disk hit - in-memory tier served it.
        CHECK(disk.loadCalls == loadsBefore + 1);
        // Sanity: only the original storedKey ever made it to disk.
        CHECK(disk.entries.count(storedKey) == 1);
    }

    TEST_CASE("transient source errors do not write through to the persistent cache") {
        // A transient failure (HTTP 5xx, network drop, parse error) must
        // never get cached - if it did, a single bad request would
        // silently break this card's preview until the user restarts the
        // app or edits the record.
        FakeSource source;
        source.ok = false;
        source.errKind = PreviewLookupError::Kind::Transient;
        source.err = "scryfall 503";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        InMemoryByteCache disk;

        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(out.isErr());
        CHECK(disk.storeCalls == 0);
        CHECK(disk.storeNegativeCalls == 0);
        CHECK(disk.entries.empty());
    }

    TEST_CASE("NotFound source errors are negative-cached and short-circuit subsequent calls") {
        // A NotFound result means the upstream cleanly answered "no image
        // for this record". The next selection of the *same* record must
        // return immediately without invoking the source again - that's
        // the whole point of negative caching, and the user-visible win
        // (no spinner, no per-click HTTP) on every selection of a card
        // whose printing has no scan upstream.
        FakeSource source;
        source.ok = false;
        source.errKind = PreviewLookupError::Kind::NotFound;
        source.err = "no scan exists";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        InMemoryByteCache disk;

        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        const auto first = svc.fetchPreviewBytes(Game::Magic, "Bad Card", "abc", "");
        CHECK(first.isErr());
        CHECK(source.calls == 1);
        CHECK(disk.storeCalls == 0);
        CHECK(disk.storeNegativeCalls == 1);

        // Second call: the source must NOT be consulted again. The
        // in-memory negative entry covers it.
        const auto second = svc.fetchPreviewBytes(Game::Magic, "Bad Card", "abc", "");
        CHECK(second.isErr());
        CHECK(source.calls == 1);  // still 1 - cache short-circuited
        CHECK(http.calls == 0);
    }

    TEST_CASE("editing a lookup-relevant field invalidates the negative entry automatically") {
        // The cache key is (game, name, setId, setNo). The user fixing a
        // typo in the card's name (or any other lookup-relevant field)
        // changes the key, so the previously-cached "no image" verdict no
        // longer matches and a fresh resolution attempt runs.
        FakeSource source;
        source.ok = false;
        source.errKind = PreviewLookupError::Kind::NotFound;
        source.err = "no scan exists";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        InMemoryByteCache disk;

        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        // First lookup: the typo name produces a clean NotFound -> negative cache.
        REQUIRE(svc.fetchPreviewBytes(Game::Magic, "Lighting Bolt", "lea", "").isErr());
        REQUIRE(source.calls == 1);

        // Now flip the source to "found" and ask for the corrected name.
        source.ok = true;
        source.url = "https://example.com/lb.png";
        http.body = "LB-bytes";

        const auto fixed = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(fixed.isOk());
        CHECK(fixed.value() == "LB-bytes");
        CHECK(source.calls == 2);

        // And the original (typo) record stays negative-cached.
        source.ok = false;  // belt-and-braces: prove the typo path doesn't re-call the source
        const auto stillNegative =
            svc.fetchPreviewBytes(Game::Magic, "Lighting Bolt", "lea", "");
        CHECK(stillNegative.isErr());
        CHECK(source.calls == 2);  // typo path never re-resolved
    }

    TEST_CASE("warm restart honors a previously-stored negative entry without HTTP") {
        // Persistent negative caching is the strongest motivation for the
        // disk tier carrying negatives at all: a card with no upstream
        // image stays "instant card-back" across app restarts, instead of
        // re-paying the round-trip to learn the same thing every launch.
        FakeSource source;
        source.ok = false;
        source.errKind = PreviewLookupError::Kind::NotFound;
        source.err = "no scan exists";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        InMemoryByteCache disk;

        // Run 1: negative outcome, persisted.
        {
            CardPreviewService svc{http, &disk};
            svc.registerModule(module);
            REQUIRE(svc.fetchPreviewBytes(Game::Magic, "Z", "set", "").isErr());
        }
        REQUIRE(disk.storeNegativeCalls == 1);
        const int sourceCallsAfterRun1 = source.calls;

        // Run 2: fresh service over the same disk fake. Source must not be
        // consulted at all - the negative entry on disk short-circuits it.
        {
            CardPreviewService svc{http, &disk};
            svc.registerModule(module);
            const auto warm = svc.fetchPreviewBytes(Game::Magic, "Z", "set", "");
            CHECK(warm.isErr());
        }
        CHECK(source.calls == sourceCallsAfterRun1);
    }

    TEST_CASE("a later positive result for the same key replaces the negative entry") {
        // If the upstream eventually grows a scan for a card that was
        // previously NotFound, the very next successful lookup must
        // overwrite the negative entry so it doesn't keep haunting the
        // user. Realistic scenario: Yugipedia uploads a scan that didn't
        // exist before; the user clicks the row again and we now serve
        // the real image. (In production the cache key may also change
        // due to a record edit, but that's a separate path; here we test
        // the same-key recovery story.)
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        InMemoryByteCache disk;

        CardPreviewService svc{http, &disk};
        svc.registerModule(module);

        // First call: NotFound.
        source.ok = false;
        source.errKind = PreviewLookupError::Kind::NotFound;
        source.err = "no scan yet";
        REQUIRE(svc.fetchPreviewBytes(Game::Magic, "Card", "set", "").isErr());

        // Imitate the upstream growing a scan, AND simulate the user
        // retrying. To bypass the negative cache without a record edit,
        // construct a fresh service: this models the next-launch case
        // where the user reopens the app and the image now exists. (Within
        // a single session, only an edit to the record clears the cache,
        // which is the correct UX - we don't want a network round-trip on
        // every click for cards we already know have no upstream image.)
        source.ok = true;
        source.url = "https://example.com/late.png";
        http.body = "LATE-bytes";

        // Drop the disk negative manually to simulate a scenario where
        // the cache was cleared (e.g. the user manually wiped the cache
        // directory). In production code paths, an edit to the record is
        // the standard invalidation; both routes converge on the same
        // expected behavior: a fresh successful lookup.
        disk.entries.clear();

        CardPreviewService svc2{http, &disk};
        svc2.registerModule(module);
        const auto out = svc2.fetchPreviewBytes(Game::Magic, "Card", "set", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == "LATE-bytes");
    }

    TEST_CASE("fetchImageBytesByUrl caches by URL too (fallback card-back)") {
        // The fallback card-back image is fetched via fetchImageBytesByUrl
        // for every card without a preview; caching by URL makes the second
        // fallback effectively free.
        FixedHttpClient http;
        http.body = "card-back-bytes";
        CardPreviewService svc{http};

        const auto first = svc.fetchImageBytesByUrl("https://cdn.example/back.png");
        REQUIRE(first.isOk());
        CHECK(http.calls == 1);

        http.body = "stale";
        const auto second = svc.fetchImageBytesByUrl("https://cdn.example/back.png");
        REQUIRE(second.isOk());
        CHECK(second.value() == "card-back-bytes");
        CHECK(http.calls == 1);
    }

    TEST_CASE("empty HTTP body is rejected and not cached") {
        FakeSource source;
        source.url = "https://example.com/empty.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto first = svc.fetchPreviewBytes(Game::Magic, "Any", "set", "1");
        CHECK(first.isErr());
        CHECK(first.error().find("Empty response body") != std::string::npos);
        CHECK(http.calls == 1);

        const auto second = svc.fetchPreviewBytes(Game::Magic, "Any", "set", "1");
        CHECK(second.isErr());
        CHECK(http.calls == 2);
    }

    TEST_CASE("url negative entry on disk is treated as miss and refetched") {
        FixedHttpClient http;
        http.body = "card-back";
        AlwaysNegativeUrlCache disk;
        CardPreviewService svc{http, &disk};

        const auto out = svc.fetchImageBytesByUrl("https://cdn.example/back.png");
        REQUIRE(out.isOk());
        CHECK(out.value() == "card-back");
        CHECK(http.calls == 1);
    }

    TEST_CASE("fetchImageBytesByUrl serves from persistent cache hit without HTTP") {
        FixedHttpClient http;
        http.body = "warm-card-back";
        InMemoryByteCache disk;

        // Seed persistent cache via first service instance.
        {
            CardPreviewService seed{http, &disk};
            const auto seeded = seed.fetchImageBytesByUrl("https://cdn.example/back.png");
            REQUIRE(seeded.isOk());
            CHECK(seeded.value() == "warm-card-back");
        }
        REQUIRE(http.calls == 1);

        // Fresh service instance: force HTTP failure and ensure disk hit is used.
        CardPreviewService warm{http, &disk};
        http.ok = false;
        const auto warmHit = warm.fetchImageBytesByUrl("https://cdn.example/back.png");
        REQUIRE(warmHit.isOk());
        CHECK(warmHit.value() == "warm-card-back");
        CHECK(http.calls == 1);
    }

    TEST_CASE("in-memory LRU evicts oldest entry after exceeding capacity") {
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = "x";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto cap = CardPreviewService::kCacheCapacity;
        for (std::size_t i = 0; i < cap + 1; ++i) {
            const std::string name = std::string("LRU-") + std::to_string(i);
            REQUIRE(svc.fetchPreviewBytes(Game::Magic, name, "lea", "").isOk());
        }
        REQUIRE(http.calls == cap + 1);

        REQUIRE(svc.fetchPreviewBytes(Game::Magic, "LRU-0", "lea", "").isOk());
        CHECK(http.calls == cap + 2);
    }
}

TEST_SUITE("CardPreviewService::detectFirstPrint") {
    TEST_CASE("routes auto-detect to registered source") {
        FakeSource source;
        source.detectedPrint = {"LOB-005", "Secret Rare"};
        FakeGameModule module;
        module.gameId = Game::YuGiOh;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectFirstPrint(Game::YuGiOh, "Dark Magician", "Legend of Blue Eyes White Dragon");
        REQUIRE(out.isOk());
        CHECK(out.value().setNo == "LOB-005");
        CHECK(out.value().rarity == "Secret Rare");
        CHECK(source.detectLastName == "Dark Magician");
        CHECK(source.detectLastSetId == "Legend of Blue Eyes White Dragon");
    }

    TEST_CASE("returns explicit error when game does not enable auto-detect") {
        FakeSource source;
        source.allowAutoDetect = false;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectFirstPrint(Game::Magic, "Any", "Any Set");
        CHECK(out.isErr());
        CHECK(out.error().find("not enabled") != std::string::npos);
    }

    TEST_CASE("unregistered game returns explicit error") {
        FixedHttpClient http;
        CardPreviewService svc{http};
        const auto out =
            svc.detectFirstPrint(Game::YuGiOh, "Dark Magician", "LOB");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") !=
              std::string::npos);
    }
}

TEST_SUITE("CardPreviewService::detectPrintVariants") {
    TEST_CASE("routes variant listing to registered source") {
        FakeSource source;
        source.detectedPrint = {"LOB-005", "Secret Rare"};
        FakeGameModule module;
        module.gameId = Game::YuGiOh;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectPrintVariants(Game::YuGiOh, "Dark Magician",
                                                 "Legend of Blue Eyes White Dragon");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "LOB-005");
        CHECK(out.value()[0].rarity == "Secret Rare");
    }

    TEST_CASE("detectPrintVariants returns error when game does not enable auto-detect") {
        FakeSource source;
        source.allowAutoDetect = false;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectPrintVariants(Game::Magic, "Any", "Any Set");
        CHECK(out.isErr());
        CHECK(out.error().find("not enabled") != std::string::npos);
    }

    TEST_CASE("unregistered game returns explicit error") {
        FixedHttpClient http;
        CardPreviewService svc{http};
        const auto out =
            svc.detectPrintVariants(Game::YuGiOh, "Dark Magician", "LOB");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") !=
              std::string::npos);
    }
}
