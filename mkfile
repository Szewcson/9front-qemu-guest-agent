# Native 9front build; no pac9 or foreign runtime required.
</$objtype/mkfile

BIN=/$objtype/bin/aux
HFILES=src/qga.h src/json.h src/native.h src/guestinfo.h src/files.h src/clock.h src/base64.h src/process.h
OFILES=main.$O json.$O qga.$O native.$O guestinfo.$O files.$O native-files.$O clock.$O native-clock.$O base64.$O process.$O native-process.$O

all:V: qga9

qga9: $OFILES
	$LD -o $target $prereq

main.$O: src/main.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/main.c

json.$O: src/json.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/json.c

qga.$O: src/qga.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/qga.c

core-test.$O: tests/core-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/core-test.c

native.$O: src/native.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native.c

guestinfo.$O: src/guestinfo.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/guestinfo.c

info-test.$O: tests/info-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/info-test.c

files.$O: src/files.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/files.c

native-files.$O: src/native-files.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-files.c

files-test.$O: tests/files-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/files-test.c

clock.$O: src/clock.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/clock.c

native-clock.$O: src/native-clock.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-clock.c

clock-test.$O: tests/clock-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/clock-test.c

native-clock-test.$O: tests/native-clock-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-clock-test.c

qga9-test: core-test.$O json.$O qga.$O
	$LD -o $target $prereq

qga9-info-test: info-test.$O json.$O guestinfo.$O
	$LD -o $target $prereq

base64.$O: src/base64.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/base64.c

process.$O: src/process.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/process.c

native-process.$O: src/native-process.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-process.c

process-test.$O: tests/process-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/process-test.c

native-process-test.$O: tests/native-process-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-process-test.c

exec-fixture.$O: tests/exec-fixture.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/exec-fixture.c

qga9-exec-fixture: exec-fixture.$O
	$LD -o $target $prereq

qga9-process-test: process-test.$O process.$O base64.$O json.$O qga.$O
	$LD -o $target $prereq

qga9-native-process-test: native-process-test.$O native-process.$O process.$O base64.$O json.$O
	$LD -o $target $prereq

qga9-files-test: files-test.$O files.$O base64.$O json.$O qga.$O
	$LD -o $target $prereq

qga9-clock-test: clock-test.$O clock.$O json.$O qga.$O
	$LD -o $target $prereq

qga9-native-clock-test: native-clock-test.$O native-clock.$O clock.$O
	$LD -o $target $prereq

test:V: qga9-test qga9-info-test qga9-files-test qga9-clock-test qga9-process-test
	./qga9-test
	./qga9-info-test
	./qga9-files-test
	./qga9-clock-test
	./qga9-process-test

install:V: qga9
	if(test -e $BIN/qga9){
		echo 'qga9 already exists; back it up and remove it explicitly before installing' >[1=2]
		exit exists
	}
	cp qga9 $BIN/qga9

uninstall:V:
	rm -f $BIN/qga9

clean:V:
	rm -f $OFILES core-test.$O info-test.$O files-test.$O clock-test.$O native-clock-test.$O process-test.$O native-process-test.$O exec-fixture.$O qga9 qga9-test qga9-info-test qga9-files-test qga9-clock-test qga9-native-clock-test qga9-process-test qga9-native-process-test qga9-exec-fixture
