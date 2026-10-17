# Native storage inventory and unsupported capabilities

Investigation/implementation: 2026-10-11. Standard schemas:
[QGA storage reference](https://www.qemu.org/docs/master/interop/qemu-ga-ref.html#command-guest-get-disks),
[upstream disk/partition implementation](https://github.com/qemu/qemu/blob/master/qga/commands-linux.c).
Native sources inspected in the running VM:
`/sys/man/3/sd`, `/sys/src/9/port/devsd.c`, `/sys/man/8/hjfs`,
`/sys/src/cmd/hjfs/cons.c`, and the serving process's namespace.

## Implemented: guest-get-disks

This handler takes no arguments. It returns genuine nonempty kernel storage
nodes through `#S`, independent of whether a login namespace bound `#S` at `/dev`.
It never binds a device into the namespace or opens partition data/raw files.

- `name` is the real `#S/sdCu/PARTITION` node, not a Linux name or a made-up file.
- The conventional `data` node is `partition: false` only if ctl says it spans
  the entire positive-size unit. Other nodes have `partition: true`.
- Partitions depend on that verified whole-data node. If `data` was deleted or
  narrowed, no whole node or dangling dependency is fabricated. Zero-size nodes
  and units without block media are omitted.
- No PCI/SCSI address, serial number, alias, SMART value, capacity extension or
  counter is invented. Optional unverified fields are omitted.
- Controller/unit names and partition components have a strict ASCII allowlist;
  names containing whitespace, separators, quotes, non-ASCII or traversal are
  unsupported by this decoder and fail the entire query, not silently disappear.
- Bounds: 16 KiB ctl text, 64 units, 64 partitions/unit, 128 returned nodes;
  each directory scan is capped at 256 entries. Components are at most 63 bytes.
  Extents and sector-byte multiplication must fit signed 64-bit file lengths.
  Malformed/duplicate/truncated records, unsupported names, I/O failures and
  response overflow return `GenericError`, discarding partial output.

The backend cross-checks every ctl partition with `devsd` directory metadata:
device type `S`, partition QID kind, unique matching name and exact byte length.
Every partition must appear in both snapshots. It rereads ctl after the scan,
reparses inventory and rejects changed geometry/extents rather than treating
changing IDE IRQ/error diagnostics as partition changes. When ATAPI ctl omits
geometry, the actual whole-data extent and directory byte length must determine
one positive integral sector size, and every partition length must agree.
Empty media do not cause a guessed sector size. Kernel QID kind 6 is verified
against the inspected stock `devsd.c`; a kernel with a different layout needs an
adapter update.
No caller-selected path is accepted and no device control write is performed.

These are bounded observations, not an atomic hotplug/filesystem transaction.
Changes after validation or an ABA change cannot be excluded; removable/broken
devices can still stall a kernel metadata read. Byte/count limits are not an I/O
deadline. A future mutation must not use this inventory as an allocation map.

The live test VM returned one whole-data node and four native partitions. The
separate read-only native test stat-checked all five. The operator subsequently
supplied the same successful inventory through PVE on `0.5.0-dev`; that host
gate is passed on operator evidence, not an independent developer host session.
The local native guest additionally passed 63 read-only checks over six nodes,
including an IDE CD-ROM, and the real virtio query returned that inventory.
Storage hotplug, multi-controller systems, broader removable-media behavior
and fault injection remain unverified. PROXMOX retains the read-only query for
future regression checks.

## Intentionally unsupported: guest-get-fsinfo

Plan 9 mounts belong to a namespace, not a global Linux mount table. `/srv/boot`
and namespace mount lines alone do not identify backing disks, filesystem type
and authoritative usage. Local and remote 9P both look like mounted file servers.
QGA excludes network filesystems, so listing every 9P mount would be misleading.
QGA usage/capacity fields are optional and unsupported hardware addresses may
be an empty array; missing counters alone do not forbid metadata-only reporting.
An adapter would still need verified local server/mount identity.

The inspected local `hjfs` console's `df` command scans the allocation/reference
table under a filesystem write lock and prints on the shared system console.
`/srv/hjfs.cmd` is a write-only command pipe, not a per-request result endpoint.
The agent must not read/consume the shared console, scrape unrelated output or
call that slow scan for every management query. Partition size is not filesystem
usable capacity or used space, and a 9P stat length is not free space.

Adding a local-fileserver status protocol or new allocation accounting is outside
the revised scope. No fileserver patch is planned just to populate QGA capacity
fields. Metadata-only fsinfo was investigated as the final roadmap item, but the
identity investigation below did not establish a safe automatic discovery
adapter. On 2026-10-15 the operator chose to retain `Unsupported` and add no new
configuration. It is therefore no longer an active implementation requirement;
the daemon does not advertise it.

### Native mount provenance experiment (2026-10-15)

The inspected 11952 kernel exposes these separate observations:

- `devproc.c:readns1` prints a mounted channel's saved `srvname` (or saved path),
  not a descriptor, current service-board entry or fileserver implementation.
- `devsrv.c:srvopen` opens the channel currently posted under a name. A service
  entry can be removed and recreated without changing an already mounted tree.
- `devmnt.c:mntdirfix` rewrites mounted stat's type/device identity to `M` and
  the mount device number. This is not an `hjfs`/`cwfs` implementation marker.
- A local pipe alone does not establish a local disk filesystem: it can serve
  ramfs, a multiplexer or a proxy for a remote server. Process text and open-disk
  observations alone do not tie that process to a particular mounted tree.

The explicit `qga9-native-fsinfo-provenance-test` compiled without warnings
using `6c -FTVw` and passed in the isolated NIC-less native guest. It makes a
private namespace and a fresh leased service board, starts stock `ramfs`, mounts
it privately at `/mnt`, and creates a one-byte synthetic marker in RAM. It then
removes only its private service entry and reposts a different pipe under the
same name. The complete namespace text and mounted root identity remain equal,
the opened posted-pipe identity changes, and the original mounted marker remains
readable. No bytes are read from or written to the posted protocol channels by
the probe; normal filesystem calls use the kernel mount client.

The fixture has a 15-second active-phase alarm, 16 KiB bounded namespace snapshots,
single-owner descriptor/allocation cleanup, and no disk-data, console or network
access. It is opt-in, not part of `mk`, `mk test`, installation or startup.
Namespace/board isolation confines mount and posting changes to the experiment;
the running QGA owner/context remained unchanged and wire ping/info still worked.
This is a counterexample to name-based provenance, not a proof that no future
native adapter is possible. Cleanup syscalls are not deadline-bounded; the test
does not exercise every failure/timeout path.

Consequently, rereading namespace text and statting the current `/srv/boot`
cannot authenticate the backing service of an existing mount, even if both
snapshots are stable. Inferring filesystem type from the presence of an `hjfs`
process or `/env/bootargs` would also be a configuration assumption, not verified
mount provenance. Do not consume the live 9P stream to try to discover it.

The operator rejected an explicitly trusted local filesystem mapping and chose
to retain `Unsupported`, with no new configuration. No mapping option, new
driver or fileserver protocol is planned. Returning an unconditional empty list
would conceal this gap rather than implement local filesystem reporting. The
opt-in experiment remains as evidence, not a production discovery mechanism.
This decision does not affect the existing disk query.

## Out of scope: guest-get-diskstats

The standard response requires `major` and `minor`, and the command currently has
Linux-only availability. Plan 9 device type/instance/QID are not Linux device
numbers. The inspected sd ctl interface reports geometry and partitions, not the
required completed-I/O/sector/timing counters. A zero/guessed counter and invented
major/minor pair would hide missing functionality rather than implement it.

This needs real per-unit kernel accounting and an upstream-compatible way to
describe native device identity. Until then it remains explicitly `Unsupported`.
Filesystem freeze/thaw/discard also remain unsupported; none is implied by disk
inventory. Keep Proxmox `freeze-fs=0,fstrim_cloned_disks=0` for either transport.
Virtio SCSI disks do not establish virtio-serial support; the separate driver
provides that channel.
