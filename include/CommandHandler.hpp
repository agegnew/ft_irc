#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

class Server;
class Client;
struct Message;

namespace CommandHandler {

// Dispatch an already-parsed message for `c` through `srv`. Unknown commands
// reply with ERR_UNKNOWNCOMMAND. Commands that require registration but
// arrive before registration reply with ERR_NOTREGISTERED.
void dispatch(Server& srv, Client& c, const Message& m);

// Individual command handlers (defined across cmd_*.cpp files).
void cmd_pass   (Server& srv, Client& c, const Message& m);
void cmd_nick   (Server& srv, Client& c, const Message& m);
void cmd_user   (Server& srv, Client& c, const Message& m);
void cmd_cap    (Server& srv, Client& c, const Message& m);
void cmd_quit   (Server& srv, Client& c, const Message& m);
void cmd_ping   (Server& srv, Client& c, const Message& m);
void cmd_pong   (Server& srv, Client& c, const Message& m);

void cmd_privmsg(Server& srv, Client& c, const Message& m);
void cmd_notice (Server& srv, Client& c, const Message& m);

void cmd_join   (Server& srv, Client& c, const Message& m);
void cmd_part   (Server& srv, Client& c, const Message& m);
void cmd_topic  (Server& srv, Client& c, const Message& m);
void cmd_names  (Server& srv, Client& c, const Message& m);

void cmd_kick   (Server& srv, Client& c, const Message& m);
void cmd_invite (Server& srv, Client& c, const Message& m);
void cmd_mode   (Server& srv, Client& c, const Message& m);

// Attempt to finalize registration: sends 001-004 + MOTD stub when all three
// of (PASS, NICK, USER) are collected. No-op otherwise.
void tryRegister(Server& srv, Client& c);

} // namespace CommandHandler

#endif
