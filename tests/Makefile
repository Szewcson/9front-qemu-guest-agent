# Optional Linux development tests; NOT a native daemon build.
CC ?= cc
CLANG ?= clang
JSON_ROOT ?= ../build/libjson-source
CPPFLAGS += -DQGA_HOST_TEST -D_POSIX_C_SOURCE=200809L -Ilibjson-host -I$(JSON_ROOT)/sys/include
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas
JSON_OBJECTS = $(OUT)/libjson/json.o $(OUT)/libjson/printjson.o $(OUT)/libjson/rune.o $(OUT)/libjson/compat.o
CORE = ../src/json.c ../src/qga.c $(JSON_OBJECTS)
BASE64 = ../src/base64.c host-base64.c
BASE64_LIBS = -lresolv
HEADERS = ../src/qjson.h ../src/qga.h ../src/guestinfo.h ../src/files.h ../src/clock.h ../src/base64.h ../src/process.h ../src/storage.h $(JSON_ROOT)/sys/include/json.h libjson-host/u.h libjson-host/libc.h
OUT = ../build

.PHONY: all test clang-test sanitize analyze
all: $(OUT)/qga9-native-files-test $(OUT)/qga9-json-codec-test $(OUT)/qga9-native-create-test $(OUT)/qga9-load-test $(OUT)/qga9-json-adapter-test
all: $(OUT)/qga9-stream-test $(OUT)/qga9-core-test $(OUT)/qga9-info-test $(OUT)/qga9-files-test $(OUT)/qga9-clock-test $(OUT)/qga9-process-test $(OUT)/qga9-storage-test $(OUT)/qga9-base64-test

$(OUT):
	mkdir -p $(OUT)

$(JSON_ROOT)/sys/include/json.h:
	@echo 'Prepare the pinned native libjson sources first: sh scripts/prepare-libjson.sh build/libjson-source (from the repository root)' >&2
	@exit 1

$(OUT)/libjson/json.o: $(JSON_ROOT)/sys/src/libjson/json.c $(HEADERS) | $(OUT)
	mkdir -p $(OUT)/libjson
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wno-unknown-pragmas -DLIBJSON_HOST_TEST -c $< -o $@

$(OUT)/libjson/printjson.o: $(JSON_ROOT)/sys/src/libjson/printjson.c $(HEADERS) | $(OUT)
	mkdir -p $(OUT)/libjson
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wno-unknown-pragmas -DLIBJSON_HOST_TEST -c $< -o $@

$(OUT)/libjson/rune.o: $(JSON_ROOT)/sys/src/libc/port/rune.c $(HEADERS) | $(OUT)
	mkdir -p $(OUT)/libjson
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wno-unknown-pragmas -DLIBJSON_HOST_TEST -c $< -o $@

$(OUT)/libjson/compat.o: libjson-host/compat.c $(HEADERS) | $(OUT)
	mkdir -p $(OUT)/libjson
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wno-unknown-pragmas -DLIBJSON_HOST_TEST -c $< -o $@

$(OUT)/qga9-native-files-test: native-files-test.c ../src/native-files.c ../src/files.c $(BASE64) $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ native-files-test.c ../src/files.c $(BASE64) $(CORE) $(BASE64_LIBS)

$(OUT)/qga9-native-create-test: native-create-test.c ../src/native-create.c ../src/native-create.h $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ native-create-test.c

$(OUT)/qga9-load-test: load-test.c ../src/load.c ../src/load.h $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ load-test.c ../src/load.c $(CORE)

$(OUT)/qga9-stream-test: host-main.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ host-main.c $(CORE)

$(OUT)/qga9-core-test: core-test.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ core-test.c $(CORE)

$(OUT)/qga9-json-codec-test: json-codec-test.c ../src/json.c $(JSON_OBJECTS) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ json-codec-test.c ../src/json.c $(JSON_OBJECTS)

$(OUT)/qga9-json-adapter-test: json-adapter-test.c ../src/json.c $(JSON_OBJECTS) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ json-adapter-test.c ../src/json.c $(JSON_OBJECTS)

