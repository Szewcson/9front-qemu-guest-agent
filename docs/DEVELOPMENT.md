# Development and verification

## Repository boundary and ownership

This repository builds the native userspace daemon and tests. The separate
[driver repository](https://github.com/Szewcson/9front-virtio-serial) owns kernel
code, fixtures and boot helpers; no sibling checkout/kernel headers are required.

`json.c` and `qga.c` own parsing/framing/dispatch; `main.c` owns native channel
setup. The guestinfo, files, clock, process and storage modules pair bounded
logic with native adapters. `base64.c` wraps native libc; `native-fd.c` performs
exact keep-set cleanup in private descriptor tables.
[CODECS](CODECS.md) and [STORAGE](STORAGE.md) record reuse/adapter evidence.

Each `Qga` instance has one synchronous owner. The writer may return a short
write, but must not retain borrowed storage or recursively call that instance.
Zero/error writes are sticky failures. Free instance state once, after use.

## Linux checks

```sh
make -C tests test
make -C tests sanitize
make -C tests analyze
```

Linux runs 11 C suites, a stream harness and 54 Python methods.
GCC/Clang, libresolv, libc iconv and Python are host development dependencies.
The installed daemon uses native 9front tools/runtime.
Sanitizer output is separate and includes ASan/UBSan/leaks; GCC analyzer is
additional evidence, not proof of safety. [STATUS](STATUS.md) records actual runs.

The stream harness supplies no native callbacks. C fixtures cover enabled
handlers with deterministic query/file/clock/process/storage mocks; Python
exercises stream framing and host-script/PVE API boundaries.
Malformed UTF-8/JSON, exact integers/IDs, fragmentation/resync, short I/O,
resource exhaustion, lifecycle cleanup and partial failures are covered.
Mocks never perform real shutdown, clock mutation or PVE RPCs.
High assertion counts include parameterized checks, not independent scenarios.

If JSON::XS is installed, rerun host-script tests using its wire-type behavior:

```sh
QGA_PVE_MOCK_CODEC=xs python3 -m unittest discover -s tests -p test_proxmox_script.py -v
```

Optional parser fuzzing, from this repository with Clang/libFuzzer:

```sh
mkdir -p build
clang -DQGA_HOST_TEST -g -O1 -fsanitize=fuzzer,address,undefined \
  tests/fuzz.c tests/host-rune.c src/json.c src/qga.c -o build/qga9-fuzz
./build/qga9-fuzz -max_len=16384 -runs=100000
```

This recipe is not an executed-test claim. Keep failures as bounded regression
fixtures; fuzzing never calls native power APIs.

## Native checks

```rc
mk
mk test
```

The native target runs 9 portable userspace C suites, including real native
libc base64, rune encoding, integer formatting and the load model.
Two additional Linux suites mock native file and creator syscalls;
their separate real-native regression is explicit.
Native split builds still need a fresh guest run.

Optional native fixtures have separate effects:

| Check | Invocation | Effect |
| --- | --- | --- |
| Raw stream | `rc tests/native-stream.rc` | Private-pipe daemon test |
| Files | `rc tests/native-files.rc` | Creates and retains a synthetic checkout file |
| Processes | `mk qga9-native-process-test qga9-exec-fixture`, then `./qga9-native-process-test` | Starts synthetic children |
| Exec stream | `rc tests/native-exec-stream.rc` | Private daemon/process responsiveness test |
| Storage | `mk qga9-native-storage-test`, then `./qga9-native-storage-test` | Reads/stat-checks native kernel metadata |
| Descriptors | `mk qga9-native-fd-test`, then `./qga9-native-fd-test` | Tests private child keep-set cleanup |
| Load sampler | `mk qga9-native-load-test`, then `./qga9-native-load-test` | Starts one private sampler, reads kernel process metadata and requests stop |
| JSON evaluation | `mk qga9-native-json-eval`, then `./qga9-native-json-eval` | Reproduces libjson incompatibilities |
| File creation guard | `mk qga9-native-files-guard-test`, then `./qga9-native-files-guard-test` | Private namespace/environment; rejects device nodes and a mixed-device union, creates/removes owned synthetic `/tmp` fixtures |
| Startup readiness | `rc tests/native-start.rc` | Privately binds a fake agent and mocks probes/sleep; logs retained, no actual channel opened |
| Clock | `mk qga9-native-clock-test`, then `./qga9-native-clock-test` | Explicitly mutates system clock/RTC |

Default `mk test` does not run the live clock test or change installed service.
Libjson evaluation does not link it into the daemon.
Follow [9FRONT](9FRONT.md) and [PROXMOX](PROXMOX.md) for actual channel tests.

## Pending fixes and release checks

The source audit's startup and rejected-device-create fixes are local;
see [STATUS](STATUS.md). Native regressions and deployment remain pending.
The isolated creator pins the parent and rejects final-parent unions; it does
not require trusting a different MCREATE branch. Its native namespace/srv-board
semantics still require actual guest execution.
Host mocks verify side effects and resource cleanup, not native namespace semantics.

Before release, rerun the independent native build and tests; record source and
binary identity. Validate each advertised command through the real PVE channel,
UI, reconnect, cold startup and rollback. Real QGA shutdown needs approved
filesystem/power verification. Driver reset/fault/unplug/interrupt gates belong
to its repository. Host parser/sanitizer tests do not establish those properties.

Native manuals are in `man/8`; install explicitly with `mk install-man`.
`mk install-service` installs the helper, not its configuration or boot hook.
Native libc now provides base64, rune encoding and decimal formatting; strict
JSON parsing/validation remains local for exact integers and resource bounds.
Upstream kernel integration belongs to the driver repository. Packaging remains
independent of the native source/toolchain/runtime requirement.

Keep credentials/private deployment data out of the repository.
Development export isolation and same-user limits are described in
[SECURITY](SECURITY.md).
