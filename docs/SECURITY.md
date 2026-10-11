# Security design and initial review

This is a bounded incremental design, not a guarantee of hypervisor isolation
or a completed native security audit. Treat the guest and Agent9/pac9/pi9 and any
other guest programs as potentially untrusted.

## Trust boundaries

The incoming serial stream is untrusted parser input. Its contents choose only
bounded protocol handlers. Guest-file requests intentionally select mounted
filesystem paths under the agent's identity/namespace and can read, create or
truncate files. Clock requests intentionally change system/RTC time, through
fixed native device paths and permission checks. Exec requests intentionally select
guest programs, literal argv, environment overrides and stdin under that identity.
The agent never interpolates them into shell code; a caller can explicitly launch
a shell or a network-capable guest program. No host credential or host-control
authority is conveyed by this API. With explicit `-S`, validated
powerdown/reboot mode selects one of two fixed native argv arrays; no incoming
string is passed to a shell. There is no integration with guest package managers.
Serial paths and baud values are local operator configuration, not protocol fields.

The QGA channel is host management plumbing, **not authentication or guest trust
attestation**. A compromised guest/kernel or same-effective-privilege application
may read, corrupt, consume or impersonate the agent's stream. Permission changes
and keeping the agent away from interactive/untrusted applications help, but do
not fix same-user or whole-guest compromise. Do not run untrusted tools under the
same privileged identity and assume namespace separation alone protects the agent.

Nothing here proves host immunity from malicious guest input to QEMU/PVE. Keep
those products patched and use their normal isolation/access controls. The agent
does not hold host credentials, forward SSH agents, or export local host files.

## Pending capabilities

Password/SSH-key mutation, filesystem
freeze/thaw, trim, host-control extensions, package installation and network
listeners are not implemented. The caller requested the remaining QGA commands;
their prerequisites and incremental implementation are recorded in FEATURES.
Their absence currently limits the legitimate daemon's capabilities, but a fully
compromised guest can replace it. Native OS/network/time/CPU/route commands read bounded
native sources. Shutdown is disabled by default and explicitly enabled with `-S`;
actual filesystem/power semantics remain unverified.
Known pending requests return `Unsupported`; unknown requests return
`CommandNotFound`. No flag enables any pending key/password API.
File handlers are enabled even in native stdio diagnostics; do not mistake that
mode for a read-only tool. Clock mutation is enabled only on management channels.
Append modes remain Unsupported rather than faking non-atomic append semantics.
Exec/status are enabled on management channels only, not stdio diagnostics.

## Memory safety

- One checked allocation owns all protocol state, frame storage, tokens and
  response storage. Caller-owned output context must outlive the instance.
- Byte/token/depth limits precede writes and recursive descent. The parser checks
  every UTF-8/escape read and rejects malformed/surrogate input and decoded
  duplicate keys. Borrowed input tokens are serialized before frame reuse.
- Output has a sticky bounds failure and is never sent on overflow. The callback
  cannot claim a negative, zero or over-large successful write. Buffers passed to
  it are borrowed only for the synchronous call.
- Sync integer validation is lexical: no floating-point precision loss, signed
  overflow arithmetic or input-driven allocation. Error messages use trusted
  constants. Small native input/control buffers are separately checked.
- Normal paths close/free resources. Default Plan 9 termination notes use kernel
  process teardown; the broken-pipe note filter never touches parser/job memory.
- Native text/status reads reject truncation/interior NUL, check allocations,
  bound directories/addresses and validate masks and MACs. Directory entries are
  freed per chunk; native-source failures discard partial output. Status-derived
  filenames allow only numeric interface indices and `/net/etherN/addr`.
- Shutdown redirection descriptors are relocated above stdio, including when
  inherited fd 0/1/2 are closed. Failed dup/open/fork paths close owned descriptors;
  native child redirection failure exits rather than using the QGA channel.
- CPU/route snapshots use one checked heap allocation capped at 65536 input
  bytes. Immutable local scanners cap fields at 63 bytes and inventories at 256
  records; decimal parsing checks overflow before multiplication. Duplicate CPU
  IDs, malformed addresses/prefixes and unterminated IP callback results fail.
  Failed native queries discard partial output rather than transmitting it.
- Timezone offset bounds precede signed negation. Native timezone storage is
  borrowed from the library cache and never freed by the agent; `Tm` and emitted
  strings are caller-owned/bounded, not the static `localtime` buffer.
- One additional checked allocation owns file handles and fixed path/base64/data
  buffers. Decoded NUL paths, invalid signed int64 arguments and oversized chunks
  fail before I/O. Canonical base64 rejects ignored suffixes and nonzero pad bits.
  Exact response ID/framing space is reserved before file side effects. Callback
  byte counts are checked; each owned descriptor is consumed once even on close
  failure, and remaining handles are released on normal teardown. OCEXEC keeps
  file descriptors out of shutdown exec. IDs are not recycled within an instance.
- Clock conversion/range checks precede multiplication and mutation. Both fixed
  devices are acquired before updating system time, with cleanup on each path.
  RTC failure after a system update is explicitly reported. No request-sized
  allocation, unchecked signed seek sum or shared static clock context is used.
