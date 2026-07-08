// ============================================================================
//  battery.h — VBAT measurement, %-curve, charge/USB status (Section 8)
// ============================================================================
#pragma once
#include "config.h"

void    battery_update(shared_state_t* s);  // fill vbat, batt_pct, usb, charging
uint8_t battery_pct(float v);               // 2S pack volts -> 0..100 (Section 8.7)
float   battery_read_volts();               // current calibrated pack volts
