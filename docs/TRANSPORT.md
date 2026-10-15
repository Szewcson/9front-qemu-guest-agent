# Transport investigation

Research date: 2026-10-10. This records upstream source inspection, not evidence
that a particular running VM's serial channel works. The supplied host command,
native UART setup and later operator-supplied successful ISA QGA CLI output are
recorded in STATUS. That success is distinct from the source investigation.

## Stock 9front

The upstream [UART manual](https://github.com/9front/9front/blob/front/sys/man/3/uart)
describes `#t`, normally bound into `/dev`, with data/control/status triples such
as `eiaN`, `eiaNctl`, and `eiaNstatus`. Data reads block until bytes are available.

The [PC i8250 driver](https://github.com/9front/9front/blob/front/sys/src/9/pc/uarti8250.c)
defines COM1 at I/O `0x3f8`, IRQ 4 and COM2 at `0x2f8`, IRQ 3. These are driver
definitions, not proof of QGA wiring. The
[PC64 kernel configuration](https://github.com/9front/9front/blob/front/sys/src/9/pc64/pc64)
includes `uart`, `uarti8250`, `uartisa`, and `uartpci`.

[devuart](https://github.com/9front/9front/blob/front/sys/src/9/port/devuart.c)
collects the physical-driver lists in `uartreset` and assigns `eiaN` names by
enumeration. Console/special ports can already have another consumer. Merely
seeing an `eiaN` file is not a test of the emulated device or its interrupt path.

The additional [ISA driver](https://github.com/9front/9front/blob/front/sys/src/9/pc/uartisa.c)
consults explicit `uart2` and later ISA configuration entries. If QGA is allocated
to COM3/COM4 by other serial devices, stock automatic COM1/COM2 coverage must not
be assumed to cover it. Investigate the existing `uartN=type=isa port=... irq=...`
boot configuration separately, or simplify the disposable VM's serial layout.
Do not alter boot configuration without backups and a recovery console.

Native UART commands used by `qga9` are fixed except the local baud whitelist:

```text
b38400 l8 pn s1 m0 x0 c0 e0 d1 r1
```

They select 8N1, disable CTS and software XON/XOFF flow control, disable hangup
on DCD/DSR, and assert DTR/RTS. `x0` is established by `devuart.c` even though the
manual's command list omits it. This matters because binary QGA framing must not
lose bytes to a terminal or flow-control layer. Opening/configuring a console
UART would interfere with it; the daemon cannot infer the host mapping.

Upstream [QEMU's ISA channel code](https://github.com/qemu/qemu/blob/master/qga/channel-posix.c)
uses 38400 and raw byte streaming. That informed the initial default; it is not
a claim that guest UART baud must match a physical host serial cable. The daemon
accepts local `-b` values 9600, 19200, 38400, 57600, or 115200.

## Proxmox and QEMU

The current official
[Proxmox QemuServer source](https://git.proxmox.com/?p=qemu-server.git;a=blob;f=src/PVE/QemuServer.pm;hb=HEAD)
adds ordinary `serialN` devices before the agent device. For `type=isa` it creates
a host QGA socket chardev named `qga0` and an `isa-serial,chardev=qga0` device.
Virtio mode creates a `virtio-serial` controller and a `virtserialport` named
`org.qemu.guest_agent.0` instead. Do not add a second competing QGA channel through
custom `args`.

[QEMU's ISA serial allocator](https://github.com/qemu/qemu/blob/master/hw/char/serial-isa.c)
can choose a default index, and default I/O bases `0x3f8`, `0x2f8`, `0x3e8`, and
`0x2e8`. Explicit indices, existing serial devices, machine types and versions
can change the result. Therefore `serial0` plus QGA often suggests COM2, but that
is an inference to verify, not a hardcoded rule or a guarantee of `eia1`.

## Reproducible compatibility gate (partially executed; see STATUS)

1. Record `pveversion -v`, `qm config VMID`, and `qm showcmd VMID --pretty` on the
   host. Match `qga0` to **its** `isa-serial` device, not the console's chardev.
2. Record actual serial I/O/IRQ/index information from the running QEMU machine
   where available; compare with device ordering and the guest kernel sources.
3. On the guest, inspect `#t`, bind it into `/dev` only if needed, list the triples,
   and record their status files and console boot configuration. Check existing
   consumers and relevant `/dev/kmesg` diagnostics.
4. Restrict data/control permissions as appropriate and start one foreground
   `qga9 -d VERIFIED_DEVICE`. Do not probe by running an agent on every UART.
5. Run `qm agent VMID ping` and `qm agent VMID info`. QGA client synchronization
   must be handled through the real QGA socket. Record output, guest stderr,
   port/status mapping, and reconnect results.
6. If it fails, preserve the logs in `docs/STATUS.md` before deciding whether the
   blocker is configuration, UART discovery, interrupts, permissions, framing,
   or missing transport support. Failure does not by itself justify a driver.

ISA success was demonstrated by operator-supplied host output. The caller also
requested real virtio, now maintained in the separate
[driver repository](https://github.com/Szewcson/9front-virtio-serial/blob/main/README.md).
The native #Y stream path, repeated PVE backend reconnects, binary file/exec
smoke tests and automatic cold startup have passed on the authorized VM.
The QGA daemon remains transport-neutral and independent of kernel sources.
Default INTx remains masked with bounded polling. A separate local checked-IRQ
test kernel passed actual emulated IRQ delivery, forced reset, malformed-ring
and named-port recovery tests; broader live fault/platform validation is pending.
See [VIRTIO](VIRTIO.md) and the
[driver ledger](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/STATUS.md).
