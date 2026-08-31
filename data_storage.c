/*
 * Copyright (c) 2019-2020, Dmitry (DiSlord) dislordlive@gmail.com
 * Based on TAKAHASHI Tomohiro (TTRFTECH) edy555@gmail.com
 * All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include "ch.h"
#include "hal.h"
#include "nanovna.h"
#include <string.h>

uint16_t lastsaveid = 0;
#if SAVEAREA_MAX >= 8
#error "Increase checksum_ok type for save more cache slots"
#endif

// properties CRC check cache (max 8 slots)
static uint8_t checksum_ok = 0;

static uint32_t calibration_slot_area(int id) {
  return SAVE_PROP_CONFIG_ADDR + id * SAVE_PROP_CONFIG_SIZE;
}

static uint32_t checksum(const void *start, size_t len) {
  uint32_t *p = (uint32_t*)start;
  uint32_t value = 0;
  // align by sizeof(uint32_t)
  len = (len + sizeof(uint32_t)-1)/sizeof(uint32_t);
  while (len-- > 0)
    value = __ROR(value, 31) + *p++;
  return value;
}

int config_save(void) {
  // Apply magic word and calculate checksum
  config.magic = CONFIG_MAGIC;
  config.checksum = checksum(&config, sizeof config - sizeof config.checksum);

  // write to flash
  flash_program_half_word_buffer((uint16_t*)SAVE_CONFIG_ADDR, (uint16_t*)&config, sizeof(config_t));
  return 0;
}

int config_recall(void) {
  const config_t *src = (const config_t*)SAVE_CONFIG_ADDR;

  if (src->magic != CONFIG_MAGIC || checksum(src, sizeof *src - sizeof src->checksum) != src->checksum)
    return -1;
  // duplicated saved data onto sram to be able to modify marker/trace
  memcpy(&config, src, sizeof(config_t));
  return 0;
}

int caldata_save(uint32_t id) {
  if (id >= SAVEAREA_MAX)
    return -1;

  // Apply magic word and calculate checksum
  current_props.magic = PROPERTIES_MAGIC;
  current_props.checksum = checksum(&current_props, sizeof current_props - sizeof current_props.checksum);

  // write to flash
  uint16_t *dst = (uint16_t*)calibration_slot_area(id);
  flash_program_half_word_buffer(dst, (uint16_t*)&current_props, sizeof(properties_t));

  lastsaveid = id;
  return 0;
}

const properties_t * get_properties(uint32_t id) {
  if (id >= SAVEAREA_MAX)
    return NULL;
  // point to saved area on the flash memory
  properties_t *src = (properties_t*)calibration_slot_area(id);
  // Check crc cache mask (made it only 1 time)
  if (checksum_ok&(1<<id))
    return src;
  if (src->magic != PROPERTIES_MAGIC || checksum(src, sizeof *src - sizeof src->checksum) != src->checksum)
    return NULL;
  checksum_ok|=1<<id;
  return src;
}

int caldata_recall(uint32_t id) {
  lastsaveid = NO_SAVE_SLOT;
  if (id == NO_SAVE_SLOT)
    return 0;
  // point to saved area on the flash memory
  const properties_t *src = get_properties(id);
  if (src == NULL){
//  load_default_properties();
    return 1;
  }
  // active configuration points to save data on flash memory
  lastsaveid = id;
  // duplicated saved data onto sram to be able to modify marker/trace
  memcpy(&current_props, src, sizeof(properties_t));
  return 0;
}

void clear_all_config_prop_data(void) {
  lastsaveid = NO_SAVE_SLOT;
  checksum_ok = 0;
  // unlock and erase flash pages (state + bands pages sit just below the properties area)
  flash_erase_pages(SAVE_STATE_ADDR, SAVE_STATE_SIZE + SAVE_BANDS_SIZE + SAVE_FULL_AREA_SIZE);
}

//
// NanoAnalyzer band preset table storage
//
#define BANDS_MAGIC 0x34444e42  // "BND4"

typedef struct {
  uint32_t      magic;
  band_preset_t band[BANDS_MAX];
  uint32_t      checksum;
} band_store_t;

int bands_save(void) {
  band_store_t s;
  s.magic = BANDS_MAGIC;
  memcpy(s.band, bands, sizeof(bands));
  s.checksum = checksum(&s, sizeof s - sizeof s.checksum);
  flash_erase_pages(SAVE_BANDS_ADDR, SAVE_BANDS_SIZE);
  flash_program_half_word_buffer((uint16_t*)SAVE_BANDS_ADDR, (uint16_t*)&s, sizeof s);
  return 0;
}

int bands_recall(void) {
  const band_store_t *src = (const band_store_t*)SAVE_BANDS_ADDR;
  if (src->magic != BANDS_MAGIC ||
      checksum(src, sizeof *src - sizeof src->checksum) != src->checksum)
    return -1;
  memcpy(bands, src->band, sizeof(bands));
  return 0;
}

//
// NanoAnalyzer last-sweep-state storage (remember the last band across power cycles)
//
#define STATE_MAGIC 0x54415453  // "STAT"

typedef struct {
  uint32_t magic;
  freq_t   f0, f1;
  uint32_t points;
  uint32_t checksum;
} sweep_state_t;

void state_save(void) {
  static freq_t l0, l1; static uint32_t lp;
  if (frequency0 == l0 && frequency1 == l1 && sweep_points == lp) return;  // no change
  sweep_state_t s = { .magic = STATE_MAGIC, .f0 = frequency0, .f1 = frequency1, .points = sweep_points };
  s.checksum = checksum(&s, sizeof s - sizeof s.checksum);
  flash_erase_pages(SAVE_STATE_ADDR, SAVE_STATE_SIZE);
  flash_program_half_word_buffer((uint16_t*)SAVE_STATE_ADDR, (uint16_t*)&s, sizeof s);
  l0 = frequency0; l1 = frequency1; lp = sweep_points;
}

void state_recall(void) {
  const sweep_state_t *src = (const sweep_state_t*)SAVE_STATE_ADDR;
  if (src->magic != STATE_MAGIC ||
      checksum(src, sizeof *src - sizeof src->checksum) != src->checksum)
    return;
  if (src->f0 < FREQUENCY_MIN || src->f1 > FREQUENCY_MAX || src->f0 >= src->f1)
    return;
  frequency0 = src->f0;
  frequency1 = src->f1;
  if (src->points >= SWEEP_POINTS_MIN && src->points <= SWEEP_POINTS_MAX)
    sweep_points = src->points;
  props_mode |= TD_CENTER_SPAN;
}

