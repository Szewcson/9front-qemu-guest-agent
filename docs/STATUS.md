# Verification ledger

Development/deployment: 2026-10-10–11. Experimental, not a verified release.

## Executed

- Inspected upstream 9front UART/PC64 sources, boot hooks, `nsec`, libip, `fshalt`,
  rc process groups, QEMU ISA allocation, QGA/QMP schemas, and Proxmox command
  generation. The delivered ISA/user-space batch needs no kernel changes or
  foreign guest runtime; the planned virtio driver is separate kernel work.
- Linux GCC 14.2.0: `make -C tests test` passed 39 core checks, 56 information
  checks, 19005 file assertions, 23 clock checks and 35 protocol Python methods;
  parameterized cases include fragmentation and 2000 random-input/resync probes.
- The revised PVE host test script passed Perl syntax checking and eleven local mock tests
  for flat helper arguments, synthetic binary round-trip, explicit clock opt-in,
  failure cleanup, capability gating, argument rejection, structured QGA errors,
  diagnostic short writes and signed handle boundaries. These mocks use no real
  Proxmox host or guest channel. Python discovery totals 46 methods.
- ASan, UBSan and leak checks passed the same suite. Leak detection required
  execution outside the development ptrace sandbox. GCC `-fanalyzer` passed.
- Stock 9front amd64: native `mk` and `mk test` passed (39 core checks, 57
  information checks, 19005 file assertions and 23 clock checks). File counts
  include byte-by-byte fragmented feeds, not 19005 distinct scenarios. Corrected
  opaque-type linker signatures, include lookup and test-macro portability issues
  found by actual compilation. Final build had no compiler warnings.
- Native stdio queries returned genuine OS/architecture, hostname, epoch
  nanoseconds, Ethernet MAC, IPv4/IPv6 addresses and contiguous prefix lengths.
- Expansion queries returned genuine active CPU IDs, native timezone/UTC offset
  and native IPv4/IPv6 routes. Route output has no invented Linux counters or
  priorities; the required metric is zero because native routes have no metric.
- Native raw-stream entry point passed a duplex-pipe ping test without opening
  a UART control file. Stream mode rejects UART options. An initial smoke-test
  assertion failed because `read` left the frame's CR; stripping CR corrected
  the fixture and the test passed. No agent protocol failure was observed.
- User supplied Proxmox VE 9.2.21 configuration and a fresh generated command
  containing `qga0` and `isa-serial,chardev=qga0`, without other serial devices.
  Earlier output lacked the channel; later output includes it. This is not a
  substitute for checking the running device and host ping.
- The operator subsequently supplied successful `qm agent ... ping`, `info`,
  `get-osinfo` and `network-get-interfaces` output from the ISA channel, then
  `0.2.0-dev` info, CPU and timezone output. The direct PVE monitor-helper route
  query also passed in supplied host output: nine IPv4 and five IPv6 entries,
  including link-local/multicast IPv6, with no IPv6 default route shown.
  Repeated-looking IPv4 entries preserve source-specific native routes whose
  source selectors QGA cannot represent. This is operator evidence, not an
  independent developer host session; it does not verify the new file/clock APIs.
- Native binary installed; machine-specific configuration and CPU boot hook
  created without replacing stock boot scripts. Shutdown explicitly opted in
  for this test deployment. UART data/control restricted to mode 0600.
- Dedicated COM1 configured as 38400/8N1/no flow control. Detached agent observed
  blocking in its read. Boot helper returned without waiting for agent exit;
  corrected rc's implicit background-job wait with native `-D`/RFNOWAIT.
- Closed the deployment Drawterm client and reconnected; the same detached agent
  process remained alive. Login-independent execution is verified, not just assumed.
