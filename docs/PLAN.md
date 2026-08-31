# NanoAnalyzer — Simplified Antenna Analyzer Firmware (NanoVNA-H4)

## Context

Fork DiSlord's NanoVNA-D firmware into a stripped-down **antenna analyzer** for
the NanoVNA-H4 (build target F303, 480×320 ST7796S). The stock firmware is a
full 2-port VNA with dozens of menus; on the bench we only ever want one answer:
"how well does this antenna match on band X" expressed as **SWR, |Z|, R, X** from
an S11 (reflection) sweep. Everything else — S21/through, time-domain/TDR, LC
match, cable measure, SD card, marker math zoo, expert config — is removed.

UX changes vs stock:
- **Band presets**: one selectable slot per US amateur band (160–10 m, 6 m–70 cm),
  GMRS/FRS, MURS, and CB, grouped into paged menus. Each preset stores a
  **center frequency + bandwidth** and sweeps that range.
- **Center/bandwidth entry** as the primary model (e.g. center 14.250 MHz, BW
  200 kHz → 14.150–14.350). The stock engine already stores start/stop and
  already implements center/span conversion — this is a UI default, not new math.
- **Editable presets**: every preset's center/BW/name is user-editable and saved
  to flash; CB especially (freeband expansion) but all of them.
- **Custom sweep**: direct center-freq + bandwidth keypad entry, no preset.
- **Three display layouts**, user-selectable: (1) SWR graph + marker readout,
  (2) graph + big-number panel, (3) big numbers + mini graph.

Hardware: one NanoVNA-H4 on the bench (Windows). Calibration model: **one
wideband SOL** done once at the coax end, auto-interpolated to every band.

## Build & flash workflow

**Git is the transport.** Repo: `https://github.com/pcpackrat/NanoAnalyzer.git`.
The Debian 13 machine is a **VM** (no NanoVNA attached to it); the H4 is on the
Windows bench.

Loop:
1. Edit firmware in the Windows working tree, commit, `git push`.
2. On the Debian VM: `git pull`, `export TARGET=F303 && make`.
3. VM copies `build/H4.bin` → `bin/H4.bin` in the repo, commits ("build: <sha>"),
   `git push`.
4. Windows: `git pull`, flash `bin/H4.bin` to the H4.

- `bin/` is a committed directory holding the latest `H4.bin` (+ a dated archive
  copy `bin/archive/H4-YYYYMMDD-<sha>.bin` per release). `build/` stays
  git-ignored (raw Makefile output).
- **Debian VM prereqs** (user needs help):
  `sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi make git`.
  Verify by building **stock** `build/H4.bin` before any edits.
- **Debian VM → GitHub push auth** (user may need help): either an SSH deploy key
  (`ssh-keygen -t ed25519`, add public key to the repo's Deploy Keys with write
  access, remote = `git@github.com:pcpackrat/NanoAnalyzer.git`) or an HTTPS fine-
  grained PAT in `git credential store`. SSH deploy key recommended.
- **Windows flashing** (user runs it): NanoVNA-H4 enters DFU by holding the jog
  switch while powering on (shows as "STM32 BOOTLOADER", VID:PID `0483:DF11`).
  Two options:
  - **STM32CubeProgrammer** (recommended): bundles the DFU driver, GUI or CLI:
    `STM32_Programmer_CLI -c port=USB1 -w bin/H4.bin 0x08000000 -v -rst`.
  - **dfu-util for Windows** + Zadig (install WinUSB driver on the STM32
    BOOTLOADER interface):
    `dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D bin/H4.bin`.
  Do not open the device serial console while flashing.
- After each change: build on the VM. If it errors, fix and retry once.

## Repository setup (Phase 0)

- `NanoAnalyzer/` currently has **no `.git`** — it is inside the `C:\Users\mikec`
  home-dir repo. In `NanoAnalyzer/`: `git init`, add remote
  `origin https://github.com/pcpackrat/NanoAnalyzer.git`, first commit, push to
  `main`. (If the GitHub repo already has content, `git pull --rebase` /
  reconcile first.)
- Copy `reference/NanoVNA-D/*` (excluding its `.git/`) to the repo root — this
  becomes the working firmware tree. Keep `reference/NanoVNA-D/` untouched (and
  git-ignored or committed as-is) as the pristine upstream baseline for diffing.
