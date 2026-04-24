#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace CommandHandler {

static void needMoreParams_o(Server& srv, Client& c, const std::string& cmd) {
    srv.sendNumeric(c, ERR_NEEDMOREPARAMS, cmd + " :Not enough parameters");
}

void cmd_kick(Server& srv, Client& c, const Message& m) {
    if (m.params.size() < 2) { needMoreParams_o(srv, c, "KICK"); return; }

    const std::string& chName = m.params[0];
    Channel* ch = srv.getChannel(chName);
    if (!ch) { srv.sendNumeric(c, ERR_NOSUCHCHANNEL, chName + " :No such channel"); return; }

    if (!ch->hasMember(c.fd())) {
        srv.sendNumeric(c, ERR_NOTONCHANNEL, ch->name() + " :You're not on that channel");
        return;
    }
    if (!ch->isOperator(c.fd())) {
        srv.sendNumeric(c, ERR_CHANOPRIVSNEEDED,
                        ch->name() + " :You're not channel operator");
        return;
    }

    const std::string comment = (m.params.size() >= 3) ? m.params[2] : c.nick();
    std::vector<std::string> targets = Utils::split(m.params[1], ',');

    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (targets[i].empty()) continue;
        Client* tgt = srv.getClientByNick(targets[i]);
        if (!tgt || !ch->hasMember(tgt->fd())) {
            srv.sendNumeric(c, ERR_USERNOTINCHANNEL,
                            targets[i] + " " + ch->name() + " :They aren't on that channel");
            continue;
        }
        std::string line = ":" + c.prefix() + " KICK " + ch->name() + " "
                         + tgt->nick() + " :" + comment + "\r\n";
        srv.broadcastToChannel(*ch, line, -1); // include self + target
        ch->removeMember(tgt->fd());
        if (ch->empty()) {
            srv.dropChannel(ch->name());
            return;
        }
    }
}

