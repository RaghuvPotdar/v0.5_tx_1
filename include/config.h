// ============================================================================
//  config.h — RAVEN TX global configuration, pin map, and shared types
//  AUTHORITATIVE pin map — must match Handbook Section 6.1 (traceability triad)
// ============================================================================
#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---------------------------------------------------------------------------
//  Identity / versioning  (FW-10)
// ---------------------------------------------------------------------------
#define FW_VERSION   "1.0.0"
#define HW_REV       "rA"
#define PRODUCT_NAME "RAVEN TX"
#define MANUF_NAME   "Raven Systems"

// USB VID/PID — PLACEHOLDER for development only. Finalize before production
// (Handbook Section 9.5; tracked as a risk in Appendix A).
#define USB_VID 0x303A      // Espressif VID (dev/test use)
#define USB_PID 0x82F0      // test PID — DO NOT SHIP

// ---------------------------------------------------------------------------
//  Pin map (ESP32-S3) — Handbook Section 6.1
// ---------------------------------------------------------------------------
#define PIN_AIN_ROLL    1   // ADC1_CH0  Gimbal L X
#define PIN_AIN_PITCH   2   // ADC1_CH1  Gimbal L Y
#define PIN_AIN_THR     3   // ADC1_CH2  Gimbal R Y (throttle)
#define PIN_AIN_YAW     4   // ADC1_CH3  Gimbal R X (yaw)
#define PIN_AIN_POTA    5   // ADC1_CH4  Pot A
#define PIN_AIN_POTB    6   // ADC1_CH5  Pot B
#define PIN_AIN_VBAT    7   // ADC1_CH6  Battery sense divider

#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9

#define PIN_SW1        10   // 2-pos toggle
#define PIN_SW2        11   // 2-pos toggle
#define PIN_SW3_UP     12   // 3-pos toggle up
#define PIN_SW3_DN     13   // 3-pos toggle down
#define PIN_SW4_UP     14   // 3-pos toggle up
#define PIN_SW4_DN     15   // 3-pos toggle down

#define PIN_BTN_OK     16   // UI select
#define PIN_BTN_UP     17   // UI up
#define PIN_BTN_DN     18   // UI down

// GPIO19/20 = native USB D-/D+ (do not reuse)
#define PIN_LED_PWR    21
#define PIN_CHG_STAT   38   // TP5100 CHRG open-drain (active low = charging)

// ---------------------------------------------------------------------------
//  Analog / control-loop configuration
// ---------------------------------------------------------------------------
#define NUM_AXES        6
#define ADC_MAXCOUNT    4095        // 12-bit
#define OVERSAMPLE      8           // averaged ADC samples per axis per loop
#define ADC_IIR_ALPHA   0.30f       // input low-pass (0..1, higher = snappier)

#define REPORT_HZ       250         // default HID report rate (FR-12, configurable)
#define CONTROL_PERIOD_MS (1000 / REPORT_HZ)

#define UI_HZ           30          // display refresh
#define SVC_HZ          5           // battery/diagnostics service rate

// Axis indices into arrays (order is internal; HID order set in hid_joystick)
enum AxisIndex { AX_ROLL = 0, AX_PITCH, AX_THR, AX_YAW, AX_POTA, AX_POTB };

// ---------------------------------------------------------------------------
//  Battery sense
// ---------------------------------------------------------------------------
#define VBAT_DIV_K     0.2481f      // R2/(R1+R2) = 33k/133k  (Section 6.6)
#define ADC_VREF       3.30f        // nominal full-scale volts at ADC_MAXCOUNT
#define VBAT_WARN_V    6.60f        // low-battery warning (Section 8.6)
#define VBAT_CRIT_V    6.40f        // critical
#define VBAT_FULL_V    8.20f        // ~100% display anchor

// ---------------------------------------------------------------------------
//  Per-axis calibration record
// ---------------------------------------------------------------------------
typedef struct {
  uint16_t cmin;       // raw min  (0..4095)
  uint16_t cmid;       // raw center
  uint16_t cmax;       // raw max
  uint16_t deadband;   // counts around center treated as 0
  uint8_t  expo;       // 0..100 (% cubic expo)
  uint8_t  invert;     // 0 / 1
  uint8_t  mode;       // 0 = centered (Roll/Pitch/Yaw), 1 = linear (Throttle/Pots)
  uint8_t  _pad2;
} axis_cal_t;

#define CAL_MAGIC    0x52565431u   // "RVT1"
#define CAL_VERSION  1

// Persistent calibration blob (stored in NVS with CRC, FW-04)
typedef struct {
  uint32_t   magic;
  uint16_t   version;
  uint16_t   _pad;
  axis_cal_t axis[NUM_AXES];
  float      vbat_gain;            // multiplicative trim for VBAT (Section 8.7)
  char       serial[16];           // per-unit serial (Section 17)
  uint32_t   crc;                  // CRC32 over all preceding bytes
} cal_blob_t;

// ---------------------------------------------------------------------------
//  Shared state snapshot (Core1 -> Core0), mutex protected (Section 10.5)
// ---------------------------------------------------------------------------
typedef struct {
  uint16_t axis_hid[NUM_AXES];     // post-calibration, 0..65535
  uint16_t raw[NUM_AXES];          // filtered raw counts (diagnostics)
  uint16_t buttons;                // HID button bitfield
  uint8_t  sw1, sw2;               // 0=up(open),1=down(closed)
  uint8_t  sw3, sw4;               // 0=up,1=center,2=down
  float    vbat;                   // volts
  uint8_t  batt_pct;               // 0..100
  bool     usb_connected;
  bool     charging;
  uint8_t  fault;                  // bitmask: see FAULT_* below
  uint32_t loop_us;                // last control-loop duration (perf)
} shared_state_t;

#define FAULT_NONE     0x00
#define FAULT_DISPLAY  0x01
#define FAULT_NVS      0x02
#define FAULT_ADC      0x04
#define FAULT_CAL      0x08

// Global shared state + mutex (defined in main.cpp)
extern shared_state_t  g_state;
extern SemaphoreHandle_t g_state_mutex;
extern cal_blob_t      g_cal;      // active calibration (defined in main.cpp)
