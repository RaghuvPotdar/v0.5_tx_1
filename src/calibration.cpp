// ============================================================================
//  calibration.cpp — calibration math + interactive capture (Section 13)
// ============================================================================
#include "calibration.h"
#include <string.h>

// Cubic expo blend: e=0 -> linear, e=1 -> full cubic. f in [0,1].
static float apply_expo(float f, uint8_t expo) {
  float e = (float)expo / 100.0f;
  return (1.0f - e) * f + e * f * f * f;
}

uint16_t cal_apply_axis(uint16_t raw, const axis_cal_t* c) {
  int32_t r = raw, lo = c->cmin, mid = c->cmid, hi = c->cmax, db = c->deadband;
  int32_t out;

  if (c->mode == 1) {
    // ---- linear axis (throttle / pots): map lo..hi -> 0..65535 ----
    int32_t span = hi - lo; if (span < 1) span = 1;
    int32_t v = r - lo; if (v < 0) v = 0; if (v > span) v = span;
    float f = (float)v / (float)span;
    f = apply_expo(f, c->expo);
    out = (int32_t)(f * 65535.0f);
  } else {
    // ---- centered axis (roll/pitch/yaw): map around mid -> 0..65535 ----
    if (r >= mid) {
      int32_t span = hi - mid; if (span < 1) span = 1;
      int32_t v = r - mid - db; if (v < 0) v = 0;
      int32_t spanDb = span - db; if (spanDb < 1) spanDb = 1;
      float f = (float)v / (float)spanDb; if (f > 1.0f) f = 1.0f;
      f = apply_expo(f, c->expo);
      out = 32768 + (int32_t)(f * 32767.0f);
    } else {
      int32_t span = mid - lo; if (span < 1) span = 1;
      int32_t v = mid - r - db; if (v < 0) v = 0;
      int32_t spanDb = span - db; if (spanDb < 1) spanDb = 1;
      float f = (float)v / (float)spanDb; if (f > 1.0f) f = 1.0f;
      f = apply_expo(f, c->expo);
      out = 32768 - (int32_t)(f * 32768.0f);
    }
  }

  if (out < 0) out = 0;
  if (out > 65535) out = 65535;
  if (c->invert) out = 65535 - out;
  return (uint16_t)out;
}

void cal_apply(const uint16_t raw[NUM_AXES], uint16_t hid_out[NUM_AXES], const cal_blob_t* b) {
  for (int i = 0; i < NUM_AXES; i++) hid_out[i] = cal_apply_axis(raw[i], &b->axis[i]);
}

void cal_default(cal_blob_t* b) {
  memset(b, 0, sizeof(*b));
  b->magic = CAL_MAGIC;
  b->version = CAL_VERSION;
  for (int i = 0; i < NUM_AXES; i++) {
    b->axis[i].cmin = 200;
    b->axis[i].cmid = 2048;
    b->axis[i].cmax = 3895;
    b->axis[i].deadband = 20;
    b->axis[i].expo = 0;
    b->axis[i].invert = 0;
    b->axis[i].mode = 0;                 // centered by default
  }
  // Throttle and both pots are linear (full-range) axes.
  b->axis[AX_THR].mode  = 1;
  b->axis[AX_POTA].mode = 1;
  b->axis[AX_POTB].mode = 1;
  b->vbat_gain = 1.0f;
  strncpy(b->serial, "RVTX-000000", sizeof(b->serial) - 1);
}

void cal_capture_center(cal_blob_t* b, const uint16_t raw[NUM_AXES]) {
  for (int i = 0; i < NUM_AXES; i++)
    if (b->axis[i].mode == 0) b->axis[i].cmid = raw[i];
}

void cal_begin_endpoints(cal_blob_t* b) {
  for (int i = 0; i < NUM_AXES; i++) {
    b->axis[i].cmin = ADC_MAXCOUNT;      // so first sample shrinks it
    b->axis[i].cmax = 0;
  }
}

void cal_update_endpoints(cal_blob_t* b, const uint16_t raw[NUM_AXES]) {
  for (int i = 0; i < NUM_AXES; i++) {
    if (raw[i] < b->axis[i].cmin) b->axis[i].cmin = raw[i];
    if (raw[i] > b->axis[i].cmax) b->axis[i].cmax = raw[i];
  }
}

void cal_finish_endpoints(cal_blob_t* b) {
  for (int i = 0; i < NUM_AXES; i++) {
    if (b->axis[i].cmax <= b->axis[i].cmin) {        // guard against no-movement
      b->axis[i].cmin = 200; b->axis[i].cmax = 3895;
    }
    if (b->axis[i].mode == 0) {                       // keep center inside range
      if (b->axis[i].cmid < b->axis[i].cmin) b->axis[i].cmid = b->axis[i].cmin;
      if (b->axis[i].cmid > b->axis[i].cmax) b->axis[i].cmid = b->axis[i].cmax;
    }
  }
}

void cal_set_vbat_gain(cal_blob_t* b, float measured_v, float actual_v) {
  if (measured_v > 0.5f) b->vbat_gain = actual_v / measured_v;
}
