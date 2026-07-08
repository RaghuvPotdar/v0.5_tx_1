// ============================================================================
//  display_ui.cpp — U8g2 OLED UI + menu/calibration FSM
//  Section 12 (screens) and Section 13 (interactive calibration).
//  Runs on Core 0 only. Reads the mutex-protected snapshot g_state.
// ============================================================================
#include "display_ui.h"
#include "inputs.h"
#include "calibration.h"
#include "storage.h"
#include <Wire.h>
#include <U8g2lib.h>

#if RAVEN_DISPLAY_SH1106
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

static bool s_present = false;

enum UiState {
  UI_BOOT, UI_MAIN, UI_MENU,
  UI_CAL_CENTER, UI_CAL_MOVE, UI_CAL_DONE,
  UI_DIAG, UI_BATT, UI_USB
};
static UiState s_state = UI_BOOT;
static uint32_t s_t0 = 0;
static int s_menu_sel = 0;
static bool s_pOK = false, s_pUP = false, s_pDN = false;

static const char* MENU[] = { "Calibrate Sticks", "Diagnostics", "Battery", "USB Info", "Exit" };
static const int   MENU_N = 5;

// ---- helpers ---------------------------------------------------------------
static int axis_centered_pct(uint16_t v) {           // 0..65535 -> -100..100
  return (int)((v - 32768) / 327.68f);
}
static int axis_linear_pct(uint16_t v) {             // 0..65535 -> 0..100
  return (int)(v / 655.35f);
}
static const char* sw3_str(uint8_t p) { return (p == 0) ? "U" : (p == 1) ? "M" : "D"; }
static const char* sw2_str(uint8_t p) { return (p == 0) ? "U" : "D"; }

static void draw_battery_icon(int x, int y, uint8_t pct, bool charging) {
  u8g2.drawFrame(x, y, 18, 9);
  u8g2.drawBox(x + 18, y + 2, 2, 5);                 // terminal
  int w = (pct * 16) / 100; if (w < 0) w = 0; if (w > 16) w = 16;
  u8g2.drawBox(x + 1, y + 1, w, 7);
  if (charging) { u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(x + 6, y + 7, "~"); }
}

static void draw_stick_box(int x, int y, int s, uint16_t ax, uint16_t ay_inv) {
  u8g2.drawFrame(x, y, s, s);
  int dx = x + 1 + (int)((ax / 65535.0f) * (s - 3));
  int dy = y + 1 + (int)((1.0f - ay_inv / 65535.0f) * (s - 3));
  u8g2.drawDisc(dx + 1, dy + 1, 2);
}

// ---- screen renderers ------------------------------------------------------
static void draw_boot() {
  char buf[24];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.drawStr(18, 26, "RAVEN TX");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(10, 40, "Simulator Training TX");
  snprintf(buf, sizeof(buf), "FW %s  HW %s", FW_VERSION, HW_REV);
  u8g2.drawStr(18, 52, buf);
  uint32_t e = millis() - s_t0;
  int w = (int)((e * 108) / 1500); if (w > 108) w = 108;
  u8g2.drawFrame(10, 56, 108, 6);
  u8g2.drawBox(10, 56, w, 6);
  u8g2.sendBuffer();
}

