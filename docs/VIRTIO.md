# Native virtio-console/serial investigation

2026-10-10: the running guest uses a working ISA QGA channel. Its native source
tree contains `pc/ethervirtio.c`, `port/ethervirtio10.c`, `pc/sdvirtio.c`,
`port/sdvirtio10.c`, `port/virtio10.h` and `pc/virtio10pc.c`. Its PC64 configuration
includes those network/storage drivers but no virtio-console/serial driver;
PCI enumeration showed virtio network/SCSI devices, not a serial controller.
No new kernel/boot image has been installed and the host channel was not changed.

Sources:
[PC64 configuration](https://github.com/9front/9front/blob/front/sys/src/9/pc64/pc64),
[virtio register helpers](https://github.com/9front/9front/blob/front/sys/src/9/pc/virtio10pc.c),
[modern Ethernet driver](https://github.com/9front/9front/blob/front/sys/src/9/port/ethervirtio10.c),
[VIRTIO 1.3 console specification, section 5.3](https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html).

## What is implemented now

`qga9 -t stream -d PATH` serves an existing native duplex stream without UART
configuration. The default remains `uart`, so old config continues working.
The boot helper understands both. Protocol handlers and shutdown have no UART
dependency. A native pipe smoke test exercises the actual raw-stream entry point.

This is the user-space adapter, **not a virtio driver**. Changing Proxmox's agent
setting now would remove the working UART before the guest can use its replacement.

## Driver shape

Implement a native kernel device, e.g. `devvconsole` plus PC transport glue, with
a single-owner byte-stream data interface and read-only named-port/status
information. Concrete namespace names must be finalized with the implementation;
there is no current `/dev/virtio-ports` path to select. Reuse PCI mapping/barrier
primitives, but do not copy unchecked DMA-ring handling from another driver.

Target modern virtio 1.x PCI capabilities first: vendor `0x1af4`, device type 3;
recognize the modern `0x1043` and transitional `0x1003` IDs with modern capability
registers. Transitional legacy I/O support is a separate compatibility task,
not required to call a modern-capability implementation real virtio.

1. Validate PCI capability BAR/index/offset/length arithmetic and map common,
   notify, ISR and device config regions. Reset; set ACKNOWLEDGE/DRIVER; negotiate
   `VIRTIO_F_VERSION_1` and `VIRTIO_CONSOLE_F_MULTIPORT`; verify FEATURES_OK.
   Do not negotiate unsupported packed-ring/indirect/event-index features.
2. Set up bounded split queues with correct physical/DMA addresses, descriptor
   direction flags, alignment and notify-offset multiplication. Validate queue
   sizes before allocation. Port 0 uses queues 0/1, control RX/TX use 2/3, port N
   greater than zero uses `2*N+2` / `2*N+3`. Memory and port limits are explicit.
3. Post control RX buffers, set DRIVER_OK and send DEVICE_READY. Process PORT_ADD,
   PORT_READY, PORT_NAME, PORT_OPEN and PORT_REMOVE in a serialized worker.
   Select the port by exact UTF-8 name `org.qemu.guest_agent.0`, never by guessing
   port 0 or treating a console port as QGA. Reject oversized IDs/names and
   duplicate/conflicting lifecycle events safely.
4. Expose raw RX/TX with no line discipline, baud settings or flow-control byte
   consumption. RX buffers must be device-writable; TX/control-TX readable.
   Single-reader/open ownership prevents two qga9 processes consuming a channel.
   Report host disconnect/unplug and wake blocked readers/writers.
5. IRQ paths only acknowledge/queue/wake; a worker owns lifecycle and buffer
   recycling. Specify lock ordering and barriers. Check every used ID/length
   against its posted descriptor, and never reuse/free a DMA buffer until reset
   completion/device quiescence. Reset/unplug cancels pending I/O and drains worker
   references; reconnect must not return stale data to a new channel generation.

The control message header is 8 bytes (LE32 port ID, LE16 event, LE16 value).
PORT_NAME is length-delimited UTF-8 without a NUL terminator. Console size and
emergency-write features are unnecessary for a QGA-only driver. Console-port
events still need spec-compliant handling or a documented restricted device
configuration; they must not silently open a QGA stream on the wrong port.

## Migration/test gates

1. Build a separate test kernel while preserving the stock boot entry and recovery
   console. Validate ring/control codecs with hostile-ID/length, wraparound,
   allocation-failure and reset/unplug fixtures before live DMA use.
2. Use a disposable clone/test VM with a virtio-serial channel named
   `org.qemu.guest_agent.0`. Do not attach a second competing QGA channel to the
   production VM's socket. Confirm the actual PCI capability/device layout.
3. Verify guest port discovery, blocking read/write, binary `0xff`, fragmentation,
   saturation/backpressure, repeated host connects/disconnects and full reboot.
4. Run actual host ping/info and every implemented QGA command over virtio. Confirm
   no use of an ISA UART remains and that two readers cannot open the same port.
5. Only after those pass, schedule a controlled full stop/start and switch the
   VM's normal agent option to `enabled=1,type=virtio,freeze-fs=0,fstrim_cloned_disks=0`.
   Point qga9 at the verified named native device with `qga9_transport=stream`.
   Keep recorded ISA configuration and the stock kernel for rollback.

There is currently no Proxmox host session available to this project; the operator
performs host hardware changes and supplies results. Guest-side source inspection
and pipe tests do not establish DMA/interrupt or Proxmox virtio compatibility.
