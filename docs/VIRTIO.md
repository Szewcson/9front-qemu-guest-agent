# Separate virtio-serial driver

Kernel code, boot helpers, native driver checks and Linux driver fixtures moved
to [9front-virtio-serial](https://github.com/Szewcson/9front-virtio-serial/blob/main/README.md).
Its [driver documentation](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/VIRTIO.md),
[development guide](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/DEVELOPMENT.md),
[security review](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/SECURITY.md) and
[evidence ledger](https://github.com/Szewcson/9front-virtio-serial/blob/main/docs/STATUS.md) are maintained there.

Each repository builds/tests independently. No submodule, generated code copy or
shared build output is used. QGA has no kernel headers or driver objects in its
native build.

## QGA side

After separately installing/enabling the driver and verifying the exact named
endpoint, QGA consumes its existing raw stream:

```rc
cat '#Y0/status'
/bin/aux/qga9 -t stream -d '#Y0/data'
```

Do not start a second agent if the boot service already owns the endpoint.
The driver enforces exclusive eve-only data access and fixed permissions.
`scripts/start.rc` and `scripts/service-virtio.example` remain in this repository
because they configure/start QGA, not the kernel. The helper skips chmod only for
exact canonical #Y data paths; ordinary stream/UART restrictions remain.
The local helper now waits up to 30 one-second attempts for asynchronous driver
publication, without opening data, then launches once or reports timeout.
Native ready/delayed/absent fixtures passed locally and on the Proxmox guest.
The updated helper was installed there and launched one detached agent on
2026-10-15. Subsequent operator host queries and normal shutdown/start passed;
see STATUS for their scope and remaining target-specific gates.

The prior verified cold-start deployment used the normal Proxmox virtio QGA
channel. Host socket reconnect preserves the guest's file/job
state and Chan ownership; positive writes wait while the backend is closed.
Actual guest close, removal or terminal controller faults follow driver reset/
drain rules. Load estimates were introduced in the Proxmox-tested `0.6.0-dev`.
The current `0.6.1-dev` source candidate migrates JSON handling to patched native
libjson; its real-Proxmox acceptance remains pending. The driver split itself
does not add protocol handlers.

The driver is QGA-port-focused and PC64/coherent little-endian only. IRQ routing
is not fully verified: default INTx stays masked and the worker polls. A separate
local checked-IRQ kernel passed real emulated IRQ, forced reset, malformed-ring
and named-port recovery tests. A later Proxmox warm boot also passed actual IRQ
delivery and operator host smoke; full platform validation is still open. Stock kernel and
ISA rollback configuration must remain available. Repo splitting does not
reinstall or reboot the guest.
