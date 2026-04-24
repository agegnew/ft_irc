#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#ifdef BONUS
#  include "Bot.hpp"
#endif

#include <string>
#include <vector>

namespace CommandHandler {

// Common routing core used by both PRIVMSG (chatty, reports errors) and
// NOTICE (silent per RFC). `verb` is either "PRIVMSG" or "NOTICE".
static void routeMessage(Server& srv, Client& c, const Message& m, const char* verb,
                         bool emitErrors) {
    // No recipient at all, OR the only token is the trailing (":text") — that
    // is PRIVMSG with no target, not PRIVMSG with empty text.
    if (m.params.empty() || (m.hasTrailing && m.params.size() == 1)) {
        if (emitErrors)
            srv.sendNumeric(c, ERR_NORECIPIENT,
                            std::string(":No recipient given (") + verb + ")");
        return;
    }
    if (m.params.size() < 2 || m.params[1].empty()) {
        if (emitErrors)
            srv.sendNumeric(c, ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }

    const std::string& targets = m.params[0];
    const std::string& text    = m.params[1];
    std::vector<std::string> tvec = Utils::split(targets, ',');

    for (std::size_t i = 0; i < tvec.size(); ++i) {
        const std::string& t = tvec[i];
        if (t.empty()) continue;

        if (t[0] == '#' || t[0] == '&') {
            Channel* ch = srv.getChannel(t);
            if (!ch) {
                if (emitErrors)
                    srv.sendNumeric(c, ERR_NOSUCHCHANNEL, t + " :No such channel");
                continue;
            }
            if (!ch->hasMember(c.fd())) {
                if (emitErrors)
                    srv.sendNumeric(c, ERR_CANNOTSENDTOCHAN,
                                    t + " :Cannot send to channel");
                continue;
            }
            std::string line = ":" + c.prefix() + " " + verb + " " + ch->name()
                             + " :" + text + "\r\n";
            srv.broadcastToChannel(*ch, line, c.fd());

#ifdef BONUS
            // If the bot is a member of this channel, deliver to it too.
            if (srv.bot() && ch->hasMember(-1))
                srv.bot()->onMessage(c.nick(), ch->name(), text);
#endif
        } else {
#ifdef BONUS
            if (srv.bot() && Utils::ircEqual(t, srv.bot()->nick())) {
                srv.bot()->onMessage(c.nick(), "", text);
                continue;
            }
#endif
            Client* tgt = srv.getClientByNick(t);
            if (!tgt) {
                if (emitErrors)
                    srv.sendNumeric(c, ERR_NOSUCHNICK, t + " :No such nick/channel");
                continue;
            }
            std::string line = ":" + c.prefix() + " " + verb + " " + tgt->nick()
                             + " :" + text + "\r\n";
            tgt->enqueue(line);
        }
    }
}

void cmd_privmsg(Server& srv, Client& c, const Message& m) {
    routeMessage(srv, c, m, "PRIVMSG", true);
}

void cmd_notice(Server& srv, Client& c, const Message& m) {
    routeMessage(srv, c, m, "NOTICE", false);
}

} // namespace CommandHandler
