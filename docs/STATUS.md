# Verification status

Evidence through 2026-10-18. Host fixtures, native tests and operator-supplied
PVE results are distinct checks.

## Current repository checks

The userspace repository builds independently of the
[driver repository](https://github.com/Szewcson/9front-virtio-serial).
Linux tests passed all 12 QGA C suites and 57 Python methods, including the
native-file/creator syscall mocks, load model and native-helper JSON boundaries.
Post-split ASan/UBSan with leak checks and GCC `-fanalyzer` passed.
The driver independently passed 15 C suites and 22 Python methods with the same
sanitizer/analyzer checks.

Current driver ThreadSanitizer attempts on three concurrency fixtures failed
runtime initialization with `unexpected memory mapping`. This is neither a
detected code race nor a passing current TSan run. Earlier combined-tree TSan
passes remain prior fixture evidence only.

## Dispatcher and native smoke cleanup (2026-10-18)

Dispatch, availability, reply policy and shutdown-state restrictions now use
named command kinds, native operation IDs and flags, not table positions or
negative query selectors. Backend registration and command behavior are
unchanged. A host scratch build with info/sync/ping/shutdown entries reordered
passed the same core suite; no permutation or test hook was added to production.

The core suite passed 14,359 parameterized checks, including all 64 backend
registration profiles before shutdown and every applicable post-shutdown
profile. It checks advertised capabilities, callback selection/operation,
missing-backend rejection, file/clock/exec refusal, duplicate shutdown, and
continued read-only/protocol operation. These behavioral tests replace the
source-format startup assertions. Three Python methods run its bounded
`--stdio-info` validator with valid, empty, malformed, error and forbidden
capability responses; rejected cases must fail normally, not crash or trip a
sanitizer. All 12 host C suites and 57 Python methods passed, as did
ASan/UBSan/leak checks and GCC analyzer. The host manual rendered cleanly.

In the private `/usr/glenda/qga-dispatch20261017/qga` checkout, the native build
and all ten portable suites passed. Native startup mocks and the real private
duplex-stream test passed. Stdio now requires a successful producer exit and
semantic validation of its complete capability response, not a negative grep.
All five native producer-failure cases passed: failed-empty, empty-success,
malformed JSON, error response, and failure after an otherwise valid response.
The initial harness run failed because quoted wildcards are literal in native
rc's `~`; the corrected assertion and final passing run are recorded separately.
The three intentional `invalid stdio capabilities` diagnostics in that final
log are expected rejections. Sources were digest-verified; the private binary's
SHA-1 identity is `8cc511ff8caeb2aeccea6cbeaf9fd1689c5633cf`.

Production allocation ownership and single-owner execution are unchanged;
argument checks, no-success-reply shutdown and lifecycle protection remain.
No installed daemon, system library, boot configuration or Proxmox VM was
changed. Successful power requests against this private candidate and real-PVE
rollout remain separate checks.
The unchanged installed lab agent then halted the filesystem and powered down
with the expected QMP guest-shutdown event and emulator exit zero. The known
post-halt GLib diagnostic recurred. The stopped image's non-repair qcow2 check
found no errors; this does not prove guest-filesystem data integrity. The lab
is powered off.

## Default management-channel shutdown (2026-10-17)

The source now registers `guest-shutdown` on UART and raw-stream management
channels alongside clock setting and execution, without an extra enable flag.
The `-S` option and helper's `qga9_shutdown` setting were removed. Old config
entries, including `no`, are ignored; custom commands still passing `-S` must
be updated. Stdio has no shutdown, clock-setting or exec backend. Argument
validation, fixed argv, one successful initiation per instance, isolated child
descriptor cleanup and post-initiation mutation refusal remain unchanged.
No allocation ownership or shared-state/concurrency model changed.

All 12 host C suites and 56 Python methods passed, with ASan/UBSan/leak checks
and GCC analyzer. The two new Python methods check source/configuration policy,
not native execution. The host manual rendered without diagnostics.

A new private checkout at `/usr/glenda/qga-shutdown20261017/qga` in the NIC-less
local QEMU guest built without warnings against the previously tested private
libjson archive. All ten native portable suites passed, as did the native
startup mocks (ready, delayed publication and bounded timeout). The real duplex
stream fixture confirmed default shutdown advertisement with
`success-response: false`, invalid-mode rejection and stdio exclusion. It did
not send a successful power request. The private daemon's SHA-1 identity is
`7d25ba3733a5c71a1f9524e7c470bbc6b4f0808c`; source transfer was digest-verified.
These checks did not install a daemon, helper or library, edit boot files, or
contact Proxmox. The real PVE rollout remains pending.
Afterward the unchanged installed lab agent performed clean filesystem halt
and powerdown, with QMP `guest: true, reason: guest-shutdown` and emulator exit
zero. The known post-halt GLib diagnostic recurred. The lab is powered off;
this cleanup is not a successful-power-request test of the private candidate.

## Native libjson migration (2026-10-16)

The source checkout is now `qga9-0.6.1-dev`, using a pinned native
[libjson patch series](../patches/libjson/README.md). Its opt-in APIs provide strict,
bounded parsing, exact number spelling/int64 conversion, counted strings/keys
and bounded native formatting. The legacy API/layouts remain compatible.
The custom grammar parser and UTF-8/string serializer were removed; `src/json.c`
is now the QGA policy/storage adapter. The local header is named `qjson.h` so it
cannot shadow the native library header. Preparation is explicit and hash-checked;
native builds can link a private archive without changing system files.

Temporary native trees are freed before dispatch after copying into fixed
document storage. No borrowed input or heap tree survives parsing. Request
allocation is capped at 64 KiB requested bytes, excluding allocator overhead;
the 8 KiB input, 512-token/depth-16 and 64 KiB output limits remain. Formatting
uses per-call state and exact-capacity buffers without global `%J` registration.
See [CODECS](CODECS.md) for ownership and remaining safety limits.

All 12 host C suites and 54 Python methods passed against the native backend,
with ASan/UBSan/leak and GCC analyzer checks. The standalone native library
fixture passed 12,908 host checks, allocation-failure/cleanup tracking, four
threads of independent parse/format/free work and deterministic byte mutations.
Current TSan could not initialize; this is not a race-free certification.
The final patch replay reproduced the reviewed sources byte-for-byte.

In the independent NIC-less local QEMU guest, the library fixture passed 1,650
checks; the daemon and ten portable suites built warning-free with native
`6c -FTVw`/`6l`. The adapter passed 167 native checks (224 on host with allocation
injection), the native process fixture passed 140 checks, storage passed 63
read-only checks over six nodes, and sampler startup/three queries/stop passed.
The final version-tagged daemon also passed private duplex ping/management
metadata, all six real file operations with binary data, and responsive native
exec/status/one-shot reaping. Its private binary SHA-1 is
`520494df8ccfbe2dfb3ca857cdac5e512b95293c` (identity, not authenticity).
These fixtures never opened the installed virtio/ISA endpoint. A first harness
wait expired; its exact job was subsequently reaped with exit zero and the log
showed completion. The final rerun used a larger bounded harness deadline and
completed/reaped normally. Earlier rc quoting/archiver harness errors occurred
before adapter acceptance; those failed transcripts are retained, not counted
as passing builds.
The private stdio binary independently reported `0.6.1-dev`. Its 19 advertised
stdio commands exclude exec, time setting and shutdown as intended. The existing
local management agent was left unchanged; it subsequently powered the lab down
with a clean filesystem halt, exact QMP guest-shutdown event and emulator exit 0.
The known post-halt QEMU GLib diagnostic recurred. This is not a powerdown test
of the new private candidate.
The stopped qcow2 non-repair check found no errors; this is metadata evidence,
not a full guest-filesystem integrity audit.
The stock-comparison fixture was retired because its old escaping-failure
expectation is intentionally fixed. Host nroff rendered both manuals without
diagnostics; this is not a new native manual-rendering claim.

An initial private prototype also ran on `p9lab`; its follow-up connection then
failed with No route to host. No Proxmox system header, library, installed agent
or boot hook was replaced. Live PVE codec rollout/host acceptance and upstream
submission remain pending; older PVE results below describe the installed
`0.6.0-dev` build, not this migration.

## Live Proxmox guest update (2026-10-15)

Drawterm authenticated to `10.99.0.2` as glenda; the guest identifies itself as
`p9lab`. Only a fresh temporary project-staging directory was exported, not the
desktop filesystem or SSH keys. The previous polling kernel, one QGA process,
enabled stream configuration and single `/cfg/p9lab/cpustart` hook were inspected.

A fresh `/usr/glenda/src/qga9-roadmap-20261015` checkout built warning-free.
All nine portable native suites passed, including 220 storage assertions.
Separate checks passed: 21 file-guard checks, 144 native process checks, exact
descriptor cleanup with 80 extras, 52 read-only storage checks over five nodes,
sampler startup/three queries/stop request, ready/delayed/absent startup cases,
duplex private-stream ping/metadata and responsive exec/one-shot reaping.
These private streams never opened the installed `#Y` channel.

Byte-compared backups of the old binary/helper/config/hook are retained in
`/usr/glenda/qga9-backup-20261015`. The exact old process's note Chan was opened
before its final executable-identity check and stop. Only after it exited were
the staged binary and helper published by same-directory renames. The hook and
configuration were not changed; shutdown remains enabled. One new detached
agent was launched through `auth/newns` and the installed readiness helper.

Installed identities (SHA-1 for comparison, not authenticity):

- `/amd64/bin/aux/qga9`: `5814a3b434fd0bb51d513f23b35707bf38af296b`.
- `/rc/bin/qga9-start`: `0ec1c9f378872e59e639d130cf8ae11010d07790`.

The installed binary reports `qga9-0.6.0-dev` in stdio diagnostics. At deployment,
process 835's text matched that binary; it alone held `#Y0/data`. Its sampler
836 holds only private IPC and the owner's process-status descriptor, not the
management channel. The daemon namespace contains no Drawterm export. After
fully closing the deployment session and authenticating a new session, the same
process and identity 2/epoch 2/generation 1 remained live. That is guest-side
detachment/ownership evidence, not a host-channel smoke test.

The new qga9(8), startup alias and separate vconsole(3) manual installed and
rendered with native `man -n` without formatter errors. A read-only system/RTC
snapshot differed by about one second; no clock write or timesync policy change
was made. Native default driver builds exposed an empty-list rc argument bug,
fixed in the separate repository. Its nine suites, 17 header objects and full
alternate polling-kernel link then passed on this guest. The new kernel is not
selected or running; no boot files or Proxmox host configuration were changed.

The operator subsequently supplied successful PVE ping/info (`0.6.0-dev`, all
23 commands), IPv4/IPv6 interface reporting, two online vCPUs, timezone/routes,
all six file operations and all five exec smoke checks. The synthetic file
fixture remains `/tmp/qga9-host-test-103-1792091912-75994-14070.bin`. These are
operator host results, not an independent developer host session. A subsequent
read-only guest inspection retained PID 835, identity 2/epoch 2/generation 1,
matching executable text and no management descriptor in sampler 836. This adds
actual host-reconnect/continued-owner evidence on the unchanged polling kernel.

The subsequent operator host metadata batch returned 9front/amd64 identity,
hostname `p9lab`, exact time `1792092203680963327` ns, one whole-data node with
four dependent native partitions, and nonzero load estimates: 1/5/15-minute
values `0.100812` / `0.083917` / `0.054831`. Load is now queried through the real
PVE client, not merely advertised. A successful time read does not establish
clock accuracy, setter behavior or continuing time-sync-policy agreement.

The subsequent operator power-cycle transcript, recorded on 2026-10-16,
reports `qm shutdown 103 --timeout 120` followed by `status: stopped` and
`qm start 103`. Initial ping/info attempts reported the agent not running;
later attempts succeeded with `0.6.0-dev` and all 23 enabled commands. No manual
agent start appears in the provided sequence. This passes the normal PVE
shutdown/start and eventual boot-launched-agent check on operator evidence.
Elapsed startup time was not recorded; do not infer a latency guarantee or that
the helper spent its full readiness wait. This is not a traced direct QGA
shutdown RPC, observed filesystem-halt console log or warm guest-reboot test.

The first independent 2026-10-16 follow-up failed with Host Unreachable/No route
to host and made no guest changes. After the operator restored the workstation
route/VPN, Drawterm authenticated again. The cold-booted guest had one QGA data
owner, PID 292, and sampler 293 without a management descriptor. Executable text
matched the installed identity above; the service namespace contained no
Drawterm export. Driver identity 1/epoch 2/generation 1 was live. The cold boot
still selected the known-good polling image, fingerprint
`cef52c141d9fa4dae81177fce3a43b0c5e6f3d9d`.

## Newer-driver tests and rollout (2026-10-16)

The existing FAT images/configuration were read through a private read-only
dossrv instance and byte-compared backups retained at
`/usr/glenda/virtio-backup-20261016`. The staged newer kernel was copied to a new
executable `/amd64/9pc64roadmap16`, compared with the build output and verified
as SHA-1 `4ee107539dec633ab0163358cff1eac215fecf81` (comparison only). Native
`auth/newns /bin/fshalt -r /amd64/9pc64roadmap16` reported `done halting` and
`rebooting`; a new authenticated session then observed
`pc64roadmap15b.bootfs` and fingerprint
`334951c61aaa29f0c445c9534d60413458d20afb` in the boot log.

The new default polling driver published identity 1/epoch 2/generation 1,
`irq-vector -1`, `irq-deliveries 0`. One matching agent and its isolated sampler
started automatically again; no manual launch or second QGA data reader was
used. These are separate processes despite PID reuse after reboot. Persistent
`plan9.ini`, stock `9pc64` and the known-good FAT `9pc64qga` were not replaced;
a full cold boot still selected the older known-good image at that stage. The
operator then supplied passing PVE ping/info, CPU/timezone/routes, all six file
operations and all five exec checks on this newer polling kernel. Both guest
IPs were reported in response to the Summary-page question. This passes explicit
kernel warm boot, guest-side startup and operator host smoke/IP reporting, not
the QGA reboot RPC or exhaustive filesystem integrity.
Retirement still consumes boot-pinned contexts; avoid gratuitous agent restarts.

The separate checked native API patch and full kernel link then passed on this
guest after verified source backups. A second explicit warm boot identifies
`pc64roadmap16irq.bootfs`, fingerprint
`a298ce6814c44ce2a798d876c414902b0ed69a6a`. Checked vector 73 deliveries rose from
13 to 234 on the same identity 1/epoch 2/generation 1 and owner PID 298. Sampler
299 holds no management descriptor and the owner has no Drawterm export. The
operator supplied passing 23-command info, CPU/timezone/routes, file and all five
exec tests, including responsive ping; the standalone ping line is garbled in
the transcript. These are target IRQ/reconnect checks, not general fault proof.

After that acceptance, a byte-compared copy was installed under new short FAT
name `9qga16i` and plan9.ini selects it with both opt-ins and unchanged full
bootargs. Stock and previous polling images/config backups remain intact.
See the driver STATUS for activation/recovery;
an unchecked fallback image must not retain `qgavirtioirq=1`.

The subsequent operator shutdown reached `stopped`; a full start then passed
ping/info and file/exec/query smoke tests without a manual agent launch. An
independent guest inspection observed BIOS `bootfile=9qga16i`, the checked
fingerprint, vector 73 with 232 recognized deliveries and exactly one matching
owner (297) plus isolated sampler (298). Its namespace contains no Drawterm
export. This passes cold-start acceptance of the persistent checked image.
An early connection refusal preceded CPU readiness; no latency was measured.
The pasted host script omitted `--set-time`, so current clock-setter/policy
acceptance was not exercised by that successful file test.

The operator then explicitly reran `perl ./test-proxmox.pl 103 --set-time`:
files and supplied-time setter/readback passed, reporting
`1792131271868666048` ns. A delayed independent read found guest and RTC both
`1792131315` seconds, within workstation timestamps `1792131313`–`1792131316`,
with timesync 270 still running. No service/time-sync/Proxmox policy was changed.
The same owner/context remained live and checked deliveries rose from 232 to
320 during this batch. This passes current supplied-time/UTC RTC and delayed
agreement checks, not omitted-time PVE behavior, prolonged drift or physical
RTC/failure-path certification. The service config and single startup hook
remain byte-equal to their verified pre-update copies. Drawterm was closed.

## Current local native evidence

An isolated local QEMU 10.0.13/TCG environment uses the official
`9front-11952.amd64.qcow2.gz`; its 984 torrent pieces verified. It uses no NIC,
shared directory or Drawterm export, only project-source tar snapshots on a
read-only CD image and explicit source-file updates through a private test QGA
channel. Emulator packages/images/control scripts stay outside both repositories.
Those local tests did not change the Proxmox VM or host configuration.

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

### Focused reliability follow-up

The driver's explicit native lifetime fixture compiled without warnings. A
single cold boot passed the complete notes/read-fault/write-fault/notes sequence:
16 alarm-interrupted native operations in total, exclusive-open denial,
nonfinal dup close, binary backend reconnect, pending read/write wakeup on exact
port removal, delayed final clunk and old status Qid/identity retention across
fresh publication. The first coordinator attempt wrongly required controller
`stopped=1` immediately on a removed-port I/O error; correcting that test
assumption allowed the complete rerun to pass. No kernel fix was needed.

All four contexts were consumed; namespace withdrawal and the explicit
`fresh epoch pool exhausted; reboot required` message were observed while the
independent management channel remained responsive. The filesystem halted
cleanly. On the following cold boot, the actually installed qga9-start helper
ran from a single machine-specific terminal hook. QGA answered guest-info with
all 23 commands without manual agent launch, after its original terminal rc
had already exited and init restarted the shell.

Only the disposable VM's hook/configuration was created. It selects UTC
timesync arguments. With timesync running, both setter modes and delayed
readback passed (guest minus host about -0.807 seconds, within RTC precision).
Descriptor inspection found one #Y data owner and a separate sampler without
the management descriptor. Twenty synchronized reconnects preserved a file
handle/offset and process result; abandoning a 4096-byte reply then resynchronizing
also passed (observed offset 4096, no rollback/replay claim). IRQ deliveries
increased from 3 to 199 on identity 1/epoch 2/generation 1. The full file/exec
smoke retest then passed, increasing deliveries to 293 on that same owner.
The boot-launched agent then powered the guest down: filesystem halt, QMP
`guest-shutdown`, no QGA success reply and QEMU exit status 0. The post-halt
QEMU GLib timer diagnostic recurred. Non-repair qcow2 check afterward found no
errors; this checks image metadata, not exhaustive filesystem/data integrity.
The NIC-less lab cannot validate actual guest IP display. The Proxmox VM was
not contacted or modified.

## Filesystem identity investigation

The explicit native fsinfo-provenance experiment built warning-free and passed
on 2026-10-15 in the isolated local guest. After reposting a different pipe under
its own private service name, complete namespace text and mounted root identity
remained unchanged and the old mounted file stayed readable. This refutes
matching historical namespace names to today's service registry as automatic
mount provenance. The virtio identity/epoch/owner and IRQ count were unchanged
through this independent-management-channel test; subsequent wire ping/info
worked. See STORAGE and DEVELOPMENT for the reproducer and confinement.

No production handler, capability advertisement, version or real Proxmox
configuration changed. On 2026-10-15 the operator chose to keep fsinfo
`Unsupported`, with no new configuration. It is no longer on the active
implementation roadmap; the identity limitation is documented, not bypassed
with a trusted local mapping or an empty-success response.
The disk protocol fixture now checks both non-advertisement and `Unsupported`
even with the storage query callback registered.
All 11 host C suites and 54 Python methods passed again, as did host
ASan/UBSan/leak checks and GCC analyzer. The nine portable native suites also
passed, including 220 storage assertions. That native aggregate job exceeded
the client's initial 30-second polling window; its result was subsequently
reaped with exit status 0 and all suite output intact. The standalone native
provenance fixture is not exercised by the host sanitizers. Its snapshot bounds
and single-owner resource paths were reviewed; private namespace/board state
avoids mutating the live service registry. No new memory/access-control finding
was identified in the added test; arbitrary note/syscall failures and cleanup
deadlines remain unverified.
After these tests, QGA powerdown again produced a guest-shutdown QMP event,
channel EOF without a success reply, native filesystem halt and QEMU exit 0.
The previously observed post-halt GLib timer diagnostic recurred. The stopped
qcow2 check found no metadata errors; this is not a full filesystem integrity
check.

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
- Native libc base64 fixtures and the historical 26-check stock-libjson
  comparison, now superseded by the library patch/adapter evidence above.

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

## Focused remaining acceptance

- Startup readiness: bounded stat-only waits and native rc fixtures passed;
  the updated helper is installed on the Proxmox guest. The operator supplied
  successful eventual ping/info after a normal PVE shutdown/start, without a
  manual agent launch in the sequence. Startup latency was not measured.
- Rejected device creation: local private creator pins the parent and rejects
  final-parent unions before relative exclusive create; it no longer trusts a
  mixed-device MCREATE branch. The public-wire adapter passed 82 mock assertions
  and the creator 285 syscall-fixture checks. The actual native mixed-union and
  leased-board path passed the 21-check fixture locally and on the Proxmox guest;
  the backend is installed there. See [SECURITY](SECURITY.md).
- `guest-get-load`: implemented in `0.6.0-dev`, with 4580 host model/protocol
  assertions, native sampler queries and actual virtio wire queries passed.
  The operator supplied a successful nonzero PVE host query on the updated VM.
  All 21 unsupported commands, including fsinfo after the operator's decision,
  are deliberately outside active scope; see FEATURES.
- Native manuals and explicit manual/helper install targets are prepared.
  Rune encoding/integer formatting reuse native libc; 540 codec-boundary host
  assertions passed on both host and native libc. Native manual rendering passed.
- The Proxmox VM was confirmed offline on 2026-10-14 and became reachable again
  on 2026-10-15. The userspace update above supersedes the earlier deployment
  blocker; the later one-time newer-kernel test is recorded above. Persistent
  boot selection later moved to the verified checked image, with rollback retained.
- Post-split native builds/tests passed locally and on the Proxmox guest;
  updated PVE ping/info/network, CPU/timezone/routes, file/exec smoke checks and
  OS/hostname/time/disk/load queries passed on operator evidence. Subsequent
  IP, kernel and supplied-time checks are recorded above. Normal shutdown/start
  passes cold-start availability, not a QGA reboot RPC, direct shutdown-path
  trace or filesystem consistency audit.
- Post-cold-boot isolation, newer polling/checked warm boots, actual target IRQ
  delivery and operator PVE smoke/IP reporting passed after route restoration.
  The selected checked image's cold-start and supplied-time clock-setter
  acceptance passed.
- UTC agreement with running timesync passed delayed checks locally and after
  the PVE supplied-time test; omitted-time PVE behavior, prolonged drift,
  physical RTC and other time-source policies are not established.

Broader physical-platform/cache ordering, general PCI-controller hot-unplug,
unreadable MMIO/failed reset and exhaustive note/copy/allocator/callback fault
injection remain unverified residual risks. They are not requirements to add
new drivers or fault infrastructure in this milestone. Existing ownership,
bounded resource and conservative reset/drain contracts must still be preserved;
scope reduction is not proof of safety. Agent restart after retirement is explicit.

No production or host-isolation certification is claimed.
