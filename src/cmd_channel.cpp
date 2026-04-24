#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#include <ctime>
#include <set>
#include <string>
#include <vector>

namespace CommandHandler {

static void needMoreParams_c(Server& srv, Client& c, const std::string& cmd) {
    srv.sendNumeric(c, ERR_NEEDMOREPARAMS, cmd + " :Not enough parameters");
}

// Emit the 353 + 366 pair for a freshly-joined or NAMES-queried channel.
static void sendNames(Server& srv, Client& c, Channel& ch) {
    std::string members;
    // Stable ordering: iterate the set of fds; prefix '@' for operators.
    for (std::set<int>::const_iterator it = ch.members().begin();
         it != ch.members().end(); ++it) {
        Client* p = srv.getClient(*it);
        if (!p) continue;
        if (!members.empty()) members += ' ';
        if (ch.isOperator(*it)) members += '@';
        members += p->nick();
    }
    srv.sendNumeric(c, RPL_NAMREPLY,
                    std::string("= ") + ch.name() + " :" + members);
    srv.sendNumeric(c, RPL_ENDOFNAMES,
                    ch.name() + " :End of /NAMES list.");
}

static void partSingleChannel(Server& srv, Client& c, const std::string& chName,
                              const std::string& reason) {
    Channel* ch = srv.getChannel(chName);
    if (!ch) { srv.sendNumeric(c, ERR_NOSUCHCHANNEL, chName + " :No such channel"); return; }
    if (!ch->hasMember(c.fd())) {
        srv.sendNumeric(c, ERR_NOTONCHANNEL, ch->name() + " :You're not on that channel");
        return;
    }
    std::string line = ":" + c.prefix() + " PART " + ch->name()
                     + (reason.empty() ? "" : " :" + reason) + "\r\n";
    srv.broadcastToChannel(*ch, line, -1); // include self
    ch->removeMember(c.fd());
    if (ch->empty()) srv.dropChannel(ch->name());
}

void cmd_join(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) { needMoreParams_c(srv, c, "JOIN"); return; }

    // "JOIN 0" — RFC 2812 §3.2.1: shortcut to part every channel the user
    // is currently in. Snapshot the channel names first so that dropChannel
    // during iteration doesn't invalidate our working list.
    if (m.params[0] == "0") {
        std::vector<std::string> mine;
        const std::map<std::string, Channel*>& all = srv.channels();
        for (std::map<std::string, Channel*>::const_iterator it = all.begin();
             it != all.end(); ++it) {
            if (it->second->hasMember(c.fd())) mine.push_back(it->second->name());
        }
        for (std::size_t i = 0; i < mine.size(); ++i)
            partSingleChannel(srv, c, mine[i], "Leaving all channels");
        return;
    }

    std::vector<std::string> chans = Utils::split(m.params[0], ',');
    std::vector<std::string> keys;
    if (m.params.size() >= 2) keys = Utils::split(m.params[1], ',');

    for (std::size_t i = 0; i < chans.size(); ++i) {
        const std::string& name = chans[i];
        if (!Utils::isValidChannelName(name)) {
            srv.sendNumeric(c, ERR_NOSUCHCHANNEL, name + " :No such channel");
            continue;
        }

        Channel* existed = srv.getChannel(name);
        if (existed) {
            if (existed->hasMember(c.fd())) continue; // silent no-op

            if (existed->inviteOnly() && !existed->isInvited(c.fd())) {
                srv.sendNumeric(c, ERR_INVITEONLYCHAN,
                                existed->name() + " :Cannot join channel (+i)");
                continue;
            }
            if (existed->hasKey()) {
                std::string givenKey = (i < keys.size()) ? keys[i] : std::string();
                if (givenKey != existed->key()) {
                    srv.sendNumeric(c, ERR_BADCHANNELKEY,
                                    existed->name() + " :Cannot join channel (+k)");
                    continue;
                }
            }
            if (existed->hasLimit() && existed->members().size() >= existed->userLimit()) {
                srv.sendNumeric(c, ERR_CHANNELISFULL,
                                existed->name() + " :Cannot join channel (+l)");
                continue;
            }
            existed->addMember(c.fd());
            existed->removeInvite(c.fd());
        } else {
            existed = srv.getOrCreateChannel(name, c.fd());
        }

        // Announce the JOIN to every member (including self).
        std::string line = ":" + c.prefix() + " JOIN :" + existed->name() + "\r\n";
        srv.broadcastToChannel(*existed, line, -1);

        if (existed->topic().empty()) {
            srv.sendNumeric(c, RPL_NOTOPIC, existed->name() + " :No topic is set");
        } else {
            srv.sendNumeric(c, RPL_TOPIC,
                            existed->name() + " :" + existed->topic());
            if (!existed->topicSetter().empty())
                srv.sendNumeric(c, RPL_TOPICWHOTIME,
                                existed->name() + " " + existed->topicSetter()
                                + " " + Utils::intToStr(existed->topicTime()));
        }
        sendNames(srv, c, *existed);
    }
}

void cmd_part(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) { needMoreParams_c(srv, c, "PART"); return; }

    std::vector<std::string> chans = Utils::split(m.params[0], ',');
    std::string reason = (m.params.size() >= 2) ? m.params[1] : std::string();

    for (std::size_t i = 0; i < chans.size(); ++i) {
        if (chans[i].empty()) continue;
        partSingleChannel(srv, c, chans[i], reason);
    }
}

void cmd_topic(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) { needMoreParams_c(srv, c, "TOPIC"); return; }

    const std::string& chName = m.params[0];
    Channel* ch = srv.getChannel(chName);
    if (!ch) { srv.sendNumeric(c, ERR_NOSUCHCHANNEL, chName + " :No such channel"); return; }
    if (!ch->hasMember(c.fd())) {
        srv.sendNumeric(c, ERR_NOTONCHANNEL, ch->name() + " :You're not on that channel");
        return;
    }

    if (m.params.size() < 2) {
        if (ch->topic().empty())
            srv.sendNumeric(c, RPL_NOTOPIC, ch->name() + " :No topic is set");
        else {
            srv.sendNumeric(c, RPL_TOPIC, ch->name() + " :" + ch->topic());
            if (!ch->topicSetter().empty())
                srv.sendNumeric(c, RPL_TOPICWHOTIME,
                                ch->name() + " " + ch->topicSetter()
                                + " " + Utils::intToStr(ch->topicTime()));
        }
        return;
    }

    if (ch->topicOpsOnly() && !ch->isOperator(c.fd())) {
        srv.sendNumeric(c, ERR_CHANOPRIVSNEEDED,
                        ch->name() + " :You're not channel operator");
        return;
    }

    const std::string& newTopic = m.params[1];
    ch->setTopic(newTopic, c.prefix(), (long)std::time(0));

    std::string line = ":" + c.prefix() + " TOPIC " + ch->name() + " :"
                     + newTopic + "\r\n";
    srv.broadcastToChannel(*ch, line, -1);
}

void cmd_names(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) {
        srv.sendNumeric(c, RPL_ENDOFNAMES, "* :End of /NAMES list.");
        return;
    }
    std::vector<std::string> chans = Utils::split(m.params[0], ',');
    for (std::size_t i = 0; i < chans.size(); ++i) {
        Channel* ch = srv.getChannel(chans[i]);
        if (!ch) {
            srv.sendNumeric(c, RPL_ENDOFNAMES, chans[i] + " :End of /NAMES list.");
            continue;
        }
        sendNames(srv, c, *ch);
    }
}

} // namespace CommandHandler
