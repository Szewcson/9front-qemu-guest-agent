# Development and verification

## Repository boundary and ownership

This repository builds the native userspace daemon and tests. The separate
[driver repository](https://github.com/Szewcson/9front-virtio-serial) owns kernel
code, fixtures and boot helpers; no sibling checkout/kernel headers are required.

`json.c` adapts patched native libjson; `qga.c` owns framing/dispatch and `main.c` owns native channel
setup. The guestinfo, files, clock, process and storage modules pair bounded
logic with native adapters. `base64.c` wraps native libc; `native-fd.c` performs
exact keep-set cleanup in private descriptor tables.
[CODECS](CODECS.md) and [STORAGE](STORAGE.md) record reuse/adapter evidence.

Each `Qga` instance has one synchronous owner. The writer may return a short
write, but must not retain borrowed storage or recursively call that instance.
Zero/error writes are sticky failures. Free instance state once, after use.

## Linux checks

```sh
mkdir -p build
sh scripts/prepare-libjson.sh build/libjson-source
make -C tests test
make -C tests clang-test
make -C tests sanitize
make -C tests analyze
```

Linux runs 12 C suites, a stream harness and 57 Python methods.
GCC/Clang, libresolv and Python are host development dependencies; source
preparation additionally uses curl, sha256sum and git. Preparation is explicit,
refuses an existing directory and downloads only pinned official native inputs.
Normal tests do not download sources. An existing patched source root can be
selected with `make -C tests JSON_ROOT=/absolute/path ...`.
The Clang check uses the normal C99 flags and a separate output directory, so
existing GCC objects cannot hide compiler-specific failures. Set `CLANG` to an
explicit compiler path if needed.
The installed daemon uses native 9front tools/runtime.
Sanitizer output is separate and includes ASan/UBSan/leaks; GCC analyzer is
additional evidence, not proof of safety. [STATUS](STATUS.md) records actual runs.

The stream harness supplies no native callbacks. C fixtures cover enabled
handlers with deterministic query/file/clock/process/storage mocks; Python
exercises stream framing and host-script/PVE API boundaries.
Malformed UTF-8/JSON, exact integers/IDs, fragmentation/resync, short I/O,
resource exhaustion, lifecycle cleanup and partial failures are covered.
Mocks never perform real shutdown, clock mutation or PVE RPCs.
The core suite covers all 64 combinations of query/load/file/clock/exec/power
backends, checking capability metadata, callback family/operation, and allowed
or refused commands after shutdown. These replace the source-format startup
assertions. Three Python methods also run the core suite's `--stdio-info`
validator on valid and invalid capability responses, including optional load,
duplicate/unknown names and forbidden management commands.
The native duplex-stream fixture checks default shutdown advertisement and
invalid-mode rejection without invoking power operations. Its stdio check first
requires the producer to exit successfully, then validates the complete response
with `./qga9-test --stdio-info`; empty/error output cannot prove exclusion.
High assertion counts include parameterized checks, not independent scenarios.

If JSON::XS is installed, rerun host-script tests using its wire-type behavior:

```sh
QGA_PVE_MOCK_CODEC=xs python3 -m unittest discover -s tests -p test_proxmox_script.py -v
```

Optional parser fuzzing, from this repository with Clang/libFuzzer, after source
preparation:

```sh
mkdir -p build
make -C tests CC=clang OUT=../build/fuzz \
  CFLAGS='-g -O1 -std=c11 -Wall -Wextra -Wno-unknown-pragmas -fsanitize=fuzzer-no-link,address,undefined' \
  ../build/fuzz/qga9-stream-test
clang -DQGA_HOST_TEST -D_POSIX_C_SOURCE=200809L -Itests/libjson-host \
  -Ibuild/libjson-source/sys/include -Wno-unknown-pragmas -g -O1 \
  -fsanitize=fuzzer,address,undefined tests/fuzz.c src/json.c src/qga.c \
  build/fuzz/libjson/*.o -o build/qga9-fuzz
./build/qga9-fuzz -max_len=16384 -runs=100000
```

This recipe is not an executed-test claim. Keep failures as bounded regression
fixtures; fuzzing never calls native power APIs.

## Native checks

```rc
rc tests/native-libjson.rc /path/to/patched-source /path/to/NEW-libjson-output
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a'
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a' test
```

The native target runs 10 portable userspace C suites, including the libjson
adapter, real native libc base64, Unicode and integer boundaries and the load model.
Unqualified `mk` requires an installed patched header/archive. Private archive
selection avoids replacing either system file; use the same variables for every
subsequent native target in that checkout.
Two additional Linux suites mock native file and creator syscalls;
their separate real-native regression is explicit.
The independent native build and these suites passed in the isolated local
guest; see STATUS for exact results, optional fixtures and remaining gates.

Optional native fixtures have separate effects:

| Check | Invocation | Effect |
| --- | --- | --- |
| Raw stream | Build `qga9` and `qga9-test`, then `rc tests/native-stream.rc` | Private-pipe daemon test; retains only synthetic request/reply files in its exclusive `/tmp` fixture directory |
| Stream failure checks | `rc tests/native-stream-errors.rc` | Repeats the private-pipe test with five stdio-only producer faults; requires the expected execution/metadata failure, including failure after a valid response; retains synthetic wrapper/request/reply files |
| Files | `rc tests/native-files.rc` | Creates and retains a synthetic checkout file |
| Processes | `mk qga9-native-process-test qga9-exec-fixture`, then `./qga9-native-process-test` | Starts synthetic children |
| Exec stream | `rc tests/native-exec-stream.rc` | Private daemon/process responsiveness test |
| Storage | `mk qga9-native-storage-test`, then `./qga9-native-storage-test` | Reads/stat-checks native kernel metadata |
| Filesystem provenance | `mk qga9-native-fsinfo-provenance-test`, then `./qga9-native-fsinfo-provenance-test` | Private namespace/leased service board and stock ramfs; demonstrates stale service-name provenance, writes only a synthetic RAM marker |
| Descriptors | `mk qga9-native-fd-test`, then `./qga9-native-fd-test` | Tests private child keep-set cleanup |
| Load sampler | `mk qga9-native-load-test`, then `./qga9-native-load-test` | Starts one private sampler, reads kernel process metadata and requests stop |
| JSON library | `rc tests/native-libjson.rc PATCHED-SOURCE NEW-OUTPUT` | Isolated native library fixture and private archive; no system install |
| File creation guard | `mk qga9-native-files-guard-test`, then `./qga9-native-files-guard-test` | Private namespace/environment; rejects device nodes and a mixed-device union, creates/removes owned synthetic `/tmp` fixtures |
| Startup readiness | `rc tests/native-start.rc` | Privately binds a fake agent and mocks probes/sleep; logs retained, no actual channel opened |
| Clock | `mk qga9-native-clock-test`, then `./qga9-native-clock-test` | Explicitly mutates system clock/RTC |

Default `mk test` does not run the live clock test or change installed service.
It does not run the filesystem-provenance experiment either. That experiment
has a 15-second active-phase alarm and 16 KiB snapshot bound, and releases its
private mount, board and descriptors on normal/error return. An external kill or failed syscall
can bypass explicit cleanup; namespace/descriptor teardown is then the OS's
responsibility. Cleanup syscalls have no separate deadline. See
[STORAGE](STORAGE.md) for what its passing result establishes.
The historical stock-libjson comparison fixture was retired: the patch fixes
the escaping behavior it previously demonstrated, and the original comparison
is still available in Git history.
Follow [9FRONT](9FRONT.md) and [PROXMOX](PROXMOX.md) for actual channel tests.

## Release checks and evidence

The source audit's startup and rejected-device-create fixes passed native
regressions locally and on the Proxmox guest. The updated daemon/helper were
installed there on 2026-10-15. See [STATUS](STATUS.md) for subsequent operator
host/power-cycle/IP/clock results and remaining checked-kernel acceptance limits.
The isolated creator pins the parent and rejects final-parent unions; it does
not require trusting a different MCREATE branch. Its native namespace/srv-board
path passed the explicit mixed-union fixture.
Host mocks verify side effects and resource cleanup, not native namespace semantics.

Before release, rerun the independent native build and tests; record source and
binary identity. Validate each advertised command through the real PVE channel,
UI, reconnect, cold startup and rollback. Local QGA powerdown and the operator's
normal PVE shutdown/start passed; those are not filesystem integrity, warm
reboot or failure-path proofs.
Driver reset/fault/unplug/interrupt gates belong
to its repository. Host parser/sanitizer tests do not establish those properties.

Native manuals are in `man/8`; install explicitly with `mk install-man`.
`mk install-service` installs the helper, not its configuration or boot hook.
Native libc provides base64, Unicode primitives and decimal formatting. The
patched native libjson provides parsing, validation, exact integers and tree/string
formatting; the local adapter enforces QGA limits and owns fixed token/text storage.
Upstream kernel integration belongs to the driver repository. Packaging remains
independent of the native source/toolchain/runtime requirement.

Keep credentials/private deployment data out of the repository.
Development export isolation and same-user limits are described in
[SECURITY](SECURITY.md).
