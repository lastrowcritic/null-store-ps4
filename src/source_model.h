#pragma once
// Runtime source table. NULL_store ships no catalogue and no built-in game
// sites: every source comes from sources.json, which the user edits from the
// console or from the phone setup page.
//
// A source declares where its listing lives and how its cards look. The app
// contains no per-site parsers; the shape below is the whole vocabulary.
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>

struct SourceShape {
    // Selector for one repeated card. "tag", ".class", "tag.class" or "#id".
    std::string item;
    // Selectors for text inside the card, resolved with the same syntax.
    std::string title;
    std::string size;
    // Where a card's artwork lives: attribute names tried in order.
    std::vector<std::string> imageAttributes;
    // A card anchor is a download when it carries one of these classes...
    std::vector<std::string> linkClasses;
    // ...or when the resolved URL contains one of these markers.
    std::vector<std::string> linkMarkers;
    // "attr=value" pairs that mark a gated, paid or delayed link.
    std::vector<std::string> blocked;
};

struct Source {
    std::string id;                        // slug, used for cache file names
    std::string label;                     // shown in the UI
    std::string base;                      // "https://host", optionally with a port
    std::string listing;                   // path for page 1
    std::string pageListing;               // path template for page N, "{page}"
    int maxPages = 20;
    bool enabled = true;
    SourceShape shape;
};

class SourceTable {
public:
    std::vector<Source> items;
    // 1-based index of the source owning this URL, 0 when unowned.
    int indexForURL(const std::string& url) const;
    int indexForId(const std::string& id) const;
    const Source* at(int index) const;
    std::string origin(int index) const;
    std::string label(int index) const;
    std::string listingURL(int index, int page) const;
    std::string resolve(int index, const std::string& reference) const;
    bool owns(int index, const std::string& url) const;
    bool empty() const { return items.empty(); }
};

inline SourceTable& sourceTable() {
    static SourceTable table;
    return table;
}

inline std::string urlHost(const std::string& url) {
    size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "";
    size_t start = scheme + 3;
    size_t end = url.find_first_of("/?#", start);
    return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

inline std::string urlOrigin(const std::string& url) {
    size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "";
    size_t start = scheme + 3;
    size_t end = url.find_first_of("/?#", start);
    return end == std::string::npos ? url : url.substr(0, end);
}

inline std::string trimmedOrigin(std::string base) {
    while (!base.empty() && base.back() == '/') base.pop_back();
    for (char& c : base) c = (char)tolower((unsigned char)c);
    return base;
}

inline bool sameHost(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    if (a.size() > 4 && a.compare(0, 4, "www.") == 0) return a.substr(4) == b;
    if (b.size() > 4 && b.compare(0, 4, "www.") == 0) return a == b.substr(4);
    return false;
}

inline std::string sourceSlug(const std::string& raw) {
    std::string out;
    for (size_t i = 0; i < raw.size() && out.size() < 32; ++i) {
        unsigned char c = (unsigned char)raw[i];
        if (isalnum(c)) out += (char)tolower(c);
        else if (c == '-' || c == '_' || c == ' ') { if (!out.empty() && out.back() != '-') out += '-'; }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

// Stable identity for a game that has no numeric id in its URL. FNV-1a, 31 bits,
// never zero so it can be used as a map/favourite key.
inline int stableGameId(const std::string& url) {
    unsigned hash = 2166136261u;
    for (size_t i = 0; i < url.size(); ++i) {
        hash ^= (unsigned char)url[i];
        hash *= 16777619u;
    }
    unsigned value = hash & 0x7fffffffu;
    return value ? (int)value : 1;
}

inline int SourceTable::indexForURL(const std::string& url) const {
    if (url.compare(0, 8, "https://") != 0) return 0;
    std::string host = urlHost(url);
    for (size_t i = 0; i < items.size(); ++i) {
        if (!items[i].enabled) continue;
        if (sameHost(host, urlHost(items[i].base))) return (int)i + 1;
    }
    return 0;
}

inline int SourceTable::indexForId(const std::string& id) const {
    for (size_t i = 0; i < items.size(); ++i) if (items[i].id == id) return (int)i + 1;
    return 0;
}

inline const Source* SourceTable::at(int index) const {
    if (index < 1 || index > (int)items.size()) return 0;
    return &items[index - 1];
}

inline std::string SourceTable::origin(int index) const {
    const Source* source = at(index);
    return source ? trimmedOrigin(source->base) : "";
}

inline std::string SourceTable::label(int index) const {
    const Source* source = at(index);
    if (!source) return "";
    return source->label.empty() ? source->id : source->label;
}

inline std::string SourceTable::listingURL(int index, int page) const {
    const Source* source = at(index);
    if (!source) return "";
    std::string origin = trimmedOrigin(source->base);
    std::string path = source->listing.empty() ? "/" : source->listing;
    if (page > 1) {
        if (source->pageListing.empty()) return "";
        path = source->pageListing;
        std::string token = "{page}";
        size_t at = path.find(token);
        if (at == std::string::npos) return "";
        path.replace(at, token.size(), std::to_string(page));
    }
    if (!path.empty() && path[0] == '/') return origin + path;
    return origin + "/" + path;
}

inline std::string SourceTable::resolve(int index, const std::string& reference) const {
    if (reference.empty()) return "";
    if (reference.compare(0, 2, "//") == 0) return "https:" + reference;
    if (reference.find("://") != std::string::npos) return reference;
    std::string origin = this->origin(index);
    if (origin.empty()) return "";
    if (reference[0] == '/') return origin + reference;
    if (reference[0] == '?' || reference[0] == '#') return origin + "/" + reference;
    return origin + "/" + reference;
}

inline bool SourceTable::owns(int index, const std::string& url) const {
    return index >= 1 && indexForURL(url) == index;
}

// Compatibility shims used across the app and the service.
inline int sourceForURL(const std::string& url) { return sourceTable().indexForURL(url); }
inline bool sourceURL(const std::string& url) { return sourceForURL(url) != 0; }
inline std::string sourceOrigin(int source) { return sourceTable().origin(source); }
inline std::string sourceName(int source) { return sourceTable().label(source); }
