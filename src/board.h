// What the pins ARE and what they MEAN, for the Open-Smartwatch Light.
//
// UNLIKE THE PARENT, THIS FILE IS NOT SPLIT IN TWO. The parent generated its
// pin numbers from its own board's netlist and kept the meanings separate so
// the generated half could be regenerated. This project targets somebody
// else's board -- the Open-Smartwatch Light edition V3.3 -- so the numbers
// below are transcribed from that project's platform header
// (include/platform/LIGHT_EDITION_V3_3.h in open-smartwatch-os) and the
// meanings were read from its HAL sources. One file, one authority, and
// board_test asserts the structural claims a host can check.
//
//     https://open-smartwatch.github.io/

#pragma once

// Arduino first, for HIGH/LOW and the pin mode names. On the target that is
// the ESP32 core's; under test/run.sh it is test/stub/Arduino.h. This header
// must be self-sufficient -- a board.h that only compiles when something else
// included Arduino.h first is a board.h the host tests cannot use.
#include <Arduino.h>

// ---- buttons ---------------------------------------------------------------
// THREE BUTTONS, WHERE THE PARENT HAD FOUR. Positions and electrical facts
// straight from open-smartwatch-os (osw_pins.h and hal/buttons.cpp):
//
//                          +--------+ ---- UP     (GPIO 13, top right)
//     SELECT (GPIO 0) ---- |  240   |
//     bottom left          |  x 240 | ---- DOWN   (GPIO 10, bottom right)
//                          +--------+
//
//   SELECT is the ESP32's BOOT strap: pulled up on the module, a press reads
//   LOW. UP and DOWN have external pull-DOWNS on the OSW board and a press
//   reads HIGH. So unlike the parent there is no single BTN_PRESSED level;
//   each button carries its own, and Buttons.cpp reads the table.
//
//   The OSW HAL configures all three as plain INPUT -- every pull the buttons
//   need is already on the board -- and so does this firmware.
#define PIN_BTN_SELECT   0
#define PIN_BTN_UP       13
#define PIN_BTN_DOWN     10

#define BTN_SELECT_ACTIVE  LOW
#define BTN_UP_ACTIVE      HIGH
#define BTN_DOWN_ACTIVE    HIGH

#define BTN_MODE           INPUT

// THE FOURTH CORNER IS RESET, AND FIRMWARE CANNOT HAVE IT. The OSW's
// top-left button is wired to the ESP32's EN line: pressing it hard-resets
// the chip. It never reaches a GPIO, so it cannot be read, remapped,
// debounced or given a role -- the parent's top-left sleep button has no pin
// to land on. What this firmware CAN do is make that reset harmless: the
// sport and the custom preset live in NVS, the time lives in the DS3231, so
// a reset (accidental or deliberate) boots straight back to the ready screen
// with everything but a running countdown intact.
//
// BUTTON ROLES ARE ASSIGNED BY POSITION, NOT BY GPIO NUMBER, exactly as the
// parent did it. The two right-hand buttons keep their parent roles -- top
// right starts the long clock, bottom right the short one -- and the bottom
// left button keeps menu/clear, with sleep as its LONG (five second) hold:
// see SLEEP_HOLD_MS in settings.h and the loop in main.cpp.
#define BTN_LONG_TIMER_PIN     PIN_BTN_UP      // top right    -> long clock
#define BTN_LONG_TIMER_ACTIVE  BTN_UP_ACTIVE
#define BTN_SHORT_TIMER_PIN    PIN_BTN_DOWN    // bottom right -> short clock
#define BTN_SHORT_TIMER_ACTIVE BTN_DOWN_ACTIVE
#define BTN_SELECT_PIN         PIN_BTN_SELECT  // bottom left  -> menu / clear / sleep
#define BTN_SELECT_PIN_ACTIVE  BTN_SELECT_ACTIVE

// Where each button physically sits, so board_test can assert the roles landed
// on the corners the comments claim. Same scheme as the parent's generated
// header.
#define BTN_UP_IS_TOP       1
#define BTN_UP_IS_RIGHT     1
#define BTN_DOWN_IS_TOP     0
#define BTN_DOWN_IS_RIGHT   1
#define BTN_SELECT_IS_TOP   0
#define BTN_SELECT_IS_RIGHT 0