static void draw_main(const shared_state_t& s) {
  char buf[28];
  u8g2.clearBuffer();
  // top bar
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 8, "RAVEN TX");
  snprintf(buf, sizeof(buf), "%u%%", s.batt_pct);
  u8g2.drawStr(98, 8, buf);
  draw_battery_icon(76, 0, s.batt_pct, s.charging);
  u8g2.drawHLine(0, 10, 128);
  // stick boxes
  draw_stick_box(0, 13, 26, s.axis_hid[AX_ROLL], s.axis_hid[AX_PITCH]);
  draw_stick_box(30, 13, 26, s.axis_hid[AX_YAW], s.axis_hid[AX_THR]);
  // right-side text
  u8g2.setFont(u8g2_font_5x8_tf);
  snprintf(buf, sizeof(buf), "%.2fV", s.vbat);          u8g2.drawStr(60, 20, buf);
  const char* us = s.usb_connected ? (s.charging ? "USB CHG" : "USB OK") : "BATTERY";
  u8g2.drawStr(60, 29, us);
  snprintf(buf, sizeof(buf), "SW %s%s %s%s", sw2_str(s.sw1), sw2_str(s.sw2),
           sw3_str(s.sw3), sw3_str(s.sw4));            u8g2.drawStr(60, 38, buf);
  // pots + axes line
  snprintf(buf, sizeof(buf), "P1 %2d%% P2 %2d%%",
           axis_linear_pct(s.axis_hid[AX_POTA]), axis_linear_pct(s.axis_hid[AX_POTB]));
  u8g2.drawStr(0, 48, buf);
  snprintf(buf, sizeof(buf), "R%+04d P%+04d Y%+04d T%02d",
           axis_centered_pct(s.axis_hid[AX_ROLL]), axis_centered_pct(s.axis_hid[AX_PITCH]),
           axis_centered_pct(s.axis_hid[AX_YAW]),  axis_linear_pct(s.axis_hid[AX_THR]));
  u8g2.drawStr(0, 56, buf);
  // footer
  if (s.fault) { u8g2.drawStr(0, 64, "FAULT - see Diag"); }
  else { snprintf(buf, sizeof(buf), "v%s   OK=menu", FW_VERSION); u8g2.drawStr(0, 64, buf); }
  u8g2.sendBuffer();
}

static void draw_menu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "MENU"); u8g2.drawHLine(0, 11, 128);
  for (int i = 0; i < MENU_N; i++) {
    int y = 22 + i * 10;
    if (i == s_menu_sel) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
    u8g2.drawStr(4, y, MENU[i]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

static void draw_cal_center() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "CALIBRATION 1/2");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 26, "Center both sticks.");
  u8g2.drawStr(0, 36, "Leave pots/throttle");
  u8g2.drawStr(0, 46, "where they are.");
  u8g2.drawStr(0, 62, "Press OK to capture");
  u8g2.sendBuffer();
}

static void draw_cal_move(const shared_state_t& s) {
  char buf[24];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "CALIBRATION 2/2");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 24, "Move EVERY stick,");
  u8g2.drawStr(0, 33, "pot & throttle to");
  u8g2.drawStr(0, 42, "their extremes.");
  snprintf(buf, sizeof(buf), "R%d P%d T%d Y%d", s.raw[AX_ROLL], s.raw[AX_PITCH],
           s.raw[AX_THR], s.raw[AX_YAW]);
  u8g2.drawStr(0, 53, buf);
  u8g2.drawStr(0, 63, "Press OK when done");
  u8g2.sendBuffer();
}

static void draw_cal_done() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 30, "CALIBRATION");
  u8g2.drawStr(40, 44, "SAVED");
  u8g2.sendBuffer();
}

static void draw_diag(const shared_state_t& s) {
  char buf[28];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 8, "DIAGNOSTICS (raw ADC)");
  const char* nm[NUM_AXES] = { "Rol", "Pit", "Thr", "Yaw", "PtA", "PtB" };
  for (int i = 0; i < NUM_AXES; i++) {
    snprintf(buf, sizeof(buf), "%s %4u", nm[i], s.raw[i]);
    u8g2.drawStr((i % 2) * 64, 20 + (i / 2) * 9, buf);
  }
  snprintf(buf, sizeof(buf), "SW %u %u  3:%u 4:%u", s.sw1, s.sw2, s.sw3, s.sw4);
  u8g2.drawStr(0, 52, buf);
  snprintf(buf, sizeof(buf), "btn0x%03X f0x%02X", s.buttons, s.fault);
  u8g2.drawStr(0, 62, buf);
  u8g2.sendBuffer();
}

