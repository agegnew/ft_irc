#include "Channel.hpp"
#include "Client.hpp"
#include "CommandHandler.hpp"
#include "Message.hpp"
#include "Numerics.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#include <map>
#include <string>

namespace CommandHandler {

// Helper: emit a plain error line tied to a command name.
static void needMoreParams(Server& srv, Client& c, const std::string& cmd) {
    srv.sendNumeric(c, ERR_NEEDMOREPARAMS, cmd + " :Not enough parameters");
}

void cmd_pass(Server& srv, Client& c, const Message& m) {
    if (c.isRegistered()) {
        srv.sendNumeric(c, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    if (m.params.empty()) { needMoreParams(srv, c, "PASS"); return; }

    if (m.params[0] != srv.password()) {
        srv.sendNumeric(c, ERR_PASSWDMISMATCH, ":Password incorrect");
        c.markForQuit("Bad password");
        return;
    }
    c.setGotPass(true);
    tryRegister(srv, c);
}

void cmd_nick(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) {
        srv.sendNumeric(c, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }
    const std::string newNick = m.params[0];
    if (!Utils::isValidNick(newNick)) {
        srv.sendNumeric(c, ERR_ERRONEUSNICKNAME, newNick + " :Erroneous nickname");
        return;
    }

    // Same nick (case-folded) is a silent no-op unless casing changes.
    if (!c.nick().empty() && Utils::ircEqual(c.nick(), newNick)) {
        if (c.nick() != newNick) {
            // Casing-only change still propagated like a rename.
            const std::string oldNick = c.nick();
            srv.unindexNick(oldNick);
            c.setNick(newNick);
            srv.indexNick(newNick, c.fd());
            if (c.isRegistered()) {
                std::string line = ":" + oldNick + "!" + c.user() + "@" + c.host()
                                 + " NICK :" + newNick + "\r\n";
                srv.broadcastToPeers(c.fd(), line);
                c.enqueue(line);
            }
        }
        return;
    }

    // Collision check: covers real clients AND reserved nicks (bot), because
    // getClientByNick returns null for fd=-1 reservations but the nick is
    // still taken.
    if (srv.isNickTaken(newNick)) {
        srv.sendNumeric(c, ERR_NICKNAMEINUSE, newNick + " :Nickname is already in use");
        return;
    }

    const std::string oldNick = c.nick();
    if (!oldNick.empty()) srv.unindexNick(oldNick);
    c.setNick(newNick);
    srv.indexNick(newNick, c.fd());

    if (c.isRegistered()) {
        // Broadcast :old!user@host NICK :new  to self + every channel peer.
        std::string line = ":" + oldNick + "!" + c.user() + "@" + c.host()
                         + " NICK :" + newNick + "\r\n";
        srv.broadcastToPeers(c.fd(), line);
        c.enqueue(line);
    } else {
        tryRegister(srv, c);
    }
}

void cmd_user(Server& srv, Client& c, const Message& m) {
    if (c.isRegistered()) {
        srv.sendNumeric(c, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    if (m.params.size() < 4) { needMoreParams(srv, c, "USER"); return; }

    c.setUser(m.params[0], m.params[3]);

    if (!c.gotPass()) {
        srv.sendNumeric(c, ERR_PASSWDMISMATCH, ":Password required");
        c.markForQuit("Password required");
        return;
    }
    tryRegister(srv, c);
}

void cmd_cap(Server& srv, Client& c, const Message& m) {
    if (m.params.empty()) return;
    const std::string sub = Utils::toUpper(m.params[0]);

    if (sub == "LS") {
        c.setCapNegotiating(true);
        c.enqueue(":" + srv.name() + " CAP * LS :");
    } else if (sub == "LIST") {
        c.enqueue(":" + srv.name() + " CAP * LIST :");
    } else if (sub == "REQ") {
        const std::string list = m.params.size() >= 2 ? m.params[1] : std::string();
        c.enqueue(":" + srv.name() + " CAP * NAK :" + list);
    } else if (sub == "END") {
        c.setCapNegotiating(false);
        tryRegister(srv, c);
    }
    // unknown CAP subs silently ignored
}

void cmd_quit(Server& srv, Client& c, const Message& m) {
    (void)srv;
    std::string reason = m.params.empty() ? "Client quit" : m.params.back();
    c.markForQuit(reason);
}

void cmd_ping(Server& srv, Client& c, const Message& m) {
    const std::string token = m.params.empty() ? c.host() : m.params[0];
    c.enqueue(":" + srv.name() + " PONG " + srv.name() + " :" + token);
}

void cmd_pong(Server& srv, Client& c, const Message& m) {
    (void)srv; (void)c; (void)m; // liveness-only, no action
}

} // namespace CommandHandler
