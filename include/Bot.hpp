#ifndef BOT_HPP
#define BOT_HPP

#ifdef BONUS

#include <string>

class Server;
class Client;

// An in-process pseudo-client. Not backed by a TCP socket; "delivered" PRIVMSGs
// are routed to this object, which synthesizes a reply and asks the Server to
// send it on behalf of the bot. No fork, no extra file descriptor, no extra
// thread — the Bot participates in the one-poll discipline as a degenerate
// Client whose sendBuf is always empty.
class Bot {
public:
    Bot(Server& srv, const std::string& nick);
    ~Bot();

    const std::string& nick() const { return _nick; }

    // Called by PRIVMSG/NOTICE handlers when the target resolves to the bot.
    // `from` is the sending client's nick. If `channel` is non-empty, the bot
    // should reply into that channel; otherwise it replies via PRIVMSG to
    // `from`.
    void onMessage(const std::string& from,
                   const std::string& channel,
                   const std::string& text);

    // The bot's own prefix, e.g. "ftbot!bot@ircserv.42".
    std::string prefix() const;

private:
    Server&     _srv;
    std::string _nick;

    void reply(const std::string& from,
               const std::string& channel,
               const std::string& text);

    Bot(const Bot&);
    Bot& operator=(const Bot&);
};

#endif // BONUS
#endif
