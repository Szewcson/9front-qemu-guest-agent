# Implement a native QEMU Guest Agent for 9front

You are an experienced systems programmer specializing in Plan 9/9front, QEMU, Proxmox VE, serial communication, and secure C programming.

Create a standalone open-source project named `9front-qemu-guest-agent`, providing a minimal but functional implementation of the standard QEMU Guest Agent (QGA) protocol for stock 9front.

## Target environment

- Guest: stock 9front, amd64, running inside Proxmox VE 9.x.
- Hypervisor: QEMU/KVM managed through Proxmox VE.
- Additional software: Agent9, pac9, pi9 and ladybird9 may be installed in the guest.
- Agent9 components must be treated as potentially untrusted.
- The guest agent must NOT depend on Agent9, pac9, Python, Node.js, Go, POSIX compatibility layers or external runtimes.
- Use native Plan 9 C, the stock 9front toolchain and `mk`.
- Keep the project compatible with an otherwise unmodified 9front installation whenever possible.

The implementation must be independently usable by the wider 9front community.

## Phase 1: Investigate transport support

Before implementing the agent, investigate the actual 9front kernel, UART interfaces, and Proxmox QGA device configuration.

Proxmox supports both:
- QGA over virtio-serial (`type=virtio`)
- QGA over ISA serial (`type=isa`)

**Prefer ISA serial for the first working implementation.**

Investigate whether the guest can communicate with the QGA channel through `/dev/eia0`, `/dev/eia1`, or another verified Plan 9 serial device.

Do not assume a particular device path or port number. Account for a separate serial console and potentially conflicting serial devices.

Inspect:
- 9front UART support and device enumeration.
- Relevant device control files and serial configuration.
- Proxmox-generated QEMU command lines.
- QEMU guest-agent transport and synchronization requirements.
- Correct handling of binary sentinel bytes and JSON message framing.

If ISA serial works, implement a native userspace daemon without modifying the kernel.

If it does not work, document the precise blocker and investigate implementing a minimal virtio-console/virtio-serial driver as a separately scoped second phase. Do not write a kernel driver merely because Linux QGA normally uses virtio.

Provide the exact steps used to establish device compatibility.

## Phase 2: Implement the agent

Implement `qga9`, a native 9front daemon communicating over a configurable serial device.

Required QGA operations for the initial release:

1. `guest-sync`
2. `guest-sync-delimited`
3. `guest-ping`
4. `guest-info`
5. `guest-get-osinfo`
6. `guest-get-host-name`
7. `guest-get-time`, if the required time representation can be implemented correctly
8. `guest-network-get-interfaces`, using actual 9front networking information
9. `guest-shutdown`, supporting powerdown, halt and reboot where verified safe

Use the official QEMU Guest Agent protocol documentation as the authoritative specification:
https://www.qemu.org/docs/master/interop/qemu-ga-ref

Use the actual QGA schemas, including argument types, required response fields, network address representation and error formats.

Implement the required `guest-info` structure accurately, including the supported command list and the correct `success-response` metadata. Do not advertise unsupported commands as available.

For `guest-shutdown`, investigate `fshalt`, `fshalt -r`, filesystem synchronization and machine poweroff behavior. Follow QGA's asynchronous shutdown semantics, including no success response for this command. Do not use an unsafe direct process kill or filesystem halt.

If an operation cannot be implemented safely or correctly, return a compliant `Unsupported` response instead of fabricated information.

## Phase 3: Security design

Security is a priority.

The guest may contain untrusted applications, and the Proxmox host must remain protected.

Mandatory requirements:

- No arbitrary command execution functionality.
- No `guest-exec` or `guest-exec-status`.
- No guest filesystem read/write commands.
- No password modification commands.
- No shell commands constructed using incoming untrusted strings.
- No guest filesystem freeze/thaw initially.
- No `guest-fstrim` initially.
- No custom host-control protocol.
- No Proxmox API tokens or host credentials.
- No network listener.
- No automatic package installation or execution of remote scripts.
- No integration with Agent9 or pac9 libraries.

For parsing and I/O:
- Bound the maximum incoming message size.
- Validate JSON structure and field types.
- Handle escaping and 64-bit integer values correctly.
- Handle partial reads and writes.
- Detect malformed or oversized messages.
- Implement correct QGA stream resynchronization after the 0xFF sentinel.
- Avoid memory corruption, buffer overflows and unbounded allocation.
- Avoid busy loops after device disconnection.
- Support clean termination and resource cleanup.

Restrict access to the serial device and agent process to the extent supported by 9front. Document limitations in the Plan 9 security model: code with the same effective user privileges or control of the guest may still interfere with or impersonate the agent.

Never present the guest-agent protocol as an authentication or trust-attestation mechanism.

