// ============================================================================
//  main.cpp — RAVEN TX firmware entry point
//  Dual-core: control loop (Core 1) + UI/services (Core 0).  Sections 10, 11.
// ============================================================================
#include "config.h"
#include "hid_joystick.h"
#include "inputs.h"
#include "calibration.h"
#include "storage.h"
#include "battery.h"
#include "display_ui.h"
#include "diagnostics.h"

// ---- globals declared extern in config.h -----------------------------------
shared_state_t   g_state;
SemaphoreHandle_t g_state_mutex = nullptr;
cal_blob_t       g_cal;

static bool s_displayFault = false;
static bool s_nvsFault     = false;
static TaskHandle_t s_controlTask = nullptr;
static TaskHandle_t s_uiTask      = nullptr;

// ---------------------------------------------------------------------------
//  Core 1 — real-time control loop (no blocking calls, FW-11)
// ---------------------------------------------------------------------------
static void controlTask(void* arg) {
  const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
  TickType_t last = xTaskGetTickCount();
  uint16_t raw[NUM_AXES], hid[NUM_AXES];
  shared_state_t loc;
  raven_hid_report_t rpt;

  for (;;) {
    uint32_t t0 = micros();

    inputs_sample(raw);
    inputs_scan_switches(&loc);
    cal_apply(raw, hid, &g_cal);

    // map internal axis order -> HID report fields (Section 9.4)
    rpt.x  = hid[AX_ROLL];
    rpt.y  = hid[AX_PITCH];
    rpt.z  = hid[AX_THR];
    rpt.rx = hid[AX_POTA];
    rpt.ry = hid[AX_POTB];
    rpt.rz = hid[AX_YAW];
    rpt.buttons = loc.buttons;
    hid_send(&rpt);

    uint32_t dt = micros() - t0;

    // publish snapshot without ever blocking the loop (timeout 0)
    if (xSemaphoreTake(g_state_mutex, 0) == pdTRUE) {
      for (int i = 0; i < NUM_AXES; i++) { g_state.axis_hid[i] = hid[i]; g_state.raw[i] = raw[i]; }
      g_state.buttons = loc.buttons;
      g_state.sw1 = loc.sw1; g_state.sw2 = loc.sw2;
      g_state.sw3 = loc.sw3; g_state.sw4 = loc.sw4;
      g_state.loop_us = dt;
      xSemaphoreGive(g_state_mutex);
    }

    vTaskDelayUntil(&last, period);   // fixed cadence -> low jitter (FR-12)
  }
}

// ---------------------------------------------------------------------------
//  Core 0 — UI + background services
// ---------------------------------------------------------------------------
static void uiTask(void* arg) {
  uint32_t lastSvc = 0;
  for (;;) {
    uint32_t now = millis();
    if (now - lastSvc >= (uint32_t)(1000 / SVC_HZ)) {
      lastSvc = now;

      shared_state_t b;
      battery_update(&b);
      uint8_t faults = diagnostics_selftest();
      if (s_displayFault) faults |= FAULT_DISPLAY;
      if (s_nvsFault)     faults |= FAULT_NVS;

      shared_state_t logcopy;
      if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_state.vbat          = b.vbat;
        g_state.batt_pct      = b.batt_pct;
        g_state.usb_connected = b.usb_connected;
        g_state.charging      = b.charging;
        g_state.fault         = faults;
        logcopy = g_state;
        xSemaphoreGive(g_state_mutex);
        diagnostics_serial_log(&logcopy);
      }

      // power LED: solid normally, blink on low battery (Section 8.6)
      digitalWrite(PIN_LED_PWR, (b.vbat < VBAT_WARN_V) ? ((now / 300) & 1) : HIGH);
    }

    display_tick();
    vTaskDelay(pdMS_TO_TICKS(1000 / UI_HZ));
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  g_state_mutex = xSemaphoreCreateMutex();
  memset(&g_state, 0, sizeof(g_state));

  // 1) storage + calibration (must precede hid_init for the serial number)
  bool nvsOK = storage_init();
  if (!nvsOK || !storage_load(&g_cal)) {
    cal_default(&g_cal);
    if (nvsOK) storage_save(&g_cal);
  }
  s_nvsFault = !nvsOK;

  // 2) inputs + display
  inputs_init();
  s_displayFault = !display_init();

  // 3) USB HID (enumerate)
  hid_init();

  // 4) launch tasks (control on Core 1, UI on Core 0)
  xTaskCreatePinnedToCore(controlTask, "control", 4096, nullptr, 3, &s_controlTask, 1);
  xTaskCreatePinnedToCore(uiTask,      "ui",      8192, nullptr, 1, &s_uiTask,      0);
}

void loop() {
  // Everything runs in the two tasks; idle here.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
