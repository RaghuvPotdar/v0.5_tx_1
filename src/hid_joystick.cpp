// ============================================================================
//  hid_joystick.cpp — native USB-OTG HID joystick on ESP32-S3 (TinyUSB)
//  Requires build flag ARDUINO_USB_MODE=0 (see platformio.ini).
// ============================================================================
#include "hid_joystick.h"
#include "USB.h"
#include "USBHID.h"

// TinyUSB mount status (linked from the core's TinyUSB). Declared to avoid
// pulling tusb.h include-path assumptions.
extern "C" bool tud_mounted(void);

static USBHID HID;

// ---- HID Report Descriptor (Handbook Section 9.4) --------------------------
static const uint8_t RAVEN_HID_REPORT_DESC[] = {
  0x05, 0x01,                    // Usage Page (Generic Desktop)
  0x09, 0x04,                    // Usage (Joystick)
  0xA1, 0x01,                    // Collection (Application)
  0x85, 0x01,                    //   Report ID (1)
  0xA1, 0x00,                    //   Collection (Physical)
  0x09, 0x30,                    //     Usage (X)   Roll
  0x09, 0x31,                    //     Usage (Y)   Pitch
  0x09, 0x32,                    //     Usage (Z)   Throttle
  0x09, 0x33,                    //     Usage (Rx)  Pot A
  0x09, 0x34,                    //     Usage (Ry)  Pot B
  0x09, 0x35,                    //     Usage (Rz)  Yaw
  0x15, 0x00,                    //     Logical Minimum (0)
  0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65535)
  0x75, 0x10,                    //     Report Size (16)
  0x95, 0x06,                    //     Report Count (6)
  0x81, 0x02,                    //     Input (Data,Var,Abs)
  0xC0,                          //   End Collection (Physical)
  0x05, 0x09,                    //   Usage Page (Button)
  0x19, 0x01,                    //   Usage Minimum (Button 1)
  0x29, 0x10,                    //   Usage Maximum (Button 16)
  0x15, 0x00,                    //   Logical Minimum (0)
  0x25, 0x01,                    //   Logical Maximum (1)
  0x75, 0x01,                    //   Report Size (1)
  0x95, 0x10,                    //   Report Count (16)
  0x81, 0x02,                    //   Input (Data,Var,Abs)
  0xC0                           // End Collection
};

// ---- HID device subclass ---------------------------------------------------
class RavenJoystick : public USBHIDDevice {
 public:
  RavenJoystick() {
    static bool registered = false;
    if (!registered) {           // register exactly once
      registered = true;
      HID.addDevice(this, sizeof(RAVEN_HID_REPORT_DESC));
    }
  }
  void begin() { HID.begin(); }

  // Core asks for the report descriptor during enumeration.
  uint16_t _onGetDescriptor(uint8_t* dst) override {
    memcpy(dst, RAVEN_HID_REPORT_DESC, sizeof(RAVEN_HID_REPORT_DESC));
    return sizeof(RAVEN_HID_REPORT_DESC);
  }

  bool send(const raven_hid_report_t* r) {
    return HID.SendReport(1 /*report id*/, (const void*)r, sizeof(raven_hid_report_t), 4);
  }
};

static RavenJoystick s_joy;

void hid_init() {
  USB.VID(USB_VID);
  USB.PID(USB_PID);
  USB.productName(PRODUCT_NAME);
  USB.manufacturerName(MANUF_NAME);
  USB.serialNumber(g_cal.serial[0] ? g_cal.serial : "RVTX-000000");
  s_joy.begin();
  USB.begin();
}

bool hid_send(const raven_hid_report_t* r) { return s_joy.send(r); }
bool hid_usb_mounted() { return tud_mounted(); }