- Exec argument/input/capture buffers and eight retained job slots are fixed-size.
  Array counts, UTF-8/NUL, environment names, canonical base64, positive native
  PIDs and result lengths are validated before use. Output/ID space is reserved
  before spawning or consuming a result. Request buffers are cleared after spawn.
  Poll errors/invalid result bounds cancel and consume the owned handle once.
  The shared base64 codec preserves file API bounds and canonical padding tests.

## Thread safety and races

Each instance has one synchronous owner; there are no threads or shared mutable
parser globals. Read, parse, dispatch, serialize and write complete in that order.
The API is not reentrant/thread-safe for shared instances; introducing concurrency
requires a new ownership/locking design. Static command lists are read-only.
Detached/service and shutdown children use RFPROC without shared memory, private
file-descriptor/namespace/environment/note groups, and RFNOWAIT. They cannot race
on the parent's parser storage. A shutdown accepted once sets a single-owner flag;
duplicate requests do not launch another child. Child failures after fork remain
asynchronous and do not reset that flag automatically.

UART open is **not exclusive**. The helper deliberately has no race-prone pidfile
check pretending to enforce exclusivity; operators must arrange a single startup
path. Concurrent consumers or same-user attackers remain an external access risk.
Local UART-control changes, device permission changes, build/install paths and
boot hooks must be administrator controlled. `mk install`'s existing-file check is
not an atomic security boundary: use a trusted target directory and install once.
Raw-stream mode likewise does not add exclusive ownership to an arbitrary
device. A future virtio driver must enforce channel ownership and DMA generation
lifetime itself. CPU/route queries are bounded kernel snapshots, not atomic
transactions with later management actions. The timezone library caches the
fixed local name under its own lock; restart after changing timezone configuration.

File state is created in the serving process after detachment and has a single
owner. Native open avoids path-check-then-truncate: it validates the actual
descriptor and applies length changes through that descriptor. Missing files
use exclusive create, preventing a competing creator from being accidentally
truncated by a fallback create. There is no seek-then-write append emulation and
no permanent append-only metadata change. External writers and fileserver
metadata changes can still affect open files; these operations are not atomic
transactions. IDs are never reused within an instance, but restart invalidates
all handles. File/clock/execution requests are refused after shutdown initiation;
already-open descriptors are not a filesystem-consistency guarantee.

Clock state is stack-owned, with no mutable global context. System time and RTC
are separate devices and cannot be updated atomically. Permission preflight
does not guarantee both later writes succeed. Independent time-sync processes
are not locked/stopped and can override a clock change. Failures explicitly
report an already-changed system clock; there is no automatic rollback.

## Execution expansion review

The changed process/core/codec/build/test paths were reviewed across all five
required areas: memory safety, thread safety, race-free behavior, CVE-style unsafe
input/framing/descriptor patterns, and insecure access paths. No unresolved
critical/high finding was identified in this scoped review, not an independent
audit or guarantee against malicious guest programs.

- Private `rfork` memory/fd/namespace/env groups and pipe message passing replace
  shared-memory workers. Only the UART owner mutates the job table/parser. A
  temporary startup waiter cannot mutate its supervisor's buffers. The small
  broken-pipe note handler only compares its argument and returns; other notes
  retain native default behavior. Registration/removal occurs at single-owner
  lifecycle boundaries, never from a concurrent worker.
- Every executed child closes inherited descriptors except redirected 0/1/2
  and a close-on-exec failure handshake. No UART, file-API handle, other job pipe
  or diagnostic console descriptor reaches the executable. Parent-side job IPC
  and process guards are also close-on-exec for the fixed shutdown executor.
  Native descriptor-isolation tests inspect the actual executable's `#d` table.
- An EOF failure handshake distinguishes actual exec success from fork success.
  Startup polling is bounded, and failure cancels the fresh owned group. Real
  PIDs are checked against retained IDs; status cannot select an arbitrary guest
  PID. Final status consumes metadata once; poll failure consumes/cancels it.
- Supervisors actively drain both pipes, discard excess after capture limits and
  publish one bounded result. No unbounded spool or blocking stdout-then-stderr
  collector is used. Native flood tests exceed both pipe capacities. Direct-child
  completion drains a finite queue snapshot, so background descendants cannot
  keep the result open forever. Later descendant output is intentionally not
  guaranteed captured; see PROTOCOL for this compatibility restriction.
- Group cancellation uses an already-open native note-group descriptor, not a
  later lookup by caller-supplied or stale PID. Already-open process-status
  descriptors detect supervisor/agent death without inspecting recycled names.
  Normal cleanup cancels owned groups; abnormal owner death is detected during
  startup and runtime and while awaiting reaping. Native live tests passed the
  normal and abnormal owner-death paths.

