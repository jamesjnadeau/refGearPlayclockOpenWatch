#include "RefPanel.h"

#include <Adafruit_GC9A01A.h>
#include <SPI.h>

#include "board.h"
#include "settings.h"

// The lib defines these; the guards are here so a rename upstream fails to a
// working value rather than to a build error in a file nobody expects it in.
#ifndef GC9A01A_SLPIN
#define GC9A01A_SLPIN 0x10
#endif
#ifndef GC9A01A_SLPOUT
#define GC9A01A_SLPOUT 0x11
#endif
#ifndef GC9A01A_DISPOFF
#define GC9A01A_DISPOFF 0x28
#endif
#ifndef GC9A01A_DISPON
#define GC9A01A_DISPON 0x29
#endif
#ifndef GC9A01A_BLACK
#define GC9A01A_BLACK 0x0000
#endif
#ifndef GC9A01A_WHITE
#define GC9A01A_WHITE 0xFFFF
#endif

namespace RefPanel {

GFXcanvas1 display(LCD_WIDTH, LCD_HEIGHT);

namespace {

// Hardware SPI on the default VSPI pins, which the OSW's wiring matches
// exactly (board.h). No MISO: the panel is write-only.
Adafruit_GC9A01A tft(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);

// LEDC channel for the backlight. Any free channel; the OSW OS uses low
// channels for the same job.
const uint8_t  BL_CHANNEL = 1;
const uint32_t BL_FREQ_HZ = 5000;
const uint8_t  BL_BITS    = 8;

void backlight(uint8_t level) { ledcWrite(BL_CHANNEL, level); }

} // namespace

void begin() {
  tft.begin(LCD_SPI_HZ);
  tft.setRotation(0);   // the OSW's own rotation for this edition

  // First frame before first light: the canvas starts zeroed (all black), so
  // pushing it now guarantees the backlight never illuminates init noise.
  refresh();

  ledcSetup(BL_CHANNEL, BL_FREQ_HZ, BL_BITS);
  ledcAttachPin(PIN_LCD_BL, BL_CHANNEL);
  backlight(BACKLIGHT_LEVEL);
}

void refresh() {
  // Expand 1-bit canvas rows into 16-bit pixels and stream the lot in one
  // transaction. The canvas packs each row MSB-first at 30 bytes per row
  // (240/8 exactly), which is what the inner loop walks.
  const uint8_t *buf = display.getBuffer();
  static uint16_t line[LCD_WIDTH];

  tft.startWrite();
  tft.setAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
  for (int16_t y = 0; y < LCD_HEIGHT; y++) {
    const uint8_t *row = buf + (size_t)y * (LCD_WIDTH / 8);
    for (int16_t x = 0; x < LCD_WIDTH; x++) {
      const bool lit = row[x >> 3] & (0x80 >> (x & 7));
      line[x] = lit ? GC9A01A_WHITE : GC9A01A_BLACK;
    }
    tft.writePixels(line, LCD_WIDTH);
  }
  tft.endWrite();
}

void blank() {
  // Backlight first: the panel is invisible the instant the hold registers,
  // which is the parent's "the blank IS the sleep indication" behaviour. Then
  // the controller itself sleeps -- worth a milliamp or so of its own.
  backlight(0);
  tft.sendCommand(GC9A01A_DISPOFF);
  tft.sendCommand(GC9A01A_SLPIN);
  delay(5);   // datasheet: wait before stopping the interface clock
}

void unblank() {
  tft.sendCommand(GC9A01A_SLPOUT);
  delay(120); // datasheet: sleep-out needs up to 120 ms before display-on
  tft.sendCommand(GC9A01A_DISPON);
  // The caller repaints before anyone could notice, but the canvas is intact
  // regardless -- light the backlight only after the panel is displaying it.
  backlight(BACKLIGHT_LEVEL);
}

} // namespace RefPanel
