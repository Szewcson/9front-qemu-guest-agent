# Development and verification

## Organization

- `src/json.c`: bounded strict JSON tree, Unicode validation and ASCII serialization.
- `src/qga.c`: byte framing, resynchronization, command policy and handlers.
- `src/main.c`: stock 9front serial/stdio entry point and resource cleanup.
- `src/native.c`: bounded native data queries and opt-in fixed-argv shutdown.
- `src/guestinfo.c`: bounded CPU/route snapshot decoders and timezone JSON.
- `src/files.c`, `src/native-files.c`: single-owner opaque file handles, bounded
  base64 and native descriptor-based validation/create/truncate/seek.
- `src/clock.c`, `src/native-clock.c`: testable clock/RTC transaction and native
  fixed-device implementation, with explicit partial failures.
- `src/base64.c`: shared canonical bounded binary codec.
- `src/process.c`, `src/native-process.c`: bounded job ownership and private
  supervisors, exec handshake, concurrent pipe draining and cancellation.
- `src/qga.h`: single-owner transport-neutral feed/write API.
- `mkfile`: native build/test/install/uninstall, no foreign runtime.
- `tests/core-test.c`: portable core checks, natively runnable with `mk test`.
- `tests/info-test.c`: the same CPU/timezone/route fixtures under native libc and
  Linux sanitizers; native IP formatting is compared with the host test shim.
- `tests/native-stream.rc`: native duplex raw-stream smoke test, no UART use.
- `tests/files-test.c`, `tests/clock-test.c`: native-compatible bounded transfer,
  handle lifecycle, overflow/argument validation and clock failure-order mocks.
- `tests/native-files.rc`: live six-operation binary round-trip; retains fixture.
- `tests/native-clock-test.c`: explicitly invoked live system/RTC mutation test;
  never automatically run by `mk test`.
- `tests/process-test.c`: native-compatible exec validation/ownership mocks.
- `tests/native-process-test.c`, `tests/exec-fixture.c`: native argv/env/stdin,
  capture, descriptor hygiene, flood output, reaping and owner-death cleanup.
- `tests/native-exec-stream.rc`: actual daemon responsiveness while a job runs.
- `scripts/test-proxmox.pl`: host integration via PVE's synchronized client;
  synthetic file mutation, optional explicit current-time clock mutation.
- `scripts/test-proxmox-exec.pl`: separate host exec smoke test, no host-file reads.
- `tests/test_proxmox_script.py`, `tests/pve-mock/`: optional Perl-helper API and
  cleanup regression tests; deliberately fake PVE, never evidence of live QGA.
- `tests/host-main.c`: Linux-only stream harness, never the delivered daemon.
- `tests/test_protocol.py`: real harness requests/replies, malformed cases and
  deterministic random-input recovery. Python is development-only.
- `tests/fuzz.c`: optional libFuzzer entry point for the same C core.

The writer is synchronous and may return a short write. It must not retain its
borrowed response pointer or recursively call the instance. A zero/error write
is fatal and sticky. Protocol state is freed exactly once by its owner. A future
virtio transport can reuse this API without changing command handlers.

## Tests on Linux

```sh
make -C tests test
make -C tests sanitize
make -C tests analyze
```

GCC/Clang and optional Python are required only on the development host. The
sanitizer target uses separate output files and ASan/UBSan with leak checks.
`analyze` uses GCC's `-fanalyzer` and compiles to temporary object files (analyzer
passes are not a proof of memory safety). See STATUS for what was actually run.

The suite exercises sync bounds/type checking, delimited sync metadata/framing,
ping/info, arbitrary/escaped transaction IDs, adjacent/multiline/fragmented
objects, partial writes, invalid UTF-8/surrogates/JSON, duplicate decoded keys,
oversize/depth/token limits, truncated input, error classes, unknown/excluded
commands, resync after garbage and prompt replies without waiting for EOF.

