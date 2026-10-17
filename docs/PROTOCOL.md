# Protocol scope and compatibility

Authoritative specifications:
[QEMU Guest Agent reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html),
[QMP wire specification](https://www.qemu.org/docs/master/interop/qmp-spec.html).

QGA is not the QMP monitor: there is no unsolicited greeting or
`qmp_capabilities` handshake. Send normal `execute` requests. Request `arguments`
may be omitted when a command takes none; if present they must be an object.
Unknown top-level or implemented-command argument members are rejected.

## Current command table

| Command | Arguments / return | Availability | Successful response |
| --- | --- | --- | --- |
| `guest-sync` | required signed int64 `id`; returns it | Implemented | yes |
| `guest-sync-delimited` | same, with leading binary `0xFF` on reply | Implemented | yes |
| `guest-ping` | no arguments; returns `{}` | Implemented | yes |
| `guest-info` | agent version and `supported_commands` | Implemented | yes |
| `guest-get-osinfo` | native OS/architecture; no arguments | Native daemon | yes |
| `guest-get-host-name` | native `host-name`; no arguments | Native daemon | yes |
| `guest-get-time` | UTC epoch nanoseconds; no arguments | Native daemon | yes |
| `guest-network-get-interfaces` | actual native interfaces; no arguments | Native daemon | yes |
| `guest-get-vcpus` | active native CPUs; no arguments | Native daemon | yes |
| `guest-get-timezone` | native zone and UTC offset in seconds; no arguments | Native daemon | yes |
| `guest-network-get-route` | native IPv4/IPv6 routes; no arguments | Native daemon | yes |
| `guest-get-disks` | actual nonempty `#S` data/partition nodes; no arguments | Native daemon, including stdio | yes |
| `guest-get-load` | estimated numeric `load1m`, `load5m`, `load15m`; no arguments | Native daemon, including stdio, if sampler starts | yes |
| `guest-file-open` | required absolute `path`, optional `mode` (default `r`); returns opaque int64 handle | Native daemon, including stdio | yes |
| `guest-file-close` | required `handle`; returns `{}` | Native daemon | yes |
| `guest-file-read` | required `handle`, optional `count`; returns `count`, `buf-b64`, `eof` | Native daemon | yes |
| `guest-file-write` | required `handle`, `buf-b64`, optional `count`; returns actual `count`, `eof` | Native daemon | yes |
| `guest-file-seek` | required `handle`, signed `offset`, symbolic/numeric `whence`; returns `position`, `eof` | Native daemon | yes |
| `guest-file-flush` | required `handle`; returns `{}` | Native daemon | yes |
| `guest-set-time` | optional signed int64 nanosecond `time`; omitted means read RTC; returns `{}` | Native management channel, not stdio | yes |
| `guest-exec` | required `path`, optional `arg`, `env`, `input-data`, `capture-output`; returns native `pid` | Native management channel, not stdio | yes |
| `guest-exec-status` | required owned `pid`; running or final result; consumes final metadata | Native management channel, not stdio | yes |
| `guest-shutdown` | optional `mode`: powerdown (default), reboot; halt unsupported | Explicit `-S` only | **no** |

With the load sampler running, `guest-info` lists twenty-two commands on native
management channels, twenty-three with `-S`. Native stdio lists nineteen
(no shutdown, clock setter or exec/status). If sampler startup fails, each count
is one lower and load is not advertised. The minimal
Linux stream harness registers no native callbacks and lists only four. All
listed entries have `enabled: true`; only shutdown has `success-response: false`.
Excluded commands are not advertised. The current source version is `qga9-0.6.1-dev`.
The last verified Proxmox deployment is `0.6.0-dev`; real-PVE acceptance of the
`0.6.1-dev` native-libjson migration remains pending. See [STATUS](STATUS.md) for
deployment evidence. Earlier versions through 0.4 were accepted in
operator-supplied Proxmox output, including the five exec smoke tests. The previous
file/clock batch passed corrected-script host binary round-trip and supplied-time
clock tests; omitted-time and failure injection remain separate checks.

## Load estimates

`guest-get-load` has no arguments and returns three JSON numbers. A continuous
private native sampler counts Ready/Running processes, excluding itself, and
updates 60/300/900-second exponential estimates at one-second intervals.
These are estimated runnable counts, not percentages or the kernel's roughly
one-second-decayed `#c/sysstat` value. The estimator starts at zero when the
sampler starts; it cannot reconstruct boot history and needs time to settle.

Process snapshots are non-atomic. Fixed-point arithmetic uses millionths and
serializes six decimal places; rounding has a small residual error during decay.
The elapsed source is native process milliseconds modulo 2^32, not wall time.
Invalid metadata, excessive process counts or gaps longer than three seconds
reset the model and fail that query rather than inventing missing samples.
Later valid samples restart estimation. The model/protocol have host tests;
normal native sampler/query tests and real virtio queries also passed. This is
not exhaustive failure/lifetime validation.

## JSON and framing

- Maximum input object size: 8192 bytes, including internal whitespace; maximum
  tokens: 512; maximum recursive value depth: 16. Duplicate decoded object keys
  are rejected. Framing/output and adapter token/text storage are fixed after
  creation. Patched native libjson uses at most 64 KiB of cumulative requested
  allocation bytes per request (excluding allocator overhead), frees its temporary
  tree before dispatch and retains no borrowed input. Test/client response parsing
  has a separate 256 KiB allocation budget. Exec adds bounded per-job native
  supervisors and fixed-size result buffers.
- Strict JSON with validated UTF-8 input; no QMP single-quoted-string extension.
  Output is ASCII JSON with CRLF framing. Unicode is escaped, including surrogate
  pairs for non-BMP scalars. Invalid UTF-8, unpaired surrogates and raw string
  control characters are rejected.
- A complete top-level object is dispatched when its closing delimiter arrives:
  no newline or EOF is required. Fragmented and adjacent objects are supported;
  newlines outside strings may also occur inside a valid object.
- A raw `0xFF` byte discards all unfinished/discarded input state, even inside a
  string. It emits no reply by itself. Escaped Unicode `\u00ff` is ordinary data.
  A valid delimited sync prefixes **only its own success response** with `0xFF`.
- Oversized or non-object garbage emits one bounded error, then is discarded
  through newline or sentinel. A partial object can remain buffered until more
  data, a sentinel or EOF; there is no incomplete-message timer. Invalid complete
  objects produce an error; EOF on buffered partial JSON produces a truncation
  error in stream-test mode. Serial errors cause exit rather than retry loops.
- Writes are retried only while a callback consumes positive, in-range bytes.
  Zero/error/impossible byte counts stop the instance. Output overflow sends
  nothing rather than a truncated response. Output is capped at 65536 bytes.
  Native queries now reserve escaped transaction-ID/framing space before
  serialization; oversized query results return a bounded error and leave the
  stream usable instead of overflowing when the ID is appended.

Sync IDs must be JSON integer literals in `[-9223372036854775808,
9223372036854775807]`. Floats, exponent notation and unsigned values above the
signed range are not accepted for this argument. Validation/echo use digit strings,
not `double` or unchecked integer conversions. A top-level transaction `id` is
separate: any valid JSON value within the parser's limits is reserialized in
responses, with ASCII escaping. Parse errors before a valid request omit its ID.

Examples:

```json
{"execute":"guest-sync","arguments":{"id":9223372036854775807}}
{"return":9223372036854775807}
{"execute":"guest-ping","id":"probe"}
{"return":{},"id":"probe"}
```

The sentinel is a binary byte, not the four printed characters `0xFF` or a JSON
field. Host clients must ignore stale replies until the matching sync response
and reset their own parsing state when they encounter the response sentinel.

Errors use `{"error":{"class":"...","desc":"..."},"id":...}`:
`GenericError` for malformed/invalid input and bounds, `Unsupported` for known
pending or policy-excluded commands, and `CommandNotFound` for unknown names.
Descriptions are for people, not machine parsing. No reply implies no permission
or trust guarantee; a guest can forge its own answers.

## Native values and shutdown limitations

- OS information identifies 9front and reads `/env/cputype` for `machine`; no
  unverified kernel release/version is invented. Host-name reads `#c/sysname`.
- Time uses native `nsec()` (signed 64-bit UTC Unix epoch nanoseconds from bintime);
  errors produce `GenericError`, not a guessed time.
- Interfaces use required `name`, optional `hardware-address`/`ip-addresses`, and
  entries with `ip-address`, `ip-address-type` (`ipv4`/`ipv6`) and integer `prefix`.
  Actual `/net/ipifc/N/status` records supply addresses; contiguous masks are
  checked. Names are `ipifcN`; verified `/net/etherN/addr` supplies optional MACs.
  Unbound conversations and unverified counters are omitted. Status is capped at
  8192 bytes, interfaces and addresses per interface at 64; errors/overflow fail
  the query rather than sending fabricated or truncated output.
- vCPU inventory reads `#c/sysstat`, which includes active kernel CPUs. Sparse
  native IDs are preserved, `online` is true, and `can-offline` is false: this
  agent cannot hotplug/offline CPUs. There is no fictitious inactive CPU list.
  Snapshots with duplicate IDs, malformed records, or over 256 CPUs fail.
- Timezone uses native `tzload("local")` and caller-owned `tmnow` storage, including
  the current native DST decision. Failure does not silently substitute GMT.
  The zone abbreviation is informational, not an IANA timezone identifier.
  The library caches `/env/timezone`; restart the agent after changing that
  configuration. The agent does not rewrite or correct native timezone tables.
- Routes read a bounded `/net/iproute` snapshot, preserving interface numbers,
  destinations, IPv4 masks, and IPv6 destination/source prefixes and next hops.
  Native IPv4 mask lengths include 96 IPv4-mapped bits; these are removed in QGA
  output. Built-in interface/local/broadcast/multicast routes omit a gateway.
  9front has no scalar route metric, so the required QGA `metric` is zero, not a
  measured priority. Linux flags/counters are omitted. Native source-specific
  IPv4 routes can look duplicate because QGA has no IPv4 source-prefix fields;
  they are not deduplicated. Snapshots are limited to 65536 bytes/256 routes,
  with malformed records, bad addresses or overflow failing the whole query.
- Successful shutdown is asynchronous and has **no success response**. Advertise
  `success-response: false` only when enabled. Without `-S`, shutdown returns
  `Unsupported`. With `-S`, powerdown/reboot select fixed native argv; `halt`
  remains `Unsupported`, invalid modes/arguments return `GenericError`.
  One successfully initiated operation is permitted per instance. Executor-fork
  success is not confirmation of flush completion or actual poweroff: later exec,
  filesystem or ACPI failures are console-only. Local emulated powerdown passed;
  reboot, other platforms and power-failure paths remain unverified.
  Once shutdown is initiated, file, clock and execution operations are rejected.

## File semantics and limits

- At most 16 live opaque handles. IDs are monotonic within one process, not OS
  descriptors; closed IDs are not reused. All handles must be discarded after an
  agent restart. EOF remains sticky until seek/write, as for a file stream.
- Decoded absolute paths have a maximum of 1023 UTF-8 bytes and no NUL. Supported
  modes: `r`, `rb`, `r+`, `r+b`, `rb+`, `w`, `wb`, `w+`, `w+b`, `wb+`.
  All append variants return `Unsupported`: native per-open atomic append is not
  available, and changing a file's permanent append-only mode is not equivalent.
- Native open validates the acquired descriptor as a mounted filesystem file,
  not a directory/auth file or direct kernel device. New files are created 0600
  with exclusive create; existing files are truncated through the validated
  descriptor. Missing files require a readable mounted-M directory parent
  before create, and the backend takes a private namespace to prevent peer
  rebinding. `/env`, `/srv` and non-M aliases are rejected before create.
  A private helper anchors its cwd, rejects final-parent unions using native
  MCREATE self-bind validation, rechecks the mounted directory, and creates the
  relative leaf exclusively. A leased, owner-only srv board transfers the
  descriptor back without sharing the helper's descriptor table. Native
  mixed-union/leased-board regression passed locally and on the Proxmox guest;
  the updated backend is installed there and the operator supplied a passing
  host binary round-trip. See STATUS for the exact scope and remaining checks.
  Access follows the agent's native identity and namespace; errors are not a
  general no-side-effect or rollback guarantee.
- Read count defaults to 4096; read/write count is limited to 0..4096 bytes.
  This is deliberately smaller than upstream QGA's read maximum. Use repeated
  chunks. Write buffers must decode to at most 4096 bytes, with canonical padded
  RFC 4648 base64 (no whitespace, ignored suffix, NUL or nonzero pad bits).
  A supplied write count cannot exceed decoded length. Short writes return the
  actual count; errors may follow a partial file mutation or offset advance.
- Seek accepts `set`/`cur`/`end` or integer 0/1/2. Signed arithmetic overflow and
  positions below zero fail. Seek clears EOF; an exact-length read only reports
  EOF after a subsequent read observes it.
- Flush has no pending user-space buffer because writes go directly to the
  native kernel/fileserver. Success does not mean fsync, filesystem freeze,
  transaction completion or durable storage. Closing consumes the handle even
  if close reports failure. Process teardown releases remaining descriptors.
- The response's exact transaction ID/framing space is reserved before file
  side effects. A stalled mounted fileserver can still block the single owner.

## Clock semantics and limits

The management-only setter acquires `#c/bintime` and `#r/rtc` before mutation.
Both require native eve permission for writes. Supplied time must be between 0
and 3155759999999999999 nanoseconds (1970 through 2069 UTC), matching the PC RTC
year decoder. With no time argument, the RTC seconds are checked before conversion.
The handler updates system time, reads actual current time, then updates RTC in
whole UTC seconds. If readback/RTC update fails after the system change, the error
explicitly says so; there is no pretend atomic update or automatic rollback.
An independent time-sync process can concurrently adjust/override the system
clock; the agent does not stop it. Stdio mode cannot invoke this setter.
Native timesync `-L` assumes a local-time RTC and conflicts with this UTC policy;
an immediate successful readback does not establish persistent synchronization.

## Execution semantics and limits

Management channels enable exec; `-s` does not. Requests are direct `exec` argv,
never agent-generated shell code. A caller may explicitly choose `/bin/rc`, with
the same guest authority as any other executable. This is not a command sandbox.

- Path: 1..1023 UTF-8 bytes, no NUL; absolute or a bare command resolved under
  `/bin`. Relative paths containing `/` are rejected. Argv[0] is the supplied path.
- Up to 32 extra argument strings and 32 `NAME=value` environment overrides;
  decoded strings including terminators share 4096 bytes. Empty arguments and
  values are valid. Names contain only ASCII letters/digits/underscore and are
  nonempty; arbitrary `/env` paths are not accepted. The child's copied native
  environment is overridden in array order, so the last repeated name wins.
  No shell expansion occurs, and the agent's environment is not modified.
- Optional `input-data` is canonical padded base64, at most 4096 decoded bytes.
  The complete JSON request still must fit 8192 bytes. Stdin receives those bytes
  followed by EOF. No host files are read or shared by this API.
- `capture-output` accepts false/`none` (default), true/`separated`, `stdout`,
  `stderr`, or `merged`. Uncaptured streams go to guest `/dev/null`; merged mode
  actually redirects both descriptors to the same pipe, reported as `out-data`.
- At most eight retained jobs, including exited but unqueried results. PIDs are
  real positive guest PIDs, not OS descriptors or synthetic job IDs. Only this
  agent's retained PIDs can be queried. PID collisions with old retained results
  are refused. Agent restart invalidates all retained results.
- Each supervisor has private memory, namespace, environment and descriptors;
  stdout/stderr are drained independently while the UART owner services requests.
  First 4096 bytes per captured stream are retained; excess is drained/discarded
  and `out-truncated`/`err-truncated` is true. No disk spool is created. Internal
  pipes are capped at 32768 bytes; a smaller native pipe maximum rejects spawn.
- Startup has an approximately five-second polling budget. Executable/setup
  failure returns `GenericError`, not a false successful PID. Once successfully
  exec'd, runtime is not limited. A running result contains only `exited: false`.
- Final results include `exited: true`, `exitcode`, and selected base64 stream
  fields/truncation flags, including empty captured streams. Plan 9 uses string
  exit status: empty maps to 0, nonempty (including fatal notes) to 1. No Linux
  signal number or numeric interpretation of native exit strings is invented.
- Once the direct child exits, the supervisor drains a bounded snapshot of bytes
  already queued and closes capture pipes. Background descendants cannot pin
  completion indefinitely; their later writes are not guaranteed captured.
  Deliberately backgrounded descendants may survive ordinary result reaping.
- The first successful final status query consumes metadata; subsequent queries
  fail. Exact transaction-ID/response space is reserved before spawn or reaping.
  Capture replies can exceed the request size, but remain below the 64 KiB
  response limit. A separate bounded response parser is used by native tests;
  the daemon continues using the 8 KiB request parser.
- Normal agent cleanup cancels retained job note groups; supervisors detect
  owner death using an already-open process-identity descriptor. A tiny note
  handler treats only broken-pipe notes as I/O errors and never touches parser
  or job state. Cancellation is not containment: guest commands can change note
  groups or install handlers, and same-user attackers can interfere. Ambiguous
  host timeouts can leave a command executing; do not blindly retry mutations.

The upstream schema is described by the
[QGA execution reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html#command-guest-exec).
Native limits and exit/descendant behavior above are deliberate compatibility
restrictions, not Linux process semantics transplanted onto 9front.
