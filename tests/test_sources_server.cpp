#define PORTAL_HOST
#include "../src/sources_server.h"
#include "../src/sources.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static std::string gFetched;

static bool stubFetch(const std::string& url, std::string& out, size_t, const std::string&) {
    gFetched = url;
    if (url.find("games.example.org") == std::string::npos) return false;
    out = readFile("tests/fixtures/store-listing.html");
    return !out.empty();
}

struct Reply { int status = 0; std::string body; };

static Reply call(int port, const std::string& method, const std::string& target, const std::string& body = "") {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(fd, (sockaddr*)&address, sizeof address) == 0);
    std::string request = method + " " + target + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    assert(send(fd, request.data(), request.size(), 0) == (ssize_t)request.size());
    std::string raw;
    char buffer[4096];
    for (;;) {
        ssize_t got = recv(fd, buffer, sizeof buffer, 0);
        if (got <= 0) break;
        raw.append(buffer, (size_t)got);
    }
    close(fd);
    Reply reply;
    size_t space = raw.find(' ');
    if (space != std::string::npos) reply.status = atoi(raw.c_str() + space + 1);
    size_t split = raw.find("\r\n\r\n");
    if (split != std::string::npos) reply.body = raw.substr(split + 4);
    return reply;
}

int main() {
    char dir[] = "build/tmp/sources-server-XXXXXX";
    assert(mkdtemp(dir));
    std::string root = dir;
    sourceTable().items.clear();

    SourcesServer server(root, stubFetch);
    std::string error;
    assert(server.start(0, error));
    int port = server.port();
    assert(port > 0);
    std::string code = server.code();
    assert(code.size() == 6);

    Reply blocked = call(port, "GET", "/");
    assert(blocked.status == 403);

    Reply page = call(port, "GET", "/" + code);
    assert(page.status == 200 && page.body.find("NULL_store sources") != std::string::npos);

    Reply empty = call(port, "GET", "/api/sources?c=" + code);
    assert(empty.status == 200 && empty.body.find("\"ok\":true") != std::string::npos && empty.body.find("\"sources\":[") != std::string::npos && empty.body.find("games.example.org") == std::string::npos);

    std::string entry = "{\"label\":\"Example\",\"base\":\"https://games.example.org\",\"listing\":\"/games/\",\"item\":\"article.card\",\"title\":\".card-title\"}";
    Reply added = call(port, "POST", "/api/add?c=" + code, entry);
    assert(added.status == 200 && added.body.find("\"ok\":true") != std::string::npos);
    assert(sourceTable().items.size() == 1);
    assert(readFile(root + "/sources.json").find("games.example.org") != std::string::npos);

    Reply tested = call(port, "POST", "/api/test?c=" + code, entry);
    assert(tested.status == 200 && tested.body.find("\"games\":3") != std::string::npos);
    assert(gFetched == "https://games.example.org/games/");

    Reply badEntry = call(port, "POST", "/api/add?c=" + code, "{\"label\":\"Bad\",\"base\":\"http://insecure.test\",\"item\":\".card\"}");
    assert(badEntry.status == 200 && badEntry.body.find("\"ok\":false") != std::string::npos);
    assert(sourceTable().items.size() == 1);

    Reply broken = call(port, "POST", "/api/save?c=" + code, "{");
    assert(broken.status == 200 && broken.body.find("\"ok\":false") != std::string::npos);

    Reply saved = call(port, "POST", "/api/save?c=" + code, "{\"version\":1,\"sources\":[]}");
    assert(saved.status == 200 && saved.body.find("\"ok\":true") != std::string::npos);
    assert(sourceTable().items.empty());

    Reply badCode = call(port, "GET", "/api/sources?c=000000");
    assert(badCode.status == 403);

    Reply missing = call(port, "GET", "/api/nothing?c=" + code);
    assert(missing.status == 404);

    server.stop();
    std::cout << "Setup page: code auth, add, test, save, removal of bad input and unknown routes passed.\n";
}