- `.gitignore`: `build/`, `*.o`, `*.elf` (keep `bin/`), `.dep/`, `*.lst`, `*.map`.
- Record upstream provenance in `UPSTREAM.md`: repo `github.com/DiSlord/NanoVNA-D`,
  base commit `794c04b`. Keeps future cherry-picks of DiSlord fixes possible.
- **Save this plan to the project**: copy to `docs/PLAN.md`, commit.
- Keep the project `CLAUDE.md`; add the concrete build/flash commands to it once
  Phase 0 verifies.

## Design

### Frequency / center-bandwidth (reuse, near-zero new code)

Internal model stays `frequency0/frequency1` (u32 Hz). Bandwidth == span.
- `set_sweep_frequency(ST_CENTER, f)` / `(ST_SPAN, f)` (`main.c:1541-1602`)
  already do center↔start/stop. `TD_CENTER_SPAN` bit in `props_mode` selects the
  center/span label + edit fields; set it in `load_default_properties()`.
- STIMULUS menu (`ui.c:2263-2273`): promote **CENTER FREQ** and **BANDWIDTH**
  (relabel SPAN), drop/hide START/STOP/CW/STEP/JOG. Keypad descriptors
  `KM_CENTER`/`KM_SPAN` (`ui.c:3193-3199`) → `input_freq` unchanged; just the
  label string changes.
- Jog wheel on center/span already handled by `lever_frequency()`
  (`ui.c:3538-3559`) when `FREQ_IS_CENTERSPAN()`.

### Band presets (new, small)

New module `bands.c` / `bands.h`:
```c
typedef struct { char name[10]; freq_t center; freq_t span; } band_preset_t;
```
- `static const band_preset_t default_bands[]` — every band with center + span.
  Groups: HF (160,80,60,40,30,20,17,15,12,10 m), VHF/UHF (6,2,1.25 m, 70 cm;
  optional 33/23 cm), GMRS/FRS + MURS, CB. ~22 entries; allocate 32.
- RAM working copy `band_preset_t bands[BANDS_MAX]`, loaded from a **new dedicated
  flash page** at boot, editable, re-saved on edit. `RESET PRESETS` restores
  `default_bands`.
- Flash: add `SAVE_BANDS_ADDR` / `SAVE_BANDS_SIZE 0x800` below the config page in
  `hardware.h:171-205`; shift `SAVE_PROP_CONFIG_ADDR` down by one page (256 KB
  flash, currently only ~0x1C800 used — ample room). New `BANDS_MAGIC`; bump
  nothing else. `data_storage.c`: add `bands_save()` / `bands_recall()` mirroring
  `config_save/recall` (`data_storage.c:48-66`).
- Applying a preset = `set_sweep_frequency(ST_CENTER, c)` then `(ST_SPAN, s)` →
  `update_frequencies()` → wideband cal auto-interpolates (`CALSTAT_INTERPOLATED`,
  `cal_interpolate()` `main.c:1967-2021`). No per-band cal.

### Menu restructure

New root menu (replace `menu_top` `ui.c:2563-2578`):
```
BANDS      → group submenus → per-band items (tap = apply preset & sweep)
CUSTOM     → CENTER FREQ (keypad), BANDWIDTH (keypad)
DISPLAY    → FORMAT (SWR/R/X/|Z|), LAYOUT (1/2/3), SCALE/AUTO SCALE, BRIGHTNESS
CALIBRATE  → OPEN, SHORT, LOAD, DONE  (auto-saves slot 0)
EDIT PRESETS → pick band → CENTER / BANDWIDTH / NAME / RESET PRESETS
▶/❚❚ SWEEP  (pause/resume, existing menu_pause_acb)
```
- Menu tables are brace-literal `menuitem_t[]` (`ui.c:231-236`); each group is its
  own table, linked; if a group exceeds ~12 items chain with
  `{ MT_SUBMENU, 0, S_RARROW " MORE", menu_next }` (existing pattern, `ui.c:2157`).
- Delete tables/callbacks for: MEASURE (`ui.c:2339-2420`, +`menu_measure_list`),
  SD CARD (`ui.c:1988`), TRANSFORM (`ui.c:2177`), DATA SMOOTH, CHANNEL, S21
  formats (`menu_formatS21`/`menu_format4`), marker ops beyond none, RECALL,
  most of EXPERT (`menu_device`/`menu_device1`), RTC/CONNECTION.