## Phase 4: Project structure

Create an organized repository containing:

- `README.md` — introduction, features, limitations and support matrix.
- `src/` — native 9front C implementation.
- `mkfile` — native 9front build and install targets.
- `scripts/` — installation, startup and uninstallation scripts where appropriate.
- `tests/` — protocol, malformed-input, serialization and regression tests.
- `docs/PROXMOX.md` — Proxmox VE configuration.
- `docs/9FRONT.md` — native installation instructions.
- `docs/SECURITY.md` — threat model and security design.
- `docs/PROTOCOL.md` — supported QGA commands and compatibility.
- `docs/DEVELOPMENT.md` — development and debugging.
- `LICENSE` — an appropriate permissive open-source license.

Prefer a small implementation with straightforward code over a large compatibility framework.

## Phase 5: Build and installation

Provide:
- A native `mk` build.
- A `mk install` target.
- A clean uninstall procedure.
- An executable installation script if it is genuinely useful.
- Installation paths following 9front conventions.
- Manual foreground execution for debugging.
- Optional startup integration with stock 9front boot scripts.
- Instructions for both terminal and CPU-server configurations.
- Reversible changes; do not overwrite existing boot scripts.

The daemon must support a configurable serial device path.

Document how to discover the correct port rather than assuming `/dev/eia0`.

Do not require `pac9` to install the agent.

If development or automated tests take place on Linux, ensure the delivered binary is still compiled and tested with the native 9front toolchain. Do not label a Linux-only build as a working 9front binary.

## Phase 6: Testing

Create automated protocol tests using a simulated bidirectional byte stream.

Test at least:

- `guest-ping`
- `guest-info`
- `guest-sync`
- `guest-sync-delimited`
- Fragmented JSON messages
- Multiple consecutive commands
- Malformed JSON
- Invalid argument types
- Oversized input
- Truncated messages
- Unknown commands
- Proper error responses
- Output escaping
- 64-bit synchronization identifiers
- Reconnection or channel resynchronization
- Unsupported-command behavior
- Correct asynchronous shutdown behavior

Add fuzz testing for the parser where practical.

Provide a real integration-test procedure for Proxmox using:

`qm agent <VMID> ping`

`qm agent <VMID> info`

`qm agent <VMID> get-osinfo`

`qm agent <VMID> network-get-interfaces`

Verify actual compatibility with the Proxmox UI, not merely the existence of syntactically valid JSON responses.

Do not claim any tests passed unless they were actually executed successfully. If live Proxmox or 9front access is unavailable, clearly separate verified tests from unverified integration steps.

Shutdown testing must only be performed against a disposable VM or after explicit approval.

## Phase 7: Proxmox deployment

Provide instructions for configuring QGA using the native Proxmox VM option:

`qm set <VMID> --agent enabled=1,type=isa,freeze-fs=0,fstrim_cloned_disks=0`

Investigate serial device conflicts and how to inspect the generated QEMU arguments with `qm showcmd`.

Explain why filesystem freeze should remain disabled until actual 9front-specific support exists.

Provide clear troubleshooting for:
- Guest agent not responding
- Missing UART device
- Serial console conflicts
- JSON protocol synchronization failure
- Incorrect network information
- Agent failing to start on boot
- Shutdown or reboot failures

## Phase 8: Future extensions

Document, but do not require, future work:
- Native virtio-serial support
- 9front kernel-driver integration, if necessary
- Better network-interface discovery
- Additional safe read-only QGA commands
- Upstreaming to 9front or packaging for broader distribution

Keep the implementation modular enough to support ISA and virtio transports without rewriting the protocol handler.

## Acceptance criteria

A successful first release must:

1. Compile under stock 9front amd64 using native tools.
2. Open and use an actual QEMU agent communication channel.
3. Respond correctly to QGA synchronization and ping.
4. Be recognized by Proxmox VE as a functioning guest agent.
5. Report accurate 9front OS information.
6. Report actual interface IP addresses, if that feature is enabled.
7. Support safe guest shutdown/reboot if verified.
8. Have no dependencies on Agent9, pac9, or external runtimes.
9. Pass relevant protocol tests.
10. Provide reproducible installation, uninstallation and testing instructions.

If something cannot be completed, document exactly what remains, including the failing test, logs and files involved. Never substitute mock success for an integration result.

## Execution instructions

Start by researching the transport and existing 9front interfaces, then implement the smallest working vertical slice:

ISA serial → JSON parser → guest-sync → guest-ping → Proxmox test.

Only after that works should you implement additional commands.

Create the actual source files, tests, build scripts and documentation. Do not stop after writing an architectural proposal.

Make reasonable engineering decisions independently. Preserve all security requirements and report the final test status accurately.
