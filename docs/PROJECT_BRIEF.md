# Current project scope

Provide a community-usable QEMU Guest Agent for 9front, with native Plan 9 C
sources, the stock 9front toolchain, `mk` and native runtime interfaces.
JSON now uses the repository's opt-in patch series for native libjson; private
archive linking avoids a global system-library update. See CODECS for the
source/deployed distinction and exact build requirements.
The active scope is existing native management and focused reliability work,
not the full cross-platform QGA union. `guest-get-fsinfo` stays unsupported;
no operator-declared filesystem-mapping configuration is planned.

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
shutdown. [FEATURES](FEATURES.md) records implemented and intentionally unsupported
commands and the revised reliability roadmap.

Report genuine native data. Commands requiring unavailable filesystem, account,
hotplug or power semantics return `Unsupported`; implementing new subsystems
to support them is outside the active scope.
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
available on management channels; actual power tests require an approved or disposable guest.
Filesystem freeze and automatic trim stay disabled. Their native prerequisite
mechanisms are not planned. Live backups without quiescing do not gain an
application-consistency guarantee; prefer a clean shutdown for that workflow.

## Acceptance criteria

1. Build and test with native 9front amd64 tools; separate host fixture evidence.
2. Use a real QEMU agent channel and pass synchronization/ping/metadata through PVE.
3. Validate each advertised command's schema and native semantics.
4. Record cold startup, reconnect, access denial and rollback behavior.
5. For the existing PC64 driver, preserve DMA reset/drain and pinned-callback
   lifetimes; validate IRQ delivery, pending-I/O cancellation, named-port
   removal/recovery and bounded context exhaustion. General hardware portability
   and new fault-injection infrastructure are outside this milestone.
6. Provide working commands and honest current evidence in [STATUS](STATUS.md).

Current code is experimental. Independent native builds and basic local QEMU
integration passed. The updated userspace daemon/helper also built/tested
natively and were installed on the Proxmox guest on 2026-10-15. Subsequent
operator host queries and normal shutdown/start passed; current evidence and
remaining target/UI/clock gates are in STATUS. Broader platform safety is not
certified.
Startup-readiness and rejected-device-create fixes passed native regressions
both locally and on that guest. The private creator rejects final-parent unions
instead of trusting mixed-device union creation policy. Continuous native load
estimates passed native sampler and real local virtio-query tests.
Native manuals are prepared, as is the driver's upstream-layout export.
Neither implies completed deployment or upstream acceptance. Driver-neutral
compatibility names remain a future migration decision.
