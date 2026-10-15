# Verification status

Evidence through 2026-10-15. Host fixtures, native tests and operator-supplied
PVE results are distinct checks.

## Current repository checks

The userspace repository builds independently of the
[driver repository](https://github.com/Szewcson/9front-virtio-serial).
Linux tests passed all 11 QGA C suites and 54 Python methods, including the
native-file/creator syscall mocks, load model and native-helper JSON boundaries.
Post-split ASan/UBSan with leak checks and GCC `-fanalyzer` passed.
The driver independently passed 15 C suites and 21 Python methods with the same
sanitizer/analyzer checks.

Current driver ThreadSanitizer attempts on three concurrency fixtures failed
runtime initialization with `unexpected memory mapping`. This is neither a
detected code race nor a passing current TSan run. Earlier combined-tree TSan
passes remain prior fixture evidence only.

Native builds and VM tests have not been rerun after the split. The earlier
combined checkout passed all 16 native C suites and a warning-free full PC64
kernel link. Those results do not establish the revised independent mkfiles.

An isolated local QEMU/TCG environment was prepared from the official
`9front-11952.amd64.qcow2.gz`; its 984 torrent pieces verified. It uses no NIC,
shared directory or Drawterm export, only project-source tar snapshots on a
read-only CD image. Serial boot began, but no completed native build/test result
was obtained before this commit. Emulator packages, images and control scripts
are local artifacts outside both repositories, not source deliverables.

## Prior native userspace evidence

The native amd64 daemon and portable C suites built without warnings.
Separately invoked tests verified:

- Real OS/hostname/time, interfaces, CPU IDs, timezone/DST offset and routes.
- Duplex raw-stream ping/metadata and detached service survival after login exit.
- All six file operations, binary NUL/0xff round-trip, seek/EOF and read-only reopen.
- Supplied-current-time system/UTC RTC update, hardware readback and range rejection.
- Literal argv/env, binary stdin, all capture modes, simultaneous flood output,
  descriptor isolation, exec failure, reaping and normal/abnormal owner cleanup.
- Read-only native disk inventory and exact descriptor keep-set cleanup.
- Native libc base64 fixtures and 26 libjson incompatibility checks; see
  [CODECS](CODECS.md) for inspected source fingerprints and decisions.

Omitted-time RTC synchronization and clock failure branches were mocked.
Actual agent-initiated powerdown/reboot was not tested.

## Prior real channel evidence

The operator supplied successful PVE ISA ping/info/OS/network/CPU/timezone/route
queries, corrected-script six-operation binary file tests, supplied-time clock
tests, disk inventory and all five exec smoke tests.

The last verified guest state used `qga9-0.5.1-dev` on real virtio `#Y0/data`
after automatic cold BIOS boot. The operator supplied passing info, binary file
round-trip and five exec tests: literal argv, binary stdin, merged output,
responsive ping and one-shot reaping. These are real operator host results,
not an independent developer PVE session or complete command/UI conformance.

The guest driver denied data/status access to a private child changed to `none`.
Per-RPC host reconnect retained the live guest owner/epoch. Stock kernel and ISA
rollback copies were retained. INTx stays masked; the worker polls at 100 ms
deadlines. See [driver STATUS](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/STATUS.md)
for hardware contracts, boot caveats and remaining gates.

## Open findings and release gates

- Startup readiness: fixed locally with up to 30 one-second waits before one
  launch, probing canonical #Y paths without opening data. Native rc fixtures
  cover ready/delayed/absent endpoints but have not run; VM deployment is pending.
- Rejected device creation: local private creator pins the parent and rejects
  final-parent unions before relative exclusive create; it no longer trusts a
  mixed-device MCREATE branch. The public-wire adapter passed 82 mock assertions
  and the creator 285 syscall-fixture checks. The actual native mixed-union and
  leased-board fixture remains unrun. See [SECURITY](SECURITY.md).
- `guest-get-load`: implemented in `0.6.0-dev`, with 4580 host model/protocol
  assertions. Private native sampler/lifecycle tests are prepared but unrun.
  The other 21 standard-union commands remain unsupported; see FEATURES.
- Native manuals and explicit manual/helper install targets are prepared.
  Rune encoding/integer formatting reuse native libc; 540 codec-boundary host
  assertions passed. Native codec compilation and manual rendering are pending.
- The VM was confirmed offline by the operator on 2026-10-14. No new binary,
  helper, kernel or boot configuration was deployed by this fix work.
- Native post-split build/tests and VM integration remain unrerun.
- PVE UI, full command conformance over virtio and real agent power actions.
- Multiple interfaces/controllers, removable media, hotplug and native error,
  note/copy/allocator fault injection beyond normal I/O/access tests.
- Checked interrupt routing/activation, live reset/fault/unplug and automatic
  driver recovery plus safe userspace restart after actual owner retirement.
- Full native kernel/cache-ordering/callback lifetime validation; host fixtures
  and sanitizers do not prove those properties.

No production or host-isolation certification is claimed.
