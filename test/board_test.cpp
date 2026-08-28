// The pin map is the one thing in the firmware that MUST agree with the
// board. A mismatch is a device that boots and does nothing, with no error
// anywhere.
//
// UNLIKE THE PARENT, THIS PROJECT DOES NOT OWN ITS BOARD, so there is no
// generator writing the numbers out of a netlist -- board.h transcribes them
// from open-smartwatch-os's platform header for LIGHT_EDITION_V3_3, and this
// file is the tripwire: the values below are what that header said when the
// port was made. If board.h is ever edited -- for a new OSW revision, or a
// different edition -- this fails, which is the moment to go and look at what
// else assumed the old pin.

#include "../src/board.h"
#include "test.h"

TEST(board, display_pins_match_the_osw_platform_header) {
    ASSERT_EQ(PIN_LCD_CS,   5);
    ASSERT_EQ(PIN_LCD_DC,   12);
    ASSERT_EQ(PIN_LCD_RST,  33);
    ASSERT_EQ(PIN_LCD_SCK,  18);
    ASSERT_EQ(PIN_LCD_MOSI, 23);
    ASSERT_EQ(PIN_LCD_BL,   9);
}

TEST(board, i2c_pins_match_the_osw_platform_header) {
    ASSERT_EQ(PIN_I2C_SDA, 21);
    ASSERT_EQ(PIN_I2C_SCL, 22);
}

TEST(board, button_pins_match_the_osw_platform_header) {
    ASSERT_EQ(PIN_BTN_SELECT, 0);    // the BOOT strap; presses LOW
    ASSERT_EQ(PIN_BTN_UP,     13);
    ASSERT_EQ(PIN_BTN_DOWN,   10);
}

TEST(board, every_pin_is_used_exactly_once) {
    const int pins[] = {
        PIN_LCD_CS,     PIN_LCD_DC,   PIN_LCD_RST, PIN_LCD_SCK,
        PIN_LCD_MOSI,   PIN_LCD_BL,   PIN_BTN_SELECT, PIN_BTN_UP,
        PIN_BTN_DOWN,   PIN_I2C_SDA,  PIN_I2C_SCL, PIN_RTC_INT,
        PIN_STAT_PWR,   PIN_BATT_ADC,
    };
    const int n = sizeof(pins) / sizeof(pins[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(pins[i], pins[j]);
        }
    }
}

// Every pin has to exist on an ESP32 at all, and none may land on the
// SPI-flash pins the module owes to its own flash chip (6..11, except that
// the OSW genuinely uses 10 for a button -- the PICO-D4 package frees it).
TEST(board, every_pin_is_a_real_esp32_gpio) {
    const int pins[] = {
        PIN_LCD_CS,     PIN_LCD_DC,   PIN_LCD_RST, PIN_LCD_SCK,
        PIN_LCD_MOSI,   PIN_LCD_BL,   PIN_BTN_SELECT, PIN_BTN_UP,
        PIN_BTN_DOWN,   PIN_I2C_SDA,  PIN_I2C_SCL, PIN_RTC_INT,
        PIN_STAT_PWR,   PIN_BATT_ADC,
    };
    for (int p : pins) {
        ASSERT_TRUE(p >= 0 && p <= 39);
        ASSERT_TRUE(p < 34 || p == PIN_BATT_ADC || p == PIN_RTC_INT);
        // 34..39 are input-only; nothing this firmware drives may sit there.
    }
    ASSERT_TRUE(PIN_LCD_BL < 34);
    ASSERT_TRUE(PIN_LCD_CS < 34);
}

// The RTC address, distinct and legal. One device this firmware talks to.
TEST(board, the_i2c_address_is_legal) {
    ASSERT_EQ(RTC_I2C_ADDR, 0x68);   // every DS3231 ever made
    ASSERT_TRUE(RTC_I2C_ADDR > 0x07 && RTC_I2C_ADDR < 0x78);
}

// Active levels, asserted because getting one backwards produces silence
// rather than an error. The one that catches people here: the three buttons
// DO NOT SHARE a pressed level.
TEST(board, active_levels_are_what_the_osw_schematic_implies) {
    ASSERT_EQ(BTN_SELECT_ACTIVE, LOW);    // BOOT strap, pulled up
    ASSERT_EQ(BTN_UP_ACTIVE, HIGH);       // external pull-downs
    ASSERT_EQ(BTN_DOWN_ACTIVE, HIGH);
    ASSERT_EQ(RTC_INT_ASSERTED, LOW);     // open-drain nINT, pulled up
    ASSERT_EQ(CHRG_ACTIVE, HIGH);         // TPS2115A STAT: high on USB power
}

// The divider guess, pinned so a change to it is a decision rather than a
// drift. board.h says in as many words that it is unverified.
TEST(board, the_battery_divider_guess_is_what_board_h_documents) {
    ASSERT_EQ((int)(BATT_DIVIDER * 10), 20);
}

// The Light edition has no motor, and the Buzzer must know it: a PIN_VIB
// that grew a value without a board to match would drive a GPIO that is
// wired to something else entirely.
TEST(board, the_light_edition_has_no_motor) {
    ASSERT_EQ(PIN_VIB, 0);
}

int main() {
    return runAllTests();
}
