#!/bin/bash
set -u
cd "$(dirname "$0")/.."
pkill -9 -f 'ircserv 6667' 2>/dev/null || true
sleep 0.3
./ircserv 6667 pw > /tmp/final.log 2>&1 &
SRV=$!
sleep 0.3

(
    printf 'PASS pw\r\n'
    printf 'NICK validator\r\n'
    printf 'USER v 0 * :V\r\n'
    printf 'JOIN #final\r\n'
    printf 'MODE #final +itk secret\r\n'
    printf 'TOPIC #final :ft_irc ships\r\n'
    printf 'PRIVMSG #final :self test\r\n'
    printf 'QUIT :done\r\n'
    sleep 0.5
) | nc -w 2 127.0.0.1 6667 > /tmp/final.client 2>&1

echo "=== CLIENT OUTPUT ==="
sed -e 's/\r$//' /tmp/final.client

kill "$SRV" 2>/dev/null || true
wait "$SRV" 2>/dev/null || true

echo ""
echo "=== SERVER LOG ==="
cat /tmp/final.log

exit 0
