#!/bin/sh
# Runs random vector programs (fuzz.php) with the interpreter and with the tracing JIT, compares the
# result of every seed, with the vectors in local variables and in typed properties.
#
#   benchmark/ugl/fuzz/run-fuzz.sh /path/to/ugl [FIRST_SEED [LAST_SEED]]
#
# Prints every seed whose results differ and exits with 1 if there is one. 400 seeds take a few
# minutes. The thresholds are 1 so that every loop is compiled at once.
UGL=${1:?usage: run-fuzz.sh /path/to/ugl [first [last]]}
FIRST=${2:-1}
LAST=${3:-400}
DIR=$(dirname "$0")
JIT="-d opcache.enable_cli=1 -d opcache.jit=tracing -d opcache.jit_buffer_size=64M -d opcache.jit_hot_loop=1 \
-d opcache.jit_hot_func=1 -d opcache.jit_hot_side_exit=1 -d opcache.file_update_protection=0"
bad=0; errors=0; runs=0
seed=$FIRST
while [ "$seed" -le "$LAST" ]; do
	for props in 0 1; do
		ref=$("$UGL" -n -d opcache.enable_cli=0 "$DIR/fuzz.php" x "$seed" x "$props" 2>&1 | head -1)
		# shellcheck disable=SC2086
		jit=$("$UGL" -n $JIT "$DIR/fuzz.php" x "$seed" x "$props" 2>&1 | head -1)
		runs=$((runs + 1))
		case "$ref" in *rror*|*xception*) errors=$((errors + 1));; esac
		if [ "$ref" != "$jit" ]; then
			echo "DIFFERENT seed=$seed props=$props: $ref | $jit"
			bad=$((bad + 1))
		fi
	done
	seed=$((seed + 1))
done
echo "$runs programs, $bad different, $errors ended in an error in the interpreter"
[ "$bad" -eq 0 ]
