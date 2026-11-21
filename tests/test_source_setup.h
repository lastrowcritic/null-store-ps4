#pragma once
#include "../src/source_model.h"
#include <string>

// Registers a generic test source so parser tests never depend on a real site.
inline void registerTestSource(const std::string& id, const std::string& base) {
    Source source;
    source.id = id;
    source.label = id;
    source.base = base;
    source.listing = "/";
    source.pageListing = "/page/{page}/";
    source.maxPages = 2;
    source.shape.item = ".card";
    source.shape.title = ".title";
    source.shape.imageAttributes.push_back("src");
    source.shape.linkClasses.push_back("dl__btn");
    source.shape.linkClasses.push_back("download_button_like_stat");
    source.shape.linkMarkers.push_back("do=download");
    source.shape.linkMarkers.push_back(".torrent");
    sourceTable().items.push_back(source);
}
