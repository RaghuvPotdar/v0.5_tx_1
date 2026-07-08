// ============================================================================
//  storage.h — persistent calibration storage in NVS (Section 13.6, FW-04)
// ============================================================================
#pragma once
#include "config.h"

bool     storage_init();                       // open NVS namespace
bool     storage_load(cal_blob_t* b);          // true if valid (magic+crc+ver) loaded
bool     storage_save(const cal_blob_t* b);    // writes magic/version/crc then persists
uint32_t storage_crc32(const uint8_t* data, size_t len);