- Trim `menu_formatS11` (`ui.c:2148`) to SWR / RESISTANCE / REACTANCE / |Z|.

### Strip via feature flags (`nanovna.h:24-115`)

Disable: `__USE_SD_CARD__`, `__VNA_MEASURE_MODULE__` (+ `__USE_LC_MATCHING__`,
`__S21_MEASURE__`, `__S11_CABLE_MEASURE__`, `__S11_RESONANCE_MEASURE__`),
`__REMOTE_DESKTOP__`, `__USE_SMOOTH__`.
Keep: `__USE_SERIAL_CONSOLE__` (bench scripting/debug), `__USE_RTC__`,
`__USE_BACKUP__`, `__LCD_BRIGHTNESS__`, `__FLIP_DISPLAY__`, `__DIGIT_SEPARATOR__`,
`__USE_GRID_VALUES__`, `__VNA_USE_MATH__`, `__VNA_USE_MATH_TABLES__`.
Remove from Makefile `CSRC` (`Makefile:165`): `measure.c` stays only if a submodule
kept (none) — drop the `#include` in `plot.c:1059-1076`; drop `vna_modules/`.

### Reflection-only

New `#define __REFLECTION_ONLY__`:
- `get_sweep_mask()` (`main.c:1096-1126`): force `ch_mask = SWEEP_CH0_MEASURE`,
  skip the CH1 block in `sweep()` (`main.c:1188-1202`) — also halves sweep time.
- `menu_calop` (`ui.c:2004-2014`): drop THRU / ISOLN steps; `cal_done()` already
  tolerates missing THRU.
- Drop S21-derived trace types `TRC_*ser/*sh/Qs21` and `MS_SHUNT/SERIES` rows.

### Traces & default state

- `def_trace[]` (`main.c:887-892`) → `{ S11 SWR, S11 R, S11 X, S11 |Z| }`, only
  trace 0 enabled by default; SWR/R/X/|Z| callbacks already exist
  (`plot.c:509-544`). Note `resistance()` returns `|R|` (abs) — acceptable, or
  drop the `vna_fabsf` for signed R (small, optional).
- `load_default_properties()` (`main.c:920-954`): default center 14.175 MHz / BW
  350 kHz (20 m), `_mode = TD_CENTER_SPAN`, wide `_cal_frequency0/1` for the
  wideband cal (e.g. 100 kHz–600 MHz).

### Display layouts (largest new-code chunk)

`config._display_mode` (0/1/2), one of `config._reserved[3]` (`nanovna.h:1011`);
`DISPLAY → LAYOUT` cycles it. Render in `plot.c`:
- **Mode 0**: stock cartesian plot + top marker-info row (existing
  `cell_draw_marker_info` `plot.c:1261-1366`).
- **Mode 1**: mode 0 + a big-font panel (SWR / R / X / |Z| at the active marker)
  drawn in the freed measure-text region `STR_MEASURE_X/Y` (`nanovna.h:774-783`)
  via `lcd_drawstring_size(..., 2)` / `numfont16x22` (`lcd.c:948-956`), Ω = `S_OHM`.
- **Mode 2**: shrink plot via `set_area_size()` (`plot.c:1544`) to a thumbnail;
  large readout fills the rest.
- After every completed sweep, move the active marker to the **SWR minimum**
  (reuse `marker_search` MIN logic; call from the post-sweep path in `Thread1`
  `main.c:271` area). Values pulled with `swr/resistance/reactance/mod_z(idx, measured[0][idx])`.

### Calibration UX

- `CALIBRATE` menu → OPEN / SHORT / LOAD / DONE only. `DONE` (`menu_caldone_cb`
  `ui.c:754-759`) → compute terms → `caldata_save(0)` automatically (no slot
  picker). Slot 0 auto-loads at boot (`load_settings()` `main.c:991`).
- Guide the user to calibrate over a wide span: add a `CUSTOM → CAL SPAN` shortcut
  that sets center/BW to the full wideband range before calibrating.
- Per-band `c` (interpolated) indicator already shown by `draw_cal_status()`
  (`plot.c:1637`).

## Critical files

- `nanovna.h` — feature flags, `def_trace`, layout constants, new `__REFLECTION_ONLY__`,
  `_display_mode`.
