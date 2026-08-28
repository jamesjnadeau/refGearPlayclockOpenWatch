#include "LowPower.h"

#include <Arduino.h>

#include "esp_sleep.h"

namespace LowPower {

void begin() {
  // 80 MHz: the lowest frequency that keeps the APB -- and with it the SPI
  // clock the panel push is derived from -- at full speed. The OSW OS
  // defaults to 240 MHz for its animated UI; a play clock repainting once a
  // second has no use for the other 160.
  setCpuFrequencyMhz(80);
}

void stopUntilInterrupt() {
  esp_light_sleep_start();
}

} // namespace LowPower
