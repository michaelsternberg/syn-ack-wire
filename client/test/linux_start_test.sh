#!/bin/bash
# Start-up checks of the linux client: a clear message and a failing exit before curses takes the screen.
#   test/linux_start_test.sh      (needs build/linux/syn-ack-wire-linux: make linux)
cd "$(dirname "$0")/.."
BIN=build/linux/syn-ack-wire-linux
fails=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fails=$((fails+1)); fi; }
[ -x $BIN ] || { echo "FAIL: $BIN is not built (make linux)"; exit 1; }

out=$($BIN --help 2>&1); rc=$?
check "--help exits 0 and shows usage" '[ $rc -eq 0 ] && echo "$out" | grep -q "usage:"'

out=$($BIN </dev/null 2>&1); rc=$?
check "no terminal: says so, exits 1" '[ $rc -eq 1 ] && echo "$out" | grep -q "needs a terminal"'

out=$(script -qec "FN_PORT=/nonexistent/pty $BIN" /dev/null 2>&1); rc=$?
check "missing PTY: names the path, exits 1" 'echo "$out" | grep -q "/nonexistent/pty" && echo "$out" | grep -q "start fujinet-nio first"'
check "missing PTY exit status" '[ $rc -eq 1 ]'

out=$(script -qec "$BIN --port /nonexistent/other" /dev/null 2>&1)
check "--port beats FN_PORT's default" 'echo "$out" | grep -q "/nonexistent/other"'

touch /tmp/nl_not_a_pty.$$
out=$(timeout 10 script -qec "$BIN --port /tmp/nl_not_a_pty.$$" /dev/null 2>&1); rc=$?
rm -f /tmp/nl_not_a_pty.$$
check "a --port that is not a terminal is refused at start" '[ $rc -eq 1 ] && echo "$out" | grep -q "not a terminal"'

out=$(script -qec "$BIN --bogus" /dev/null 2>&1); rc=$?
check "an unknown option is refused" '[ $rc -eq 2 ] && echo "$out" | grep -q "usage:"'

out=$(script -qec "$BIN --server notaport" /dev/null 2>&1); rc=$?
check "--server wants HOST:PORT" '[ $rc -eq 2 ] && echo "$out" | grep -q "HOST:PORT"'

[ $fails -eq 0 ] && echo "PASS" || { echo "$fails failed"; exit 1; }
