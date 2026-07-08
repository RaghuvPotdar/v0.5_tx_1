// ============================================================================
//  diagnostics.cpp
// ============================================================================
#include "diagnostics.h"
#include "inputs.h"

uint8_t diagnostics_selftest() {
  uint8_t f = FAULT_NONE;
  if (!inputs_adc_sane()) f |= FAULT_ADC;
  return f;   // display/NVS faults are OR'd in by their owners (main/display_ui)
}

void diagnostics_serial_log(const shared_state_t* s) {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 500) return;     // 2 Hz
  last = now;
  Serial.printf("[RAVEN] V=%.2f %u%% USB=%d CHG=%d SW=%u%u%u%u AX=",
                s->vbat, s->batt_pct, (int)s->usb_connected, (int)s->charging,
                s->sw1, s->sw2, s->sw3, s->sw4);
  for (int i = 0; i < NUM_AXES; i++) Serial.printf("%u ", s->axis_hid[i]);
  Serial.printf("btn=0x%03X fault=0x%02X loop=%uus\n",
                s->buttons, s->fault, s->loop_us);
}