// ---- display ---------------------------------------------------------------
// A GC9A01 round TFT, 240 x 240, on VSPI. The OSW's wiring happens to match
// the ESP32's default VSPI pins exactly (SCK 18, MOSI 23, CS 5), so SPI.begin()
// with no arguments finds it. There is no MISO -- the panel is write-only.
//
// THIS IS THE OPPOSITE KIND OF PANEL FROM THE PARENT'S. The Sharp memory LCD
// was reflective, static-friendly and owed a VCOM inversion every second; the
// GC9A01 is emissive, needs a BACKLIGHT to be visible at all, and owes the
// firmware nothing while it sits still. RefPanel owns the consequences: the
// framebuffer lives in the MCU (a 1-bit GFX canvas -- this watch is black and
// white), refresh() pushes it whole, and blank()/unblank() are the panel's
// sleep-in/sleep-out commands plus the backlight, not a DISP line.
#define PIN_LCD_CS       5
#define PIN_LCD_DC       12
#define PIN_LCD_RST      33
#define PIN_LCD_SCK      18
#define PIN_LCD_MOSI     23
#define PIN_LCD_BL       9     // backlight, PWM via LEDC

#define LCD_WIDTH        240
#define LCD_HEIGHT       240
#define LCD_SPI_HZ       27000000   // what the OSW OS runs this panel at

// ---- I2C -------------------------------------------------------------------
// One bus, shared with the OSW's accelerometer (which this firmware does not
// touch). The RTC is a DS3231MZ -- same I2C register map as every DS3231.
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22
#define RTC_I2C_ADDR     0x68   // DS3231, 7-bit

// The DS3231's nINT/SQW is wired to GPIO 32 with a pull-up. This firmware
// never programs an alarm, so the pin is parked as an input and left alone;
// it is here so nothing else claims it.
#define PIN_RTC_INT      32
#define RTC_INT_ASSERTED LOW

// ---- haptics ---------------------------------------------------------------
// THE LIGHT EDITION HAS NO MOTOR. open-smartwatch-os guards every vibration
// call behind OSW_PLATFORM_HARDWARE_VIBRATE, and the Light's platform header
// leaves it unset. PIN_VIB 0 is this project's spelling of the same guard:
// Buzzer.cpp compiles to a no-op and main.cpp says so once at boot. An
// edition with a motor sets this to its GPIO and gets the parent's tick /
// warn / expire patterns back, driven as timed pulses.
#define PIN_VIB          0
#define VIB_ACTIVE       HIGH

// ---- power -----------------------------------------------------------------
// TPS2115A power-mux STAT: HIGH while the watch is running from external
// (USB) power, which is the closest thing this board has to the parent's
// charger STAT line. Reading HIGH here is what BattGuard's `charging` input
// means; like the parent's MCP73832, it cannot distinguish "charging" from
// "charged, still plugged in", and does not need to.
#define PIN_STAT_PWR     15
#define CHRG_ACTIVE      HIGH

// ---- battery sense ---------------------------------------------------------
// GPIO 25 (ADC2_CH8) behind the OSW's divider. *** THE SCALE OF THIS READING
// IS UNVERIFIED, AND THE SHIPPED OSW FIRMWARE DOES NOT TRUST IT EITHER: *** it
// never converts to volts at all -- it self-calibrates raw counts against the
// lowest and highest values it has ever seen. This port keeps the parent's
// volts-based gauge and guard, reads the pin through the ESP32 core's
// calibrated analogReadMilliVolts(), and states plainly that BATT_DIVIDER is
// a guess until someone measures the divider against a bench meter. The
// reason that guess is safe to ship is BattGuard's arming rule: a reading
// that never looks like a healthy cell can warn, but can never sleep the
// watch. See BattGuard.h.
//
// ADC2 note: ADC2 is unusable while Wi-Fi runs. This firmware never starts
// Wi-Fi -- there is nothing to sync (the parent deleted its radio on purpose
// and this port inherits the decision) -- so the channel is always available.
#define PIN_BATT_ADC     25
#define BATT_DIVIDER     2.0f
