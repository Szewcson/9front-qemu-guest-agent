# Native storage inventory and remaining adapters

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
Every partition must appear in both snapshots. It rereads ctl after the scan and
rejects detected changes. Kernel QID kind 6 is verified against the inspected
stock `devsd.c`; a kernel with a different layout needs an adapter update.
No caller-selected path is accepted and no device control write is performed.

These are bounded observations, not an atomic hotplug/filesystem transaction.
Changes after validation or an ABA change cannot be excluded; removable/broken
devices can still stall a kernel metadata read. Byte/count limits are not an I/O
deadline. A future mutation must not use this inventory as an allocation map.

The live test VM returned one whole-data node and four native partitions. The
separate read-only native test stat-checked all five. The operator subsequently
supplied the same successful inventory through PVE on `0.5.0-dev`; that host
gate is passed on operator evidence, not an independent developer host session.
Hotplug, multi-controller systems, removable media and fault injection remain
unverified. PROXMOX retains the read-only query for future regression checks.

## Remaining: guest-get-fsinfo

Plan 9 mounts belong to a namespace, not a global Linux mount table. `/srv/boot`
and namespace mount lines alone do not identify backing disks, filesystem type
and authoritative usage. Local and remote 9P both look like mounted file servers.
QGA excludes network filesystems, so listing every 9P mount would be misleading.

The inspected local `hjfs` console's `df` command scans the allocation/reference
table under a filesystem write lock and prints on the shared system console.
`/srv/hjfs.cmd` is a write-only command pipe, not a per-request result endpoint.
The agent must not read/consume the shared console, scrape unrelated output or
call that slow scan for every management query. Partition size is not filesystem
usable capacity or used space, and a 9P stat length is not free space.

Next work: a bounded, read-only local-fileserver status endpoint with explicit
server/backing identity, consistent allocation counters and namespace-aware mount
association. Start with `hjfs`; add cwfs/other servers through separate genuine
adapters. Return optional capacity fields only when their semantics are known.
No local fileserver patch or fsinfo handler is deployed in this batch.

## Remaining: guest-get-diskstats

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
