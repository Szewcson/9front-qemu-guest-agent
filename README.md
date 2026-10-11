# 9front QEMU Guest Agent

`qga9` is an experimental, standalone native Plan 9 C implementation of a small
part of the standard QEMU Guest Agent (QGA) protocol for stock 9front. It targets
amd64 guests on Proxmox VE 9.x and starts with ISA serial, not a new kernel driver.
The installed daemon has no Agent9, pac9, pi9, ladybird9, Python, Go, Node.js, or
POSIX-layer dependency.

**Experimental, not a verified production release.** The native daemon builds
and runs on stock 9front amd64; native queries and core tests have passed. It has
been installed and enabled on a test VM. The operator supplied successful host
ping/info/OS/network/CPU/timezone/route output through `0.2.0-dev`, followed by
successful `0.3.0-dev` host file round-trip and supplied-time clock tests using
the corrected host script. Native system/RTC readback also passed. `0.4.0-dev`
exec/status is now installed and enabled; native process and daemon-stream tests
passed, with its new host-side integration test still pending. See
[the ledger](docs/STATUS.md).

## Implemented

- `guest-sync` and `guest-sync-delimited`, including signed 64-bit identifiers.
- `guest-ping` and `guest-info` with accurate command availability metadata.
- OS, hostname, UTC epoch nanoseconds and real IPv4/IPv6 interface information.
- Active vCPU inventory, native timezone/DST offset, and IPv4/IPv6 routes.
- Six guest-file operations, with binary-safe base64, 16 handles and 4 KiB chunks.
- Management-channel clock setting: native system time plus UTC RTC update.
- Management-channel `guest-exec` / `guest-exec-status`: native argv execution,
  binary stdin, five capture modes, bounded output and one-shot result reaping.
- Optional `-S` shutdown: fixed native `fshalt` / `fshalt -r`, no success reply.
- Configurable native serial data/control paths and foreground operation.
- `-t stream` for an already-provided native raw stream, without UART controls.
- Optional `-D` detachment and a machine-specific boot helper.
- Bounded JSON parsing, UTF-8 validation, ASCII response escaping, arbitrary JSON
  transaction IDs, fragmented input, partial writes, and `0xFF` resynchronization.
- Native core tests, optional Linux stream tests, and an optional fuzzing entry
  point. Linux development tools are not guest runtime requirements.

Shutdown is disabled by default; `halt` without poweroff is always unsupported.
The caller expanded the original brief to cover the remaining standard QGA
commands and real virtio-serial. Implementation is incremental; see
[the feature roadmap](docs/FEATURES.md). This does not replace integration testing.
There is no fabricated OS or network information.

File append modes remain unsupported; `guest-file-flush` drains no user-space
buffer and is not an fsync/freeze guarantee. Clock setting requires native eve
permissions and a representable PC RTC date (1970–2069 UTC). Stdio debugging can
read/write guest files, but cannot execute commands, change the clock or initiate shutdown.

There is no password/key modification,
freeze/thaw, trim, network listener, package installation, or host-control API.
Exec intentionally grants the host command execution under the guest agent's
identity, not host-machine shell access. See the limits in [PROTOCOL](docs/PROTOCOL.md).

## Support matrix

| Environment | Current evidence |
| --- | --- |
| Stock 9front amd64 toolchain | Native build, core/information/file/clock checks passed |
| Stock PC UART interfaces | Dedicated UART opens/configures; daemon awaits data |
| Proxmox VE 9.x ISA QGA | Operator supplied successful queries through 0.2 and file/clock host tests on 0.3 |
| Native files and clock | All six file APIs passed natively and through PVE; native system/RTC readback and host supplied-time test passed |
| Native execution (0.4) | Native process/capture/cleanup and duplex-daemon tests passed; new PVE-side exec test supplied, host run pending |
| Linux C core/stream harness | Regression tests and sanitizer checks run; see STATUS |
| Virtio-serial | Driver not implemented; raw-stream adapter and driver design supplied |
| Shutdown/reboot | Explicit `-S` opt-in; actual power actions not independently tested |

## Build on 9front

In a native 9front checkout:

```rc
mk
mk test
mk install
```

The default installation target is `/$objtype/bin/aux/qga9`. `mk install` refuses
to overwrite an existing binary. `mk uninstall` removes only that installed
binary; startup configuration is deliberately manual and reversible.

Discover and verify the UART first. There is **no default device**:

```rc
./qga9 -d /dev/eiaN
```

Replace `N` only after checking the host and guest mapping, and do not use a
serial console port. Foreground stdio mode is also available:

```rc
echo '{"execute":"guest-ping"}' | ./qga9 -s
```

For local Linux development only:

```sh
make -C tests test
make -C tests sanitize
make -C tests analyze
```

These commands build **test harnesses**, not the installed native daemon.
Read [native startup and shutdown options](docs/9FRONT.md) before enabling a
service. No SSH keys or host credentials are needed in the guest.

## Documentation

- [PROXMOX](docs/PROXMOX.md): configuration, port conflicts, live integration gate.
- [9FRONT](docs/9FRONT.md): discovery, native installation, optional boot hooks.
- [TRANSPORT](docs/TRANSPORT.md): researched driver/QEMU evidence and its limits.
- [VIRTIO](docs/VIRTIO.md): native virtio-serial design and migration gates.
- [FEATURES](docs/FEATURES.md): all standard QGA commands and implementation phases.
- [PROTOCOL](docs/PROTOCOL.md): wire format, limits, supported/error behavior.
- [SECURITY](docs/SECURITY.md): threat model, access limitations, review scope.
- [DEVELOPMENT](docs/DEVELOPMENT.md): tests, fuzzing, debug workflow, roadmap.
- [STATUS](docs/STATUS.md): executed checks versus unmet acceptance criteria.
- [PROJECT_BRIEF](docs/PROJECT_BRIEF.md): the original requested scope and release criteria.

The protocol follows the [official QGA reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html)
and [QMP wire format](https://www.qemu.org/docs/master/interop/qmp-spec.html).
This project is not affiliated with QEMU, Proxmox, or 9front. MIT licensed.
