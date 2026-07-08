// ============================================================================
//  hid_joystick.h — USB HID joystick device (6 axes + 16 buttons)
//  Handbook Section 9.  Report descriptor + report struct are authoritative.
// ============================================================================
#pragma once
#include "config.h"

// 14-byte HID input report (little-endian). Order matches the report descriptor.
typedef struct __attribute__((packed)) {
  uint16_t x;        // Roll
  uint16_t y;        // Pitch
  uint16_t z;        // Throttle
  uint16_t rx;       // Pot A
  uint16_t ry;       // Pot B
  uint16_t rz;       // Yaw
  uint16_t buttons;  // bit0..bit9 used (Section 9.4 mapping)
} raven_hid_report_t;

// HID button bit assignments (Section 9.4)
enum HidButtonBit {
  BTN_SW1_UP = 0, BTN_SW1_DN,
  BTN_SW2_UP,     BTN_SW2_DN,
  BTN_SW3_UP,     BTN_SW3_MID, BTN_SW3_DN,
  BTN_SW4_UP,     BTN_SW4_MID, BTN_SW4_DN
};

void hid_init();                              // configure USB + register HID, USB.begin()
bool hid_send(const raven_hid_report_t* r);   // send one input report
bool hid_usb_mounted();                       // true once host has enumerated us
