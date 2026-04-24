*This project has been created as part of the 42 curriculum by amersha, nalkhate, zaldhahe.*

# ft_irc

## Description

`ft_irc` is an RFC 2812-style Internet Relay Chat server written from scratch
in **C++98**. A real IRC client (HexChat is our reference) can connect to it,
authenticate with a password, set a nickname and username, join channels,
exchange private messages, and use the channel-operator commands
**KICK / INVITE / TOPIC / MODE** with the flags **i / t / k / o / l**.

The server runs as a single process driven by one `poll()` loop. Every file
descriptor — the listening socket included — lives in the same `pollfd` array;
there is no `fork()`, no thread, and no `recv`/`send` call that is not gated by
`poll()`. Partial TCP frames are aggregated per-client before any command is
dispatched, and outbound bytes go through a per-client send-buffer whose
drain is armed by `POLLOUT` only when work is pending.

## Instructions

```sh
# Build (mandatory only):
make

# Build with bonus (adds Bot):
make bonus

# Run:
./ircserv <port> <password>
# example:
./ircserv 6667 hunter2

# Then, in HexChat:
#   Server:   127.0.0.1 / 6667
#   Password: hunter2
```

`make` rules: `all`, `$(NAME)`, `clean`, `fclean`, `re`, `bonus`.
Compile flags: `-Wall -Wextra -Werror -std=c++98 -Iinclude`.
The build uses `-MMD -MP` dependency files so `make` never relinks unnecessarily.

## Features

### Mandatory
- **Authentication** — `PASS`, `NICK`, `USER`, minimal `CAP LS/LIST/REQ/END`
  handshake for HexChat compatibility, full numeric replies 001–004.
- **Messaging** — `PRIVMSG`, `NOTICE`, with multi-target (comma-separated)
  delivery. Channel targets require membership; unknown targets return 401/403.
- **Channels** — `JOIN`, `PART`, `TOPIC`, `NAMES`, implicit creation on first
  `JOIN`, auto-destruction when empty, channel-operator promotion for the
  creator.
- **Channel operators** — `KICK`, `INVITE` with RPL_INVITING (341).
- **Channel modes** — `+i/-i` invite-only, `+t/-t` topic-ops-only,
  `+k key/-k` channel key (password), `+o nick/-o nick` op promotion/demotion,
  `+l N/-l` user limit. Unknown modes reply 472. `MODE #chan` with no change
  string replies `324 RPL_CHANNELMODEIS`.
- **Robustness** — partial-frame aggregation (subject-mandated),
  POLLOUT-gated writes, two-pass iteration (no `_pfds` mutation during
  dispatch), `SO_REUSEADDR` on the listener, `SIGPIPE` ignored, `SIGINT` and
  `SIGTERM` trigger a clean shutdown, out-of-memory in `new` is caught in
  `Server::run` and converted into a graceful exit.

### Bonus (`make bonus`)
- **Bot** — an in-process pseudo-client (no extra fd, no fork). Reserved
  nick: `ftbot`. Responds to `PRIVMSG ftbot :<message>` with commands
  `!help`, `!time`, `!roll <N>`, `!say <text>`. Any message that doesn't
  start with `!` gets a friendly greeting.

## Technical choices

- **One `poll()` for everything.** A single `std::vector<pollfd>` contains the
  listener plus every client. Before each `poll()` we rebuild the `events`
  mask per fd: always `POLLIN`, plus `POLLOUT` only when the client's
  `_sendBuf` is non-empty.
- **Non-blocking fds.** `fcntl(fd, F_SETFL, O_NONBLOCK)` is applied to the
  listener and every `accept()`-returned socket. This is the macOS-safe form
  (the only `fcntl` call the subject permits), even though we target Linux.
- **Per-client buffers.** `std::string _recvBuf` aggregates partial TCP
  frames; `Client::extractLine` pops one CRLF- (or LF-) terminated line at a
  time, trimmed to 512 bytes per the spec. `std::string _sendBuf` queues
  outbound bytes; `handleWritable` drains it only when `POLLOUT` fires.
- **Two-pass loop.** We snapshot `_pfds` into local `readFds` / `writeFds`
  vectors before dispatching, so handlers that close connections or mutate
  the fd set can't invalidate iterators.
- **Deferred disconnect.** When a handler marks the client for quit
  (bad password, QUIT command, I/O error), we wait for `_sendBuf` to drain
  through `POLLOUT` before calling `close()`, so the final error numeric
  (e.g. 464 PASSWDMISMATCH) reaches the wire before the fd is reaped.
- **RFC 1459 case folding** for nickname and channel comparison
  (`{`=`[`, `}`=`]`, `|`=`\`, `^`=`~`).
- **C++98 only.** `std::map`, `std::set`, `std::vector`, `std::string`. No
  external libraries, no Boost.
- **Dispatch table** — a static array of `{name, fn_ptr, preRegOk}`
  triples in `CommandHandler.cpp`; unknown commands reply 421, commands that
  require registration reply 451 before registration is complete.

## File tree

```
ft_irc/
├── Makefile
├── README.md
├── include/
│   ├── Server.hpp, Client.hpp, Channel.hpp, Message.hpp
│   ├── CommandHandler.hpp, Numerics.hpp, Utils.hpp, Bot.hpp
└── src/
    ├── main.cpp, Server.cpp, Client.cpp, Channel.cpp, Message.cpp
    ├── CommandHandler.cpp, Utils.cpp
    ├── cmd_auth.cpp      (PASS, NICK, USER, CAP, QUIT, PING, PONG)
    ├── cmd_chat.cpp      (PRIVMSG, NOTICE)
    ├── cmd_channel.cpp   (JOIN, PART, TOPIC, NAMES)
    ├── cmd_operator.cpp  (KICK, INVITE, MODE)
    └── Bot.cpp           (bonus)
```

## Usage examples

After registering (`PASS`, `NICK`, `USER`), try:

```
/join #general
/topic #general :Welcome to the server
/mode #general +itk hunter2
/invite friend #general
/mode #general +o friend
/kick #general troll :no spamming
/msg friend :hello
/msg ftbot :!roll 20
```

## Testing

A small suite of manual integration tests lives in `tests/`:

```
bash tests/run_smoke.sh   # register, partial frame, multi-client JOIN/KICK
bash tests/run_edge.sh    # error numerics: 464, 421, 451, 401, 432, 442, 482, ...
bash tests/run_bot.sh     # bonus: !help / !time / !roll / !say
```

These tests drive the server with `nc` and are only intended for local
sanity-checking — they are not submitted for grading.

The critical subject test is **partial-frame aggregation**: send a single IRC
command split across multiple TCP segments (the subject's `com^D man^D d`
example). `tests/run_edge.sh` exercises this by sleeping between `printf`
calls; the server reassembles the bytes into exactly one IRC line.

## Resources

- **RFC 2812** — Internet Relay Chat: Client Protocol
- **RFC 1459** — The original Internet Relay Chat Protocol
- **[modern.ircdocs.horse](https://modern.ircdocs.horse)** — the modern IRC
  protocol reference (fills in gaps the RFCs leave ambiguous)
- **Beej's Guide to Network Programming** — classic intro to BSD sockets
- **poll(2), send(2), recv(2)** — Linux man pages
- **AI usage** — Claude was used to cross-reference IRC numerics against
  RFC 2812, brainstorm edge cases the test suite should cover (partial
  frames, POLLOUT backpressure, RFC 1459 case-fold), and to draft the
  initial implementation plan. All source code, test scripts, and the
  README were written and reviewed by the team members named above, who
  remain responsible for every line of the project.
