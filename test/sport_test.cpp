// Host test for the sport preset table.
//
// PORTED FROM THE PARENT with its storage swapped. The parent compiled the
// real source against a stub Preferences; this compiles the real source
// against RefStore's host backend, which is the same serialisation the watch
// runs -- only the three byte-level functions differ. Everything the parent
// asserted is here: the table's values, the label lengths the menu rows are
// laid out around, the clamps on the way in and on the way out, and the
// survival of a restart.

#include "../src/RefSport.h"
#include "../src/RefStore.h"
#include "../src/settings.h"
#include "test.h"

#include <string.h>

namespace {

void fresh() {
    RefStore::begin();
    RefStore::clear();
    RefStore::begin();
    RefSport::begin();
}

int find(const char *name) {
    for (uint8_t i = 0; i < RefSport::count(); i++) {
        if (strcmp(RefSport::preset(i).name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void expectPreset(const char *name, int lng, int shrt, int w1, int w2,
                  int fin) {
    const int i = find(name);
    ASSERT_TRUE(i >= 0);
    if (i < 0) {
        return;
    }
    const RefSport::Preset p = RefSport::preset((uint8_t)i);
    ASSERT_EQ(p.longSeconds, lng);
    ASSERT_EQ(p.shortSeconds, shrt);
    ASSERT_EQ(p.warnAtSeconds, w1);
    ASSERT_EQ(p.warn2AtSeconds, w2);
    ASSERT_EQ(p.finalCountdownFrom, fin);
}

} // namespace

TEST(sport, the_table_is_what_the_spec_says) {
    fresh();
    ASSERT_EQ(RefSport::count(), 7);
    expectPreset("Football",  40, 25, 10,  0, 5);
    expectPreset("Lacrosse", 120, 20, 30, 10, 5);
    expectPreset("Base NCAA",120, 20, 30, 10, 5);
    expectPreset("Base NFHS", 80, 20, 30, 10, 5);
    expectPreset("Soft NCAA", 90, 20, 30, 10, 5);
    expectPreset("Soft NFHS", 60, 20, 20, 10, 5);
}

TEST(sport, every_label_fits_the_row_it_is_drawn_in) {
    // Menu rows are laid out on the assumption these stay short. A name that
    // grows past nine glyphs does not error -- it is clipped, on the screen
    // that tells you which sport is loaded.
    for (uint8_t i = 0; i < RefSport::count(); i++) {
        const RefSport::Preset p = RefSport::preset(i);
        ASSERT_TRUE(strlen(p.name) <= 9);
        ASSERT_TRUE(strlen(p.description) <= 14);
    }
}

TEST(sport, nothing_stored_means_the_settings_h_default) {
    fresh();
    ASSERT_TRUE(strcmp(RefSport::active().name, DEFAULT_SPORT) == 0);
    ASSERT_TRUE(RefSport::isCustom(RefSport::count() - 1));
    ASSERT_TRUE(!RefSport::isCustom(0));
}

TEST(sport, an_index_off_the_end_falls_back_rather_than_reading_past_it) {
    fresh();
    ASSERT_TRUE(strcmp(RefSport::preset(200).name, "Football") == 0);
    RefSport::setIndex(200);
    ASSERT_TRUE(strcmp(RefSport::active().name, "Football") == 0);
}

TEST(sport, setCustom_clamps_every_field) {
    fresh();
    // A clock of 0 would expire the instant it started; a value over 199 is
    // one the display cannot draw.
    RefSport::setCustom(0, 9000, 9000, 0, 9000);
    ASSERT_EQ(RefSport::custom().longSeconds, 1);
    ASSERT_EQ(RefSport::custom().shortSeconds, RefSport::MAX_SECONDS);
    ASSERT_EQ(RefSport::custom().warnAtSeconds, RefSport::MAX_SECONDS);
    ASSERT_EQ(RefSport::custom().warn2AtSeconds, 0);   // 0 turns a mark off
    ASSERT_EQ(RefSport::custom().finalCountdownFrom, RefSport::MAX_SECONDS);
}

TEST(sport, the_chosen_sport_survives_a_restart) {
    fresh();
    const int i = find("Soft NFHS");
    ASSERT_TRUE(i >= 0);
    RefSport::setIndex((uint8_t)i);

    RefStore::begin();      // the reboot
    RefSport::begin();
    ASSERT_TRUE(strcmp(RefSport::active().name, "Soft NFHS") == 0);
}

TEST(sport, the_custom_slot_survives_a_restart_and_a_reflash) {
    fresh();
    RefSport::setCustom(90, 30, 20, 8, 3);
    const int i = find("Custom");
    ASSERT_TRUE(i >= 0);
    RefSport::setIndex((uint8_t)i);

    RefStore::begin();
    RefSport::begin();
    ASSERT_TRUE(strcmp(RefSport::active().name, "Custom") == 0);
    ASSERT_EQ(RefSport::active().longSeconds, 90);
    ASSERT_EQ(RefSport::custom().shortSeconds, 30);
    ASSERT_EQ(RefSport::custom().warnAtSeconds, 20);
    ASSERT_EQ(RefSport::custom().warn2AtSeconds, 8);
    ASSERT_EQ(RefSport::custom().finalCountdownFrom, 3);
}

TEST(sport, a_stored_value_out_of_range_clamps_on_the_way_IN) {
    // The parent's version of this planted values straight into NVS, bypassing
    // setCustom() entirely, to exercise the READ path rather than the write
    // path. Same idea here: a store written by an older build, or corrupted,
    // must not produce a clock of 0.
    fresh();
    RefStore::set(RefStore::KEY_CUSTOM_LONG, 0);
    RefStore::set(RefStore::KEY_CUSTOM_WARN, 9000);
    RefStore::set(RefStore::KEY_SPORT, 200);
    RefStore::commit();

    RefStore::begin();
    RefSport::begin();
    ASSERT_EQ(RefSport::custom().longSeconds, RefSport::MIN_CLOCK_SECONDS);
    ASSERT_EQ(RefSport::custom().warnAtSeconds, RefSport::MAX_SECONDS);
    ASSERT_TRUE(strcmp(RefSport::active().name, "Football") == 0);
}

TEST(sport, saving_a_custom_preset_is_one_page_erase_not_five) {
    // The parent wrote five NVS keys separately and the ESP32's wear levelling
    // made that harmless. Here each one would be an erase cycle of a page
    // rated for ten thousand.
    fresh();
    const uint32_t before = RefStore::writeCount();
    RefSport::setCustom(55, 35, 15, 5, 2);
    ASSERT_EQ((int)(RefStore::writeCount() - before), 1);
}

int main() { return runAllTests(); }
