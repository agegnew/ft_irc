#!/bin/bash
set -u
cd "$(dirname "$0")/.."
pkill -9 -f 'ircserv 6667' 2>/dev/null || true
sleep 0.3

./ircserv 6667 pw > /tmp/s.log 2>&1 &
SRV=$!
sleep 0.4

echo "fire 50 concurrent connects..."
for i in $(seq 1 50); do
    ( echo | nc -w 1 127.0.0.1 6667 > /dev/null 2>&1 ) &
done
wait
sleep 0.3

echo ""
echo "check server is still responsive after the burst..."
( printf 'PASS pw\r\nNICK alive\r\nUSER a 0 * :A\r\nQUIT :ok\r\n'; sleep 0.5 ) \
    | nc -w 2 127.0.0.1 6667 | sed -e 's/\r$//'

echo ""
echo "log stats:"
wc -l /tmp/s.log 2>/dev/null || echo "(no log)"
echo "log head:"
head -3 /tmp/s.log 2>/dev/null
echo "log tail:"
tail -3 /tmp/s.log 2>/dev/null

kill -TERM $SRV 2>/dev/null
wait $SRV 2>/dev/null
echo ""
echo "final log entry (should be 'shutting down cleanly'):"
tail -1 /tmp/s.log 2>/dev/null
exit 0
