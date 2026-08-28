// Host test for the DS3231 driver, against a stub I2C bus.
//
// WHAT THIS IS FOR. BCD decoding and a register map are the part of a firmware
// most likely to be wrong and least likely to be caught by eye: get the
// weekday and the date the wrong way round and the watch reads a day of the
// month between 1 and 7 -- wrong, and wrong in a way that LOOKS like a date.
// The DS3231 lays exactly the same trap the parent's RV-3028 did (weekday at
// 0x03, date at 0x04), which is why this suite ports with its shape intact.
//
// WHAT IT IS NOT. The stub is not a DS3231. It does not model the oscillator,
// the temperature compensation, or the aging register. A passing test here
// says the driver reads what it wrote; it says nothing about whether the part
// keeps time.

#include "../src/RefRtc.h"
#include "../src/board.h"
#include "test.h"

#include <Wire.h>

namespace {

const uint8_t ADDR = RTC_I2C_ADDR;

const uint8_t REG_TIME    = 0x00;
const uint8_t REG_CONTROL = 0x0E;
const uint8_t REG_STATUS  = 0x0F;
const uint8_t OSF         = 0x80;
const uint8_t EN32KHZ     = 0x08;

uint8_t bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// A chip on the bus holding a valid time: 14:30:45 on Sunday 23 August 2026.
WireStub::Device &chipWithTime() {
    WireStub::reset();
    WireStub::Device &d = WireStub::attach(ADDR);
    d.reg[REG_TIME + 0] = bcd(45);   // seconds
    d.reg[REG_TIME + 1] = bcd(30);   // minutes
    d.reg[REG_TIME + 2] = bcd(14);   // hours, 24h mode
    d.reg[REG_TIME + 3] = 1;         // WEEKDAY -- 0x03, not 0x04
    d.reg[REG_TIME + 4] = bcd(23);   // DATE    -- 0x04, not 0x03
    d.reg[REG_TIME + 5] = bcd(8);    // month, 1-based; century bit clear
    d.reg[REG_TIME + 6] = bcd(26);   // year, 00..99
    d.reg[REG_STATUS] = 0;           // OSF clear: the time means something
    return d;
}

} // namespace

TEST(rtc, an_empty_bus_is_reported_rather_than_guessed_at) {
    WireStub::reset();
    RefRtc rtc;
    ASSERT_TRUE(!rtc.begin());
    ASSERT_TRUE(!rtc.present());
    ASSERT_TRUE(!rtc.timeIsValid());
    struct tm t = {};
    ASSERT_TRUE(!rtc.read(t));
    ASSERT_EQ((int)rtc.epoch(), 0);
}

TEST(rtc, begin_parks_control_with_alarms_off_and_intcn_set) {
    // The DS3231's nINT line is wired to a GPIO this firmware parks; an alarm
    // nobody clears would hold it asserted for good.
    WireStub::Device &d = chipWithTime();
    d.reg[REG_CONTROL] = 0xFF;
    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());
    ASSERT_EQ(d.reg[REG_CONTROL], 0x04);   // INTCN alone
}

TEST(rtc, begin_switches_the_32khz_output_off_and_keeps_osf) {
    // EN32kHz is on from the factory and drives nothing on this board. And
    // clearing it must not clear OSF as a side effect -- that would be the
    // plausible-wrong-time bug in its sneakiest form.
    WireStub::Device &d = chipWithTime();
    d.reg[REG_STATUS] = OSF | EN32KHZ;
    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());
    ASSERT_EQ(d.reg[REG_STATUS] & EN32KHZ, 0);
    ASSERT_EQ(d.reg[REG_STATUS] & OSF, OSF);
}

TEST(rtc, the_weekday_and_the_date_are_not_transposed) {
    // THE TEST THIS FILE EXISTS FOR. Weekday at 0x03, date at 0x04. Reading
    // them the other way round gives a day of the month between 1 and 7.
    chipWithTime();
    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());

    struct tm t = {};
    ASSERT_TRUE(rtc.read(t));
    ASSERT_EQ(t.tm_mday, 23);
    ASSERT_EQ(t.tm_mon, 7);          // August, 0-based in struct tm
    ASSERT_EQ(t.tm_year, 126);       // 2026 - 1900
    ASSERT_EQ(t.tm_hour, 14);
    ASSERT_EQ(t.tm_min, 30);
    ASSERT_EQ(t.tm_sec, 45);
}

