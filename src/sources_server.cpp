#include "sources_server.h"
#include "sources.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cerrno>

namespace {

std::string lowerText(std::string value) {
    for (char& c : value) c = (char)tolower((unsigned char)c);
    return value;
}

std::string urlDecode(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            char hex[3] = {value[i + 1], value[i + 2], 0};
            out += (char)strtol(hex, 0, 16);
            i += 2;
        } else if (value[i] == '+') {
            out += ' ';
        } else {
            out += value[i];
        }
    }
    return out;
}

std::string queryValue(const std::string& query, const std::string& key) {
    size_t at = 0;
    while (at < query.size()) {
        size_t end = query.find('&', at);
        if (end == std::string::npos) end = query.size();
        std::string pair = query.substr(at, end - at);
        size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key) return urlDecode(pair.substr(eq + 1));
        at = end + 1;
    }
    return "";
}

std::string httpResponse(const std::string& status, const std::string& type, const std::string& body) {
    return "HTTP/1.1 " + status + "\r\nContent-Type: " + type +
           "\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n" + body;
}

std::string setupPage(const std::string& code) {
    std::string page = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NULL_store sources</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;padding:16px;background:#0b1a14;color:#e8f5ee}
h1{font-size:1.25rem;margin:0 0 4px}p.lead{margin:0 0 16px;color:#9fc4b2;font-size:.9rem}
.card{background:#122a20;border:1px solid #1f4436;border-radius:12px;padding:14px;margin:0 0 12px}
label{display:block;font-size:.8rem;color:#9fc4b2;margin:10px 0 4px}
input{width:100%;box-sizing:border-box;padding:12px;border-radius:8px;border:1px solid #2a5643;background:#0b1a14;color:#e8f5ee;font-size:1rem}
button{min-height:44px;padding:10px 16px;border-radius:10px;border:0;background:#2ecc8f;color:#06231a;font-weight:600;font-size:1rem;margin-top:12px}
button.ghost{background:#1f4436;color:#e8f5ee}
.hint{font-size:.8rem;color:#84a898;margin-top:8px}
#log{margin-top:12px;font-size:.85rem;white-space:pre-wrap}
.ok{color:#5ee6a8}.bad{color:#ff8a8a}
</style></head><body>
<h1>NULL_store sources</h1>
<p class="lead">Saved on your console. Press OPTIONS in the app to refresh the library.</p>
<div class="card" id="list"></div>
<div class="card">
  <label>Name</label><input id="label" placeholder="My store">
  <label>Site address</label><input id="base" placeholder="https://games.example.org">
  <label>Listing path for page 1</label><input id="listing" placeholder="/games/">
  <label>Listing path for later pages (optional)</label><input id="pageListing" placeholder="/games/page/{page}/">
  <label>Card element</label><input id="item" placeholder="article.card">
  <label>Title element inside the card</label><input id="title" placeholder=".card-title">
  <label>Download button classes (comma separated)</label><input id="linkClasses" placeholder="download-button">
  <button id="test" class="ghost">Test</button>
  <button id="add">Add source</button>
  <div id="log"></div>
</div>
<div class="card">
  <label>Full file (advanced)</label>
  <textarea id="raw" style="width:100%;height:180px;box-sizing:border-box;padding:10px;border-radius:8px;border:1px solid #2a5643;background:#0b1a14;color:#e8f5ee;font-family:ui-monospace,monospace;font-size:.85rem"></textarea>
  <button id="save">Save file</button>
</div>
<script>
const code=location.pathname.split('/').filter(Boolean).pop();
const log=(m,ok)=>{const e=document.getElementById('log');e.textContent=m;e.className=ok?'ok':'bad'};
const api=(path,body)=>{
  const opt=body?{method:'POST',headers:{'Content-Type':'application/json'},body}:{};
  return fetch(path+'?c='+encodeURIComponent(code),opt).then(r=>r.json());
};
function render(list){
  const box=document.getElementById('list');
  if(!list.length){box.innerHTML='<em>No sources yet.</em>';return}
  box.innerHTML=list.map((s,i)=>'<div style="display:flex;gap:8px;align-items:center;padding:6px 0;border-bottom:1px solid #1f4436"><span style="flex:1">'+s.label+'<br><small style="color:#84a898">'+s.base+'</small></span><button class="ghost" data-i="'+i+'">Remove</button></div>').join('');
  box.querySelectorAll('button').forEach(b=>b.onclick=()=>api('/api/remove',JSON.stringify({index:+b.dataset.i})).then(load));
}
function load(){
  api('/api/sources').then(d=>{
    render(d.sources||[]);
    document.getElementById('raw').value=JSON.stringify({version:1,sources:d.sources||[]},null,2);
    if(!d.ok)log(d.error||'Could not load',false);
  }).catch(()=>log('Console not reachable',false));
}
document.getElementById('add').onclick=()=>{
  const body=JSON.stringify({
    label:document.getElementById('label').value,
    base:document.getElementById('base').value,
    listing:document.getElementById('listing').value||'/',
    pageListing:document.getElementById('pageListing').value,
    item:document.getElementById('item').value,
    title:document.getElementById('title').value,
    linkClasses:document.getElementById('linkClasses').value.split(',').map(s=>s.trim()).filter(Boolean)
  });
  api('/api/add',body).then(d=>{log(d.ok?'Added':'Could not add: '+(d.error||''),d.ok);if(d.ok)load()});
};
document.getElementById('test').onclick=()=>{
  log('Testing…',true);
  const body=JSON.stringify({
    label:document.getElementById('label').value,
    base:document.getElementById('base').value,
    listing:document.getElementById('listing').value||'/',
    pageListing:document.getElementById('pageListing').value,
    item:document.getElementById('item').value,
    title:document.getElementById('title').value,
    linkClasses:document.getElementById('linkClasses').value.split(',').map(s=>s.trim()).filter(Boolean)
  });
  api('/api/test',body).then(d=>log(d.ok?('Page 1 parsed. '+d.games+' games found.'):('Test failed: '+(d.error||'')),d.ok));
};
document.getElementById('save').onclick=()=>{
  api('/api/save',document.getElementById('raw').value).then(d=>{log(d.ok?'Saved':'Could not save: '+(d.error||''),d.ok);if(d.ok)load()});
};
load();
</script></body></html>)HTML";
    return page;
}

// Accepts the flat fields the phone form sends and nests them under "shape",
// so the form stays simple while the file keeps one canonical layout.
std::string entryToSourceJSON(const std::string& body) {
    JValue value;
    if (!jsonParse(body, value) || value.type != JValue::Object) return body;
    if (value.get("shape")) return body;
    std::vector<std::string> images = stringList(value.list("imageAttributes"));
    if (images.empty()) { images.push_back("data-src"); images.push_back("src"); }
    std::string out = "{";
    out += "\"id\":" + jsonEscape(value.str("id")) + ",";
    out += "\"label\":" + jsonEscape(value.str("label")) + ",";
    out += "\"base\":" + jsonEscape(value.str("base")) + ",";
    out += "\"listing\":" + jsonEscape(value.str("listing", "/")) + ",";
    out += "\"pageListing\":" + jsonEscape(value.str("pageListing")) + ",";
    out += "\"maxPages\":" + std::to_string(value.integer("maxPages", 20)) + ",";
    out += std::string("\"enabled\":") + (value.boolean("enabled", true) ? "true" : "false") + ",";
    out += "\"shape\":{";
    out += "\"item\":" + jsonEscape(value.str("item")) + ",";
    out += "\"title\":" + jsonEscape(value.str("title")) + ",";
    out += "\"size\":" + jsonEscape(value.str("size")) + ",";
    out += "\"imageAttributes\":" + shapeListJSON(images) + ",";
    out += "\"linkClasses\":" + shapeListJSON(stringList(value.list("linkClasses"))) + ",";
    out += "\"linkMarkers\":" + shapeListJSON(stringList(value.list("linkMarkers"))) + ",";
    out += "\"blocked\":" + shapeListJSON(stringList(value.list("blocked")));
    out += "}}";
    return out;
}

static std::string localLanAddressImpl() {
    ifaddrs* list = 0;
    if (getifaddrs(&list) != 0) return "";
    std::string found;
    for (ifaddrs* item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        sockaddr_in* address = (sockaddr_in*)item->ifa_addr;
        unsigned host = ntohl(address->sin_addr.s_addr);
        if ((host >> 24) == 169 && ((host >> 16) & 0xff) == 254) continue;
        char text[32];
        if (!inet_ntop(AF_INET, &address->sin_addr, text, sizeof text)) continue;
        found = text;
        break;
    }
    freeifaddrs(list);
    return found;
}

} // namespace

std::string localLanAddress() { return localLanAddressImpl(); }

SourcesServer::SourcesServer(std::string dataRoot, Fetch fetchFunction)
    : root(std::move(dataRoot)), fetch(fetchFunction), listenFd(-1), boundPort(0), running(false) {
    pthread_mutex_init(&lock, 0);
    unsigned seed = (unsigned)time(0) ^ ((unsigned)getpid() << 16) ^ (unsigned)(size_t)this;
    sessionCode.clear();
    const char* digits = "0123456789";
    for (int i = 0; i < 6; ++i) {
        seed = seed * 1103515245u + 12345u;
        sessionCode += digits[(seed >> 16) % 10];
    }
}

SourcesServer::~SourcesServer() {
    stop();
    pthread_mutex_destroy(&lock);
}

std::string SourcesServer::urlFor(const std::string& address) const {
    if (boundPort <= 0) return "";
    return "http://" + address + ":" + std::to_string(boundPort) + "/" + sessionCode;
}

bool SourcesServer::start(int port, std::string& error) {
    if (running) return true;
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) { error = "could not create the setup socket"; return false; }
    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);
    if (bind(listenFd, (sockaddr*)&address, sizeof address) != 0) {
        error = "port " + std::to_string(port) + " is already in use";
        close(listenFd);
        listenFd = -1;
        return false;
    }
    if (listen(listenFd, 4) != 0) {
        error = "could not listen for the phone";
        close(listenFd);
        listenFd = -1;
        return false;
    }
    sockaddr_in actual;
    socklen_t length = sizeof actual;
    boundPort = port;
    if (getsockname(listenFd, (sockaddr*)&actual, &length) == 0) boundPort = ntohs(actual.sin_port);
    running = true;
    if (pthread_create(&thread, 0, entry, this) != 0) {
        running = false;
        close(listenFd);
        listenFd = -1;
        error = "could not start the setup thread";
        return false;
    }
    error.clear();
    return true;
}

void SourcesServer::stop() {
    if (!running) return;
    running = false;
    pthread_join(thread, 0);
    if (listenFd >= 0) close(listenFd);
    listenFd = -1;
}

void* SourcesServer::entry(void* self) {
    static_cast<SourcesServer*>(self)->loop();
    return 0;
}

void SourcesServer::loop() {
    while (running) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(listenFd, &set);
        timeval wait = {0, 250000};
        int ready = select(listenFd + 1, &set, 0, 0, &wait);
        if (ready < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        if (ready == 0) continue;
        int client = accept(listenFd, 0, 0);
        if (client < 0) continue;
        handle(client);
        close(client);
    }
}

void SourcesServer::handle(int client) {
    std::string request;
    char buffer[2048];
    size_t headerEnd = std::string::npos;
    while (request.size() < 64 * 1024) {
        ssize_t got = recv(client, buffer, sizeof buffer, 0);
        if (got <= 0) break;
        request.append(buffer, (size_t)got);
        headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos) break;
    }
    if (headerEnd == std::string::npos) {
        std::string reply = httpResponse("400 Bad Request", "text/plain", "bad request");
        send(client, reply.data(), reply.size(), 0);
        return;
    }
    size_t lineEnd = request.find("\r\n");
    std::string requestLine = request.substr(0, lineEnd);
    size_t firstSpace = requestLine.find(' ');
    size_t secondSpace = requestLine.find(' ', firstSpace + 1);
    std::string method = requestLine.substr(0, firstSpace);
    std::string target = firstSpace == std::string::npos ? "/" : requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string path = target;
    std::string query;
    size_t question = target.find('?');
    if (question != std::string::npos) {
        path = target.substr(0, question);
        query = target.substr(question + 1);
    }
    size_t contentLength = 0;
    std::string headers = lowerText(request.substr(0, headerEnd));
    size_t found = headers.find("content-length:");
    if (found != std::string::npos) contentLength = (size_t)strtoul(headers.c_str() + found + 15, 0, 10);
    if (contentLength > 1 << 20) contentLength = 0;
    std::string body = request.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        ssize_t got = recv(client, buffer, sizeof buffer, 0);
        if (got <= 0) break;
        body.append(buffer, (size_t)got);
    }
    if (body.size() > contentLength) body.resize(contentLength);

    std::string supplied = queryValue(query, "c");
    if (supplied.empty() && path.size() > 1) {
        size_t slash = path.find_last_of('/');
        if (slash != std::string::npos && slash + 1 < path.size()) supplied = path.substr(slash + 1);
    }
    bool authorised = !sessionCode.empty() && supplied == sessionCode;
    if (!authorised) {
        std::string reply = httpResponse("403 Forbidden", "text/plain", "Open the address shown on your console, including the code.");
        send(client, reply.data(), reply.size(), 0);
        return;
    }

    std::string reply;
    if (method == "GET" && (path == "/" || path == "/index.html" || path == "/" + sessionCode)) {
        reply = httpResponse("200 OK", "text/html; charset=utf-8", setupPage(sessionCode));
    } else if (method == "GET" && path == "/api/sources") {
        std::string dump = sourcesToJSON(sourceTable());
        size_t openBracket = dump.find('[');
        size_t closeBracket = dump.rfind(']');
        std::string array = openBracket == std::string::npos || closeBracket == std::string::npos ? "[]" : dump.substr(openBracket, closeBracket - openBracket + 1);
        reply = httpResponse("200 OK", "application/json", "{\"ok\":true,\"code\":" + jsonEscape(sessionCode) + ",\"sources\":" + array + "}");
    } else if (method == "POST" && path == "/api/add") {
        SourceTable current = sourceTable();
        SourceTable parsed;
        std::string error;
        std::string wrapped = "{\"sources\":[" + entryToSourceJSON(body) + "]}";
        JValue probeValue;
        if (!jsonParse(wrapped, probeValue)) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":\"that entry is not valid JSON\"}");
        } else if (!parseSourcesJSON(wrapped, parsed, error)) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
        } else {
            if (!parsed.items.empty()) current.items.push_back(parsed.items[0]);
            std::string path_ = root + "/sources.json";
            if (!saveSources(path_, current, error)) {
                reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
            } else {
                parseSourcesJSON(sourcesToJSON(current), sourceTable(), error);
                reply = httpResponse("200 OK", "application/json", "{\"ok\":true}");
            }
        }
    } else if (method == "POST" && path == "/api/remove") {
        int index = -1;
        JValue request_; 
        if (jsonParse(body, request_)) index = request_.integer("index", -1);
        SourceTable current = sourceTable();
        std::string error;
        if (index < 0 || index >= (int)current.items.size()) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":\"no such source\"}");
        } else {
            current.items.erase(current.items.begin() + index);
            if (!saveSources(root + "/sources.json", current, error)) {
                reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
            } else {
                sourceTable() = current;
                reply = httpResponse("200 OK", "application/json", "{\"ok\":true}");
            }
        }
    } else if (method == "POST" && path == "/api/save") {
        SourceTable parsed;
        std::string error;
        if (!parseSourcesJSON(body, parsed, error)) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
        } else if (!saveSources(root + "/sources.json", parsed, error)) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
        } else {
            sourceTable() = parsed;
            reply = httpResponse("200 OK", "application/json", "{\"ok\":true}");
        }
    } else if (method == "POST" && path == "/api/test") {
        SourceTable probe;
        std::string error;
        std::string wrapped = "{\"sources\":[" + entryToSourceJSON(body) + "]}";
        if (!parseSourcesJSON(wrapped, probe, error)) {
            reply = httpResponse("200 OK", "application/json", "{\"ok\":false,\"error\":" + jsonEscape(error) + "}");
        } else {
            std::string page;
            std::string listing = probe.listingURL(1, 1);
            bool fetched = fetch && !listing.empty() && fetch(listing, page, 4 * 1024 * 1024, "");
            int games = fetched ? (int)parseListingWith(probe, page, 1).size() : 0;
            std::string payload = fetched
                ? "{\"ok\":true,\"games\":" + std::to_string(games) + "}"
                : "{\"ok\":false,\"error\":\"the page could not be fetched\"}";
            reply = httpResponse("200 OK", "application/json", payload);
        }
    } else {
        reply = httpResponse("404 Not Found", "text/plain", "not found");
    }
    send(client, reply.data(), reply.size(), 0);
}
