# Verification status

Evidence through 2026-10-15. Host fixtures, native tests and operator-supplied
PVE results are distinct checks.

## Current repository checks

The userspace repository builds independently of the
[driver repository](https://github.com/Szewcson/9front-virtio-serial).
Linux tests passed all 11 QGA C suites and 54 Python methods, including the
native-file/creator syscall mocks, load model and native-helper JSON boundaries.
Post-split ASan/UBSan with leak checks and GCC `-fanalyzer` passed.
The driver independently passed 15 C suites and 22 Python methods with the same
sanitizer/analyzer checks.

Current driver ThreadSanitizer attempts on three concurrency fixtures failed
runtime initialization with `unexpected memory mapping`. This is neither a
detected code race nor a passing current TSan run. Earlier combined-tree TSan
passes remain prior fixture evidence only.

## Current local native evidence

An isolated local QEMU 10.0.13/TCG environment uses the official
`9front-11952.amd64.qcow2.gz`; its 984 torrent pieces verified. It uses no NIC,
shared directory or Drawterm export, only project-source tar snapshots on a
read-only CD image and explicit source-file updates through a private test QGA
channel. Emulator packages/images/control scripts stay outside both repositories.
No Proxmox VM or host configuration was changed.

The independent daemon builds with native `6c -FTVw`/`6l` without warnings.
All nine portable native suites passed, including libc base64, rune/integer
boundaries and load estimation. Explicit native checks passed:

- 21 file-guard checks: mounted-file create/round-trip and rejection of direct
  devices and a mixed-device creation union without device/file side effects.
- Sampler startup, three queries and stop request; real wire load queries also
  returned zero and nonzero estimates. Exhaustive sampler failure/lifetime
  injection is not implied by this normal-path test.
- Exact descriptor keep-set cleanup with 80 extras, and native process checks
  (144 and 146 assertions on successive runs; polling affects the count).
- Duplex stream ping, six file operations and binary round-trip, and responsive
  exec/status with one-shot reaping.
- 63 read-only disk checks across six hard-disk/CD-ROM nodes. Native testing
  exposed geometry-less IDE ATAPI ctl and changing diagnostic counters; geometry
  now derives only from verified metadata and snapshots compare parsed inventory.
- Startup helper ready/delayed/absent fixtures, using native rc and a private
  overlay, without launching an actual service.
- Explicit manual installation and native `man -n` rendering of qga9(8), its
  qga9-start alias and the separate driver's vconsole(3), without formatter errors.

Daemon SHA-1 identity: `b7f65621f020537536b3ef420e5820e3975e6727`
(not an authenticity proof).

The separate driver built nine native suites, 17 kernel-header objects and a
warning-free checked-IRQ kernel. Real local virtio queries, synthetic binary
files and all five exec smoke tests passed before and after forced device status
reset, exact named-port removal/re-add and an out-of-range used-ring ID.
Checked IRQ counters increased on each unchanged live context; recovery published
new identities. Supplied-time and RTC-derived clock synchronization passed over
virtio, with load queries still working after clock changes.

The stock image ran `aux/timesync -rLa1000000`, whose local-time RTC interpretation
overrode the agent's UTC RTC update by two hours. After stopping only that
disposable guest's running timesync process, both setter modes passed immediately
and five seconds later (within one second of host time). No persistent time-sync
configuration or Proxmox policy was changed. See [9FRONT](9FRONT.md).

Agent-triggered powerdown completed filesystem halt and emulator exit status 0.
A second cold boot automatically selected the verified short FAT kernel name
`9qgairq`, with no bootargs acknowledgement needed. Its repeated QGA powerdown
produced QMP `SHUTDOWN` with `guest: true, reason: guest-shutdown`, no QGA success
reply, and exit status 0. Both exits emitted a QEMU GLib timer diagnostic after
halt; the first also emitted a guest `echo` write error. These diagnostics were
retained, not silently treated as absent. The stopped qcow2 image check found no
errors. Reboot and power-failure branches remain untested.

These are local QEMU results, not new PVE results or proof of physical hardware,
all failure paths or cache ordering.

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

At that earlier stage omitted-time RTC synchronization and clock failure
branches were mocked, and agent-initiated powerdown/reboot was not tested.
Current local evidence above supersedes only the successful omitted-time and
powerdown paths, not clock/power failures or reboot.

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

- Startup readiness: bounded stat-only waits and native rc fixtures passed;
  deployment/cold-start verification on the Proxmox VM is pending.
- Rejected device creation: local private creator pins the parent and rejects
  final-parent unions before relative exclusive create; it no longer trusts a
  mixed-device MCREATE branch. The public-wire adapter passed 82 mock assertions
  and the creator 285 syscall-fixture checks. The actual native mixed-union and
  leased-board path passed the 21-check fixture. See [SECURITY](SECURITY.md).
- `guest-get-load`: implemented in `0.6.0-dev`, with 4580 host model/protocol
  assertions, native sampler queries and actual virtio wire queries passed.
  The other 21 standard-union commands remain unsupported; see FEATURES.
- Native manuals and explicit manual/helper install targets are prepared.
  Rune encoding/integer formatting reuse native libc; 540 codec-boundary host
  assertions passed on both host and native libc. Native manual rendering passed.
- The Proxmox VM was confirmed offline by the operator on 2026-10-14. No new
  binary, helper, kernel or boot configuration was deployed there by this work.
- Local post-split native builds/tests passed; Proxmox deployment remains pending.
- PVE UI, full command conformance over virtio, reboot and power-failure paths.
- Multiple interfaces/controllers, removable media, hotplug and native error,
  note/copy/allocator fault injection beyond normal I/O/access tests.
- Basic checked IRQ, forced reset, invalid used ID and named-port recovery passed locally.
  PCI-controller unplug, failed reset/MMIO, resource faults and exhaustive
  recovery remain driver release gates. Restarting userspace is still explicit.
- Full native kernel/cache-ordering/callback lifetime validation; host fixtures
  and sanitizers do not prove those properties.

No production or host-isolation certification is claimed.
