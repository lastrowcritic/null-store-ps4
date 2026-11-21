#define PORTAL_HOST
#include "../src/providers.h"
#include "test_source_setup.h"
#include <cassert>
#include <unistd.h>
#include <iostream>

static void expectOne(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAILED: " << message << "\n"; assert(false); }
}

static std::vector<Game> makeGames(const std::string& base, int count, int offset) {
    std::vector<Game> games;
    for (int i = 1; i <= count; ++i) {
        Game game;
        game.id = offset + i;
        game.title = "Game " + std::to_string(i);
        game.url = base + "/games/" + std::to_string(i) + ".html";
        game.image = base + "/art/" + std::to_string(i) + ".jpg";
        games.push_back(game);
    }
    return games;
}

int main() {
    sourceTable().items.clear();
    registerTestSource("one", "https://example.org");
    registerTestSource("two", "https://games2.example.org");

    char temp[] = "build/tmp/nullstore-cache-XXXXXX";
    assert(mkdtemp(temp));
    std::string root = temp;

    auto first = makeGames("https://example.org", 1200, 0);
    auto second = makeGames("https://games2.example.org", 40, 5000000);

    expectOne(saveProviderCache(root, 1, first, true, 1000), "first source cache saved");
    expectOne(saveProviderCache(root, 2, second, true, 1000), "second source cache saved");
    expectOne(providerCached(root, 1).size() == first.size(), "first source cache reloaded");
    expectOne(providerCached(root, 2).size() == second.size(), "second source cache reloaded");
    expectOne(providerCacheFresh(root, 1, 1100), "fresh inside the window");
    expectOne(!providerCacheFresh(root, 1, 1000 + 6 * 60 * 60), "expired after the window");
    expectOne(!providerCacheFresh(root, 1, 999), "clock rollback is not fresh");

    auto merged = mergedCatalog(root + "/catalog.tsv");
    expectOne(merged.size() == first.size() + second.size(), "merged library is the union of caches");

    expectOne(!saveProviderCache(root, 2, first), "wrong-host rows are rejected");
    expectOne(!saveProviderCache(root, 1, {}), "empty cache is rejected");

    auto cached = providerCached(root, 1);
    expectOne(saveProviderCache(root, 1, cached, false), "unstamped resave works");
    expectOne(writeFile(providerCachePath(root, 1), "corrupt"), "cache can be corrupted");
    expectOne(providerCached(root, 1).size() == cached.size(), "checksum recovery restores the cache");

    expectOne(providerCachePath(root, 1).find("catalog-source-one") != std::string::npos, "cache file is named after the source slug");
    expectOne(providerCoverPath(root, 2).find("covers/source-two") != std::string::npos, "cover folder is named after the source slug");

    sourceTable().items.clear();
    expectOne(mergedCatalog(root + "/catalog.tsv").empty(), "no sources means an empty library");
    expectOne(providerCached(root, 1).empty(), "no sources means no cached rows");
    std::cout << "Per-source cache isolation, expiry, clock rollback, checksum recovery and merged library passed.\n";
}
