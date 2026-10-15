# Security model and safety contracts

The QGA stream is untrusted parser input and authorized host management plumbing.
A malicious host's guest file, execution and time authority is accepted by this
model. The daemon applies bounds and native identity/namespace permissions; it
does not confine a host that controls the VM or a compromised guest kernel.

QGA provides no protocol authentication or guest trust attestation. Same-user
programs and guest eve can interfere with the agent, replace its configuration
or impersonate replies. These checks make no host-escape or QEMU/PVE isolation
claim and do not convey host credentials.

## Privileged operations

File requests can read, create or truncate mounted files under the agent's
identity. Execution passes literal native argv/env/stdin, without interpolation
into rc source. The host can explicitly launch a shell or network-capable guest
program. Execution is not a sandbox: jobs can allocate, fork, modify files,
escape a note group or ignore cancellation.

Clock setting changes system time and UTC RTC on management channels.
Shutdown is off by default; `-S` selects fixed `fshalt` or `fshalt -r` argv.
Fork success gives no QGA success reply and does not prove flush or poweroff.
Later shutdown failures are console-only; actual power semantics remain untested.
`halt`, password/key mutation, freeze/thaw and trim are unsupported.

Stdio diagnostics enable file operations but disable exec, clock setting and
shutdown. Namespace contents and launch identity determine accessible files.

## Memory and parser boundaries

Each protocol instance has one checked allocation and one synchronous owner.
Requests are capped at 8192 bytes, 512 tokens and depth 16; responses at 65536
bytes. UTF-8, escapes, decoded duplicate keys and int64 syntax are checked.
Integer lexemes preserve exact IDs. File/exec handlers reserve response space,
including the transaction ID, before state-changing I/O. Output overflow is
sticky and not sent.

Writer buffers are borrowed for the synchronous call only. Short writes are
handled; zero, negative or oversized successful counts fail the instance.
Borrowed input is serialized before frame reuse. The API is not reentrant or
safe for concurrent access to one instance.

Native inventory reads cap text, fields, directories and records. Truncated,
malformed or inconsistent sources discard partial results. Native source I/O
can still block. CPU/routes/disks are snapshots, not atomic hotplug transactions;
disk metadata changes after validation or ABA changes remain possible.

The base64 wrapper validates canonical syntax, exact output length and capacity
before native libc conversion. Native libjson is not linked into the daemon;
the evaluated API loses exact integers and lacks required parser/serialization
semantics. [CODECS](CODECS.md) records scoped evidence, not a blanket judgment
about all native helper reuse.

## Files and clocks

File handles are single-owner, monotonically issued and never reused within an
instance; restart invalidates them. Bounds are 16 handles and 4096 bytes per
read/write. Canonical base64, signed arithmetic and paths are validated before
use. Append modes remain unsupported. Close consumes its handle even on failure,
and normal teardown releases remaining descriptors.

Existing files are validated through the acquired descriptor before truncation.
The final descriptor must be a mounted M file, excluding directories, auth files
and direct kernel devices. Missing truncate-mode paths require a readable
mounted-M directory parent. A private RFNAMEG namespace prevents peer rebinding.
An isolated helper anchors its cwd, self-binds it with MREPL|MCREATE, revalidates
the mounted directory and exclusively creates the relative leaf. Native cmount
rejects a multi-branch source for this bind, so a mixed-device union cannot
redirect creation to an uninspected device. All final-parent creation unions
are rejected conservatively, not merely mixed-device ones.

The helper copies rather than shares the descriptor group and closes inherited
management/job descriptors before filesystem operations. A native leased srv
board, restricted to owner access, transfers the created Chan to the serving
process. Close-on-remove posting and lease cleanup release the board/entry;
there is no guessed-path deletion or permanent global descriptor publication.
The public-wire adapter mock and creator syscall mock cover denial, bounds,
collisions and resource cleanup. The actual namespace/leased-board regression
is prepared but not executed. Host mocks do not establish native correctness.

The agent does not itself rebind while serving, and exec supervisors copy its
namespace. Readable-parent validation adds a restriction even if a fileserver
would allow create without read. Final descriptor checks remain; later failures
can leave a newly created ordinary file. Rejection is not general rollback.

External writers/metadata changes remain possible. Flush means no pending
userspace buffer, not fsync, freeze or durable storage. Errors/timeouts can follow
offset advancement or partial mutation; blind retry can repeat side effects.
Mounted 9P servers are not made safe by descriptor type checks.