$(OUT)/qga9-info-test: info-test.c ../src/json.c ../src/guestinfo.c $(JSON_OBJECTS) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ info-test.c ../src/json.c ../src/guestinfo.c $(JSON_OBJECTS)

$(OUT)/qga9-files-test: files-test.c ../src/files.c $(BASE64) $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ files-test.c ../src/files.c $(BASE64) $(CORE) $(BASE64_LIBS)

$(OUT)/qga9-process-test: process-test.c ../src/process.c $(BASE64) $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ process-test.c ../src/process.c $(BASE64) $(CORE) $(BASE64_LIBS)

$(OUT)/qga9-base64-test: base64-test.c $(BASE64) ../src/json.c $(JSON_OBJECTS) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ base64-test.c $(BASE64) ../src/json.c $(JSON_OBJECTS) $(BASE64_LIBS)

$(OUT)/qga9-clock-test: clock-test.c ../src/clock.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ clock-test.c ../src/clock.c $(CORE)

$(OUT)/qga9-storage-test: storage-test.c ../src/storage.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ storage-test.c ../src/storage.c $(CORE)

test: all
	$(OUT)/qga9-json-adapter-test
	$(OUT)/qga9-load-test
	$(OUT)/qga9-native-create-test
	$(OUT)/qga9-json-codec-test
	$(OUT)/qga9-native-files-test
	$(OUT)/qga9-core-test
	$(OUT)/qga9-info-test
	$(OUT)/qga9-files-test
	$(OUT)/qga9-clock-test
	$(OUT)/qga9-process-test
	$(OUT)/qga9-storage-test
	$(OUT)/qga9-base64-test
	QGA_TEST_BINARY=$(OUT)/qga9-stream-test QGA_CORE_TEST_BINARY=$(OUT)/qga9-core-test python3 -m unittest discover -s . -p 'test_*.py' -v

clang-test:
	$(MAKE) JSON_ROOT='$(JSON_ROOT)' CC='$(CLANG)' OUT='$(OUT)/clang' test

sanitize:
	$(MAKE) JSON_ROOT='$(JSON_ROOT)' OUT=../build/sanitize CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' LDFLAGS='-fsanitize=address,undefined -no-pie' all
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-json-adapter-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-load-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-native-create-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-json-codec-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-native-files-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-core-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-info-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-files-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-clock-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-process-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-storage-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-base64-test
	QGA_TEST_BINARY=../build/sanitize/qga9-stream-test QGA_CORE_TEST_BINARY=../build/sanitize/qga9-core-test ASAN_OPTIONS=detect_leaks=1 python3 -m unittest discover -s . -p 'test_*.py' -v

analyze: | $(OUT)
	mkdir -p $(OUT)/analyze
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c json-adapter-test.c -o $(OUT)/analyze/json-adapter-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/load.c -o $(OUT)/analyze/load.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c load-test.c -o $(OUT)/analyze/load-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c native-create-test.c -o $(OUT)/analyze/native-create-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c json-codec-test.c -o $(OUT)/analyze/json-codec-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c native-files-test.c -o $(OUT)/analyze/native-files-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c host-main.c -o $(OUT)/analyze/host-main.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c core-test.c -o $(OUT)/analyze/core-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/json.c -o $(OUT)/analyze/json.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/qga.c -o $(OUT)/analyze/qga.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/guestinfo.c -o $(OUT)/analyze/guestinfo.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c info-test.c -o $(OUT)/analyze/info-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/files.c -o $(OUT)/analyze/files.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c files-test.c -o $(OUT)/analyze/files-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/clock.c -o $(OUT)/analyze/clock.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c clock-test.c -o $(OUT)/analyze/clock-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/base64.c -o $(OUT)/analyze/base64.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c host-base64.c -o $(OUT)/analyze/host-base64.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c base64-test.c -o $(OUT)/analyze/base64-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/process.c -o $(OUT)/analyze/process.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c process-test.c -o $(OUT)/analyze/process-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c ../src/storage.c -o $(OUT)/analyze/storage.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas -fanalyzer -c storage-test.c -o $(OUT)/analyze/storage-test.o
