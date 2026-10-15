# Codec reuse and native JSON evaluation

## Base64: native libc conversion

Starting with `qga9-0.5.1-dev`, the native daemon calls stock 9front `enc64` and
`dec64`. `src/base64.c` no longer implements byte packing/unpacking. It retains
only protocol validation, output bounds and JSON-string framing.

The deployed VM's `/sys/src/libc/port/u64.c` and `encode(2)` were inspected.
The encoder emits RFC 4648 padding and a terminating NUL. The implementation's
decoder skips nonalphabet characters, including padding, and can return a short
result when capacity is exhausted. The encoder can also return a short result;
the inspected manual's failure description is not sufficient for these cases.
Both wrapper paths therefore check the exact expected result length.

Before calling `dec64`, the wrapper checks the complete input: length divisible
by four, standard alphabet, final-only padding, zero unused pad bits and enough
destination capacity. Invalid input or inadequate capacity leaves the destination
untouched. Empty decoding needs no library call. The expected byte count is
computed without signed overflow. Input and output are borrowed and nonoverlapping.

Encoding checks the entire available JSON output capacity before writing, then
calls `enc64` directly into the caller-owned response buffer. The closing quote's
slot temporarily holds libc's terminating NUL and is then overwritten with that
quote. No codec allocation, temporary conversion buffer or shared mutable state
is added. The protocol's file/input/capture limits remain
4096 bytes. The generic encoder also rejects sizes beyond the 64 KiB response
budget before doing size arithmetic or reading input.

Linux development builds use `tests/host-base64.c`, a small adapter to existing
libresolv `b64_ntop`/`b64_pton` routines, linked with `-lresolv`. It is not compiled
by native `mk` and is not a guest dependency. Linux sanitizer checks do not
instrument the native libc or prove its behavior; the same fixtures run against
the actual native implementation separately.

`tests/base64-test.c` passed 428614 assertions on both backends: RFC vectors,
all 256 single bytes and all 65536 two-byte combinations, noncanonical pad bits,
every length 0–4096, the generic output boundary, malformed alphabet/padding/NUL,
capacity/overflow errors, destination guards and no writes on rejection.
Counts include parameterized assertions, not that many distinct scenarios.
Existing file and exec tests exercise the same wrapper in protocol handlers.

## JSON: keep the bounded exact-integer parser

The VM's `/sys/include/json.h`, `/sys/src/libjson/json.c`, `printjson.c` and
`json(2)` were inspected. An explicitly invoked development comparison links
stock libjson alongside the project's parser:

```rc
mk qga9-native-json-eval
./qga9-native-json-eval
```

Its 26 native checks passed, confirming these incompatibilities:

| Requirement | Inspected native libjson | Decision |
| --- | --- | --- |
| Exact QGA int64 values and arbitrary numeric transaction IDs | `JSONNumber` stores `double`; adjacent positive/negative values above 2^53, near signed-int64 maximum and at epoch-nanosecond scale compare equal | Preserve integer lexemes and checked int64 conversion |
| Duplicate decoded-key rejection | A duplicate-key object is accepted and retained as two linked-list entries | Keep strict key validation |
| Fixed depth/token/resource limits | Recursive tree allocation has no caller-supplied limits; the bounded comparison accepts 17 nested arrays rejected by qga9 | Keep 8192-byte requests, 512 tokens and depth 16 |
| Correct arbitrary object-key serialization | `printobject` inserts key text using `%s`, without escaping; a valid key containing a quote produces invalid JSON | Keep the checked ASCII/Unicode serializer |

These are integration blockers, not claims of a newly assigned CVE or a complete
security audit of libjson. The comparison uses small bounded fixtures, never
unbounded recursion or a float-to-int64 cast outside its representable range.
Every successful native tree and temporary string is freed.

Casting a `double` back to int64 cannot recover lost bits. Wrapping libjson with
our full parser to recover original number lexemes and enforce the same bounds
would keep both parsers, add allocation/maintenance cost and remove little code.
Therefore libjson is **not linked into the daemon**. A future migration needs an
upstream exact-number/lexeme API, resource limits, duplicate-key and strict Unicode
handling, and a correct bounded serializer, with the existing regression suite
retained. No upstream library files were modified by this evaluation.

## Additional native libc reuse

The JSON decoder retains strict scalar/escape/NUL and capacity checks but now
uses native `runetochar` for validated scalar encoding. Integer output uses
bounded native `snprint`, preserving signed 64-bit values without floating
point. Inventory unsigned formatting also uses the native formatter.
No parser limits or authorization are relaxed. Fixed stack buffers are sized
for four UTF-8 bytes and the complete signed-int64 decimal representation.

`tests/json-codec-test.c` passed 540 host assertions on Unicode boundaries,
malformed scalars, exact capacities/guards, interior NUL, int64 extrema and
nanosecond-scale values. The same fixture is part of native `mk test`, but
this new reuse still needs actual native compilation/execution.
Linux uses libc iconv with a private per-call descriptor, not locale mutation;
this adapter is not installed or a guest dependency. GCC's analyzer models
`iconv_open` allocation but not its `(iconv_t)-1` failure sentinel; only that
sentinel-return warning is locally suppressed in the host adapter. Successful
descriptors are always closed. Sanitizers still exercise the unsuppressed code.

## Scoped safety review

The review covered memory, thread and race safety, parser/overflow/truncation
patterns, and privileged access paths. No unresolved finding was identified in
the changed wrapper: lengths precede reads/writes, result sizes are checked,
buffers stay caller-owned and the synchronous single-owner API is unchanged.
No file/exec/shutdown authorization or transport setting was broadened.
The libjson incompatibilities above are avoided by not integrating it.

Residual limits: this is not a comprehensive audit of either platform library;
Linux sanitizers do not instrument their internals. Actual native guard and
protocol tests supplement those checks. The operator subsequently supplied
successful PVE ping/info, file round-trip and all five exec smoke tests on
`qga9-0.5.1-dev`. This passes that codec host-regression gate on operator
evidence, not an independent developer host session. The run did not opt into
clock mutation and is not a new live clock-setting test.

Upstream source references: [9front base64](https://github.com/9front/9front/blob/front/sys/src/libc/port/u64.c),
[JSON API](https://github.com/9front/9front/blob/front/sys/include/json.h),
[JSON parser](https://github.com/9front/9front/blob/front/sys/src/libjson/json.c),
[JSON formatter](https://github.com/9front/9front/blob/front/sys/src/libjson/printjson.c).
Evidence above refers to the inspected VM sources and actual native tests, not
an assumption that a moving upstream branch exactly matches the VM.

Inspected source fingerprints (`sha1sum`, identity records, not authenticity
proofs), captured on 2026-10-11:

```text
d15268a1cd0d5e0f670ee4e400d9a37f01c47dae  /sys/src/libc/port/u64.c
fceb02d8c32b723a85f24acfb54d5dfe11c1b3e5  /sys/include/json.h
ca9bd51fe4e64db712e1a488ebd14fb397446446  /sys/src/libjson/json.c
17267d9daec98531da7c605a6bccf26cf57a2334  /sys/src/libjson/printjson.c
```