Clock state is local and bounds precede conversion/mutation. Both fixed devices
are acquired before changing system time, which is updated before the RTC.
System/RTC updates are not atomic; later failures explicitly report partial
change. Other time-sync processes can override the result; no rollback is added.

## Load sampler

One private process reads bounded kernel process metadata and owns the entire
fixed-point load model. It shares neither mutable memory nor its descriptor table
with the agent. The only retained descriptors are its private IPC endpoint and
the original owner's opened status Chan; owner disappearance stops the sampler.
Requests/replies have fixed framing and numeric bounds. Shutdown requests stop
on the next sampling iteration, not a hard deadline. Kernel metadata I/O can
still block, and a broken sampler can fail a query. This native lifecycle has
not passed a real guest test yet.

## Processes and concurrency

Only the serving process mutates protocol, file and job state. Native detachment,
shutdown and exec supervisors use private memory/fd/namespace/env/note groups.
Pipe messages replace shared-memory worker state. The small broken-pipe note
handler does not access parser or job storage.

Eight job/result slots and fixed input/capture buffers bound agent storage.
Exec handshake distinguishes exec from fork success. Supervisors drain both
output pipes, discard excess and publish one bounded result. Direct-child exit
drains a finite queued snapshot; later descendant output is not guaranteed.
Ordinary reaping may leave deliberately backgrounded descendants running.

Each child closes inherited descriptors except redirected stdio and its
close-on-exec handshake. Detached agents retain stdio plus channel handles.
Cancellation uses an already-open note-group descriptor, avoiding a later lookup
through a recycled PID. Retained process-status descriptors detect owner death.
Cancellation is best effort. Jobs have no runtime deadline; unqueried finished
results occupy slots, and limits do not bound a guest executable's resources.
Host disconnect/timeout does not cancel or safely replay launched work.

CPU/route/storage handlers have no shared mutable globals. The timezone library
owns its locked cache; restart after changing timezone configuration.
File, clock and exec requests are refused after shutdown initiation, but already
open descriptors are not a filesystem-consistency guarantee.

## Transport and startup access

UART and arbitrary raw streams add no exclusive owner lock. Use one trusted
launch path and restrict device permissions. The separate
[driver](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/SECURITY.md)
enforces exact Chan ownership and eve-only #Y data/status permissions.
Host socket reconnect preserves the guest owner, RX and file/job state.
Actual guest close/removal/fault follows driver drain/reset rules.

The helper sources trusted local rc configuration and checks/restricts channel
permissions. Canonical #Y data paths retain fixed driver permissions. The helper
now makes up to 30 one-second readiness waits, using stat-only existence checks,
then launches once or reports timeout. It does not open/claim data to probe or
restart a daemon after owner retirement. Scheduling and fileserver delays can
exceed the nominal wait budget; no hard wall-clock bound is claimed.

`mk install`'s existing-file check is not an atomic security boundary. Binary,
configuration, hooks and target directories must be trusted. Private namespaces
do not protect eve from another eve process.

## Evidence and remaining risks

Review scope covers memory safety, thread ownership, races, parser/lifetime/
overflow patterns and privileged access. Local fixes preserve single-owner file
and protocol state, checked buffer capacities and exact descriptor cleanup.
Native validation/deployment and trust in mounted fileservers remain explicit.
See [STATUS](STATUS.md) for the consolidated evidence.

Post-split Linux C/Python regressions, ASan/UBSan/leaks and GCC analyzer passed.
Prior native normal-path tests and operator PVE results passed. Native post-split
checks have not been rerun. Current driver TSan attempts failed runtime
initialization, not with a code race report; no current TSan pass is claimed.

Fixtures do not prove native kernel IRQ/DMA/reset/unplug or callback lifetime
safety. Incomplete input can occupy the parser until resynchronization;
blocking device/fileserver I/O can stop progress. Bounds limit each request,
not total traffic or wall-clock time. UI and actual power actions remain
unverified.

Drawterm exports require independent filesystem/process isolation, cleared
credentials and narrowly selected source access. Drawterm `-r` alone is not a
sandbox; command devices, environment and symlinks can expose additional host
resources. No network-isolation claim follows from filesystem isolation.

Send reports privately through a channel published by the repository maintainer;
no private contact or advisory setup is asserted here.
