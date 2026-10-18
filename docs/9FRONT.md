# Native 9front installation

Native amd64 builds, userspace tests, installation and service startup passed.
The independent build and native suites passed in a separate local QEMU guest
and, on 2026-10-15, in the Proxmox guest before its userspace agent/helper update.
See [STATUS](STATUS.md) for the subsequent operator PVE host/power-cycle results,
current access limits and remaining gates; native fixtures alone are not host
or UI acceptance.

## Transport choice

This repository builds only the userspace agent. UART works on stock 9front;
real virtio requires the separately built/installed
[driver repository](https://github.com/Szewcson/9front-virtio-serial/blob/main/README.md). The last verified
VM cold-boots that kernel and runs QGA on #Y0/data automatically.
Keep stock-kernel/ISA recovery available when enabling virtio.

## Discover the QGA UART

Compare the host's configuration first; see [TRANSPORT](TRANSPORT.md). On 9front:

```rc
ls '#t'
ls -l /dev/eia*
cat /dev/eia*status
cat /dev/kmesg
```

If UART devices are absent from the namespace but available under `#t`, bind only
as needed:

```rc
bind -a '#t' /dev
```

Inspect the active boot configuration for `console=` and `uartN=` entries and
existing consumers. Device numbers reflect guest enumeration, not Linux ttyS
names. `eiaN` existing is not proof the corresponding QEMU device is wired.
Do not write test data or change control settings on a serial console port.

## Build, test, install

Copy or clone the source into a native filesystem using your normal trusted
workflow. This checkout requires the [native libjson patch series](../patches/libjson/README.md),
not the stock double-valued parser. Build its private archive first; no global
header/library replacement is needed. Within the checkout:

```rc
echo $objtype
rc tests/native-libjson.rc /path/to/patched-source /path/to/NEW-libjson-output
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a'
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a' test
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a' install
```

Use the same include/archive variables for all native targets unless the patched
header and archive are installed in their conventional system locations. The
archive is linked statically; the installed binary needs no separate runtime
library file. Keep the previous binary for rollback.

The intended initial target is `amd64`. The root `mkfile` uses the architecture's
stock compiler and linker and installs `/$objtype/bin/aux/qga9`, usually accessible
as `/bin/aux/qga9` in a normal namespace. It will refuse to overwrite an existing
binary. Installation requires permission to write the architecture's bin directory.
`mk install-man` installs [qga9(8)](../man/8/qga9) and its startup alias;
`mk install-service` installs `/rc/bin/qga9-start` mode 0755. Both refuse existing
targets; neither creates configuration or modifies the site's boot hook.

For an update, back up the installed binary and helper and verify the copies
before stopping the exact existing daemon. Install and start once; never run
two consumers on the channel. Rollback restores the byte-verified binary/helper
copies and starts the service once with its preserved configuration.
See [VIRTIO](VIRTIO.md) for kernel and host-channel rollback.

Run the new binary in the foreground after choosing the verified QGA UART:

```rc
/bin/aux/qga9 -d /dev/eiaN
```

Replace `N`; it is not a literal device name. The control path defaults to the
data path with `ctl` appended. A local `-c CONTROL_PATH` overrides it; `-b BAUD`
accepts the documented whitelist. The daemon stays in the foreground by default.
`-D` forks after UART setup with private groups/state/fds and RFNOWAIT, allowing
the launcher to return. Errors go
to stderr, not to the QGA channel. EOF/read/write failure closes resources and
exits; a persistent UART reconnect uses the sentinel to reset parsing state.

For protocol debugging without serial hardware:

```rc
echo '{"execute":"guest-ping"}' | ./qga9 -s
```

This does not test the UART or host integration. Do not bind the debug mode to a
network service. UART status/permissions are not modified by stdio mode.
Stdio mode rejects `-D` and does not register a native shutdown backend.
Clock setting and execution are also disabled in stdio, but guest-file APIs
are enabled: file-debugging requests can read, create and truncate guest files.

## Execution

Management channels enable `guest-exec`/`guest-exec-status`; `-s` does not. Exec
uses native argv and private supervisors, not an rc command string. At most eight
jobs/results and 4096 captured bytes per stream are retained. See PROTOCOL for
environment/input/capture limits, native string exit-status mapping and descendant
behavior. Keep the agent's identity/namespace trusted: execution is not a sandbox.

Explicit native synthetic tests (no existing operator-file reads):

```rc
mk qga9-native-process-test qga9-exec-fixture
./qga9-native-process-test
rc tests/native-exec-stream.rc
```

The fixture tests binary stdin, literal argv/env, all capture modes, large output,
descriptor isolation, exec failure, reaping and normal/abnormal owner cleanup.
The stream test starts a separate daemon on private pipes and checks that ping
remains responsive during `/bin/sleep`. Neither opens the installed UART.
Use `scripts/test-proxmox-exec.pl` on PVE for the separate host integration gate.

## Files and clocks

The native file backend supports read, update and truncate modes with at most
16 handles and 4096 bytes per request. Append modes return `Unsupported`; see
[PROTOCOL](PROTOCOL.md) for path/base64/seek/EOF details. Access uses the agent's
identity and mounted namespace, not a separate file-sharing service. Device
files and directories are rejected after descriptor acquisition. Missing paths
require a readable mounted parent before create, in a private namespace.
An isolated helper pins the parent, rejects final-parent unions using native
MCREATE self-bind validation, and exclusively creates the relative leaf. It no
longer trusts a mixed-device create branch. Mounted fileservers and later
metadata changes are not confined by this check.
See [SECURITY](SECURITY.md). Agent restart invalidates all file handles.
`guest-file-flush` has no user-space buffer to drain and is not stable-storage
sync or a backup freeze operation.

This test deliberately creates one synthetic fixture in the current checkout
and retains it for inspection; it does not read personal files:

```rc
rc tests/native-files.rc
```

On management channels, `guest-set-time` updates system time and the UTC RTC.
Both native clock devices must be writable by eve. Dates outside 1970–2069 UTC
are rejected before mutation. A partial system/RTC failure is reported, not
silently presented as full success; a separate time-sync process may override
the update. No boot-helper flag is needed for this handler.

The following live test is deliberately **not** part of `mk test`. It writes
the current system time back to the system clock and RTC, then verifies readback
and range rejection. Run only when clock mutation is acceptable:

```rc
mk qga9-native-clock-test
./qga9-native-clock-test
```

Omitted-time RTC synchronization and failure branches have deterministic mocks;
the live test above uses supplied current time, not a deliberately shifted date.

## Read-only native disk inventory

`guest-get-disks` reads `#S` control/directory metadata even if `#S` is not bound
to `/dev`. It never opens partition data or raw command files for I/O. The
whole-data node and nonempty partitions are reported with verified dependencies;
unverified hardware fields are omitted. See STORAGE for bounds and restrictions.

```rc
echo '{"execute":"guest-get-disks"}' | ./qga9 -s
mk qga9-native-storage-test
./qga9-native-storage-test
```

The native fixture is read-only and checks the real returned nodes with stat.
It does not read or transfer their contents. This is not fsinfo, diskstats,
filesystem quiescing, stable-storage flushing or discard support.

## Raw-stream transport

UART remains the default, including for older deployment configuration files.
`-t stream -d PATH` opens an existing native duplex byte-stream device without
opening a `PATHctl` file or sending UART baud/flow-control commands. It rejects
`-b`, `-c` and `-s`; `-D` retains its normal meaning. The boot helper accepts
`qga9_transport=uart` (default) or `stream` and restricts only the stream's data
file in the latter case.

This option does **not** create a virtio device or install a kernel driver.
Stock 9front has no supplied driver for this port; build/enable it separately
using [VIRTIO](VIRTIO.md). The test VM now has that driver, while this QGA build
remains independent. The helper skips chmod only for exact canonical #Y data
paths because their fixed eve-only permissions reject wstat. Generic streams
and UARTs still require permission restriction. A native pipe tests the actual
raw-stream entry point without touching an active QGA channel:

```rc
rc tests/native-stream.rc
```

This test uses a private namespace and stops only its own test child.
It tests a duplex byte stream, not PCI discovery, DMA queues, interrupts or
Proxmox virtio integration.

## Management-channel shutdown

Shutdown is available on every management channel, with no separate flag:

```rc
/bin/aux/qga9 -d VERIFIED_DEVICE
```

Default/explicit powerdown uses `/bin/rc /bin/fshalt` and reboot uses
`/bin/rc /bin/fshalt -r`. Only these fixed argv choices are reachable; no request
string becomes shell code. `halt` without poweroff remains unsupported.
Success means the isolated shutdown child was started, not that poweroff already
completed. Local QEMU testing observed filesystem halt, a QMP guest-shutdown
event and emulator exit status 0. Reboot, failure paths and updated Proxmox
acceptance remain unverified; always watch the kernel console. See PROXMOX for
a disruptive test and SECURITY for access limitations.

The former `-S` flag is removed: delete it from custom launch commands. The
startup helper no longer reads `qga9_shutdown`; old entries, including `no`, do
not disable power operations. Enabling the management service grants powerdown
and reboot authority alongside exec and clock control. Stdio remains excluded.

## RTC policy

QGA reads and writes UTC RTC seconds. Ensure other guest time-sync services
agree: native [timesync(8)](https://github.com/9front/9front/blob/front/sys/man/8/timesync)
`-L` instead assumes local-time RTC. The local release image's
`TIMESYNCARGS=-rLa1000000` reproduced a two-hour clock change after successful
QGA synchronization. Inspect the actual service/configuration before changing
it; do not simply disable all synchronization. An independent timesync process
is not stopped or reconfigured by the agent. Test both immediate and delayed
readback after selecting a consistent UTC policy.

## Access control

Default upstream data/control permissions are `0660`. The UART driver allows
permission changes by eve and applies a port's mode to both data and control.
After identifying the port, an administrator can restrict it, for example:

```rc
chmod 600 /dev/eiaN /dev/eiaNctl
```

Verify actual owner/group/permissions with `ls -l` and test the intended launch
identity. Do not assume arbitrary owner changes are supported by this device's
wstat implementation. Do not broaden permissions to get past an error. Permission
changes on device files may need reapplying after reboot. Save the old modes so
you can restore them deliberately. See [SECURITY](SECURITY.md) for limitations.

## Optional startup, CPU server or terminal

No boot file is changed by `mk install`. First validate the mapping and foreground
operation. `scripts/start.rc` is a native configuration-driven helper. Create a
trusted machine-specific config from `scripts/service.example`, selecting the
verified device and granting the host management authority, including shutdown. Install the
helper as `/rc/bin/qga9-start` (mode 0755); keep config owner-only writable/readable.

```rc
rc scripts/start.rc /cfg/$sysname/qga9
```

The helper restricts the dedicated UART's data/control to 0600 and refuses to
continue if chmod fails. It starts with `-D`, a private note group/namespace/env,
stdin/stdout on `/dev/null`, and stderr on kernel `#c/cons`, not a Drawterm console.
It returns after channel setup/fork. It does not supervise/retry the daemon.
UART/generic-stream exclusivity must be arranged operationally; #Y enforces one
owner. Canonical #Y paths get up to 30 one-second readiness waits before one
launch; the helper probes existence without opening data. Missing/disabled
drivers time out with a diagnosis. Check logs and actual agent presence.
Foreground processes can still die with an interactive login; detached mode is
intended for boot/manual service use.
The serving child closes inherited descriptors except 0/1/2 and its management
data/control handles. Shutdown children also close everything except redirected
0/1/2 before executing the fixed command. This does not sanitize the inherited
namespace or turn Drawterm into a filesystem/network sandbox.

For boot use, make a trusted copy of the helper at a suitable local rc path,
back up the existing local hook, and add a **single manual line**:

```rc
/rc/bin/qga9-start /cfg/$sysname/qga9
```

Stock CPU startup provides `/rc/bin/cpurc.local` and
`/cfg/$sysname/cpustart`; terminal startup provides `/rc/bin/termrc.local` and
`/cfg/$sysname/termrc`. Inspect the current versions and hook order on your guest.
Use a machine-specific hook when possible, after the required namespace is ready.
`qga9_enabled=no` disables later launches. There is no separate shutdown setting.
Editing config does not change or stop an already-running
agent: identify/stop that process, then launch once with the changed config.
Do not put it in every graphical login/profile, replace `cpurc`/`termrc`, or start
it separately from both boot and login hooks. No supplied script edits these files.

Startup-source references:
[cpurc](https://github.com/9front/9front/blob/front/rc/bin/cpurc),
[termrc](https://github.com/9front/9front/blob/front/rc/bin/termrc).

## Stop and uninstall

Identify the agent PID and stop **that process** with a normal exit note:

```rc
ps
echo exit > /proc/AGENT_PID/note
```

Substitute the actual PID and check its identity before sending the note. Native
default note termination closes its descriptors via kernel process teardown;
there are no worker threads, pidfiles or external cleanup artifacts. A requested
shutdown child runs separately and is not canceled by stopping the agent. Normal
EOF/error paths also explicitly close/free resources.

Remove only the startup line/helper you added, then from the source checkout:

```rc
mk uninstall
```

This removes only `/$objtype/bin/aux/qga9`; it does not stop a running process,
edit startup files, restore device modes, or alter the Proxmox configuration.
Restore recorded modes/configuration separately. `mk clean` removes only named
local build outputs.