Residual execution risks are availability/authority concerns, not memory
containment: running jobs have no runtime deadline, and unqueried final results
occupy eight slots. Slot/capture limits do not limit what a guest executable can
allocate, fork or modify. Commands run under the agent identity/namespace and
can explicitly use a shell, contact networks, escape a note group or ignore
termination notes. Cancellation is best effort, not a security sandbox. Ordinary
reaping allows deliberately backgrounded descendants to survive. Host timeouts
can leave ambiguous launched work; blind retries can repeat guest side effects.
No host key is installed/forwarded, and no host filesystem export is added.

The separate PVE exec smoke script validates integer PID wire types after string
checking (including JSON::XS), capture base64 and returned errors. It prints only
JSON-escaped metadata, never evaluates/prints decoded guest output as commands
or raw terminal text. It reads no existing host files and sends only synthetic
stdin bytes. Local helper mocks are not proof of real PVE exec integration.

## Parser/CVE-pattern and access review

The scoped review, including the file/clock expansion, covered memory safety, thread safety, race-free handler
ownership, unsafe-deserialization/framing/format-string patterns, and insecure
access paths. No unresolved critical/high code finding was identified in this
slice during local review. This is not a formal independent audit or a claim that
future features or native transport behavior are safe.

The host test-script follow-up also checked all five areas in its changed scope.
Signed handle bounds are validated before conversion; JSON::XS type changes are
corrected before transmission. Returned QGA error objects now fail tests rather
than being mistaken for results (especially a failed clock setter followed by
already-near-current readback). Original errors are JSON-escaped, not evaluated
or printed as raw terminal control strings. There are no new worker threads,
shared mutable state, guest permissions, shell commands or automatic retries;
existing single-owner handle cleanup remains. This is test-script hardening,
not a hypervisor security audit; ambiguous timeout/resource cleanup and blocking
guest I/O remain limitations.

Residual limitations:

- **Access:** any process with UART privileges can interfere/impersonate. No
  protocol authentication is added and host trust cannot be inferred from replies.
- **Availability:** one incomplete object can occupy the bounded parser until
  resynchronization, and blocking writes to a stalled device can stop progress.
  Default termination notes remain available. No transport timer/reconnect supervisor or
  output-rate limiting exist yet; malformed traffic can consume bounded work and
  emit errors. Depth, input and token limits bound each parse, not overall traffic.
  Bounded file chunks do not impose a deadline on blocking mounted-fileserver
  I/O. A mounted 9P server is not proven safe by descriptor type validation.
  Read/write errors can follow offset advancement/partial writes; blindly
  retrying mutations after an ambiguous timeout can repeat side effects.
- **Concurrency:** two running agents/consumers corrupt the stream; no lock is
  enforced. Boot-hook duplicate starts must be prevented operationally.
- **Testing:** native build, core/information/file/clock tests, binary file
  round-trip, current-time system/RTC update, raw-stream metadata and UART setup
  passed. The operator supplied host results through CPU/timezone/routes and
  corrected-script file/supplied-time clock success. UI, restart persistence and
  real power actions remain unverified. Omitted-time clock synchronization and
  clock failure injection are mocked, not live-tested.
  Linux ASan/UBSan and analyzer results cover the shared core/formatters/harness, not the
  native kernel, transport entry point or hypervisor.
- **Deployment:** permissions, namespace and trusted startup/binary ownership
  require site-specific validation. This project does not install or harden a
  service account automatically.
- **Development client:** Drawterm `-r` is not an OS security boundary. Its command
  device, inherited environment and symlink traversal can expose more than the
  selected directory. Diagnostic sessions used separate filesystem/PID isolation,
  cleared environment and a read-only project export, without home/SSH keys.
  This was not network isolation or a hardening change to the desktop launcher.

## Shutdown investigation: explicitly opt-in, power actions unverified

Upstream [fshalt](https://github.com/9front/9front/blob/front/rc/bin/fshalt) works
through native filesystem control channels and copies tools to a ramfs before
halting storage. Reboot mode eventually uses the kernel reboot device; non-reboot
mode attempts `scram`. This is materially different from a direct kill or writing
to a kernel reboot file without synchronization.

Do not infer that `fshalt` guarantees QGA's powerdown/halt semantics on every
filesystem/CPU-server setup. Verify local versus remote servers, filesystem flush
completion, `scram`/ACPI behavior, permissions, and asynchronous failure reporting
on a disposable guest before relying on the opt-in fixed-argv executor. Never
turn a request string into shell code. The user explicitly opted into shutdown
for the test deployment; no actual destructive power test has been performed.
Successful fork initiates the operation and produces no QGA success reply;
later exec, filesystem and ACPI failures can only be reported on `#c/cons`.

The boot helper sources trusted local rc configuration, restricts the dedicated
UART and starts the daemon with `-D`; it uses kernel console logging rather than
a login/Drawterm console. It has no automatic retry/supervisor or exclusive UART
lock. Keep configuration/binary/hooks administrator-controlled and use one launch
path. Any attacker with the same effective privilege can bypass these controls,
replace native tools/configuration, impersonate replies or request powerdown.

Security reports should be sent privately to the repository maintainer using a
channel the maintainer publishes; no private contact or GitHub security-advisory
setup is invented by this local-only repository.
