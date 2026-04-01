#include "Arduino.h"
#include <zephyr/llext/symbol.h>

extern "C" void toggle_led(int pin) {
  digitalWrite(pin, !digitalRead(pin));
}
LL_EXTENSION_SYMBOL(toggle_led);
