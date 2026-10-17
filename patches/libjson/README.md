# Native libjson patch series

Not an upstream submission or installed system-library update. The QGA source
checkout now uses these APIs through the policy/storage adapter in `src/json.c`;
the custom grammar parser and string serializer have been removed. The running
Proxmox agent is still the preceding binary. Private builds avoid replacing any
system header/archive or live service.

Base: official 9front commit
[`a411fb63cb27f6f367605b2ddbeaf2161b1dd2c1`](https://github.com/9front/9front/commit/a411fb63cb27f6f367605b2ddbeaf2161b1dd2c1).
`SHA256SUMS` pins the original header, parser, formatter, manual and native rune
source used by the host fixture. Native sources remain upstream inputs, not a
replacement JSON implementation copied into the daemon. Preserve upstream notices.

## Patches

1. `0001-object-cleanup-and-escaping.patch`: attach an allocated list element to
   its owner before parsing its name/value, so normal tree cleanup also releases
   the malformed-object error paths. Escape object keys using the existing
   native string printer; fix the missing indentation count.
2. `0002-bounded-exact-json.patch`: extend the same native lexer/tree parser with
   a bounded entry point, exact number spelling, counted strings/keys and a
   bounded formatter. It does not wrap our custom parser or add a second native
   grammar parser. Old `jsonparse`, enum values, `JSON`/`JSONEl` layouts and
   positional initializers remain available; only opt-in callers receive the
   new value types. The formatter also rejects non-finite legacy doubles and
   malformed string scalars rather than emitting invalid JSON.

The new API is deliberately generic, not hardcoded to QGA:

```c
JSONLimits limits = {16, 512, 65536};
JSON *tree;
vlong value;
char output[1024];

tree = jsonparsebounded(input, inputbytes, &limits);
/* Check tree; inspect members with jsonbyname/jsonbynameN/jsonstring/jsonkey.
 * Check jsonint64(node, &value) before using an integer argument. */
if(tree != nil){
    if(jsonformat(output, sizeof output, tree, JSONCompact|JSONAscii) < 0){
        /* Discard output on any failure. */
    }
    jsonfree(tree);
}
```

Limits count root depth from zero, value/key nodes, and cumulative requested
allocation bytes including scratch and text storage. Allocator overhead, CPU
and wall-clock time are not bounded by that byte budget. Maximum supported
depth is 256. Strict parsing validates number grammar, complete Unicode escapes,
UTF-8 scalar values and duplicate decoded keys. Escaped NUL survives counted
strings and object keys; C-string convenience access rejects truncation aliases.
Exact numeric IDs retain spelling even outside int64 or floating-point range.
`jsonint64` performs range-checked integer conversion without double rounding.
`jsonfmt` exposes the same printer to a caller-initialized Fmt, without a NUL
terminator; the caller supplies the flush policy and bounds. This supports an
exact-capacity protocol buffer without temporary output allocations.

Parser and formatter state is per call. Sharing a tree between concurrent
readers/mutators/freeing code still requires caller-owned lifetime coordination.
The existing global `%J` registration should happen before concurrent printing;
`jsonformat` itself needs no global formatter registration.

## Reproduce on Linux

These are explicit developer operations. Normal build/test does not fetch
sources, patch the system or contact a VM. Use a NEW output directory:

```sh
mkdir -p build
sh scripts/prepare-libjson.sh build/libjson-source
sh tests/libjson-host.sh build/libjson-source test
sh tests/libjson-host.sh build/libjson-source sanitize
sh tests/libjson-host.sh build/libjson-source analyze
```

The preparation helper downloads only pinned official inputs, checks their
digests, checks/applies each patch to that new copy, and refuses an existing
destination. No Git commit or upstream submission is made. Failed copies are
retained for inspection. To use an existing prepared native source checkout,
pass it directly to the host-test helper; no download is needed then.

The Linux fixture compiles the actual patched parser/formatter and original
native `rune.c`, with small allocation/Fmt/OS shims. Native rune decoding is not
reimplemented in a test-only parser. Allocation failures are injected at each
position through the fixture's complete successful allocation sequence, and
live allocations are counted independently of LeakSanitizer.

`analyze` requires GCC. `thread-sanitize` is optional; its latest execution failed
runtime initialization with `unexpected memory mapping`, not a reported code
race or a passing TSan run. LeakSanitizer likewise cannot initialize under some
ptrace sandboxes; the successful leak run here was outside that restriction.
Sanitizers do not instrument the host's native OS allocator/Fmt primitives.

## Native 9front checks

Apply the series, in order, to a separate native source copy after checking
contexts against that revision. Do not apply it blindly to `/sys`. From this
QGA checkout on amd64:

```rc
rc tests/native-libjson.rc /path/to/patched-source /path/to/NEW-output-directory
```

The helper compiles the two native library objects and the same fixture with
native `6c -FTVw`/`6l`, then runs it. It uses native libc/Fmt/rune functions;
it also creates `NEW-output-directory/libjson.a` for private QGA linking. It does
not install headers, archives, manuals, a daemon or a boot hook. Do not
run the ordinary system libjson mkfile as an isolated test: its library target
is the system archive.

## Executed evidence and limits (2026-10-16)

- Host fixture: 12,908 checks passed, including exact int64 endpoints and
  nanosecond-scale values, arbitrary number spelling, NUL-containing values/keys,
  malformed UTF-8/surrogates, duplicate decoded keys, depth/node/byte budgets,
  output guards and terminators, malformed-object cleanup, and allocation failures.
- Four threads each ran 1,000 independent parse/format/free cycles with distinct
  policies. Per-thread allocation tracking ended at zero. This is a stress check,
  not absence-of-races certification; current TSan did not initialize.
- 10,000 deterministic byte-mutation inputs exercised rejection/cleanup and
  stable serialize/reparse for accepted documents. This is not exhaustive fuzzing.
- ASan/UBSan/leak checks and GCC analyzer passed. The sanitizer run includes the
  actual native parser, formatter and rune sources, but not native libc/Fmt internals.
- An initial prototype compiled/tested in `p9lab` without system changes. Its
  follow-up connection then failed with No route to host; final target-side
  retest was not claimed. The final source passed 1,650 checks with native libc
  in the independent NIC-less local 9front QEMU guest, compiled without warnings.
  Captured logs are outside Git in the private lab. Normal filesystem halt and
  guest-shutdown completed afterward; its known post-halt QEMU GLib diagnostic
  recurred with emulator exit status zero.

No new privileged operations, credentials or file/device access are introduced
by these in-memory APIs. The scoped review addressed bounds, integer overflow,
partial ownership cleanup, per-call concurrency state and parser ambiguity.
Legacy `jsonparse` remains an unbounded, double-valued compatibility API; it is
not the API to use for untrusted QGA input. Programmatically supplied trees must
be valid and acyclic with live owned storage. Broad platform/library certification,
asynchronous allocator faults and every malformed legacy-input behavior are not
established by these checks.

## QGA adapter and remaining rollout gate

The source adapter retains the 8 KiB request/512 token/depth-16 policy and
64 KiB response limit. Native temporary trees are copied into fixed document-owned
storage and freed before dispatch, including failure paths. String matching and
integer access use the library APIs; formatting uses its bounded Fmt printer.
No second grammar parser remains. QGA command authorization stays outside the
generic library. All 12 host C suites and 54 Python methods passed against it,
with ASan/UBSan/leak and GCC analyzer checks. See [CODECS](../../docs/CODECS.md)
for ownership, allocation budgets, build dependencies and wire-escape details.
The final native private archive, daemon and ten portable suites passed, as did
private-pipe ping/metadata, binary file operations and native exec/reaping.

Before live rollout, retain the old binary, test the updated agent on the real
Proxmox transport and repeat host smoke checks. Installing
the private statically linked agent does not require replacing the system library.
Do not silently turn this patch series into a global system update.
