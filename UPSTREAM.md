# Upstream

This firmware is a fork of DiSlord's NanoVNA-D.

- Source: https://github.com/DiSlord/NanoVNA-D
- Base commit: `794c04b9aabf7113cefd97616056a249b2112138`
- Base tag: `v1.2.46-55-g794c04b`
- Imported: 2026-08-31

The full pristine tree at that commit is kept locally under `reference/NanoVNA-D/`
(git-ignored) for diffing. To pull later upstream fixes, diff a file against
`reference/NanoVNA-D/<file>` and cherry-pick by hand.

## Target

NanoVNA-H4 only: `export TARGET=F303 && make` → `build/H4.bin`.
