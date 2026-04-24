#ifdef BONUS

#include "Bot.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

Bot::Bot(Server& srv, const std::string& nick)
    : _srv(srv), _nick(nick) {
    std::srand(static_cast<unsigned int>(std::time(0)));
}

Bot::~Bot() {}

std::string Bot::prefix() const {
    return _nick + "!bot@" + _srv.name();
}

static std::string isoTimeNow() {
    std::time_t t = std::time(0);
    std::tm* tm = std::gmtime(&t);
    char buf[32];
    if (tm) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    else    std::snprintf(buf, sizeof(buf), "%ld", (long)t);
    return std::string(buf);
}

void Bot::onMessage(const std::string& from,
                    const std::string& channel,
                    const std::string& text) {
    std::string trimmed = Utils::trim(text);
    if (trimmed.empty()) return;

    if (trimmed[0] != '!') {
        reply(from, channel, "Hi " + from + "! Try !help.");
        return;
    }

    std::vector<std::string> parts = Utils::split(trimmed, ' ');
    std::string cmd = parts.empty() ? std::string() : Utils::toUpper(parts[0]);

    if (cmd == "!HELP") {
        reply(from, channel, "Commands: !help  !time  !roll <N>  !say <text>");
    } else if (cmd == "!TIME") {
        reply(from, channel, "UTC now: " + isoTimeNow());
    } else if (cmd == "!ROLL") {
        long n = 6;
        if (parts.size() >= 2) {
            long v = 0;
            if (Utils::strToInt(parts[1], v) && v >= 1 && v <= 1000000) n = v;
        }
        long r = (std::rand() % n) + 1;
        reply(from, channel, "rolled " + Utils::intToStr(r) + " (1.." + Utils::intToStr(n) + ")");
    } else if (cmd == "!SAY") {
        std::string rest;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            if (i > 1) rest += ' ';
            rest += parts[i];
        }
        if (rest.empty()) rest = "(nothing to say)";
        reply(from, channel, rest);
    } else {
        reply(from, channel, "Unknown command: " + trimmed + " — try !help");
    }
}

void Bot::reply(const std::string& from,
                const std::string& channel,
                const std::string& text) {
    // Channel reply: broadcast via the Server.
    if (!channel.empty()) {
        Channel* ch = _srv.getChannel(channel);
        if (!ch) return;
        std::string line = ":" + prefix() + " PRIVMSG " + ch->name() + " :" + text + "\r\n";
        _srv.broadcastToChannel(*ch, line, /*exceptFd=*/-1);
        return;
    }
    // DM reply: look up the sender and enqueue directly.
    Client* peer = _srv.getClientByNick(from);
    if (!peer) return;
    std::string line = ":" + prefix() + " PRIVMSG " + peer->nick() + " :" + text + "\r\n";
    peer->enqueue(line);
}

#endif // BONUS
