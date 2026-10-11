# Complete QGA inventory and incremental implementation

Investigation: 2026-10-10, against the current
[QGA reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html) and native
9front sources in the test VM. The operator expanded the original brief to ask
for all missing standard features plus native virtio-serial. This supersedes the
original exclusions, but does not make platform-specific kernel APIs exist.

The upstream cross-platform union has 44 commands. This revision implements 21
(shutdown opt-in), leaving 23 pending. Some implemented commands have explicit
native restrictions below. It advertises only implemented commands;
all known pending names return `Unsupported`, not a bogus success.

| Group | Commands | Status / native implementation prerequisites |
| --- | --- | --- |
| Protocol (4) | `guest-sync`, `guest-sync-delimited`, `guest-ping`, `guest-info` | Implemented, bounded parser/stream tests |
| Existing native queries (4) | `guest-get-osinfo`, `guest-get-host-name`, `guest-get-time`, `guest-network-get-interfaces` | Implemented; operator supplied host OS/network output |
| Power (1) | `guest-shutdown` | Opt-in native powerdown/reboot; `halt` remains unsupported |
| First expansion (3) | `guest-get-vcpus`, `guest-get-timezone`, `guest-network-get-route` | Implemented; native tests and operator-supplied Proxmox results passed |
| Guest files (6) | `guest-file-open`, `guest-file-close`, `guest-file-read`, `guest-file-write`, `guest-file-seek`, `guest-file-flush` | Implemented; native and operator-supplied PVE binary round-trips passed. 16 opaque handles, 4 KiB chunks, bounded canonical base64, read/update/truncate modes; append explicitly Unsupported. Flush is user-space buffering semantics, not stable-storage sync |
| Clock mutation (1) | `guest-set-time` | Implemented on management channels; native system/UTC RTC readback and operator-supplied PVE current-time test passed. Eve permission preflight, 1970–2069 range, explicit partial failures; RTC-derived omitted time tested with mocks, not live hardware |
| Processes (2) | `guest-exec`, `guest-exec-status` | Implemented on management channels: direct native argv execution, literal environment overrides, binary stdin, all capture modes, 8 jobs, 4 KiB/stream, active concurrent draining and one-shot result reaping. Native and daemon-stream tests passed; host-side test pending |
| Storage queries (3) | `guest-get-disks`, `guest-get-fsinfo`, `guest-get-diskstats` | `/dev/sd*` and native filesystem-specific adapters. Namespace mounts are not a single global mount table. No Linux device names/counter meanings may be invented |
| Session/load/CPU queries (3) | `guest-get-users`, `guest-get-load`, `guest-get-cpustats` | No utmp-style session registry or direct 1/5/15-minute load API found. Native process owners are not login sessions. CPU-stat union currently has Linux-only schema; requires an upstream-compatible extension, not falsely labeling 9front as Linux |
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
3. Done: bounded processes/status, native argv/env/stdin/capture/cleanup and
   duplex-daemon tests. Run the new PVE exec test before treating host integration
   as verified. Next: storage/native inventories after schema and source mapping.
4. Native virtio-serial kernel driver in a recovery-capable test VM; no foreign
   runtime or ISA-over-IP substitute. See [VIRTIO](VIRTIO.md).
5. Filesystem/auth/kernel-specific features behind real capability checks. Never
   enable Proxmox backup freeze/trim merely because a handler name exists.

## Host-side security constraints

Our protocol/resource limits protect the legitimate daemon, not Proxmox against
a fully compromised guest which can replace it. The host must bound untrusted
responses, apply timeouts and avoid evaluating guest-returned strings. QGA is
not QMP and no host-control extension is being added. Exec/file/credential APIs
operate only inside the guest; they do not convey host shell authority.

The desktop Drawterm `-r` setting is not a sandbox: its command device, inherited
environment and symlinks require independent OS isolation. Development sessions
must hide the developer's home/SSH keys and clear inherited credentials. The
current diagnostic sandbox is filesystem/PID isolation, not complete network
isolation. Do not infer that it safely confines every exported Drawterm service.