The minimal Linux stream harness intentionally registers no native callbacks;
its native query/file/clock/exec/shutdown requests return `Unsupported`. Native-compatible C mocks
also verify enabled metadata, real handler dispatch, query failures/overflow,
default powerdown/reboot modes, halt rejection, executor-failure retry, duplicate
shutdown prevention and no success reply. Mocks never call native power APIs.
File mocks cover 16-handle exhaustion, single cleanup, stale handles, invalid
canonical base64, binary data, short/error reads/writes, EOF, seek modes,
integer/path/chunk boundaries and no file mutation after shutdown starts. Clock
mocks cover permission acquisition/cleanup, supplied/RTC-derived time, overflow,
system-before-RTC ordering and explicit partial failure. High assertion counts
include byte-by-byte fragmented request feeds, not distinct feature scenarios.
Host-script mocks cross a real JSON boundary and check integer wire types,
structured QGA error propagation and cleanup. If JSON::XS is available on the
development host, run the same script suite with PVE-like scalar encoding:

```sh
QGA_PVE_MOCK_CODEC=xs python3 -m unittest discover -s tests -p test_proxmox_script.py -v
```

XS is optional, development-only; native guest code needs no Perl runtime.
An isolated temporary XS checkout can be supplied through `PERL5LIB`. The test
suite adds only its own fake-PVE directory in front of that search path.
Native UART reconnect, UI and actual shutdown need the live procedure; parser
sentinel recovery is not a transport test.

Optional coverage-guided fuzzing with an installed Clang/libFuzzer toolchain:

```sh
clang -DQGA_HOST_TEST -g -O1 -fsanitize=fuzzer,address,undefined \
  tests/fuzz.c src/json.c src/qga.c -o build/qga9-fuzz
./build/qga9-fuzz -max_len=16384 -runs=100000
```

Create `build` first if it is missing. This recipe is optional and is not recorded
as executed unless the compiler and run were available. Keep failures as small
regression fixtures; never call native shutdown during fuzzing.

## Native and live release gates

1. Native `mk` and `mk test` on stock 9front amd64. Record toolchain and source
   revision, complete diagnostics, binary identity and exact commands.
2. Verify QEMU ISA mapping, guest UART/control/status and permissions. Run the
   foreground native daemon and `qm agent VMID ping`/`info`; save logs and UI
   observations. See TRANSPORT/PROXMOX. Do not claim this gate from host tests.
3. Verify native OS/host/time reporting and bounded `/net/ipifc`/Ethernet discovery;
   add more multiple-interface/failure fixtures. The inspected `readipifc` source
   had unchecked allocation dereferences, so this project uses its own bounded
   status reader and checked allocation, with native libip address/mask parsing.
4. Verify Proxmox OS/network commands and UI IP display, including IPv4/IPv6,
   loopback, absent MACs, multiple addresses/interfaces and error handling.
5. Verify the separately gated fixed-argv shutdown executor's native filesystem
   flush and actual power behavior. Test host state on a disposable/approved VM;
   no-success-reply mock tests are not evidence of flush or poweroff completion.
6. Re-run safety/access review, native tests, fuzzing and live reconnect/boot
   checks; publish a reproducible evidence ledger before calling it a release.

The caller explicitly overrode the original live-ping sequencing gate to request
deployment readiness and opt-in shutdown. This does not waive remaining tests.

## Expanded scope

The operator subsequently requested all remaining standard QGA features and
native virtio-serial even though ISA works. [FEATURES](FEATURES.md) records the
full inventory and native prerequisites; [VIRTIO](VIRTIO.md) records the driver
plan and migration gates. These requests supersede the original brief's feature
exclusions and ISA-gap-only driver gate. The original brief is retained as
history, not silently rewritten. Do not add Agent9 dependencies, custom
host-control APIs or pretend unsupported kernel features work. Keep stock-native
installation independent of any optional packaging system.

No private addresses, passwords, SSH keys, agent sockets, API tokens, or personal
boot configuration should be committed. Do not test by exposing a developer's
whole filesystem to an untrusted guest via Drawterm.
