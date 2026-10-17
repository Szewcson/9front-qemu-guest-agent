# 9front QEMU Guest Agent

`qga9` is an experimental native Plan 9 C implementation of the QEMU Guest Agent
protocol for 9front amd64 and Proxmox VE. It implements 23 standard commands,
including opt-in shutdown, using native 9front sources, tools and runtime.

This repository owns the userspace daemon, codecs, service startup and QGA tests.
The separate [9front-virtio-serial repository](https://github.com/Szewcson/9front-virtio-serial)
owns the kernel driver. Each repository builds and tests independently.
The daemon uses either a verified ISA UART or an existing duplex raw stream.

## Capabilities and limits

- Synchronization, ping and accurate command-availability metadata.
- Native OS, hostname, UTC time, interfaces, vCPUs, timezone, routes and disks.
- Continuous 1/5/15-minute runnable-process load estimates, when the sampler starts.
- Six guest-file operations: 16 handles, 4096-byte chunks and canonical base64.
- Management-channel system/UTC RTC setting, restricted to 1970–2069.
- Management-channel execution with literal argv, binary stdin, five capture
  modes, eight jobs/results, bounded output and one-shot reaping.
- Optional `-S` shutdown through fixed native `fshalt` argv.
- UART, raw-stream and stdio modes; optional `-D` detachment.
- Bounded JSON, exact integers, UTF-8 validation and binary resynchronization.

Shutdown defaults off; `halt` is unsupported. Stdio exposes file operations but
cannot execute commands, set time or shut down. Append modes, filesystem
freeze/thaw, trim and password/key mutation remain unsupported.
`guest-file-flush` is not a stable-storage or backup-consistency guarantee.
The [feature inventory](docs/FEATURES.md) describes intentionally unsupported
commands. The active roadmap is reliability of existing features, not all 44
cross-platform commands or new guest subsystems. `guest-get-fsinfo` remains
intentionally unsupported; no trusted filesystem-mapping configuration is planned.
Keep backup freeze/trim disabled;
unquiesced live backups have no application-consistency guarantee.

QGA gives the host guest file and execution authority under the agent identity.
It provides no protocol authentication or guest trust attestation. A host can
explicitly run a guest shell through exec. Read [SECURITY](docs/SECURITY.md) and
[PROTOCOL](docs/PROTOCOL.md) before enabling a service.

## Evidence

Independent Linux checks passed 12 C suites and 54 Python methods, ASan/UBSan
with leak checks, and GCC analyzer. Prior native userspace tests and real PVE
queries, binary files, clock setting and execution passed. The prior verified VM
cold-booted the separate virtio kernel and automatically started `0.5.1-dev`;
the operator supplied passing PVE info, file and five exec smoke tests.

The independent daemon and ten portable suites now build/test natively in an
isolated local QEMU guest. Real file-guard, sampler, process and stream tests
passed; disk inventory includes an IDE hard disk and CD-ROM.
Current ThreadSanitizer fixtures failed runtime initialization, so no current
TSan pass is claimed. The separate driver defaults to masked INTx/polling;
its explicit checked-IRQ test kernel passed real emulated IRQ delivery,
forced status reset and named-port removal/re-add. This is not full platform
safety or PCI-controller hot-unplug certification.
Local fixes add a bounded endpoint-readiness wait and pinned-parent exclusive
creation in a private helper. Creation rejects final-parent unions, including
mixed-device unions, rather than trusting their create-branch policy. Native
regressions and wire queries passed locally. On 2026-10-15 the Proxmox guest
passed native builds/regressions and received the updated `0.6.0-dev` daemon,
helper and manuals. One detached owner runs without a Drawterm export. The
operator supplied passing updated PVE ping/info/network, CPU/timezone/routes,
file/exec checks and OS/hostname/time/disk/load queries. A later normal PVE
shutdown/start reached a boot-launched agent answering all 23 commands after
transient early failures. After route restoration, independent owner/isolation
inspection, newer polling/checked warm boots and operator PVE smoke/IP checks
passed. The checked image showed increasing real IRQ deliveries and is now
selected persistently with rollback images/config retained. Cold startup,
automatic-agent/host smoke and supplied-time clock/UTC RTC checks also passed,
including delayed agreement with timesync running. Broader fault/reboot tests
remain separate.
See [STATUS](docs/STATUS.md) for evidence and remaining gates.

## Build and run

This checkout now requires the [patched native libjson](patches/libjson/README.md)
for bounded parsing, exact numbers and JSON formatting. The custom grammar parser
has been replaced with a protocol/storage adapter. The installed Proxmox agent
is still the previous build; no system header or library was replaced.

For a private native build, prepare the pinned sources with the helper below,
transfer that source copy to 9front, then build its isolated archive:

```rc
rc tests/native-libjson.rc /path/to/patched-source /path/to/NEW-libjson-output
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a'
mk 'JSONINCLUDE=/path/to/patched-source/sys/include' 'JSONLIB=/path/to/NEW-libjson-output/libjson.a' test
```

If the patched header and archive have already been installed on native 9front:

```rc
mk
mk test
mk install
```

Installation targets `/$objtype/bin/aux/qga9` and refuses an existing binary.
`mk uninstall` removes that binary; startup configuration is manual.
Explicit `mk install-man` and `mk install-service` install the native manuals
and helper respectively, refusing existing targets. Neither configures a hook.

Verify the dedicated UART mapping before selecting a device:

```rc
/bin/aux/qga9 -d /dev/eiaN
```

Replace `N` with the verified port, separate from a serial console.
For an installed, unowned virtio endpoint:

```rc
/bin/aux/qga9 -t stream -d '#Y0/data'
```

Stdio protocol debugging:

```rc
echo '{"execute":"guest-ping"}' | ./qga9 -s
```

Linux development checks build test harnesses:

```sh
mkdir -p build
sh scripts/prepare-libjson.sh build/libjson-source
make -C tests test
make -C tests sanitize
make -C tests analyze
```

## Documentation

- [qga9(8)](man/8/qga9): native command, startup and security contract.
- [9FRONT](docs/9FRONT.md): native installation, service configuration and removal.
- [PROXMOX](docs/PROXMOX.md): channel configuration and host integration tests.
- [TRANSPORT](docs/TRANSPORT.md), [VIRTIO](docs/VIRTIO.md): transport mapping and driver interface.
- [PROTOCOL](docs/PROTOCOL.md), [FEATURES](docs/FEATURES.md): wire semantics and command inventory.
- [STORAGE](docs/STORAGE.md), [CODECS](docs/CODECS.md): native adapters and reuse evidence.
- [SECURITY](docs/SECURITY.md): trust, ownership and safety limitations.
- [DEVELOPMENT](docs/DEVELOPMENT.md), [STATUS](docs/STATUS.md): tests and verified evidence.
- [PROJECT_BRIEF](docs/PROJECT_BRIEF.md): current scope and acceptance criteria.

Protocol references: [QGA](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html)
and [QMP wire format](https://www.qemu.org/docs/master/interop/qmp-spec.html).
Unaffiliated with QEMU, Proxmox or 9front. MIT licensed.
