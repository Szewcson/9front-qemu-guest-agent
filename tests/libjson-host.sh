#!/bin/sh
# Explicit host checks of patched native sources, not the live agent.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ] || [ ! -d "$1/sys/src/libjson" ]; then
	echo 'usage: sh tests/libjson-host.sh PATCHED-9FRONT-ROOT [test|sanitize|analyze|thread-sanitize]' >&2
	exit 1
fi
libjson_source=$(CDPATH= cd "$1" && pwd)
libjson_repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
libjson_mode=${2:-test}
case "$libjson_mode" in test|sanitize|analyze|thread-sanitize) ;; *) exit 1 ;; esac
libjson_out=$libjson_repo/build/libjson-host/$libjson_mode
mkdir -p "$libjson_out"
cd "$libjson_repo"
if [ "$libjson_mode" = analyze ]; then
	for libjson_file in json printjson; do
		"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -DLIBJSON_HOST_TEST \
			-Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer \
			-Itests/libjson-host -I"$libjson_source/sys/include" \
			-c "$libjson_source/sys/src/libjson/$libjson_file.c" -o "$libjson_out/$libjson_file.o"
	done
	exit 0
fi
set -- -std=c11 -D_POSIX_C_SOURCE=200809L -DLIBJSON_HOST_TEST \
	-Wall -Wextra -Werror -Wno-unknown-pragmas -O1 -g -pthread
case "$libjson_mode" in
	sanitize) set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie ;;
	thread-sanitize) set -- "$@" -fsanitize=thread -fno-omit-frame-pointer ;;
esac
"${CC:-cc}" "$@" -Itests/libjson-host -I"$libjson_source/sys/include" \
	-o "$libjson_out/test" tests/native-libjson-test.c tests/libjson-host/compat.c \
	"$libjson_source/sys/src/libc/port/rune.c" "$libjson_source/sys/src/libjson/json.c" \
	"$libjson_source/sys/src/libjson/printjson.c"
if [ "$libjson_mode" = sanitize ]; then
	ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$libjson_out/test"
else
	"$libjson_out/test"
fi
