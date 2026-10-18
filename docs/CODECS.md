# Native codec reuse

## Base64: native libc conversion

Starting with `qga9-0.5.1-dev`, the native daemon calls 9front `enc64`/`dec64`.
`src/base64.c` retains protocol validation, capacity checks and JSON framing,
not the byte-packing implementation.

The inspected encoder emits padding and a terminating NUL. The decoder skips
nonalphabet characters, including padding, and both routines can return a short
result. The wrappers check complete canonical syntax, final-only padding,
unused pad bits, destination capacity and the exact conversion result.
Invalid input or inadequate decoding capacity leaves the destination untouched.
Empty decoding needs no library call. Size arithmetic is checked before use.

Encoding writes directly into the caller's response buffer; the closing quote
replaces libc's terminating NUL. No allocation or shared mutable state is added.
The protocol's transfer/capture limit remains 4096 bytes. The generic encoder
also rejects input beyond the response budget before reading it.

Linux uses existing libresolv routines through `tests/host-base64.c`, linked
with `-lresolv`. This shim is not installed or a native guest dependency.
The same fixture passed 428614 parameterized assertions on host and native
implementations: RFC vectors, all one/two-byte inputs, lengths through 4096,
canonical padding, overflow/capacity errors and destination guards. File/exec
tests also exercise the wrapper. Host sanitizers do not instrument libresolv
or native libc internals.

## JSON: patched native libjson

The source checkout now uses the [native patch series](../patches/libjson/README.md).
`src/json.c` is a QGA policy/storage adapter, not a grammar parser. Native libjson
owns JSON lexing, Unicode/escape validation, duplicate decoded-key rejection,
exact number spelling, int64 conversion and string/tree formatting. No second
parser remains as a pre-validator. Native rune primitives are reused by the
patched library. Integer output uses native bounded `snprint`, not `double`.

The deployed Proxmox binary is still the preceding build. This change has not
replaced system headers, archives, manuals, agent binaries or startup hooks.
The library patches have not been submitted or accepted upstream.

Why a library patch was necessary:

| Stock API limitation | Opt-in native extension |
| --- | --- |
| Numbers stored as `double`, rounding large IDs/timestamps | Exact number text and range-checked integer conversion |
| No parse resource limits | Per-call depth, value/key-node and allocation budgets |
| Permissive parsing/duplicate keys | Strict number/UTF-8/escape grammar and decoded-key rejection |
| C-string-only strings/keys | Counted strings and keys, including escaped NUL |
| Unescaped object keys in printer | Shared native escaping and bounded compact/ASCII formatting |

The original `JSON`/`JSONEl` layouts, enum values, positional initializers and
legacy entry points remain available. Only opt-in callers receive the new types.
Legacy `jsonparse` remains permissive, unbounded and double-valued; QGA never
uses it for input. The old comparison fixture is historical, not a current
acceptance test for a library whose escaping behavior is intentionally fixed.

### Ownership and bounds

Requests remain limited to 8192 bytes, 512 value/key tokens and depth 16. Native
parser allocations have a separate 64 KiB cumulative-request budget, excluding
allocator metadata; failure rejects the request. The response/test entry point
accepts 64 KiB and uses a 256 KiB parse-allocation budget. These are not CPU or
wall-clock limits.

The adapter flattens the temporary native tree into document-owned fixed
token/text storage, then frees the native tree before returning. Input may be
reused immediately; no heap or borrowed-input lifetime crosses dispatch.
`Jdoc` contains self-referential borrowed native views: do not copy it or call
`jsonfree` on its embedded nodes. Reparse safely replaces its previous contents.
Memory use per document is larger than the former token-only representation;
it is bounded and avoids retained per-request allocations.

String consumers reject interior NUL rather than truncating paths/arguments.
Transaction IDs retain complete counted keys/strings and arbitrary numeric
spelling, including exponents outside floating-point range. Formatting uses
native `jsonfmt` with an instance-owned bounded Fmt; it needs no global `%J`
registration. Equivalent native short control escapes, such as `\n` instead of
`\u000a`, are valid wire output. CRLF framing and ASCII-only output are unchanged.
Overflow or invalid output is sticky and never transmitted.

### Build and verification

See the patch README for pinned-source preparation, standalone library tests
and private native archive creation. Both QGA test backends compile/link the
patched native parser/formatter. Linux also compiles the actual native `rune.c`,
with small allocation/Fmt/OS shims; it has no handwritten UTF-8 codec. Tests do
not download sources or install a system library automatically.

The adapter passed all 12 host C suites and 54 Python methods, ASan/UBSan/leak
checks and GCC analyzer. Its new fixture checks allocation failure at every
prefix, zero retained native allocations, input-buffer destruction, maximum-size
number storage/output, exact token/capacity boundaries and NUL-key isolation.
Native evidence and rollout gates are recorded in [STATUS](STATUS.md).

## Scoped safety review

The review covered memory/thread/race safety, parser ambiguity, overflow and
privileged access paths. Native allocations are checked and freed on success
and error; copies and formatter writes are capacity-checked. Mutable parser and
formatter state is per call. A protocol/document instance still has one owner;
shared-tree mutation or concurrent use of an instance is not supported.
The JSON migration itself did not broaden file/exec/clock/shutdown authorization
or transport policy. Current management-channel policy is in [SECURITY](SECURITY.md).

This is not complete platform/library certification. The host Fmt shim is not
native libc, legacy parser behavior is not comprehensively hardened, and current
ThreadSanitizer failed runtime initialization rather than establishing race
freedom. Native builds and real native buffer tests supplement host checks.

Pinned source, complete patch interfaces, reproduction commands and test limits
are in [the patch README](../patches/libjson/README.md). Baseline checksums are
identity checks, not signed-source authenticity proofs.
