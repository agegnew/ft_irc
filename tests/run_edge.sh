#!/bin/bash
# Edge-case tests. Each test is one nc invocation; we avoid waiting on two
# parallel ncs by using timeouts on each.
set -u
cd "$(dirname "$0")/.."

pkill -9 -f 'ircserv 6667' 2>/dev/null || true
sleep 0.2
rm -f /tmp/ircserv.log /tmp/edge*.out
./ircserv 6667 secret > /tmp/ircserv.log 2>&1 &
SERVER_PID=$!
sleep 0.3
trap 'kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null' EXIT

run() {
    local name="$1"
    local inputs="$2"
    local timeout="${3:-1}"
    echo "=== $name ==="
    echo -e "$inputs" | nc -w "$timeout" 127.0.0.1 6667 | sed -e 's/\r$//'
    echo ""
}

run "EDGE 1: wrong password (expect 464)" \
'PASS wrongpw\r\nNICK who\r\nUSER u 0 * :r\r\n' 2

run "EDGE 2: valid register + PING (expect 001-004 + PONG)" \
'PASS secret\r\nNICK eve\r\nUSER eve 0 * :E\r\nPING :token\r\nQUIT :bye\r\n' 2

run "EDGE 3: partial-frame subject test (expect one 421 for command)" \
"PASS secret\r\nNICK ed\r\nUSER ed 0 * :E\r\ncom\\c" 0

# The above fails to preserve the flush between 'com', 'man', 'd' because
# echo -e doesn't do timed writes. Use a shell subshell instead.
echo "=== EDGE 3b: partial-frame with real delays ==="
(
  printf 'PASS secret\r\nNICK ed2\r\nUSER ed2 0 * :E\r\n'; sleep 0.3
  printf 'com'; sleep 0.2
  printf 'man'; sleep 0.2
  printf 'd\r\n'; sleep 0.3
  printf 'QUIT :b\r\n'; sleep 0.2
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo ""

run "EDGE 4: unknown command -> 421" \
'PASS secret\r\nNICK u1\r\nUSER u 0 * :u\r\nFLIBBERTIGIBBET\r\nQUIT :b\r\n' 2

run "EDGE 5: pre-registration command -> 451" \
'NICK early\r\nJOIN #x\r\nQUIT :b\r\n' 2

run "EDGE 6: PRIVMSG unknown nick -> 401" \
'PASS secret\r\nNICK s1\r\nUSER s 0 * :S\r\nPRIVMSG ghost :hi\r\nQUIT :b\r\n' 2

run "EDGE 7: invalid nick chars -> 432" \
'PASS secret\r\nNICK 1starts_with_digit\r\nQUIT :b\r\n' 2

run "EDGE 8: MODE inspection sweep" \
'PASS secret\r\nNICK m1\r\nUSER m 0 * :M\r\nJOIN #mm\r\nMODE #mm +itkl pw 5\r\nMODE #mm\r\nMODE #mm -il\r\nMODE #mm\r\nMODE #mm +z\r\nQUIT :b\r\n' 2

run "EDGE 9: PART non-member -> 442" \
'PASS secret\r\nNICK p1\r\nUSER p 0 * :P\r\nPART #nope\r\nQUIT :b\r\n' 2

run "EDGE 10: KICK as non-op -> 482" \
'PASS secret\r\nNICK k1\r\nUSER k 0 * :K\r\nJOIN #kc\r\nMODE #kc -o k1\r\nKICK #kc k1 :gone\r\nQUIT :b\r\n' 2

echo "=== SERVER LOG (tail) ==="
tail -30 /tmp/ircserv.log
