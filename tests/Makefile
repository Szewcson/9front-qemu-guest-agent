# Optional Linux development tests; NOT a native daemon build.
CC ?= cc
CPPFLAGS += -DQGA_HOST_TEST
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Werror
CORE = ../src/json.c ../src/qga.c
HEADERS = ../src/json.h ../src/qga.h ../src/guestinfo.h ../src/files.h ../src/clock.h ../src/base64.h ../src/process.h
OUT = ../build

.PHONY: all test sanitize analyze
all: $(OUT)/qga9-stream-test $(OUT)/qga9-core-test $(OUT)/qga9-info-test $(OUT)/qga9-files-test $(OUT)/qga9-clock-test $(OUT)/qga9-process-test

$(OUT):
	mkdir -p $(OUT)

$(OUT)/qga9-stream-test: host-main.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ host-main.c $(CORE)

$(OUT)/qga9-core-test: core-test.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ core-test.c $(CORE)

$(OUT)/qga9-info-test: info-test.c ../src/json.c ../src/guestinfo.c $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ info-test.c ../src/json.c ../src/guestinfo.c

$(OUT)/qga9-files-test: files-test.c ../src/files.c ../src/base64.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ files-test.c ../src/files.c ../src/base64.c $(CORE)

$(OUT)/qga9-process-test: process-test.c ../src/process.c ../src/base64.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ process-test.c ../src/process.c ../src/base64.c $(CORE)

$(OUT)/qga9-clock-test: clock-test.c ../src/clock.c $(CORE) $(HEADERS) | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ clock-test.c ../src/clock.c $(CORE)

test: all
	$(OUT)/qga9-core-test
	$(OUT)/qga9-info-test
	$(OUT)/qga9-files-test
	$(OUT)/qga9-clock-test
	$(OUT)/qga9-process-test
	python3 -m unittest discover -s . -p 'test_*.py' -v

sanitize:
	$(MAKE) OUT=../build/sanitize CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' LDFLAGS='-fsanitize=address,undefined -no-pie' all
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-core-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-info-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-files-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-clock-test
	ASAN_OPTIONS=detect_leaks=1 ../build/sanitize/qga9-process-test
	QGA_TEST_BINARY=../build/sanitize/qga9-stream-test ASAN_OPTIONS=detect_leaks=1 python3 -m unittest discover -s . -p 'test_*.py' -v

analyze: | $(OUT)
	mkdir -p $(OUT)/analyze
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c host-main.c -o $(OUT)/analyze/host-main.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c core-test.c -o $(OUT)/analyze/core-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/json.c -o $(OUT)/analyze/json.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/qga.c -o $(OUT)/analyze/qga.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/guestinfo.c -o $(OUT)/analyze/guestinfo.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c info-test.c -o $(OUT)/analyze/info-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/files.c -o $(OUT)/analyze/files.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c files-test.c -o $(OUT)/analyze/files-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/clock.c -o $(OUT)/analyze/clock.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c clock-test.c -o $(OUT)/analyze/clock-test.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/base64.c -o $(OUT)/analyze/base64.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c ../src/process.c -o $(OUT)/analyze/process.o
	$(CC) $(CPPFLAGS) -std=c99 -Wall -Wextra -Werror -fanalyzer -c process-test.c -o $(OUT)/analyze/process-test.o
