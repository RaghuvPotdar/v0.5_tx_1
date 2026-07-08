// ============================================================================
//  battery.cpp — battery monitor
// ============================================================================
#include "battery.h"
#include "inputs.h"
#include "hid_joystick.h"

uint8_t battery_pct(float v) {
  // 2S Li-ion piecewise map (Handbook Section 8.7)
  struct P { float v; uint8_t p; };
  static const P tbl[] = {
    {8.40f,100},{8.00f,85},{7.70f,70},{7.40f,55},
    {7.10f,40},{6.90f,25},{6.70f,12},{6.40f,0}
  };
  const int N = sizeof(tbl) / sizeof(tbl[0]);
  if (v >= tbl[0].v) return 100;
  if (v <= tbl[N-1].v) return 0;
  for (int i = 0; i < N - 1; i++) {
    if (v <= tbl[i].v && v > tbl[i+1].v) {
      float f = (v - tbl[i+1].v) / (tbl[i].v - tbl[i+1].v);
      return (uint8_t)(tbl[i+1].p + f * (tbl[i].p - tbl[i+1].p));
    }
  }
  return 0;
}

float battery_read_volts() {
  uint16_t raw = inputs_read_vbat_raw();
  float vadc = ((float)raw / (float)ADC_MAXCOUNT) * ADC_VREF;
  return (vadc / VBAT_DIV_K) * g_cal.vbat_gain;
}

void battery_update(shared_state_t* s) {
  s->vbat = battery_read_volts();
  s->batt_pct = battery_pct(s->vbat);
  s->usb_connected = hid_usb_mounted();
  s->charging = (digitalRead(PIN_CHG_STAT) == LOW);   // TP5100 CHRG: low = charging
}
