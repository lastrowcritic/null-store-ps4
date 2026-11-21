#define PORTAL_HOST
#include "../src/sources.h"
#include <cassert>
#include <iostream>

static void expectOne(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAILED: " << message << "\n"; assert(false); }
}

int main() {
    SourceTable table;
    std::string error;
    expectOne(parseSourcesJSON(readFile("tests/fixtures/sources.json"), table, error), error.c_str());
    expectOne(table.items.size() == 1, "one source parsed");
    expectOne(table.items[0].id == "example", "slug kept");
    expectOne(table.items[0].shape.imageAttributes.size() == 2, "image attribute defaults kept");
    expectOne(table.items[0].shape.blocked.size() == 1, "blocked rule parsed");
    sourceTable() = table;

    expectOne(sourceForURL("https://games.example.org/games/1.html") == 1, "own host recognised");
    expectOne(sourceForURL("https://www.games.example.org/games/1.html") == 1, "www host recognised");
    expectOne(sourceForURL("https://elsewhere.example/games/1.html") == 0, "foreign host rejected");
    expectOne(sourceForURL("http://games.example.org/games/1.html") == 0, "plain http rejected");
    expectOne(sourceForURL("https://games.example.org.attacker.test/x") == 0, "lookalike host rejected");
    expectOne(sourceURL("https://games.example.org/games/1.html"), "sourceURL agrees with table");

    expectOne(table.listingURL(1, 1) == "https://games.example.org/games/", "page one listing");
    expectOne(table.listingURL(1, 3) == "https://games.example.org/games/page/3/", "later page listing");
    Source single = table.items[0];
    single.pageListing.clear();
    sourceTable().items[0] = single;
    expectOne(sourceTable().listingURL(1, 2).empty(), "no page template means no later pages");
    sourceTable() = table;

    expectOne(table.resolve(1, "/art/1.jpg") == "https://games.example.org/art/1.jpg", "absolute path resolved");
    expectOne(table.resolve(1, "//cdn.example.org/a.jpg") == "https://cdn.example.org/a.jpg", "protocol-relative resolved");
    expectOne(table.resolve(1, "https://elsewhere.example/a.jpg") == "https://elsewhere.example/a.jpg", "absolute URL kept");

    auto games = parseListing(readFile("tests/fixtures/store-listing.html"), 1);
    expectOne(games.size() == 3, "offsite cards are skipped, artwork-less cards are kept");
    expectOne(games[0].title == "First Game", "title from selector");
    expectOne(games[0].url == "https://games.example.org/games/101-first-game.html", "card URL resolved");
    expectOne(games[0].image == "https://games.example.org/art/101.jpg", "data-src preferred");
    expectOne(games[0].size == "1.2 GB", "size from selector");
    expectOne(games[1].title == "Second Game & Friends", "entities decoded");
    expectOne(games[1].image == "https://games.example.org/art/102.jpg", "src fallback");
    expectOne(games[2].title == "No Artwork" && games[2].image.empty(), "card without artwork still lists");
    expectOne(games[0].id != games[1].id, "ids differ");
    expectOne(games[0].id == stableGameId(games[0].url), "id is stable for a URL");

    auto again = parseListing(readFile("tests/fixtures/store-listing.html"), 1);
    expectOne(again.size() == games.size() && again[0].id == games[0].id, "repeat parse is stable");

    auto links = parseDownloadLinks(readFile("tests/fixtures/store-detail.html"), 1);
    expectOne(links.size() == 2, "locked, offsite, plain and duplicate anchors dropped");
    expectOne(links[0].url == "https://games.example.org/files/first-game.torrent", "class link kept");
    expectOne(links[1].url == "https://games.example.org/index.php?do=download&id=101", "marker link kept");
    expectOne(links[1].title == "Mirror", "anchor text becomes the label");

    std::string round = sourcesToJSON(table);
    SourceTable reloaded;
    expectOne(parseSourcesJSON(round, reloaded, error), error.c_str());
    expectOne(reloaded.items.size() == table.items.size(), "round trip keeps count");
    expectOne(reloaded.items[0].shape.linkClasses == table.items[0].shape.linkClasses, "round trip keeps classes");
    expectOne(reloaded.items[0].base == table.items[0].base, "round trip keeps base");

    SourceTable empty;
    expectOne(!parseSourcesJSON("{", empty, error), "truncated JSON rejected");
    expectOne(!parseSourcesJSON("{\"sources\":{}}", empty, error), "non-array sources rejected");
    expectOne(!parseSourcesJSON("{\"sources\":[{\"base\":\"http://x.test\",\"shape\":{\"item\":\".c\"}}]}", empty, error), "http base rejected");
    expectOne(!parseSourcesJSON("{\"sources\":[{\"base\":\"https://x.test\"}]}", empty, error), "missing shape rejected");
    expectOne(parseSourcesJSON("{\"sources\":[]}", empty, error), "empty source list allowed");
    expectOne(empty.items.empty(), "empty table stays empty");

    JValue parsed;
    expectOne(jsonParse("{\"a\":[1,true,\"x\\u00e9\\n\"],\"b\":{\"c\":null}}", parsed), "json parser accepts mixed values");
    expectOne(parsed.list("a") && parsed.list("a")->items.size() == 3, "json array parsed");
    expectOne(parsed.list("a")->items[2].text == "x\xc3\xa9\n", "json unicode and escape decoded");

    sourceTable().items.clear();
    expectOne(parseListing(readFile("tests/fixtures/store-listing.html"), 1).empty(), "no sources means no games");
    std::cout << "Source model, generic listing, download detection, JSON round trip and rejection paths passed.\n";
}
