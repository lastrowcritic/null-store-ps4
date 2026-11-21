#pragma once
// Phone setup page served by the console itself.
//
// The console shows a short code; the phone opens http://<console-ip>:<port>/<code>
// and edits the same sources.json the app reads. Nothing leaves the LAN and the
// page is only reachable with the code visible on the TV.
#include <string>
#include <pthread.h>

// First usable IPv4 address of this console, or an empty string.
std::string localLanAddress();

class SourcesServer {
public:
    using Fetch = bool (*)(const std::string& url, std::string& out, size_t cap, const std::string& referer);

    SourcesServer(std::string dataRoot, Fetch fetch);
    ~SourcesServer();

    // port 0 asks the system for a free port; the chosen one is reported by port().
    bool start(int port, std::string& error);
    void stop();

    int port() const { return boundPort; }
    std::string code() const { return sessionCode; }
    std::string urlFor(const std::string& address) const;

private:
    static void* entry(void* self);
    void loop();
    void handle(int client);

    std::string root;
    Fetch fetch;
    std::string sessionCode;
    int listenFd;
    int boundPort;
    bool running;
    pthread_t thread;
    pthread_mutex_t lock;
};
