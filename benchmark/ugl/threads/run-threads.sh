#!/bin/sh
# Multithread stress test of the JIT: random vector programs, run by several threads of a ZTS build
# while the tracing JIT compiles them, each thread checked against the interpreter.
#
#   benchmark/ugl/threads/run-threads.sh /path/to/ugl /path/to/thr [SEEDS [ROUNDS [THREADS]]]
#
# /path/to/ugl: any UGL build (the interpreter result is the reference, taken with opcache off).
# /path/to/thr: the embed program, see the comment in thr.c for how to build it (ZTS, opcache).
# Defaults: 24 programs, 3 rounds, 8 threads: 576 thread runs. Every thread must print the hash the
# interpreter printed; a wrong, missing or interleaved line is reported and the exit status is 1.
UGL=${1:?usage: run-threads.sh /path/to/ugl /path/to/thr [seeds [rounds [threads]]]}
THR=${2:?usage: run-threads.sh /path/to/ugl /path/to/thr [seeds [rounds [threads]]]}
SEEDS=${3:-24}
ROUNDS=${4:-3}
THREADS=${5:-8}
DIR=$(dirname "$0")
WORK=$(mktemp -d)
cat > "$WORK/jit.ini" <<INI
opcache.enable=1
opcache.enable_cli=1
opcache.jit=tracing
opcache.jit_buffer_size=64M
opcache.jit_hot_loop=1
opcache.jit_hot_func=1
opcache.jit_hot_side_exit=1
opcache.jit_hot_return=1
opcache.file_update_protection=0
memory_limit=-1
INI
seed=1
while [ "$seed" -le "$SEEDS" ]; do
	# a plain script; one echo, so that the lines of the threads do not interleave
	"$UGL" -n "$DIR/../fuzz/fuzz.php" x "$seed" script $((seed % 2)) | sed 's/^echo run(), "\\n";$/echo run() . "\\n";/' > "$WORK/p$seed.php"
	"$UGL" -n -d opcache.enable_cli=0 "$WORK/p$seed.php" > "$WORK/ref$seed.txt" 2>&1
	seed=$((seed + 1))
done
bad=0; total=0
round=1
while [ "$round" -le "$ROUNDS" ]; do
	seed=1
	while [ "$seed" -le "$SEEDS" ]; do
		ref=$(cat "$WORK/ref$seed.txt")
		good=$(PHPINI="$WORK/jit.ini" WORK="$WORK/p$seed.php" THREADS="$THREADS" "$THR" 2>&1 | grep -c "^$ref$")
		total=$((total + THREADS))
		if [ "$good" != "$THREADS" ]; then
			echo "program $seed, round $round: $good of $THREADS threads gave the reference result"
			bad=$((bad + 1))
		fi
		seed=$((seed + 1))
	done
	round=$((round + 1))
done
rm -rf "$WORK"
echo "$total thread runs, $bad program runs with a wrong or missing result"
[ "$bad" -eq 0 ]
