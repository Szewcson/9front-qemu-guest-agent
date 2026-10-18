# Supported scope and QGA inventory

Inventory inspected on 2026-10-10 against the
[QGA reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html) and native
9front sources in the test VM. Scope revised on 2026-10-15: maintain useful
native management features and their reliability, not the complete cross-platform
command union. Native semantics are required for every advertised capability.

The upstream cross-platform union has 44 commands. This revision implements 23
(shutdown on management channels; load requires a running sampler), leaving 21 unsupported.
All 21 are intentionally outside active scope, not unfinished requirements.
On 2026-10-15 the operator chose to keep `guest-get-fsinfo` unsupported rather
than add trusted filesystem-mapping configuration after the identity investigation.
The load model, native sampler and real local virtio queries passed. The updated
userspace code built/tested natively and was deployed on the Proxmox guest on
2026-10-15. The operator supplied passing updated PVE ping/info/network,
CPU/timezone/routes, file/exec smoke checks and OS/hostname/time/disk/load queries.
The operator also reported a normal PVE shutdown/start with eventual automatic
agent availability. Later newer polling/checked warm boots passed guest-side
startup/isolation and operator host smoke/IP checks. See STATUS for current gates.
Some implemented commands have explicit
native restrictions below. It advertises only implemented commands;
all known unsupported names return `Unsupported`, not a bogus success.

The practical Proxmox target is reliable synchronization/ping/capability
reporting, graceful powerdown, IP reporting, OS/hostname data and clock control.
These exist already. Keep the existing file/exec and other queries; no removal
is implied. New drivers, filesystem-server protocols, session registries,
authentication infrastructure and invented Linux interfaces are not planned.

| Group | Commands | Status / native implementation prerequisites |
| --- | --- | --- |
| Protocol (4) | `guest-sync`, `guest-sync-delimited`, `guest-ping`, `guest-info` | Implemented, bounded parser/stream tests |
| Existing native queries (4) | `guest-get-osinfo`, `guest-get-host-name`, `guest-get-time`, `guest-network-get-interfaces` | Implemented; operator supplied host OS/network output |
| Power (1) | `guest-shutdown` | Native powerdown/reboot on management channels, without separate opt-in; local filesystem halt and QMP guest-shutdown passed. Reboot/failure paths unverified; `halt` remains unsupported |
| First expansion (3) | `guest-get-vcpus`, `guest-get-timezone`, `guest-network-get-route` | Implemented; native tests and operator-supplied Proxmox results passed |
| Guest files (6) | `guest-file-open`, `guest-file-close`, `guest-file-read`, `guest-file-write`, `guest-file-seek`, `guest-file-flush` | Implemented; native and operator-supplied PVE binary round-trips passed. 16 opaque handles, 4 KiB chunks, bounded canonical base64, read/update/truncate modes; append explicitly Unsupported. Flush is user-space buffering semantics, not stable-storage sync |
| Clock mutation (1) | `guest-set-time` | Implemented on management channels; native system/UTC RTC readback and operator-supplied PVE current-time test passed. Eve permission preflight, 1970–2069 range, explicit partial failures. Supplied-time and RTC-derived synchronization also passed over local virtio with QEMU's emulated RTC; physical RTC/failure cases remain separate |
| Processes (2) | `guest-exec`, `guest-exec-status` | Implemented on management channels: direct native argv execution, literal environment overrides, binary stdin, all capture modes, 8 jobs, 4 KiB/stream, active concurrent draining and one-shot result reaping. Native, daemon-stream and operator-supplied PVE tests passed |
| Disk inventory (1) | `guest-get-disks` | Implemented using native `#S` ctl plus directory metadata. Genuine nonempty whole-data/partition nodes and verified dependencies; no raw/data reads, SMART requests or invented hardware addresses. Native tests and operator-supplied PVE query passed |
| Remaining storage queries (2) | `guest-get-fsinfo`, `guest-get-diskstats` | Intentionally unsupported. Fsinfo lacks verified local mount/server identity; no trusted mapping configuration will be added. `hjfs df` is a slow shared-console operation, not an agent RPC. Diskstats requires Linux major/minor IDs with no native counterpart. See STORAGE; no counters or global mount table may be invented |
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

## Revised reliability roadmap

1. Verify the installed helper, single boot hook, detached process survival,
   shutdown/restart, IP reporting and UTC time policy on the actual Proxmox VM.
   Local QEMU results do not replace that host acceptance.
2. Exercise pending native reads/writes during notes, backend reconnect, final
   close and named-port removal. Preserve exact ownership and bounded cleanup.
3. Exercise repeated fresh-context recovery, retained old status descriptors,
   the four-context limit, explicit exhaustion and clean cold-boot recovery.
4. Retain parser/resource bounds, access checks and conservative DMA/reset
   handling. Fix failures within the existing architecture; do not add features
   merely to increase the advertised command count.

The [driver repository](https://github.com/Szewcson/9front-virtio-serial)
owns native transport tests and its own evidence ledger. General PCI hot-unplug,
other CPU/cache architectures, additional controllers and extensive emulator
fault infrastructure are not this milestone's requirements. Safety limitations
must remain documented; this scope reduction is not a proof of their absence.

`guest-get-fsinfo` was investigated as a final metadata-only step. A native
private-namespace experiment demonstrated that a stable mount source name can
refer to a different currently posted pipe. Optional capacity fields are not the
blocker; verified local mount/server identity is. The operator chose to retain
`Unsupported`, with no new configuration, so fsinfo is removed from the active
implementation roadmap. No fileserver protocol, driver, console scraping or
empty-success inventory will be added to bypass that limitation. See
[STORAGE](STORAGE.md).

Freeze/trim stay disabled in Proxmox. Live backups without quiescing must not be
advertised as application-consistent; use a clean shutdown for conservative
backup consistency. The host explicitly supports disabling freeze separately
from the agent; see [Proxmox agent configuration](https://github.com/proxmox/qemu-server/blob/master/src/PVE/QemuServer/Agent.pm).

## Host-side security constraints

Our protocol/resource limits protect the legitimate daemon, not Proxmox against
a fully compromised guest which can replace it. The host must bound untrusted
responses, apply timeouts and avoid evaluating guest-returned strings. QGA is
not QMP and no host-control extension is being added. Exec/file/credential APIs
operate only inside the guest; they do not convey host shell authority.

Development-client export limitations are recorded in [SECURITY](SECURITY.md).
