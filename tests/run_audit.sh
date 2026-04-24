#!/bin/bash
# Targeted regression tests for the audit fixes.
set -u
cd "$(dirname "$0")/.."

pkill -9 -f 'ircserv 6667' 2>/dev/null || true; sleep 0.2
rm -f /tmp/audit*.out

./ircserv.bonus 6667 secret > /tmp/audit.log 2>&1 &
SRV=$!
sleep 0.3
trap 'kill "$SRV" 2>/dev/null; wait "$SRV" 2>/dev/null' EXIT

one() {
    local name="$1"; local inputs="$2"; local to="${3:-2}"
    echo "=== $name ==="
    echo -e "$inputs" | nc -w "$to" 127.0.0.1 6667 | sed -e 's/\r$//'
    echo ""
}

# Fix 1: nick collision against reserved bot nick
one "FIX 1: NICK ftbot (bot-reserved nick) → expect 433" \
'PASS secret\r\nNICK ftbot\r\nUSER u 0 * :u\r\nQUIT :b\r\n'

# Also test nick collision case-insensitively
one "FIX 1b: NICK FtBoT (case-insensitive) → expect 433" \
'PASS secret\r\nNICK FtBoT\r\nUSER u 0 * :u\r\nQUIT :b\r\n'

# Fix 2: CAP END before welcome
echo "=== FIX 2: CAP LS → welcome deferred until CAP END ==="
(
    printf 'CAP LS 302\r\n';        sleep 0.3
    printf 'PASS secret\r\n';       sleep 0.1
    printf 'NICK hex\r\n';          sleep 0.1
    printf 'USER hex 0 * :H\r\n';   sleep 0.3
    # At this point welcome should NOT have fired yet (cap still negotiating).
    printf 'CAP END\r\n';           sleep 0.3
    # Now welcome should arrive.
    printf 'QUIT :b\r\n';           sleep 0.2
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo ""

# Fix 3: MODE on another user nick → 502
echo "=== FIX 3: MODE other_nick → expect 502 ERR_USERSDONTMATCH ==="
(
    printf 'PASS secret\r\nNICK me\r\nUSER m 0 * :M\r\n'; sleep 0.3
    printf 'MODE otheruser +i\r\n';  sleep 0.3
    printf 'MODE me +i\r\n';         sleep 0.3
    printf 'QUIT :b\r\n';            sleep 0.2
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo ""

# Fix 4: MODE partial success before 461
echo "=== FIX 4: MODE +ik no-key → +i applied and broadcast, then 461 ==="
(
    printf 'PASS secret\r\nNICK z\r\nUSER z 0 * :Z\r\n'; sleep 0.3
    printf 'JOIN #a\r\n'; sleep 0.3
    # +i takes no arg, +k needs one but we provide none → partial success + 461
    printf 'MODE #a +ik\r\n'; sleep 0.3
    printf 'MODE #a\r\n';  sleep 0.3
    printf 'QUIT :b\r\n';  sleep 0.2
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo ""

# Fix 5: long nick (>9 chars, <=30) accepted
one "FIX 5: NICK very_long_name_29 (extended limit) → expect welcome" \
'PASS secret\r\nNICK abcdefghijklmnop\r\nUSER a 0 * :A\r\nQUIT :b\r\n'

# Fix 5b: oversized nick (>30) still rejected
one "FIX 5b: NICK with 31 chars → expect 432" \
'PASS secret\r\nNICK aaaaaaaaaabbbbbbbbbbccccccccccX\r\nQUIT :b\r\n'

# Fix 6: PRIVMSG with only trailing → 411 (not 412)
one "FIX 6: PRIVMSG :hi (no target) → expect 411" \
'PASS secret\r\nNICK q\r\nUSER q 0 * :Q\r\nPRIVMSG :hi\r\nQUIT :b\r\n'

# Fix 7: JOIN 0 leaves all channels
echo "=== FIX 7: JOIN 0 parts every channel the user is in ==="
(
    printf 'PASS secret\r\nNICK roam\r\nUSER r 0 * :R\r\n'; sleep 0.3
    printf 'JOIN #one\r\n';   sleep 0.2
    printf 'JOIN #two\r\n';   sleep 0.2
    printf 'JOIN #three\r\n'; sleep 0.3
    printf 'JOIN 0\r\n';      sleep 0.4
    printf 'QUIT :b\r\n';     sleep 0.2
) | nc -w 3 127.0.0.1 6667 | sed -e 's/\r$//'
echo ""

echo "=== server log (tail) ==="
tail -20 /tmp/audit.log
