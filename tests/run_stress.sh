#!/bin/bash
set -u
cd "$(dirname "$0")/.."

pkill -9 -f 'ircserv 6667' 2>/dev/null || true; sleep 0.3

echo "=== arg validation ==="
./ircserv 2>&1 || true
./ircserv 99999 x 2>&1 || true
./ircserv 6667 '' 2>&1 || true
./ircserv notanum pw 2>&1 || true
./ircserv 0 pw 2>&1 || true
echo ""

echo "=== port reuse: start + stop + start (SO_REUSEADDR) ==="
./ircserv 6667 pw > /tmp/reuse1.log 2>&1 &
P=$!; sleep 0.3
kill $P 2>/dev/null; wait $P 2>/dev/null
./ircserv 6667 pw > /tmp/reuse2.log 2>&1 &
P=$!; sleep 0.3
kill $P 2>/dev/null; wait $P 2>/dev/null
cat /tmp/reuse1.log /tmp/reuse2.log
echo ""

echo "=== stress: 100 concurrent connects (no login, just bring up fd + close) ==="
./ircserv 6667 pw > /tmp/stress.log 2>&1 &
SRV=$!; sleep 0.3
for i in $(seq 1 100); do
    ( echo "" | nc -w 1 127.0.0.1 6667 > /dev/null 2>&1 ) &
done
wait
sleep 0.3
# Server should still be alive:
( printf "PASS pw\r\nNICK survived\r\nUSER s 0 * :S\r\nQUIT :ok\r\n"; sleep 0.3 ) \
    | nc -w 2 127.0.0.1 6667 | sed -e "s/\r$//"
echo "--- first and last 10 log lines ---"
head -5 /tmp/stress.log
echo "..."
tail -5 /tmp/stress.log
echo "--- fd count after stress ---"
ls /proc/$SRV/fd 2>/dev/null | wc -l
kill $SRV 2>/dev/null; wait $SRV 2>/dev/null
exit 0
