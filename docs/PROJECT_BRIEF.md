# Current project scope

Provide a community-usable QEMU Guest Agent for 9front, with native Plan 9 C
sources, the stock 9front toolchain, `mk` and native runtime interfaces.
This document describes the active scope, including the requested standard QGA
features and separate virtio driver.

## Project boundary

The userspace repository owns `qga9`, protocol/native adapters, codecs,
service startup and host integration tests. The separate
[9front-virtio-serial repository](https://github.com/Szewcson/9front-virtio-serial)
owns kernel transport and kernel build/boot helpers. Builds and tests must
remain independent; the runtime connection is a native duplex stream.

Native 9front is the build/runtime requirement. Packaging is independent and
must preserve a reproducible native build and independently usable sources.
Linux tools are optional development fixtures and do not constitute a native
guest build.

## Functional requirements

Follow the [standard QGA schema](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html),
including exact integers, command metadata, framing, errors and asynchronous
shutdown. [FEATURES](FEATURES.md) records implemented and remaining commands.

Report genuine native data. Commands requiring unavailable filesystem, account,
hotplug or power semantics return `Unsupported` until those semantics exist.
Do not fabricate Linux-specific IDs, counters, mount tables or success.

Support verified UART mappings and existing raw streams without transport
dependencies in protocol handlers. The driver currently targets PC64 modern
virtio PCI capabilities and the exact nonconsole QGA port; its wider integration
work is recorded in its own documentation.

## Safety and deployment

Keep bounded parsing/output, checked ownership, single-owner userspace state,
private native process groups and literal exec argv. File/exec/time operations
intentionally grant guest authority under the agent identity. QGA is neither
authentication nor guest attestation and provides no host-isolation guarantee.
Preserve the limits in [SECURITY](SECURITY.md) and [PROTOCOL](PROTOCOL.md).

Installation must be explicit, reproducible and reversible. Preserve existing
boot configuration and recovery access. Use one channel consumer. Shutdown is
opt-in, and actual power tests require an approved or disposable guest.
Filesystem freeze and automatic trim stay disabled until implemented.

## Acceptance criteria

1. Build and test with native 9front amd64 tools; separate host fixture evidence.
2. Use a real QEMU agent channel and pass synchronization/ping/metadata through PVE.
3. Validate each advertised command's schema and native semantics.
4. Record cold startup, reconnect, access denial and rollback behavior.
5. For driver changes, preserve DMA reset/drain and pinned-callback lifetimes;
   validate real IRQ, reset/fault/unplug and platform behavior before release.
6. Provide working commands and honest current evidence in [STATUS](STATUS.md).

Current code is experimental. The repository split still needs native build/VM
verification. Startup-readiness and rejected-device-create fixes are local with
native regressions pending. The private creator rejects final-parent unions
instead of trusting mixed-device union creation policy. Continuous native load
estimates are implemented locally but have no passing native run yet.
Native manuals are prepared, as is the driver's upstream-layout export.
Neither implies completed deployment or upstream acceptance. Driver-neutral
compatibility names remain a future migration decision.
