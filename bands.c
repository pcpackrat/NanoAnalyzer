/*
 * NanoAnalyzer - band presets (center frequency + bandwidth)
 *
 * Licensed under GPL. Part of the NanoAnalyzer fork of NanoVNA-D.
 */
#include "ch.h"
#include "hal.h"
#include "nanovna.h"
#include <string.h>

// Built-in presets. span == full band allocation (a little wide where useful),
// so the SWR dip is always on screen. Names <= BAND_NAME_LEN-1 chars.
// Grouping used by the BANDS menu:
//   HF        : index 0..9
//   VHF/UHF   : index 10..15
//   SERVICE   : index 16..18
//   H4  CUSTOM: index 19..27 (3 wide-range presets + 6 empty user slots)
//   H   USER  : index 19      (1 empty user slot)
static const band_preset_t default_bands[BANDS_MAX] = {
  // ---- HF amateur ----
  { "160m",       1900000,     200000 },   // 1.800 - 2.000
  { "80m",        3750000,     500000 },   // 3.500 - 4.000
  { "60m",        5368000,     100000 },   // 5 channels ~5.332 - 5.405
  { "40m",        7150000,     300000 },   // 7.000 - 7.300
  { "30m",       10125000,      50000 },   // 10.100 - 10.150
  { "20m",       14175000,     350000 },   // 14.000 - 14.350
  { "17m",       18118000,     100000 },   // 18.068 - 18.168
  { "15m",       21225000,     450000 },   // 21.000 - 21.450
  { "12m",       24940000,     100000 },   // 24.890 - 24.990
  { "10m",       28850000,    1700000 },   // 28.000 - 29.700
  // ---- VHF / UHF amateur ----
  { "6m",        52000000,    4000000 },   // 50.000 - 54.000
  { "2m",       146000000,    4000000 },   // 144.000 - 148.000
  { "1.25m",    223500000,    3000000 },   // 222.000 - 225.000
  { "70cm",     435000000,   30000000 },   // 420.000 - 450.000
  { "33cm",     915000000,   26000000 },   // 902 - 928 (H4 accuracy poor this high)
  { "23cm",    1270000000,   60000000 },   // 1240 - 1300 (near H4 upper limit)
  // ---- Licensed / unlicensed services ----
  { "GMRS/FRS", 465137500,    5175000 },   // 462.5500 - 467.7250
  { "MURS",     153210000,    2900000 },   // 151.820 - 154.600
  { "CB",        27185000,     450000 },   // 26.965 - 27.405 (edit wider for freeband)
#if defined(NANOVNA_F303)
  // ---- CUSTOM: wide-range scans, then empty user slots ----
  { "1.7-54 M",   27850000,   52300000 },   // HF through 6 m
  { "118-160 M", 139000000,   42000000 },   // air band through 2 m
  { "420-470 M", 445000000,   50000000 },   // 70 cm through GMRS
  // indices 22..27 left empty for user presets
#else
  // index 19 left empty for a user preset
#endif
};

band_preset_t bands[BANDS_MAX];

void bands_reset_defaults(void) {
  memcpy(bands, default_bands, sizeof(bands));
  bands_save();
}

void bands_init(void) {
  if (bands_recall() != 0)
    memcpy(bands, default_bands, sizeof(bands));
}

void band_apply(int idx) {
  if ((unsigned)idx >= BANDS_MAX) return;
  if (bands[idx].center == 0) return;   // empty slot
  set_sweep_frequency(ST_CENTER, bands[idx].center);
  set_sweep_frequency(ST_SPAN,   bands[idx].span);
  state_save();                         // remember this band on next power-up
}