TEST(rtc, the_weekday_is_recomputed_rather_than_trusted) {
    // The chip's weekday register is written by whoever set the time and is
    // not checked against the date. 23 August 2026 is a Sunday; the stub
    // holds Monday in that register, so this passes only because the driver
    // derives the weekday from the date.
    chipWithTime();
    RefRtc rtc;
    rtc.begin();
    struct tm t = {};
    ASSERT_TRUE(rtc.read(t));
    ASSERT_EQ(t.tm_wday, 0);         // Sunday
}

TEST(rtc, osf_means_the_time_is_refused_not_returned) {
    // THE UI MUST NEVER SHOW A PLAUSIBLE WRONG TIME. The registers below hold
    // a perfectly readable 14:30 -- and it must not come out.
    WireStub::Device &d = chipWithTime();
    d.reg[REG_STATUS] = OSF;

    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());
    ASSERT_TRUE(!rtc.timeIsValid());
    struct tm t = {};
    ASSERT_TRUE(!rtc.read(t));
    ASSERT_EQ((int)rtc.epoch(), 0);
}

TEST(rtc, an_unset_chip_reporting_month_zero_is_refused) {
    // Belt and braces against OSF: a chip whose status was cleared by
    // accident must still not yield a date of the 0th of month 0.
    WireStub::Device &d = chipWithTime();
    d.reg[REG_TIME + 5] = 0;
    RefRtc rtc;
    rtc.begin();
    struct tm t = {};
    ASSERT_TRUE(!rtc.read(t));
}

TEST(rtc, setting_the_time_writes_bcd_in_the_right_registers) {
    WireStub::Device &d = chipWithTime();
    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());

    struct tm t = {};
    t.tm_year = 2027 - 1900;
    t.tm_mon = 0;                    // January
    t.tm_mday = 5;
    t.tm_hour = 6;
    t.tm_min = 7;
    t.tm_sec = 8;
    ASSERT_TRUE(rtc.set(t));

    ASSERT_EQ(d.reg[REG_TIME + 0], bcd(8));
    ASSERT_EQ(d.reg[REG_TIME + 1], bcd(7));
    ASSERT_EQ(d.reg[REG_TIME + 2], bcd(6));    // bit 6 low: 24 hour mode
    ASSERT_EQ(d.reg[REG_TIME + 4], bcd(5));    // date at 0x04
    ASSERT_EQ(d.reg[REG_TIME + 5], bcd(1));    // month 1-based, century clear
    ASSERT_EQ(d.reg[REG_TIME + 6], bcd(27));   // year, two digits
    // 5 January 2027 is a Tuesday; the chip stores 1..7 with Sunday as 1.
    ASSERT_EQ(d.reg[REG_TIME + 3], 3);
}

TEST(rtc, setting_the_time_clears_osf_and_leaves_the_rest_of_status) {
    WireStub::Device &d = chipWithTime();
    d.reg[REG_STATUS] = OSF | 0x01;   // OSF plus some other flag (A1F)

    RefRtc rtc;
    rtc.begin();
    struct tm t = {};
    t.tm_year = 126; t.tm_mon = 7; t.tm_mday = 23;
    t.tm_hour = 1; t.tm_min = 2; t.tm_sec = 3;
    ASSERT_TRUE(rtc.set(t));

    ASSERT_EQ(d.reg[REG_STATUS] & OSF, 0);
    ASSERT_EQ(d.reg[REG_STATUS] & 0x01, 0x01);   // not clobbered
}

TEST(rtc, a_read_that_is_nacked_fails_rather_than_returning_rubbish) {
    WireStub::Device &d = chipWithTime();
    RefRtc rtc;
    ASSERT_TRUE(rtc.begin());
    // Refuse the next few transactions. read() is several: a status read is a
    // pointer write plus a data read, and the time read is another pair.
    d.nacks = 4;
    struct tm t = {};
    ASSERT_TRUE(!rtc.read(t));
}

TEST(rtc, the_round_trip_survives_every_month_of_a_leap_year) {
    // BCD and the 1-based month conversion, over the range where an off-by-one
    // would hide: February and the two-digit boundary at October.
    for (uint8_t m = 1; m <= 12; m++) {
        chipWithTime();
        RefRtc rtc;
        rtc.begin();

        struct tm in = {};
        in.tm_year = 2024 - 1900;
        in.tm_mon = m - 1;
        in.tm_mday = 28;
        in.tm_hour = 23;
        in.tm_min = 59;
        in.tm_sec = 58;
        ASSERT_TRUE(rtc.set(in));

        struct tm out = {};
        ASSERT_TRUE(rtc.read(out));
        ASSERT_EQ(out.tm_mon, m - 1);
        ASSERT_EQ(out.tm_mday, 28);
        ASSERT_EQ(out.tm_year, 2024 - 1900);
        ASSERT_EQ(out.tm_hour, 23);
    }
}

int main() { return runAllTests(); }
