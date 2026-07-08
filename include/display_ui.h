// ============================================================================
//  display_ui.h — OLED screens + menu state machine (Sections 12, 13)
// ============================================================================
#pragma once
#include "config.h"

bool display_init();      // returns true if panel ACKs on I2C (else FAULT_DISPLAY)
void display_tick();      // call at UI_HZ on Core 0: reads snapshot, renders, menu
bool display_in_calibration();   // true while a cal screen is active
