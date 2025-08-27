// boot_mode.h
#ifndef BOOT_MODE_H
#define BOOT_MODE_H

#include <Arduino.h>
#include <Preferences.h>

// Boot modes
#define MEDIA_MODE 0
#define USB_MODE   1

// NVS namespace/key
static constexpr const char* BOOT_NS  = "boot";
static constexpr const char* BOOT_KEY = "mode";

static inline void set_boot_mode(int mode) {
  Preferences prefs;
  if (prefs.begin(BOOT_NS, false)) {
    prefs.putUChar(BOOT_KEY, static_cast<uint8_t>(mode));
    prefs.end();
  }
}

static inline int get_boot_mode() {
  Preferences prefs;
  uint8_t mode = MEDIA_MODE;
  if (prefs.begin(BOOT_NS, true)) {
    mode = prefs.getUChar(BOOT_KEY, MEDIA_MODE);
    prefs.end();
  }
  return mode;
}

static inline void clear_boot_mode() {
  set_boot_mode(MEDIA_MODE);
}

#endif // BOOT_MODE_H
