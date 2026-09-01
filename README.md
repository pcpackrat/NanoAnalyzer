# NanoAnalyzer

Simplified **antenna analyzer** firmware for the **NanoVNA-H4**.

A stripped-down fork of [DiSlord's NanoVNA-D](https://github.com/DiSlord/NanoVNA-D)
that does one job: tell you how well an antenna is matched on a given band —
**SWR, resistance, reactance and |Z|** from an S11 (reflection) sweep. Everything
else in the stock firmware (S21/through, time domain, LC match, SD card, expert
menus) is removed.

## Features

- **Reflection-only S11** — faster sweeps, one-port Short-Open-Load (SOL) calibration.
- **Center frequency + bandwidth** entry model (not start/stop).
- **Band presets** — every US amateur band (160 m – 23 cm), GMRS/FRS, MURS, CB,
  plus a CUSTOM group with wide-scan presets and editable user slots. Each preset
  stores a center frequency and bandwidth; tap to sweep it.
- **One wideband SOL calibration** — calibrate once over a wide span; every band
  interpolates from it.
- **Dedicated SWR-minimum marker** ("S") that auto-tracks the dip, plus four free
  user markers with the usual readout.
- **Three display layouts** — graph, graph + data, big-numbers + mini graph.
- Powers up on the last band used.

## Build

Requires an ARM bare-metal toolchain (`gcc-arm-none-eabi`, `make`).

```sh
export TARGET=F303
make
```

Output: `build/H4.bin`. Released binaries are committed under `bin/`.

## Flash

Put the H4 in DFU (hold the jog switch while powering on), then:

```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D bin/H4.bin
```

or use STM32CubeProgrammer.

## Credit

Based on [NanoVNA-D](https://github.com/DiSlord/NanoVNA-D) by
[@DiSlord](https://github.com/DiSlord/), itself based on the original
[NanoVNA](https://github.com/ttrftech/NanoVNA) by
[@edy555](https://github.com/edy555). Licensed under the GPL.
