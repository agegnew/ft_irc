#include "CommandHandler.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#include <cstring>
#include <map>
#include <string>

namespace CommandHandler {

typedef void (*CmdFn)(Server&, Client&, const Message&);

// Some commands are valid before registration; the rest get 451.
struct Entry { const char* name; CmdFn fn; bool preRegOk; };

static const Entry kTable[] = {
    { "PASS",    &cmd_pass,    true  },
    { "NICK",    &cmd_nick,    true  },
    { "USER",    &cmd_user,    true  },
    { "CAP",     &cmd_cap,     true  },
    { "QUIT",    &cmd_quit,    true  },
    { "PING",    &cmd_ping,    true  },
    { "PONG",    &cmd_pong,    true  },

    { "PRIVMSG", &cmd_privmsg, false },
    { "NOTICE",  &cmd_notice,  false },

    { "JOIN",    &cmd_join,    false },
    { "PART",    &cmd_part,    false },
    { "TOPIC",   &cmd_topic,   false },
    { "NAMES",   &cmd_names,   false },

    { "KICK",    &cmd_kick,    false },
    { "INVITE",  &cmd_invite,  false },
    { "MODE",    &cmd_mode,    false }
};
static const std::size_t kTableSize = sizeof(kTable) / sizeof(kTable[0]);

void dispatch(Server& srv, Client& c, const Message& m) {
    if (m.command.empty()) return;

    for (std::size_t i = 0; i < kTableSize; ++i) {
        if (m.command == kTable[i].name) {
            if (!kTable[i].preRegOk && !c.isRegistered()) {
                srv.sendNumeric(c, ERR_NOTREGISTERED,
                                ":You have not registered");
                return;
            }
            kTable[i].fn(srv, c, m);
            return;
        }
    }
    srv.sendNumeric(c, ERR_UNKNOWNCOMMAND,
                    m.command + " :Unknown command");
}

void tryRegister(Server& srv, Client& c) {
    if (c.isRegistered()) return;
    // If the client ever sent CAP LS, wait until CAP END before firing the
    // welcome sequence. Otherwise HexChat may reject 001 as premature.
    if (c.capNegotiating()) return;
    if (!(c.gotPass() && c.gotNick() && c.gotUser())) return;

    c.markRegistered();

    const std::string& nick = c.nick();
    srv.sendNumeric(c, RPL_WELCOME,
        ":Welcome to the ircserv network, " + c.prefix());
    srv.sendNumeric(c, RPL_YOURHOST,
        ":Your host is " + srv.name() + ", running version 1.0");
    srv.sendNumeric(c, RPL_CREATED,
        ":This server was created " + srv.creationTs());
    srv.sendNumeric(c, RPL_MYINFO,
        srv.name() + " 1.0 o itkol");
    (void)nick;
}

} // namespace CommandHandler
