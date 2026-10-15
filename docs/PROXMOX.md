# Proxmox VE deployment and integration tests

Use one normal PVE QGA channel. UART works with stock 9front; virtio needs the
separate [driver](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/INSTALL.md)
and an enabled native stream endpoint. Both use the same daemon and host scripts.
The last verified test VM used virtio and automatic cold startup;
[STATUS](STATUS.md) distinguishes prior evidence from unrerun native split checks.

## Configure the channel

On the PVE host, set the actual VMID once; 123 below is an example:

```sh
VMID=123
pveversion -v
qm config "$VMID"
qm pending "$VMID"
qm config "$VMID" --current 1
qm showcmd "$VMID" --pretty
```

Record the current agent/serial options and prepare guest service/kernel rollback.
For a verified ISA UART mapping:

```sh
qm set "$VMID" --agent enabled=1,type=isa,freeze-fs=0,fstrim_cloned_disks=0
```

For an installed/tested virtio driver and stream configuration:

```sh
qm set "$VMID" --agent enabled=1,type=virtio,freeze-fs=0,fstrim_cloned_disks=0
```

Change hardware only with a planned full VM Stop/Start and console recovery.
An ordinary guest reboot may not recreate QEMU devices.
The default config includes pending values; `--current 1` does not.
`qm showcmd` shows generated arguments, not proof of the running command.
Do not add duplicate devices through custom QEMU `args`.

