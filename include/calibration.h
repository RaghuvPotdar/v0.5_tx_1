// ============================================================================
//  calibration.h — apply calibration + interactive calibration routines
//  Section 13.  Math (cal_apply_axis) is pure and host-testable.
// ============================================================================
#pragma once
#include "config.h"

void     cal_default(cal_blob_t* b);                                   // safe defaults
void     cal_apply(const uint16_t raw[NUM_AXES],
                   uint16_t hid_out[NUM_AXES], const cal_blob_t* b);    // all axes
uint16_t cal_apply_axis(uint16_t raw, const axis_cal_t* c);            // one axis

// Interactive (UI-driven) helpers — mutate the blob, caller persists via storage
void cal_capture_center(cal_blob_t* b, const uint16_t raw[NUM_AXES]);
void cal_begin_endpoints(cal_blob_t* b);
void cal_update_endpoints(cal_blob_t* b, const uint16_t raw[NUM_AXES]);
void cal_finish_endpoints(cal_blob_t* b);
void cal_set_vbat_gain(cal_blob_t* b, float measured_v, float actual_v);
