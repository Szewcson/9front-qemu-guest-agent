#!/bin/sh
# Explicit developer download into a NEW directory; no install or VM contact.
set -eu
if [ "$#" -ne 1 ] || [ -z "$1" ] || [ -e "$1" ]; then
	echo 'usage: sh scripts/prepare-libjson.sh NEW-SOURCE-DIRECTORY' >&2
	exit 1
fi
libjson_repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
mkdir -- "$1"
libjson_target=$(CDPATH= cd "$1" && pwd)
mkdir -p "$libjson_target/sys/include" "$libjson_target/sys/src/libjson" "$libjson_target/sys/man/2" "$libjson_target/sys/src/libc/port"
libjson_revision=a411fb63cb27f6f367605b2ddbeaf2161b1dd2c1
for libjson_file in sys/include/json.h sys/src/libjson/json.c sys/src/libjson/printjson.c sys/man/2/json sys/src/libc/port/rune.c; do
	curl --fail --silent --show-error --location --max-time 30 \
		"https://raw.githubusercontent.com/9front/9front/$libjson_revision/$libjson_file" \
		-o "$libjson_target/$libjson_file"
done
cd "$libjson_target"
sha256sum -c "$libjson_repo/patches/libjson/SHA256SUMS"
while IFS= read -r libjson_patch; do
	case "$libjson_patch" in
		0001-object-cleanup-and-escaping.patch|0002-bounded-exact-json.patch) ;;
		*) echo 'unexpected patch name' >&2; exit 1 ;;
	esac
	# Avoid finding the parent QGA Git repository when staging inside build/.
	GIT_CEILING_DIRECTORIES="$libjson_target" git apply --check "$libjson_repo/patches/libjson/$libjson_patch"
	GIT_CEILING_DIRECTORIES="$libjson_target" git apply "$libjson_repo/patches/libjson/$libjson_patch"
done <"$libjson_repo/patches/libjson/series"
printf 'Prepared native source copy: %s\nNo system library or agent changed.\n' "$libjson_target"