The inspected [qm.conf schema](https://pve.proxmox.com/pve-docs/qm.conf.5.html)
supports these options; check the installed `qm help set` if it differs.
Freeze/thaw and trim remain unsupported. Keep both flags off; this does not make
backups application-consistent.

ISA must connect `qga0` to its own `isa-serial`, separate from console
`serialN` channels. Extra serial devices can allocate QGA beyond stock COM1/2.
Verify index/I/O/IRQ and guest UART enumeration with
[TRANSPORT](TRANSPORT.md) and [9FRONT](9FRONT.md).
Virtio must provide the exact `org.qemu.guest_agent.0` nonconsole port.

## Read-only integration

After starting one native daemon on the verified channel:

```sh
qm agent "$VMID" ping
qm agent "$VMID" info
qm agent "$VMID" get-osinfo
qm agent "$VMID" get-host-name
qm agent "$VMID" get-time
qm agent "$VMID" network-get-interfaces
qm agent "$VMID" get-vcpus
qm agent "$VMID" get-timezone
```

Current source reports `qga9-0.6.0-dev`: 22 advertised management commands,
23 with `-S`, when the load sampler starts. Sampler startup failure reduces
each count by one. The last verified deployed version remains `0.5.1-dev`;
the new version has not passed native or PVE integration tests.
Save exact output, guest device/status/logs and versions. Repeat host connections,
then inspect PVE UI status/IP display separately. Compare reported native values
with guest OS, time, `/net/ipifc/*/status`, CPU and route sources.
Harness JSON alone does not establish UI compatibility.

The inspected PVE CLI has no route wrapper. Use its synchronized monitor helper
rather than a competing raw socket client:

```sh
perl -MPVE::QemuServer::Monitor -MJSON -e '
    my $vmid = shift;
    my $r = PVE::QemuServer::Monitor::mon_cmd(
        $vmid, "guest-network-get-route", timeout => 10);
    print JSON::encode_json($r), "\n";
' "$VMID"
```

For disk inventory, the same helper accepts `guest-get-disks`:

```sh
perl -MPVE::QemuServer::Monitor -MJSON -e '
    my $vmid = shift;
    my $r = PVE::QemuServer::Monitor::mon_cmd(
        $vmid, "guest-get-disks", timeout => 10);
    print JSON::encode_json($r), "\n";
' "$VMID"
```

These inspect guest metadata without opening disk data. Check errors rather than
accepting an empty inventory. Native source-specific IPv4 routes may look
duplicated because QGA cannot represent their source selectors; metric zero is
the required placeholder for a native table without scalar metrics.

## Synthetic file and exec tests

Copy `scripts/test-proxmox.pl` and `scripts/test-proxmox-exec.pl` to one
directory on PVE. From that directory as root:

```sh
perl ./test-proxmox.pl "$VMID"
perl ./test-proxmox-exec.pl "$VMID"
```

The first queries CPU/timezone/routes and exercises all six file operations with
seven synthetic binary bytes in a generated `/tmp/qga9-host-test-*.bin` file.
It checks binary/EOF round-trip and read-only reopen, then closes handles.
The fixture path is printed and retained. A name collision could truncate an
existing file; the generated name is unlikely to collide, not exclusive-create
proof. Remove only the exact synthetic artifact if desired.

The exec script uses guest `/bin/echo`, `/bin/cat` and `/bin/sleep`.
It verifies literal argv, binary stdin, merged output, ping while running and
one-shot result reaping. It does not read existing host files or invoke a shell.
Expected final output includes `"exec":"passed"`.

Use the current scripts: handles/PIDs must stay numeric through PVE JSON
encoding, including JSON::XS. Returned error objects fail the tests; results are
printed as escaped metadata. Never evaluate guest-returned strings or decode
them as raw trusted terminal output.

Timeouts do not cancel launched mutations. A failed file test may leave a handle
or fixture; a failed exec call may leave a job/result. Inspect/reap exact owned
state instead of blindly repeating requests.

## Explicit clock test

Only when system/RTC mutation is acceptable and the host clock is correct:

```sh
perl ./test-proxmox.pl "$VMID" --set-time
```

This runs the file test first, then writes host current UTC to guest system time
and RTC and checks nanosecond readback within ten seconds.
It does not directly verify hardware RTC readback, deliberately shift the date
or test omitted-time RTC synchronization. Native hardware readback is a separate
test. A timeout can follow a completed update.

## Virtio reconnect and startup

PVE closes its backend socket per RPC. The driver preserves the live guest Chan,
owner, RX and file/job state; positive writes wait for reconnect.
A host close does not cancel/replay an executed command.
Actual guest close/removal/fault requires drained driver reset and a fresh epoch.

Fixed #Y permissions are eve-only and immutable. Configure the QGA helper with
`qga9_transport=stream` and the verified canonical data path.
The local helper now makes up to 30 one-second readiness waits, then launches
once or reports timeout. It does not claim data to probe. Native regression
and deployment remain pending; earlier cold-boot evidence used the old helper.

INTx stays masked with 100 ms polling; checked IRQ and deliberate hardware
fault/reset/unplug tests remain incomplete. Preserve stock kernel/config and ISA
service rollback. See the driver's
[STATUS](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/STATUS.md).

## Optional disruptive power test

With explicit `-S` or `qga9_shutdown=yes`, on an approved/disposable guest,
save work and arrange host recovery before testing:

```sh
qm shutdown "$VMID" --timeout 120
qm status "$VMID"
# Only after confirmed stopped:
qm start "$VMID"
# After boot:
qm agent "$VMID" ping
```

QGA initiation has no success reply. Fixed native `fshalt` argv attempts storage
halt before powerdown; fork success/timeout is not proof of flush or poweroff.
Inspect the VM console and host state. Do not substitute forced stop/reset while
the filesystem is active. Actual QGA power behavior remains unverified.

## Troubleshooting and rollback

Check active hardware versus pending config, exact channel mapping, endpoint
readiness, permissions and one owner before retrying startup.
For UART, inspect `#t`, namespace binding, COM configuration and console conflicts.
For framing, verify raw 8-bit data, no flow control or competing reader, and one
leading 0xff for delimited sync. For IP/UI issues compare native status/errors.
For shutdown inspect opt-in, console, filesystem channels and ACPI/`scram`.

Rollback stops the identified service, restores verified guest kernel/config/
helper and prior PVE agent settings, and uses a controlled full Stop/Start when
hardware changes. Preserve recovery access and unrelated VM options.

References: [PVE agent commands](https://github.com/proxmox/qemu-server/blob/master/src/PVE/API2/Qemu/Agent.pm),
[monitor helper](https://github.com/proxmox/qemu-server/blob/master/src/PVE/QemuServer/Monitor.pm)
and [QGA client](https://github.com/proxmox/qemu-server/blob/master/src/PVE/QMPClient.pm).
