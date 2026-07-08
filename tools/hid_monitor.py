#!/usr/bin/env python3
"""
hid_monitor.py — quick host-side check that RAVEN TX is sending HID reports.
Reads the 14-byte input report (6x uint16 axes + uint16 buttons) and prints it.

Requires:  pip install hidapi
Usage:     python hid_monitor.py            (auto-find by product string)
           python hid_monitor.py 0x303A 0x82F0
Notes: On Windows the in-box HID driver also exposes the device to DirectInput;
this tool is only for engineering verification (Section 16, TR-03).
"""
import sys, struct, time
import hid  # type: ignore

VID_DEFAULT = 0x303A
PID_DEFAULT = 0x82F0
PRODUCT_HINT = "RAVEN TX"

def find_device(vid, pid):
    for d in hid.enumerate():
        if (d["vendor_id"] == vid and d["product_id"] == pid) or \
           (PRODUCT_HINT.lower() in str(d.get("product_string", "")).lower()):
            return d["vendor_id"], d["product_id"]
    return None

def main():
    vid = int(sys.argv[1], 0) if len(sys.argv) > 1 else VID_DEFAULT
    pid = int(sys.argv[2], 0) if len(sys.argv) > 2 else PID_DEFAULT
    found = find_device(vid, pid)
    if not found:
        print("RAVEN TX not found. Is it plugged in and enumerated?")
        return 1
    vid, pid = found
    h = hid.device()
    h.open(vid, pid)
    h.set_nonblocking(True)
    print(f"Opened {vid:04X}:{pid:04X}. Move sticks/switches. Ctrl-C to stop.")
    try:
        while True:
            data = h.read(64)
            if data:
                # report id may be stripped depending on OS; handle both 14 and 15 byte
                payload = bytes(data[-14:]) if len(data) >= 14 else bytes(data)
                if len(payload) == 14:
                    x, y, z, rx, ry, rz, btn = struct.unpack("<6HH", payload)
                    print(f"R{x:5d} P{y:5d} T{z:5d} A{rx:5d} B{ry:5d} Y{rz:5d}  "
                          f"btn={btn:#013b}", end="\r")
            time.sleep(0.004)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        h.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
