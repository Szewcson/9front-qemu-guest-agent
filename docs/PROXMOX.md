# Proxmox VE deployment and test gate

Native installation/startup has been tested on an amd64 test guest. The operator
supplied successful ping/info/OS/network/CPU/timezone/route output through
`0.2.0-dev`, followed by corrected-script file and supplied-time clock success on
`0.3.0-dev`. UI and other remaining checks are separate; see STATUS.

## Configure the native agent option

Record the current VM options before changing anything. Substitute the actual
VMID (do not paste the placeholder literally):

```sh
pveversion -v
qm config <VMID>
qm showcmd <VMID> --pretty
qm set <VMID> --agent enabled=1,type=isa,freeze-fs=0,fstrim_cloned_disks=0
qm pending <VMID>
qm config <VMID> --current 1
qm showcmd <VMID> --pretty
```

Current [qm.conf documentation](https://pve.proxmox.com/pve-docs/qm.conf.5.html)
and the [agent schema](https://git.proxmox.com/?p=qemu-server.git;a=blob;f=src/PVE/QemuServer/Agent.pm;hb=HEAD)
support these options. `freeze-fs-on-backup` is a deprecated alias in the inspected
schema; check the installed version's `qm help set` if it differs.

Do not enable freeze/thaw or automatic trim: this agent does not implement them.
Disabling freeze does **not** make backups application-consistent. Use a planned
guest shutdown or another verified filesystem-consistency strategy for backups
where crash consistency is insufficient.

Changing this hardware channel requires a controlled full VM stop/start
before it is instantiated. That is disruptive; schedule it or use a disposable
VM. An ordinary guest reboot may not recreate the QEMU devices. Preserve previous
`agent`/serial settings so they can be restored later.

If the generated command lacks `qga0`, compare `qm pending VMID`,
`qm config VMID --current 1` and `qm config VMID`. The default config view includes
pending changes; current config does not. Recheck after the controlled stop/start.
`qm showcmd` has no `--current` option and is not proof of the running command.
Do not duplicate the device with custom `args` to work around a stale view.

## Verify the port, do not assume it

The generated command should have a QGA socket chardev identified by `qga0` and
an `isa-serial` device attached to it. A console may instead use `serial0`, which
is a different channel. Verify index/I/O base/IRQ against the running machine and
guest driver configuration; use [TRANSPORT](TRANSPORT.md) and [9FRONT](9FRONT.md).

Proxmox can allocate the agent after other ISA serial devices. A VM with two
existing serial ports may put QGA beyond the stock driver's first two predefined
ports. Keep the first test layout simple, without sacrificing the recovery
console. Do not manually duplicate the agent with custom QEMU arguments.

## First integration gate

After compiling natively and starting `qga9` on the verified guest UART:

```sh
qm agent <VMID> ping
qm agent <VMID> info
qm agent <VMID> get-osinfo
qm agent <VMID> get-host-name
qm agent <VMID> get-time
qm agent <VMID> network-get-interfaces
qm agent <VMID> get-vcpus
qm agent <VMID> get-timezone
```

Record exact stdout/stderr, versions, chosen guest device, control/status output,
and any guest-side log. Repeat after disconnect/reconnect of the host client.
Check the Proxmox UI's agent status and record what the UI actually displays.
A syntactically correct JSON reply in the Linux harness is not UI compatibility.

Native queries should return real OS/architecture, hostname, UTC epoch nanoseconds
and interfaces. Compare addresses with `/net/ipifc/*/status` in the guest. Query
failures return errors; the agent does not substitute invented values.

CPU and timezone queries were added in `qga9-0.2.0-dev`, alongside the standard
`guest-network-get-route` handler. The inspected current
[Proxmox command list](https://github.com/proxmox/qemu-server/blob/master/src/PVE/API2/Qemu/Agent.pm)
contains `get-vcpus` and `get-timezone` but no route CLI/API wrapper; do not treat
`qm agent VMID network-get-route` rejection as a guest-handler failure. Check the
installed command list if your version differs. Routes can be tested directly
with the native stdio diagnostic below; this does not test host integration:

```rc
echo '{"execute":"guest-network-get-route"}' | /bin/aux/qga9 -s
```

For a root-only host integration check, the inspected
[PVE monitor helper](https://github.com/proxmox/qemu-server/blob/master/src/PVE/QemuServer/Monitor.pm)
routes `guest-*` commands to the QGA socket using PVE's client. This read-only
example was also confirmed by operator-supplied host output; substitute the
VMID argument and check your installed helper if its version differs:

```sh
perl -MPVE::QemuServer::Monitor -MJSON -e '
    my $vmid = shift;
    my $r = PVE::QemuServer::Monitor::mon_cmd(
        $vmid, "guest-network-get-route", timeout => 10);
    print JSON::encode_json($r), "\n";
' <VMID>
```

The apparently repeated IPv4 default/subnet routes can represent native
source-specific entries: QGA has no IPv4 source-prefix fields. A zero metric
is the schema-required placeholder because native routes expose no scalar metric.
IPv6 link-local/multicast routes are real, not evidence of an IPv6 default route.

## File and clock batch integration

Copy only `scripts/test-proxmox.pl` onto the PVE host (no guest keys or developer
credentials are required). As root, from a directory containing that script:

```sh
qm agent <VMID> ping
qm agent <VMID> info
perl scripts/test-proxmox.pl <VMID>
```

Expected agent version: `qga9-0.3.0-dev`, 19 advertised commands with shutdown
enabled (18 without). The script uses the installed PVE monitor helper's normal
synchronized QGA client, not a competing raw socket reader. It queries CPU,
timezone and routes, exercises all six file commands on one uniquely named
`/tmp/qga9-host-test-*.bin` fixture, checks binary/EOF round-trip and read-only
reopen, then closes its handles. The synthetic fixture is retained and its path
is printed; it never takes a personal file path. `w+` would truncate a preexisting
file at that generated name; timestamp/microseconds/VMID/PID make accidental
collision unlikely, not an exclusive-create protocol guarantee. Remove only
that exact test artifact manually if desired. No filesystem durability is asserted.

For an explicit clock test, this changes guest system time **and RTC** to the
host's current UTC time. Ensure the host clock is correct and schedule around
time-sensitive guest workloads:

```sh
perl scripts/test-proxmox.pl <VMID> --set-time
```

The same file test runs first; clock readback must be within ten seconds of the
supplied value. This checks the QGA path, not the hardware RTC readback (the
separate native live clock test checks that). No deliberate wrong-date test or
omitted-time RTC rewind is performed. PVE helper versions may differ; check the
installed helper if calls fail. Syntax/local mock tests are not host integration.
A timeout does not cancel an already running guest mutation. Do not blindly
retry file writes/open or clock changes after an ambiguous timeout.

### Host-script handle encoding fix (2026-10-11)

Use the updated script if the initial copy ended with `short or invalid write`.
Its validation stringified the numeric handle before passing it to PVE's JSON
encoder. JSON::XS then encoded `"handle":"1"`, which the guest correctly rejects
instead of coercing a string to an integer. The revised script checks signed
64-bit bounds and explicitly converts the handle back to an integer after
validation. It also detects returned `{error: ...}` results from PVE's QGA client,
not only thrown exceptions, and prints the original structured error safely as
JSON. Failed/short mutations are not automatically retried.

This correction changes only the host test script, not the VM agent. Replace
the PVE-side script copy and rerun the file test. A successful capability list
does not establish that file/clock handlers completed successfully.

Primary references:
[PVE QMPClient return/error handling](https://github.com/proxmox/qemu-server/blob/master/src/PVE/QMPClient.pm),
[JSON::XS scalar encoding](https://metacpan.org/release/MLEHMANN/JSON-XS-4.04/view/XS.pm).

There is no native
virtio-serial driver yet: do not change `type=isa` to `type=virtio` on the working
guest. Follow [VIRTIO](VIRTIO.md) when the driver is ready.

## Native execution tests (0.4)

Transfer `scripts/test-proxmox-exec.pl` to the PVE host using your normal trusted
workflow, then run it there. It uses PVE's synchronized QGA client, not a competing
raw socket reader:

```sh
qm agent <VMID> ping
qm agent <VMID> info
perl ./test-proxmox-exec.pl <VMID>
```

Expected final output includes `"exec":"passed"`. The script runs guest
`/bin/echo`, `/bin/cat` and `/bin/sleep`, checks literal argv, seven synthetic
binary stdin bytes, merged output, ping while running and one-shot result reaping.
It reads/uploads no existing host file, creates no guest file, invokes no shell,
changes no clock and does not shut the VM down. Failed/ambiguous calls may leave
one retained job; inspect/reap with `guest-exec-status`, do not blindly repeat a
mutation. The new host integration test has not yet been run by the developer.

For an individual argv-only probe from PVE:

```sh
perl -MPVE::QemuServer::Monitor -MJSON -e '
    my $r = PVE::QemuServer::Monitor::mon_cmd(
        103, "guest-exec", path => "/bin/echo", arg => ["hello from QGA"],
        "capture-output" => JSON::true, timeout => 10);
    print JSON->new->canonical->pretty->encode($r);
'
```

Replace VMID 103 as needed. Query the **numeric** returned PID until exited:

```sh
perl -MPVE::QemuServer::Monitor -MJSON -e '
    my $r = PVE::QemuServer::Monitor::mon_cmd(
        103, "guest-exec-status", pid => 123, timeout => 10);
    print JSON->new->canonical->pretty->encode($r);
'
```

Replace 123 with the returned PID, not the installed agent's PID. The first final
status consumes its result. Output fields are base64; do not evaluate guest-returned
strings or decode them directly into a trusted terminal. Native exit status is
0 for empty/success and 1 for nonempty/failure, not a Linux signal/exit mapping.

## Optional disruptive shutdown test

`-S` / `qga9_shutdown=yes` explicitly enables powerdown and reboot; `halt` without
poweroff remains unsupported. First complete the read-only checks, save work,
and arrange host access to start the VM again. This test actually shuts it down:

```sh
qm shutdown <VMID> --timeout 120
qm status <VMID>
# Only after confirmed stopped:
qm start <VMID>
# After guest boot completes:
qm agent <VMID> ping
```

Do not use forced stop/reset while the guest filesystem is active. A timeout is
not proof that native flushing/poweroff succeeded: inspect the VM console. Native
`fshalt` (or `fshalt -r` for reboot) runs with fixed argv. Successful QGA shutdown
initiation emits **no success response**; later failures are console-only. Fork
success is not proof of actual poweroff. Verify boot persistence after restarting.

## Troubleshooting

- **Agent not responding:** verify the full VM stop/start happened, ISA mode is
  actually instantiated, `qga0` maps to the selected UART, permissions allow access,
  and only one daemon owns the stream. Check foreground stderr.
- **Missing UART:** inspect `#t` and `/dev` binding, stock kernel configuration,
  COM index/I/O/IRQ assignment, and extra `uartN` configuration requirements. Do
  not infer a virtio driver is required from the Linux default.
- **Console conflict:** verify the QGA and console chardevs are separate. Restore
  console settings and stop the agent before trying another verified mapping.
- **Synchronization failure:** confirm 8-bit raw transport, flow control disabled,
  no competing reader, and that binary `0xFF` reaches the parser unchanged. A valid
  `guest-sync-delimited` response has exactly one leading `0xFF`.
- **Missing/incorrect IP data:** compare `/net/ipifc` status with host UI output.
  Check native query errors, status limits/format, MAC availability and masks.
- **Boot startup failure:** verify binary installation, namespace/device binding,
  permissions and startup-hook order; first test foreground execution.
- **Shutdown/reboot failure:** check explicit opt-in and console diagnostics;
  verify filesystem control channels, privileges, `fshalt` and ACPI/`scram` behavior.
  Do not substitute direct kernel reboot writes or forced poweroff as a fix.

Rollback means stopping the exact agent process, removing the optional startup
line, restoring the prior VM agent configuration, and scheduling a stop/start if
hardware must be removed. Do not use broad process kills or discard other options.
