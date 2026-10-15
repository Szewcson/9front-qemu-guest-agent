# Complete QGA inventory and incremental implementation

Inventory inspected on 2026-10-10 against the
[QGA reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html) and native
9front sources in the test VM. The scope includes remaining standard QGA
features and native virtio-serial. Native semantics are required for every
advertised capability.

The upstream cross-platform union has 44 commands. This revision implements 23
(shutdown opt-in; load requires a running sampler), leaving 21 pending.
The load model, native sampler and real local virtio queries passed; the updated
code has not been deployed on the Proxmox VM.
Some implemented commands have explicit
native restrictions below. It advertises only implemented commands;
all known pending names return `Unsupported`, not a bogus success.

| Group | Commands | Status / native implementation prerequisites |
| --- | --- | --- |
| Protocol (4) | `guest-sync`, `guest-sync-delimited`, `guest-ping`, `guest-info` | Implemented, bounded parser/stream tests |
| Existing native queries (4) | `guest-get-osinfo`, `guest-get-host-name`, `guest-get-time`, `guest-network-get-interfaces` | Implemented; operator supplied host OS/network output |
| Power (1) | `guest-shutdown` | Opt-in native powerdown/reboot; local filesystem halt and QMP guest-shutdown passed. Reboot/failure paths unverified; `halt` remains unsupported |
| First expansion (3) | `guest-get-vcpus`, `guest-get-timezone`, `guest-network-get-route` | Implemented; native tests and operator-supplied Proxmox results passed |
| Guest files (6) | `guest-file-open`, `guest-file-close`, `guest-file-read`, `guest-file-write`, `guest-file-seek`, `guest-file-flush` | Implemented; native and operator-supplied PVE binary round-trips passed. 16 opaque handles, 4 KiB chunks, bounded canonical base64, read/update/truncate modes; append explicitly Unsupported. Flush is user-space buffering semantics, not stable-storage sync |
| Clock mutation (1) | `guest-set-time` | Implemented on management channels; native system/UTC RTC readback and operator-supplied PVE current-time test passed. Eve permission preflight, 1970–2069 range, explicit partial failures. Supplied-time and RTC-derived synchronization also passed over local virtio with QEMU's emulated RTC; physical RTC/failure cases remain separate |
| Processes (2) | `guest-exec`, `guest-exec-status` | Implemented on management channels: direct native argv execution, literal environment overrides, binary stdin, all capture modes, 8 jobs, 4 KiB/stream, active concurrent draining and one-shot result reaping. Native, daemon-stream and operator-supplied PVE tests passed |
| Disk inventory (1) | `guest-get-disks` | Implemented using native `#S` ctl plus directory metadata. Genuine nonempty whole-data/partition nodes and verified dependencies; no raw/data reads, SMART requests or invented hardware addresses. Native tests and operator-supplied PVE query passed |
| Remaining storage queries (2) | `guest-get-fsinfo`, `guest-get-diskstats` | Require namespace-aware filesystem-specific adapters and upstream-compatible statistics semantics. `hjfs df` is a slow shared-console operation, not an agent RPC. Diskstats requires Linux major/minor IDs with no native counterpart. See STORAGE; no counters or global mount table may be invented |
| Load query (1) | `guest-get-load` | Implemented: a private continuous sampler counts native Ready/Running processes and maintains bounded 1/5/15-minute exponential estimates. Starts at zero with the sampler, not boot history. Native sampler and real local virtio queries passed; see PROTOCOL for approximation/error semantics |
| Remaining session/CPU queries (2) | `guest-get-users`, `guest-get-cpustats` | No utmp-style session registry found; process owners are not logins. CPU-stat union currently has Linux-only schema; do not falsely label 9front as Linux |
| CPU hotplug (1) | `guest-set-vcpus` | No equivalent supported control API found; a kernel interface is needed before this can do real work |
| Memory hotplug (3) | `guest-get-memory-blocks`, `guest-set-memory-blocks`, `guest-get-memory-block-info` | Linux hotplug block model has no observed 9front equivalent. A memory total is not a hotplug block list; needs genuine kernel semantics |
| Authentication (4) | `guest-set-user-password`, `guest-ssh-get-authorized-keys`, `guest-ssh-add-authorized-keys`, `guest-ssh-remove-authorized-keys` | Plan 9 auth tickets/factotum are not crypt()/OpenSSH authorized_keys. Need a defined native account/SSH-server adapter and capability checks; do not edit arbitrary users' files to mimic another OS |
| Consistent backups (4) | `guest-fsfreeze-status`, `guest-fsfreeze-freeze`, `guest-fsfreeze-freeze-list`, `guest-fsfreeze-thaw` | Native filesystem-server transaction: quiesce, sync, thaw, watchdog and rollback. `sync` alone is not freeze; preserve namespace-aware filesystem identity |
| Storage discard (1) | `guest-fstrim` | Free-extent enumeration plus verified backing-device discard, independently per native filesystem. Never discard guest blocks by guessing filesystem allocation |
| Suspend (3) | `guest-suspend-ram`, `guest-suspend-disk`, `guest-suspend-hybrid` | Real ACPI sleep/wakeup and/or hibernation image support. Poweroff is not suspend; kernel/platform work required |
| Windows devices (1) | `guest-get-devices` | Current command describes Windows device/driver identifiers, not a generic PCI inventory. Requires compatible schema/native semantics or remains explicitly unsupported |

