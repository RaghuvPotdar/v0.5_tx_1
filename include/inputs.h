// ============================================================================
//  inputs.h — ADC sampling (oversample + IIR), switch decode, debounce
//  Runs primarily on Core 1 (control loop). Section 6.2-6.4, Section 10.
// ============================================================================
#pragma once
#include "config.h"

void     inputs_init();
void     inputs_sample(uint16_t raw_out[NUM_AXES]); // filtered raw counts
uint16_t inputs_read_vbat_raw();                    // slow-filtered VBAT counts
void     inputs_scan_switches(shared_state_t* s);   // debounce + decode + buttons
bool     inputs_nav_ok();                            // debounced UI buttons
bool     inputs_nav_up();
bool     inputs_nav_dn();
bool     inputs_adc_sane();                          // crude ADC health (self-test)
