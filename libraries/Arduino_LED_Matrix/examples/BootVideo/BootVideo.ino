#include "Arduino_LED_Matrix.h"
#include "bad_apple.h"
#include <zephyr/drivers/gpio.h>

Arduino_LED_Matrix matrix;

const struct gpio_dt_spec mpu_ready =
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), control_gpios, 0);

void setup() {
  matrix.begin();
  matrix.setGrayscaleBits(8);
}

void loop() {
  if (gpio_pin_get_dt(&mpu_ready) == 0) {
    matrix.playVideo(bad_apple, bad_apple_len);
  } else {
    uint8_t blank[104] = {0};
    matrix.draw(blank);
    delay(10);
    matrix.end();
    sketch_exit = true;
  }
}
