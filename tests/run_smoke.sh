#!/bin/bash
# Smoke tests for the ircserv mandatory flow. Run from ft_irc/ via WSL:
#   bash tests/run_smoke.sh
set -u
cd "$(dirname "$0")/.."

pkill -f 'ircserv 6667' 2>/dev/null
sleep 0.2
rm -f /tmp/ircserv.log /tmp/test*.out

./ircserv 6667 secret > /tmp/ircserv.log 2>&1 &
SERVER_PID=$!
sleep 0.3

cleanup() { kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null; }
trap cleanup EXIT

echo "=== TEST 1: register + quit (one-shot) ==="
( printf 'PASS secret\r\nNICK alice\r\nUSER alice 0 * :Alice\r\n' ; sleep 0.3 ;
  printf 'QUIT :bye\r\n' ; sleep 0.2 ) \
  | nc -w 1 127.0.0.1 6667 > /tmp/test1.out
cat /tmp/test1.out
echo ""

echo "=== TEST 2: partial frame (the subject's com^D man^D d test) ==="
(
  printf 'PASS secret\r\nNICK bob\r\nUSER bob 0 * :Bob\r\n' ; sleep 0.3
  # Send the bytes of 'PING'\r\n in three separate writes with flushes.
  printf 'PI' ; sleep 0.1
  printf 'N' ; sleep 0.1
  printf 'G :hello\r\n' ; sleep 0.2
  printf 'QUIT :done\r\n' ; sleep 0.2
) | nc -w 2 127.0.0.1 6667 > /tmp/test2.out
cat /tmp/test2.out
echo ""

echo "=== TEST 3: two clients, JOIN #chan, PRIVMSG, KICK ==="
# First client (operator = creator) stays open to receive the PRIVMSG.
(
  printf 'PASS secret\r\nNICK alice\r\nUSER alice 0 * :Alice\r\n'
  sleep 0.3
  printf 'JOIN #test\r\n'
  sleep 0.8
  printf 'MODE #test +k hunter2\r\n'
  sleep 0.3
  printf 'TOPIC #test :Hello world\r\n'
  sleep 0.3
  printf 'KICK #test bob :bye bye\r\n'
  sleep 0.4
  printf 'QUIT :done\r\n'
  sleep 0.2
) | nc -w 4 127.0.0.1 6667 > /tmp/test3_alice.out &
sleep 0.5
(
  printf 'PASS secret\r\nNICK bob\r\nUSER bob 0 * :Bob\r\n'
  sleep 0.2
  # Wrong key first
  printf 'JOIN #test wrongkey\r\n'
  sleep 0.3
  # Correct key
  printf 'JOIN #test hunter2\r\n'
  sleep 0.4
  printf 'PRIVMSG #test :hi alice\r\n'
  sleep 0.4
  # Alice will KICK us now
  sleep 0.8
  printf 'QUIT :k\r\n'
  sleep 0.2
) | nc -w 4 127.0.0.1 6667 > /tmp/test3_bob.out
wait
echo "-- alice sees:"; sed -e 's/\r$//' /tmp/test3_alice.out
echo "-- bob sees:";   sed -e 's/\r$//' /tmp/test3_bob.out
echo ""

echo "=== SERVER LOG ==="
cat /tmp/ircserv.log