- Deployed native `0.2.0-dev` and the updated helper. Verified exact backup copies
  before stopping the old daemon; retained `qga9.pre-0.2.0-20261010` beside the
  binary and `qga9-start.pre-0.2.0-20261010` beside the helper. Existing config,
  shutdown opt-in, boot hook and ISA channel were preserved. Installed-binary
  stdio queries and duplex-stream ping passed, and the new detached daemon
  survived closing the isolated Drawterm client and reconnecting.
- Native file smoke test passed all six operations, binary NUL/0xff round-trip,
  seek/EOF and read-only reopen; synthetic checkout fixtures were retained.
  Native `/dev/null` open was rejected by the mounted-file descriptor filter.
- Native live clock test passed a supplied-current-time update of the actual
  system clock and UTC RTC, hardware readback and invalid-range rejection. No
  deliberately shifted date or omitted-time RTC synchronization was live-tested.
  The native test is explicitly invoked, not part of the default `mk test`.
- Final raw-stream test passed ping and management-only clock availability
  metadata without accessing the installed agent's UART. Stdio omits the clock
  setter and shutdown but exposes the six file APIs.
- Deployed native `0.3.0-dev` on 2026-10-11 after verifying the retained rollback
  binary `qga9.pre-0.3.0-20261011` byte-for-byte against the installed `0.2`
  binary. Stopped only the identified old agent, verified the installed new
  binary against the native build, and restarted once using the unchanged
  helper/config/boot hook. Shutdown opt-in and working ISA transport were
  preserved; UART remained 0600. Installed-binary stdio metadata, all six file
  APIs and the separate duplex management-stream test passed.
  Terminated only the identified deployment Drawterm client and confirmed from
  a separate isolated login that the same new daemon remained blocked in its
  UART read. The login-independent daemon check passed for this version too.
- The operator supplied successful `0.3.0-dev` host ping/info (19 commands), CPU,
  timezone and routes, followed by the initial host script's `short or invalid
  write` failure. The guest's synthetic host fixtures were empty. Native testing
  of the same seven-byte binary payload, with intervening delimited sync, passed
  using the installed binary. A string-typed handle was correctly rejected.
- Reproduced the host-script cause with actual JSON::XS 4.04: validating a Perl
  integer in string context changed its encoded wire type to a JSON string.
  Post-validation integer conversion restores numeric encoding. Downloaded XS
  and its two dependencies into an isolated temporary directory, without system
  installation or copying any dependency into the guest. All eleven revised
  script tests passed with actual XS serialization as well as core JSON::PP.
  The test script now preserves returned QGA errors and cannot silently accept
  an errored clock setter just because readback was already near host time.
- On 2026-10-11 the operator supplied successful corrected-script runs both
  with `--set-time` and without it on `qga9-0.3.0-dev`. Both reported
  `files: passed`, verifying all six file operations, binary/EOF round-trip,
  read-only reopen and handle close through PVE's QGA client. The explicit
  clock run also reported `clock: passed` after setting host current time and
  checking guest nanosecond readback. CPU, timezone and routes succeeded again.
  Each run retained its own synthetic guest fixture; none were removed. This
  is operator-supplied host integration evidence, not a developer host session.
  The host clock test does not independently read the hardware RTC; that was
  checked by the separate native test. Omitted-time RTC synchronization remains
  mock-tested only, and no deliberately shifted date was live-tested.

- The 0.4 process batch was implemented/tested on 2026-10-11 after checking the
  current upstream exec/capture/reaping schema and native `rfork`, `exec`, `wait`,
  `#d`, pipe queue-length and process note-group sources in the running VM.
  It adds genuine `guest-exec`/`guest-exec-status`, not a shell-interpolation or
  foreign-runtime substitute. Management metadata is now 20 commands, 21 with
  shutdown enabled; stdio remains 17 without clock/exec/shutdown.
- Linux regressions passed 39 core, 56 information, 19005 file, 23 clock and
  8348 process assertions plus 54 Python methods (35 protocol, 11 file/clock
  script mocks and 8 exec-script mocks). Assertion counts include per-byte
  fragmented input, not that many distinct scenarios. ASan/UBSan/leak checks and
  GCC `-fanalyzer` passed; leak checks again required running outside ptrace.
  Both Perl scripts passed syntax checking. All 19 script methods also passed
  with actual JSON::XS serialization using the existing temporary dependencies.
