#include "Server.hpp"
#include "Utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

static void usage(const char* prog) {
    std::fprintf(stderr, "Usage: %s <port> <password>\n", prog);
}

int main(int argc, char** argv) {
    // Line-buffer stdout so connect/disconnect logs are visible even when
    // the server is launched under `>log 2>&1` rather than a tty.
    std::setvbuf(stdout, 0, _IOLBF, 0);

    if (argc != 3) { usage(argv[0]); return 1; }

    long port = 0;
    if (!Utils::strToInt(argv[1], port) || port < 1 || port > 65535) {
        std::fprintf(stderr, "%s: invalid port (expected 1..65535)\n", argv[0]);
        return 1;
    }

    std::string password(argv[2]);
    if (password.empty()) {
        std::fprintf(stderr, "%s: password must not be empty\n", argv[0]);
        return 1;
    }

    try {
        Server srv(static_cast<int>(port), password);
        srv.run();
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "[ircserv] fatal: %s\n", e.what());
        return 1;
    }
    catch (...) {
        std::fprintf(stderr, "[ircserv] fatal: unknown exception\n");
        return 1;
    }
    return 0;
}
