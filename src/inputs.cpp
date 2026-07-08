// ============================================================================
//  inputs.cpp — analog + digital input acquisition
// ============================================================================
#include "inputs.h"
#include "hid_joystick.h"   // for HID button bit enums

static const uint8_t s_axis_pins[NUM_AXES] = {
  PIN_AIN_ROLL, PIN_AIN_PITCH, PIN_AIN_THR, PIN_AIN_YAW, PIN_AIN_POTA, PIN_AIN_POTB
};
static float s_filt[NUM_AXES];
static float s_vbat_filt = 0.0f;

// ---- digital debounce ------------------------------------------------------
#define NUM_SW_PINS    9
#define DEBOUNCE_COUNT 2     // consecutive equal scans (loop=4ms -> ~8ms > 5ms, FW-05)
static const uint8_t s_sw_pins[NUM_SW_PINS] = {
  PIN_SW1, PIN_SW2, PIN_SW3_UP, PIN_SW3_DN, PIN_SW4_UP, PIN_SW4_DN,
  PIN_BTN_OK, PIN_BTN_UP, PIN_BTN_DN
};
enum { IDX_SW1, IDX_SW2, IDX_SW3U, IDX_SW3D, IDX_SW4U, IDX_SW4D, IDX_OK, IDX_UP, IDX_DN };
static uint8_t s_stable[NUM_SW_PINS];
static uint8_t s_cand[NUM_SW_PINS];
static uint8_t s_cnt[NUM_SW_PINS];

static void debounce_scan() {
  for (int i = 0; i < NUM_SW_PINS; i++) {
    uint8_t v = (digitalRead(s_sw_pins[i]) == LOW) ? 1 : 0;   // active-low
    if (v == s_cand[i]) { if (s_cnt[i] < 255) s_cnt[i]++; }
    else                { s_cand[i] = v; s_cnt[i] = 0; }
    if (s_cnt[i] >= DEBOUNCE_COUNT) s_stable[i] = s_cand[i];
  }
}
static inline uint8_t sw(int idx) { return s_stable[idx]; }   // 1 = active

void inputs_init() {
  analogReadResolution(12);
  for (int i = 0; i < NUM_AXES; i++) {
    pinMode(s_axis_pins[i], INPUT);
    analogSetPinAttenuation(s_axis_pins[i], ADC_11db);        // ~0..3.1V range
    s_filt[i] = analogRead(s_axis_pins[i]);
  }
  pinMode(PIN_AIN_VBAT, INPUT);
  analogSetPinAttenuation(PIN_AIN_VBAT, ADC_11db);
  s_vbat_filt = analogRead(PIN_AIN_VBAT);

  for (int i = 0; i < NUM_SW_PINS; i++) {
    pinMode(s_sw_pins[i], INPUT_PULLUP);
    s_stable[i] = (digitalRead(s_sw_pins[i]) == LOW) ? 1 : 0;
    s_cand[i] = s_stable[i];
    s_cnt[i] = DEBOUNCE_COUNT;
  }
  pinMode(PIN_CHG_STAT, INPUT_PULLUP);
  pinMode(PIN_LED_PWR, OUTPUT);
  digitalWrite(PIN_LED_PWR, HIGH);
}

void inputs_sample(uint16_t raw_out[NUM_AXES]) {
  for (int i = 0; i < NUM_AXES; i++) {
    uint32_t acc = 0;
    for (int s = 0; s < OVERSAMPLE; s++) acc += analogRead(s_axis_pins[i]);
    float avg = (float)acc / (float)OVERSAMPLE;
    s_filt[i] += ADC_IIR_ALPHA * (avg - s_filt[i]);
    raw_out[i] = (uint16_t)(s_filt[i] + 0.5f);
  }
}

uint16_t inputs_read_vbat_raw() {
  uint32_t acc = 0;
  for (int s = 0; s < OVERSAMPLE; s++) acc += analogRead(PIN_AIN_VBAT);
  float avg = (float)acc / (float)OVERSAMPLE;
  s_vbat_filt += 0.05f * (avg - s_vbat_filt);   // slow filter for a steady reading
  return (uint16_t)(s_vbat_filt + 0.5f);
}

void inputs_scan_switches(shared_state_t* s) {
  debounce_scan();
  s->sw1 = sw(IDX_SW1);                                  // 0=up(open), 1=down(closed)
  s->sw2 = sw(IDX_SW2);
  s->sw3 = sw(IDX_SW3U) ? 0 : (sw(IDX_SW3D) ? 2 : 1);    // 0=up,1=center,2=down
  s->sw4 = sw(IDX_SW4U) ? 0 : (sw(IDX_SW4D) ? 2 : 1);

  uint16_t b = 0;
  b |= (s->sw1 == 0) ? (1u << BTN_SW1_UP) : (1u << BTN_SW1_DN);
  b |= (s->sw2 == 0) ? (1u << BTN_SW2_UP) : (1u << BTN_SW2_DN);
  if      (s->sw3 == 0) b |= (1u << BTN_SW3_UP);
  else if (s->sw3 == 1) b |= (1u << BTN_SW3_MID);
  else                  b |= (1u << BTN_SW3_DN);
  if      (s->sw4 == 0) b |= (1u << BTN_SW4_UP);
  else if (s->sw4 == 1) b |= (1u << BTN_SW4_MID);
  else                  b |= (1u << BTN_SW4_DN);
  s->buttons = b;
}

bool inputs_nav_ok() { return s_stable[IDX_OK] == 1; }
bool inputs_nav_up() { return s_stable[IDX_UP] == 1; }
bool inputs_nav_dn() { return s_stable[IDX_DN] == 1; }

bool inputs_adc_sane() {
  uint16_t raw = inputs_read_vbat_raw();
  // A floating/dead ADC tends to rail; require a plausible mid-range VBAT reading.
  return (raw > 100 && raw < (ADC_MAXCOUNT - 10));
}
