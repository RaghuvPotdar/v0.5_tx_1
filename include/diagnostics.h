// ============================================================================
//  diagnostics.h — boot self-test + serial telemetry (Section 16, FW-13)
// ============================================================================
#pragma once
#include "config.h"

uint8_t diagnostics_selftest();                    // returns FAULT_* bitmask
void    diagnostics_serial_log(const shared_state_t* s);  // rate-limited 2 Hz