- `ui.c` — menu tables (delete ~15, add BANDS/CUSTOM/EDIT PRESETS), STIMULUS
  relabel, keypad labels.
- `main.c` — `load_default_properties`, `get_sweep_mask`/`sweep` CH0-only,
  post-sweep marker-to-min, `def_trace`.
- `plot.c` — trim `trace_info_list` usage, three display-mode render paths,
  drop `measure.c` include.
- `data_storage.c` / `hardware.h` — new bands flash page, `bands_save/recall`.
- `bands.c` / `bands.h` — **new** preset table + defaults.
- `Makefile` — drop `measure.c` / `vna_modules`.
- `plot.c` / `si5351.c` — untouched math; verify build with `__USE_DSP__`.

## Phasing (build stays green after every phase)

0. **[DONE]** Repo `git init` + remote `pcpackrat/NanoAnalyzer` + source copy +
   `.gitignore` + `UPSTREAM.md` + `docs/PLAN.md` + `CLAUDE.md` build section.
   Debian VM (`nanovm` = devuser@10.10.10.53): ARM toolchain already present
   (gcc 14.2, newlib 4.5, binutils 2.44); repo at `~/src/NanoAnalyzer`; deploy
   key `nanoanalyzer_deploy` pushes to GitHub. Stock `H4.bin` built (text 90 KB,
   bss 40 KB) and committed to `bin/` (commit `7d5d831`). **Pending: user flashes
   `bin/H4.bin` to confirm the loop end-to-end.**
1. **[DONE]** Strip: disabled SD/measure/remote/smooth flags, `__REFLECTION_ONLY__`
   (CH0-only sweep), `def_trace` → SWR/R/X/|Z|, FORMAT menu → SWR/RESISTANCE/
   REACTANCE/|Z|, dropped FORMAT S21 / CHANNEL / TRANSFORM / E-DELAY / S21 OFFSET
   from DISPLAY+SCALE, MARKER OPERATIONS → ->CENTER/->BANDWIDTH. Version string
   "NanoAnalyzer 0.1a". Verified on H4: single SWR trace, MEASURE/SD CARD gone.
   (commits 4cb07c8, 4b21003, 4f6c573)
2. **[DONE]** Center/bandwidth: STIMULUS = CENTER FREQ + BANDWIDTH + JOG STEP +
   SWEEP POINTS; keypad labels updated; `load_default_properties` defaults to
   20 m (14.000-14.350) in `TD_CENTER_SPAN` mode. CUSTOM = this STIMULUS menu.
3. Band presets: `bands.c`, flash page, BANDS menu tree, apply-preset path.
   Build; confirm each band loads and sweeps with interpolated cal.
4. EDIT PRESETS: edit center/BW/name, save/reset to flash. Confirm CB freeband
   widening persists across power cycle.
5. Display layouts + auto-marker-to-min. Confirm all 3 modes.
6. Cal UX simplification + CAL SPAN shortcut. Full wideband SOL, then walk every
   band preset.
7. Branding: strings, splash, version; final size check; update `CLAUDE.md` build
   line.

## Verification

- Per phase: `TARGET=F303 make` on the Debian VM → zero errors/warnings on
  changed code; check `arm-none-eabi-size build/H4.elf` fits (stock leaves
  headroom; stripping SD + measure frees ~tens of KB). VM commits `bin/H4.bin`.
- User pulls, flashes `bin/H4.bin` to the H4 from Windows, checks on the bench:
  - Phase 1: S11 SWR sweep of a known antenna vs stock firmware — same curve.
  - Phase 3–4: select 20 m, 2 m, GMRS, CB presets; SWR dip lands at the right
    frequency; edited CB span survives a power cycle.
  - Phase 6: one wideband SOL cal, then every preset shows plausible SWR/R/X
    against a known-good and a known-bad antenna, and against a 50 Ω load
    (SWR ≈ 1, R ≈ 50, X ≈ 0).
- Regression: `sweep`, `scan`, `cal`, `frequencies` serial commands still work
  (console kept for scripted checks).

## Open decisions (not blocking Phase 0)

- Include 33 cm / 23 cm amateur bands? (H4 harmonic mode weak > ~300 MHz.)
- Firmware tree at repo root vs `firmware/` subdir.
- Signed R (drop `vna_fabsf` in `resistance()`) — yes/no.
- VM→GitHub auth: SSH deploy key (recommended) vs HTTPS PAT.