- Native warning-free `mk`/`mk test` passed the five portable C suites (57 native
  information checks). The separately invoked native exec fixture passed literal
  argv/env, binary stdin, all five capture modes, 512 KiB on each output stream
  without pipe deadlock, truncation metadata, descriptor isolation, bare `/bin`
  lookup, exec failure, native nonempty exit mapping, one-shot reaping, running
  status, descendant-independent completion, normal job cancellation, closed
  stdio and abnormal owner-death cleanup. Native polling assertion counts vary
  with scheduling; the final observed run reported 144 checks. The fixture creates
  no operator-file upload or disk output spool. An initial test used the 8 KiB
  request parser on a larger dual-capture reply; a separate bounded 64 KiB response
  parser corrected that test without raising the daemon's request limit.
- The actual daemon's private duplex-stream exec test passed running status,
  a responsive ping while `/bin/sleep` ran, exit collection and one-shot reaping.
  The original stream/management metadata and six-file-operation binary tests
  also passed again. These streams never opened the installed UART.
- Deployed `qga9-0.4.0-dev` after byte-for-byte verification of the retained
  rollback copy `qga9.pre-0.4.0-20261011` (working 0.3 binary). Stopped only the
  identified old agent and confirmed disappearance before replacing its binary.
  Compared the installed binary against the native build, retained mode 0755,
  started once through the unchanged helper/config/boot hook, and confirmed
  UART data/control remained 0600. Installed-binary stdio metadata, six file
  operations and both duplex-stream tests passed. Closed the exact isolated
  deployment Drawterm process and reconnected independently: the same new
  detached daemon remained blocked in its UART read. No PVE hardware change,
  VM reboot, power action, key copying or SSH-agent forwarding was performed.
- Added a separate `test-proxmox-exec.pl`: capability check, direct literal argv,
  seven synthetic stdin bytes, merged capture, ping during sleep and reaping.
  It reads/uploads no existing host files, creates no guest fixture, does not
  execute a shell, and changes neither clock nor power state. A host transport
  timeout cannot masquerade as successful reaping: that check requires a real
  returned guest GenericError. Its local JSON::PP/XS/error/overflow/capture mocks
  passed; **real PVE-side exec integration remains pending operator testing**.

No passwords, keys, private addresses, or personal service configuration are
included in the public project. Deployment used a read-only project export in
an OS-isolated Drawterm environment, without home-directory or SSH-agent sharing.

## Still unverified

- Real Proxmox exec/status integration for 0.4 (native and local client mocks
  passed, but no PVE host credentials/session are available to the developer).
- Exec startup deadline/resource-exhaustion and supervisor/IPC failure branches
  are not all live-injected. Job cancellation is not containment of a command
  that deliberately changes its note group or ignores termination notes.
- Omitted-time clock synchronization and deliberate clock-offset host tests;
  permission/partial-failure branches are mocked rather than live-injected.
- Proxmox UI agent/IP display (the supplied evidence is CLI output).
- Full-VM restart persistence of the installed boot hook (manual launch tested).
- Actual filesystem flush/powerdown/reboot and asynchronous post-fork failures.
- Multiple-interface/loopback/non-Ethernet native fixtures and failure injection.
- Coverage-guided fuzzing: optional entry point supplied; Clang/libFuzzer not run.
- Native virtio-console/serial PCI/DMA/interrupt/reset behavior: no driver or new
  kernel has been installed. The user-space raw-stream test does not verify it.

The user requested deployment readiness and shutdown opt-in before the original
brief's live-ping gate. Additional features were built/tested accordingly, but
incomplete integration gates remain visible here. The caller then expanded the
scope to all missing QGA commands and real virtio; see FEATURES and VIRTIO.
