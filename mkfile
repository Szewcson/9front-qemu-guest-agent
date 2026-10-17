# Native 9front build.
</$objtype/mkfile

BIN=/$objtype/bin/aux
JSONINCLUDE=/sys/include
JSONLIB=/$objtype/lib/libjson.a
CFLAGS=$CFLAGS -I$JSONINCLUDE
HFILES=src/qga.h src/qjson.h src/native.h src/guestinfo.h src/files.h src/clock.h src/base64.h src/process.h src/storage.h src/native-create.h src/load.h $JSONINCLUDE/json.h
OFILES=main.$O json.$O qga.$O native.$O guestinfo.$O files.$O native-files.$O native-create.$O clock.$O native-clock.$O base64.$O process.$O native-process.$O native-fd.$O storage.$O native-storage.$O load.$O native-load.$O

all:V: qga9

qga9: $OFILES $JSONLIB
	$LD -o $target $prereq

load.$O: src/load.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/load.c

native-load.$O: src/native-load.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-load.c

load-test.$O: tests/load-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/load-test.c

qga9-load-test: load-test.$O load.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

native-load-test.$O: tests/native-load-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-load-test.c

qga9-native-load-test: native-load-test.$O native-load.$O native-fd.$O load.$O json.$O $JSONLIB
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

native-create.$O: src/native-create.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-create.c

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

qga9-test: core-test.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

json-codec-test.$O: tests/json-codec-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/json-codec-test.c

qga9-json-codec-test: json-codec-test.$O json.$O $JSONLIB
	$LD -o $target $prereq

json-adapter-test.$O: tests/json-adapter-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/json-adapter-test.c

qga9-json-adapter-test: json-adapter-test.$O json.$O $JSONLIB
	$LD -o $target $prereq

qga9-info-test: info-test.$O json.$O guestinfo.$O $JSONLIB
	$LD -o $target $prereq

base64.$O: src/base64.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/base64.c

process.$O: src/process.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/process.c

native-process.$O: src/native-process.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-process.c

native-fd.$O: src/native-fd.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-fd.c

process-test.$O: tests/process-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/process-test.c

native-process-test.$O: tests/native-process-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-process-test.c

exec-fixture.$O: tests/exec-fixture.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/exec-fixture.c

qga9-exec-fixture: exec-fixture.$O $JSONLIB
	$LD -o $target $prereq

qga9-process-test: process-test.$O process.$O base64.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

qga9-native-process-test: native-process-test.$O native-process.$O native-fd.$O process.$O base64.$O json.$O $JSONLIB
	$LD -o $target $prereq

qga9-files-test: files-test.$O files.$O base64.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

native-files-guard-test.$O: tests/native-files-guard-test.c src/native-files.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-files-guard-test.c

qga9-native-files-guard-test: native-files-guard-test.$O native-create.$O native-fd.$O files.$O base64.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

qga9-clock-test: clock-test.$O clock.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

qga9-native-clock-test: native-clock-test.$O native-clock.$O clock.$O $JSONLIB
	$LD -o $target $prereq

storage.$O: src/storage.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/storage.c

native-storage.$O: src/native-storage.c $HFILES
	$CC $CFLAGS -Isrc -o $target src/native-storage.c

storage-test.$O: tests/storage-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/storage-test.c

qga9-storage-test: storage-test.$O storage.$O json.$O qga.$O $JSONLIB
	$LD -o $target $prereq

native-storage-test.$O: tests/native-storage-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-storage-test.c

qga9-native-storage-test: native-storage-test.$O native-storage.$O storage.$O json.$O $JSONLIB
	$LD -o $target $prereq

# Explicit experiment only: private namespace and disposable ramfs service.
native-fsinfo-provenance-test.$O: tests/native-fsinfo-provenance-test.c
	$CC $CFLAGS -o $target tests/native-fsinfo-provenance-test.c

qga9-native-fsinfo-provenance-test: native-fsinfo-provenance-test.$O $JSONLIB
	$LD -o $target $prereq

native-fd-test.$O: tests/native-fd-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/native-fd-test.c

qga9-native-fd-test: native-fd-test.$O native-fd.$O $JSONLIB
	$LD -o $target $prereq

base64-test.$O: tests/base64-test.c $HFILES
	$CC $CFLAGS -Isrc -o $target tests/base64-test.c

qga9-base64-test: base64-test.$O base64.$O json.$O $JSONLIB
	$LD -o $target $prereq

test:V: qga9-test qga9-info-test qga9-files-test qga9-clock-test qga9-process-test qga9-storage-test qga9-base64-test qga9-json-codec-test qga9-load-test qga9-json-adapter-test
	./qga9-json-adapter-test
	./qga9-load-test
	./qga9-json-codec-test
	./qga9-test
	./qga9-info-test
	./qga9-files-test
	./qga9-clock-test
	./qga9-process-test
	./qga9-storage-test
	./qga9-base64-test

install:V: qga9
	if(test -e $BIN/qga9){
		echo 'qga9 already exists; back it up and remove it explicitly before installing' >[1=2]
		exit exists
	}
	cp qga9 $BIN/qga9

uninstall:V:
	rm -f $BIN/qga9

# Explicit auxiliary installation; default build/test never writes system paths.
install-man:V:
	if(test -e /sys/man/8/qga9 || test -e /sys/man/8/qga9-start){
		echo 'qga9 manual already exists; preserve it before an explicit update' >[1=2]
		exit exists
	}
	cp man/8/qga9 man/8/qga9-start /sys/man/8/

install-service:V:
	if(test -e /rc/bin/qga9-start){
		echo 'qga9-start already exists; preserve it before an explicit update' >[1=2]
		exit exists
	}
	cp scripts/start.rc /rc/bin/qga9-start
	chmod 755 /rc/bin/qga9-start

clean:V:
	rm -f json-adapter-test.$O qga9-json-adapter-test
	rm -f native-fsinfo-provenance-test.$O qga9-native-fsinfo-provenance-test
	rm -f native-load-test.$O qga9-native-load-test
	rm -f load-test.$O qga9-load-test
	rm -f json-codec-test.$O qga9-json-codec-test
	rm -f native-files-guard-test.$O qga9-native-files-guard-test
	rm -f $OFILES core-test.$O info-test.$O files-test.$O clock-test.$O native-clock-test.$O process-test.$O native-process-test.$O exec-fixture.$O storage-test.$O native-storage-test.$O native-fd-test.$O base64-test.$O qga9 qga9-test qga9-info-test qga9-files-test qga9-clock-test qga9-native-clock-test qga9-native-process-test qga9-process-test qga9-exec-fixture qga9-storage-test qga9-native-storage-test qga9-native-fd-test qga9-base64-test
