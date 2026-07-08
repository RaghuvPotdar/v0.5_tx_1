// ============================================================================
//  storage.cpp — NVS-backed calibration store using Arduino Preferences
//  Wear-leveled by the underlying NVS (RR-07). Writes only happen on Core 0
//  and only when data changed (see display_ui/main commit logic).
// ============================================================================
#include "storage.h"
#include <Preferences.h>

static Preferences s_prefs;
static const char* NS  = "ravtx";
static const char* KEY = "cal";

uint32_t storage_crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
      crc = (crc >> 1) ^ mask;
    }
  }
  return ~crc;
}

bool storage_init() {
  return s_prefs.begin(NS, false);   // false = read/write
}

bool storage_load(cal_blob_t* b) {
  if (s_prefs.getBytesLength(KEY) != sizeof(cal_blob_t)) return false;
  if (s_prefs.getBytes(KEY, b, sizeof(cal_blob_t)) != sizeof(cal_blob_t)) return false;
  if (b->magic != CAL_MAGIC) return false;
  if (b->version != CAL_VERSION) return false;
  uint32_t crc = storage_crc32((const uint8_t*)b, sizeof(cal_blob_t) - sizeof(uint32_t));
  return (crc == b->crc);
}

bool storage_save(const cal_blob_t* b) {
  cal_blob_t tmp = *b;
  tmp.magic   = CAL_MAGIC;
  tmp.version = CAL_VERSION;
  tmp.crc     = storage_crc32((const uint8_t*)&tmp, sizeof(cal_blob_t) - sizeof(uint32_t));
  return (s_prefs.putBytes(KEY, &tmp, sizeof(cal_blob_t)) == sizeof(cal_blob_t));
}
