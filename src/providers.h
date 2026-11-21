#pragma once
#include "catalog.h"
#include "source_model.h"
#include "queue_store.h"
#include <ctime>
#include <sys/stat.h>

// Per-source cache files. Named after the source slug so that reordering or
// renaming other sources never strands a user's cached library.
inline std::string providerSlug(int source) {
    const Source* item = sourceTable().at(source);
    std::string slug = item ? sourceSlug(item->id) : "";
    if (slug.empty()) slug = "source-" + std::to_string(source);
    return slug;
}

inline std::string providerCachePath(const std::string& root, int source) {
    return root + "/catalog-source-" + providerSlug(source) + ".tsv";
}

inline std::vector<Game> providerCached(const std::string& root, int source) {
    auto games = loadCatalog(loadQueue(providerCachePath(root, source)));
    if (games.empty()) return {};
    for (auto& g : games) if (sourceForURL(g.url) != source) return {};
    return games;
}

inline bool saveProviderCache(const std::string& root, int source, const std::vector<Game>& games, bool stamp = true, time_t now = time(0)) {
    if (games.empty()) return false;
    for (auto& g : games) if (sourceForURL(g.url) != source) return false;
    if (!saveQueue(providerCachePath(root, source), serialize(games))) return false;
    return !stamp || durableReplace(providerCachePath(root, source) + ".time", std::to_string((long long)now));
}

inline bool providerCacheFresh(const std::string& root, int source, time_t now = time(0)) {
    auto timestamp = strtoll(readFile(providerCachePath(root, source) + ".time").c_str(), 0, 10);
    return timestamp > 0 && now >= timestamp && now - timestamp < 6 * 60 * 60 && !providerCached(root, source).empty();
}

inline std::string providerCoverPath(const std::string& root, int source) {
    return root + "/covers/source-" + providerSlug(source);
}

// The library is whatever the user's sources produced: their caches first, then
// the legacy combined cache from earlier builds. Nothing is bundled.
inline std::vector<Game> mergedCatalog(const std::string& cacheRoot) {
    auto slash = cacheRoot.find_last_of('/');
    auto root = slash == std::string::npos ? std::string(".") : cacheRoot.substr(0, slash);
    auto legacy = loadCatalog(loadQueue(cacheRoot));
    std::vector<Game> out;
    for (int source = 1; source <= (int)sourceTable().items.size(); ++source) {
        auto part = providerCached(root, source);
        if (part.empty()) for (auto& g : legacy) if (sourceForURL(g.url) == source) part.push_back(g);
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}
