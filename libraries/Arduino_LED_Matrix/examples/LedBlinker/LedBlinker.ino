#include "Arduino.h"

void (*toggle_led_fn)(int) = NULL;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  void *lib = extmgr_load("LedToggleLib");
  if (lib) {
    toggle_led_fn = (void (*)(int))extmgr_find_sym(lib, "toggle_led");
  }
  if (toggle_led_fn) {
    printk("LedBlinker: toggle_led resolved @ %p\n", toggle_led_fn);
  } else {
    printk("LedBlinker: toggle_led not found\n");
  }
}

void loop() {
  if (toggle_led_fn) {
    toggle_led_fn(LED_BUILTIN);
  }
  delay(500);
}