The counts sum to 44. Some commands may appropriately remain `Unsupported` on
9front unless the needed native subsystem or upstream schema is added, just as
upstream QGA does not implement its entire union on every supported OS.

## Implementation order

1. Done: bounded read-only CPU/timezone/route queries, raw-stream adapter, tests.
2. Done: bounded user-space file API and clock control, native fixtures and
   explicit limits/partial failures. Corrected-script Proxmox file and supplied-time
   clock tests also passed in operator-provided output.
3. Done: bounded processes/status, native argv/env/stdin/capture/cleanup,
   duplex-daemon tests and operator-supplied PVE exec results. Disk inventory is
   implemented, native-tested and passed the operator's read-only PVE query.
   The subsequent native-base64 refactor passed the operator's file/exec retest
   on `0.5.1-dev`. Filesystem
   inventory needs a genuine local-fileserver adapter; diskstats needs native
   counters plus a compatible schema, not fabricated Linux device numbers.
4. Native virtio-serial now belongs to a separate
   [driver repository](https://github.com/Szewcson/9front-virtio-serial/blob/main/README.md), not this QGA build.
   Its real PCI/#Y raw-stream path is installed on the authorized VM; operator
   PVE binary file/exec smoke tests and automatic cold startup passed.
   Host socket reconnect preserves guest ownership and file/job state.
   The default stays masked/polling. A separate local checked-IRQ kernel passed
   actual IRQ delivery, forced reset, malformed completion and named-port
   removal/re-add with fresh contexts. Broader platform/fault validation remains;
   see its [ledger](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/STATUS.md).
5. Implemented locally: continuous load estimates, strict codec reuse and
   pinned-parent creation that rejects final-parent unions. Native tests passed;
   this is not a claim that all remaining commands are implemented.
6. Filesystem/auth/kernel-specific features behind real capability checks. Never
   enable Proxmox backup freeze/trim merely because a handler name exists.

## Host-side security constraints

Our protocol/resource limits protect the legitimate daemon, not Proxmox against
a fully compromised guest which can replace it. The host must bound untrusted
responses, apply timeouts and avoid evaluating guest-returned strings. QGA is
not QMP and no host-control extension is being added. Exec/file/credential APIs
operate only inside the guest; they do not convey host shell authority.

Development-client export limitations are recorded in [SECURITY](SECURITY.md).