void cmd_invite(Server& srv, Client& c, const Message& m) {
    if (m.params.size() < 2) { needMoreParams_o(srv, c, "INVITE"); return; }

    const std::string& targetNick = m.params[0];
    const std::string& chName     = m.params[1];

    Client* tgt = srv.getClientByNick(targetNick);
    if (!tgt) {
        srv.sendNumeric(c, ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }

    Channel* ch = srv.getChannel(chName);
    if (!ch) {
        srv.sendNumeric(c, ERR_NOSUCHCHANNEL, chName + " :No such channel");
        return;
    }
    if (!ch->hasMember(c.fd())) {
        srv.sendNumeric(c, ERR_NOTONCHANNEL, ch->name() + " :You're not on that channel");
        return;
    }
    if (ch->inviteOnly() && !ch->isOperator(c.fd())) {
        srv.sendNumeric(c, ERR_CHANOPRIVSNEEDED,
                        ch->name() + " :You're not channel operator");
        return;
    }
    if (ch->hasMember(tgt->fd())) {
        srv.sendNumeric(c, ERR_USERONCHANNEL,
                        tgt->nick() + " " + ch->name() + " :is already on channel");
        return;
    }

    ch->addInvite(tgt->fd());
    srv.sendNumeric(c, RPL_INVITING, tgt->nick() + " " + ch->name());

    std::string line = ":" + c.prefix() + " INVITE " + tgt->nick()
                     + " :" + ch->name() + "\r\n";
    tgt->enqueue(line);
}

// -------- MODE ---------------------------------------------------------

struct ModeAccum {
    std::string              setStr; // "+it-k"
    std::vector<std::string> args;
    char                     lastSign;
    ModeAccum() : lastSign(0) {}
};

// Emit the sign only when it switches; letters coalesce under the current sign.
static void appendOp(ModeAccum& acc, char sign, char letter) {
    if (acc.lastSign != sign) {
        acc.setStr += sign;
        acc.lastSign = sign;
    }
    acc.setStr += letter;
}

void cmd_mode(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) { needMoreParams_o(srv, c, "MODE"); return; }

    const std::string& target = m.params[0];

    // User-mode branch. We don't implement user modes, so MODE <self> just
    // echoes "+" (no modes set); MODE <anotherNick> rejects with 502.
    if (!Utils::isValidChannelName(target)) {
        if (!Utils::ircEqual(target, c.nick())) {
            srv.sendNumeric(c, ERR_USERSDONTMATCH,
                            ":Cannot change mode for other users");
            return;
        }
        srv.sendNumeric(c, RPL_UMODEIS, "+");
        return;
    }

    Channel* ch = srv.getChannel(target);
    if (!ch) { srv.sendNumeric(c, ERR_NOSUCHCHANNEL, target + " :No such channel"); return; }

    // No mode string → view.
    if (m.params.size() < 2) {
        std::string letters = ch->modeLetters();
        if (letters.empty()) letters = "+";
        srv.sendNumeric(c, RPL_CHANNELMODEIS, ch->name() + " " + letters);
        return;
    }

    if (!ch->isOperator(c.fd())) {
        srv.sendNumeric(c, ERR_CHANOPRIVSNEEDED,
                        ch->name() + " :You're not channel operator");
        return;
    }

    const std::string& mods = m.params[1];
    std::size_t argCursor = 2;
    char sign = '+';

    ModeAccum acc;
    bool needMore = false;

    // Walk the mode letters; on a missing required argument, set `needMore`
    // and break out of the loop so that any mode changes applied so far are
    // still broadcast before we emit 461.
    for (std::size_t i = 0; i < mods.size() && !needMore; ++i) {
        char ch2 = mods[i];
        if (ch2 == '+' || ch2 == '-') { sign = ch2; continue; }

        switch (ch2) {
            case 'i':
                if (ch->inviteOnly() == (sign == '+')) break;
                ch->setInviteOnly(sign == '+');
                appendOp(acc, sign, 'i');
                break;
            case 't':
                if (ch->topicOpsOnly() == (sign == '+')) break;
                ch->setTopicOpsOnly(sign == '+');
                appendOp(acc, sign, 't');
                break;
            case 'k': {
                if (sign == '+') {
                    if (argCursor >= m.params.size()) { needMore = true; break; }
                    const std::string& k = m.params[argCursor++];
                    if (k.empty()) break;
                    ch->setKey(k);
                    appendOp(acc, '+', 'k');
                    acc.args.push_back(k);
                } else {
                    if (!ch->hasKey()) break;
                    ch->clearKey();
                    appendOp(acc, '-', 'k');
                }
                break;
            }
            case 'o': {
                if (argCursor >= m.params.size()) { needMore = true; break; }
                const std::string& nick = m.params[argCursor++];
                Client* tgt = srv.getClientByNick(nick);
                if (!tgt || !ch->hasMember(tgt->fd())) {
                    srv.sendNumeric(c, ERR_USERNOTINCHANNEL,
                                    nick + " " + ch->name()
                                    + " :They aren't on that channel");
                    break;
                }
                if (sign == '+') {
                    if (ch->isOperator(tgt->fd())) break;
                    ch->addOperator(tgt->fd());
                    appendOp(acc, '+', 'o');
                    acc.args.push_back(tgt->nick());
                } else {
                    if (!ch->isOperator(tgt->fd())) break;
                    ch->removeOperator(tgt->fd());
                    appendOp(acc, '-', 'o');
                    acc.args.push_back(tgt->nick());
                }
                break;
            }
            case 'l': {
                if (sign == '+') {
                    if (argCursor >= m.params.size()) { needMore = true; break; }
                    long lim = 0;
                    if (!Utils::strToInt(m.params[argCursor], lim) || lim <= 0) {
                        ++argCursor;
                        break; // silently ignore bad limit
                    }
                    ++argCursor;
                    ch->setUserLimit(static_cast<std::size_t>(lim));
                    appendOp(acc, '+', 'l');
                    acc.args.push_back(Utils::intToStr(lim));
                } else {
                    if (!ch->hasLimit()) break;
                    ch->clearUserLimit();
                    appendOp(acc, '-', 'l');
                }
                break;
            }
            default:
                srv.sendNumeric(c, ERR_UNKNOWNMODE,
                                std::string(1, ch2)
                                + " :is unknown mode char to me");
                break;
        }
    }

    // Broadcast whatever mode changes were actually applied before reporting
    // the syntax error (if any). This keeps the channel state consistent with
    // what clients observe.
    if (!acc.setStr.empty()) {
        std::string line = ":" + c.prefix() + " MODE " + ch->name() + " " + acc.setStr;
        for (std::size_t i = 0; i < acc.args.size(); ++i) line += " " + acc.args[i];
        line += "\r\n";
        srv.broadcastToChannel(*ch, line, -1);
    }

    if (needMore) needMoreParams_o(srv, c, "MODE");
}

} // namespace CommandHandler