static void draw_batt(const shared_state_t& s) {
  char buf[24];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "BATTERY"); u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(u8g2_font_logisoso16_tf);
  snprintf(buf, sizeof(buf), "%u%%", s.batt_pct); u8g2.drawStr(0, 36, buf);
  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(buf, sizeof(buf), "%.2f V (2S)", s.vbat); u8g2.drawStr(60, 30, buf);
  u8g2.drawStr(60, 42, s.charging ? "Charging" : (s.usb_connected ? "USB idle" : "On battery"));
  draw_battery_icon(0, 46, s.batt_pct, s.charging);
  u8g2.setFont(u8g2_font_5x8_tf); u8g2.drawStr(28, 54, "OK = back");
  u8g2.sendBuffer();
}

static void draw_usb(const shared_state_t& s) {
  char buf[28];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "USB STATUS"); u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 24, s.usb_connected ? "Host: CONNECTED" : "Host: not connected");
  snprintf(buf, sizeof(buf), "Report rate: %d Hz", REPORT_HZ);  u8g2.drawStr(0, 34, buf);
  snprintf(buf, sizeof(buf), "VID:%04X PID:%04X", USB_VID, USB_PID); u8g2.drawStr(0, 44, buf);
  snprintf(buf, sizeof(buf), "FW %s  6 axes/10 btn", FW_VERSION); u8g2.drawStr(0, 54, buf);
  u8g2.drawStr(0, 63, "OK = back");
  u8g2.sendBuffer();
}

// ---- public API ------------------------------------------------------------
bool display_init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.beginTransmission(0x3C);
  s_present = (Wire.endTransmission() == 0);
  if (s_present) {
    u8g2.setI2CAddress(0x3C << 1);
    u8g2.begin();
    u8g2.setBusClock(400000);
  }
  s_state = UI_BOOT;
  s_t0 = millis();
  return s_present;
}

bool display_in_calibration() {
  return (s_state == UI_CAL_CENTER || s_state == UI_CAL_MOVE);
}

void display_tick() {
  if (!s_present) return;

  // snapshot
  shared_state_t s;
  if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return;
  s = g_state;
  xSemaphoreGive(g_state_mutex);

  // edge-detected nav
  bool ok = inputs_nav_ok(), up = inputs_nav_up(), dn = inputs_nav_dn();
  bool okE = ok && !s_pOK, upE = up && !s_pUP, dnE = dn && !s_pDN;
  s_pOK = ok; s_pUP = up; s_pDN = dn;

  switch (s_state) {
    case UI_BOOT:
      draw_boot();
      if (millis() - s_t0 > 1500) s_state = UI_MAIN;
      break;

    case UI_MAIN:
      draw_main(s);
      if (okE) { s_state = UI_MENU; s_menu_sel = 0; }
      break;

    case UI_MENU:
      draw_menu();
      if (upE) s_menu_sel = (s_menu_sel + MENU_N - 1) % MENU_N;
      if (dnE) s_menu_sel = (s_menu_sel + 1) % MENU_N;
      if (okE) {
        switch (s_menu_sel) {
          case 0: s_state = UI_CAL_CENTER; break;
          case 1: s_state = UI_DIAG; break;
          case 2: s_state = UI_BATT; break;
          case 3: s_state = UI_USB; break;
          default: s_state = UI_MAIN; break;
        }
      }
      break;

    case UI_CAL_CENTER:
      draw_cal_center();
      if (okE) {
        cal_capture_center(&g_cal, s.raw);
        cal_begin_endpoints(&g_cal);
        s_state = UI_CAL_MOVE;
      }
      break;

    case UI_CAL_MOVE:
      cal_update_endpoints(&g_cal, s.raw);   // continuously expand min/max
      draw_cal_move(s);
      if (okE) {
        cal_finish_endpoints(&g_cal);
        storage_save(&g_cal);
        s_state = UI_CAL_DONE; s_t0 = millis();
      }
      break;

    case UI_CAL_DONE:
      draw_cal_done();
      if (millis() - s_t0 > 1200) s_state = UI_MAIN;
      break;

    case UI_DIAG: draw_diag(s); if (okE) s_state = UI_MAIN; break;
    case UI_BATT: draw_batt(s); if (okE) s_state = UI_MAIN; break;
    case UI_USB:  draw_usb(s);  if (okE) s_state = UI_MAIN; break;
  }
}
