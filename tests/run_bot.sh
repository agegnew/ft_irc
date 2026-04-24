#!/bin/bash
set -u
cd "$(dirname "$0")/.."

pkill -9 -f 'ircserv 6667' 2>/dev/null || true
sleep 0.2
rm -f /tmp/ircserv.log
./ircserv.bonus 6667 secret > /tmp/ircserv.log 2>&1 &
SERVER_PID=$!
sleep 0.3
trap 'kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null' EXIT

echo "=== BOT TEST: !help, !time, !roll, !say ==="
(
    printf 'PASS secret\r\nNICK human\r\nUSER h 0 * :H\r\n'; sleep 0.3
    printf 'PRIVMSG ftbot :!help\r\n';      sleep 0.2
    printf 'PRIVMSG ftbot :!time\r\n';      sleep 0.2
    printf 'PRIVMSG ftbot :!roll 100\r\n';  sleep 0.2
    printf 'PRIVMSG ftbot :!say hello world\r\n'; sleep 0.2
    printf 'PRIVMSG ftbot :hi there\r\n';   sleep 0.2
    printf 'PRIVMSG ftbot :!bogus\r\n';     sleep 0.3
    printf 'QUIT :bye\r\n';                 sleep 0.3
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo "== server log =="
tail -15 /tmp/ircserv.log
