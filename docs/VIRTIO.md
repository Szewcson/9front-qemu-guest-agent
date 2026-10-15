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
Native regression and deployment of this change remain pending.

The last verified deployment used the normal Proxmox virtio QGA channel and
automatic cold startup. Host socket reconnect preserves the guest's file/job
state and Chan ownership; positive writes wait while the backend is closed.
Actual guest close, removal or terminal controller faults follow driver reset/
drain rules. No protocol handlers, command count or agent version changed.

The driver is QGA-port-focused and PC64/coherent little-endian only. IRQ routing
is not fully verified: INTx stays masked and the worker polls. Stock kernel and
ISA rollback configuration must remain available. Repo splitting does not
reinstall or reboot the guest.
