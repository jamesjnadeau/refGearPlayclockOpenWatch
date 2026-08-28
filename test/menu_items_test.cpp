// Host test for the menu's row list.
//
// PORTED FROM THE PARENT and much smaller, because the file is. Five of nine
// rows went with the radio and RefZone, and buildVisible()/slotOf() went with
// them -- every row here is unconditional, so there is no rebuild to survive.
// What is left to check is that every row has a label, that none of them
// overflows the buffer the menu draws into, and that the one row which reads a
// live value actually does.

#include "../src/RefMenuItems.h"
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

} // namespace

TEST(menu_items, every_row_has_a_label) {
    fresh();
    char buf[RefMenu::ITEM_LABEL_MAX];
    for (uint8_t i = 0; i < RefMenu::ITEM_COUNT; i++) {
        buf[0] = '\0';
        RefMenu::itemLabel(i, buf, sizeof(buf));
        ASSERT_TRUE(strlen(buf) > 0);
    }
}

TEST(menu_items, no_label_overflows_the_row_it_is_drawn_in) {
    // The label column holds about 14 characters of FreeSans12pt7b, which is
    // what MENU_LABEL_MAX_CHARS records -- see settings.h for the reasoning
    // behind it. A character count is a weak bound for a proportional
    // face and it is the only bound a host test can check; it exists to catch
    // a label that GROWS, not to prove one fits.
    //
    // THE PARENT'S "Sport: Base NCAA" FAILED THIS AT 16, which is how the row
    // came to be split into a label and a value in the first place.
    fresh();
    for (uint8_t s = 0; s < RefSport::count(); s++) {
        RefSport::setIndex(s);
        for (uint8_t i = 0; i < RefMenu::ITEM_COUNT; i++) {
            char buf[RefMenu::ITEM_LABEL_MAX];
            RefMenu::itemLabel(i, buf, sizeof(buf));
            ASSERT_TRUE(strlen(buf) < RefMenu::ITEM_LABEL_MAX);
            ASSERT_TRUE(strlen(buf) <= MENU_LABEL_MAX_CHARS);
        }
    }
}

TEST(menu_items, the_sport_row_reads_the_live_value) {
    fresh();
    char buf[RefMenu::ITEM_LABEL_MAX];

    // The label is fixed and the value is what moves. Both are drawn on the
    // same row, which is what keeps the menu a status display for the loaded
    // sport on a panel too narrow for the parent's single string.
    RefSport::setIndex(0);
    RefMenu::itemLabel(RefMenu::ITEM_SPORT, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "Sport") == 0);
    RefMenu::itemValue(RefMenu::ITEM_SPORT, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "Football") == 0);

    RefSport::setIndex(1);
    RefMenu::itemValue(RefMenu::ITEM_SPORT, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "Lacrosse") == 0);
}

TEST(menu_items, every_row_without_a_value_says_nothing) {
    // An empty string, not a stale one and not rubbish: the menu tests for
    // buf[0] to decide whether to draw a second column at all.
    fresh();
    for (uint8_t i = 0; i < RefMenu::ITEM_COUNT; i++) {
        char buf[RefMenu::ITEM_LABEL_MAX];
        memset(buf, 'x', sizeof(buf));
        RefMenu::itemValue(i, buf, sizeof(buf));
        if (i != RefMenu::ITEM_SPORT) {
            ASSERT_EQ(buf[0], '\0');
        } else {
            ASSERT_TRUE(strlen(buf) > 0);
        }
    }
}

TEST(menu_items, no_value_overflows_the_column_it_is_drawn_in) {
    // Values ride in the built-in 6 px face, so 128 px is 21 characters and
    // the label eats into that. The widest sport name is "Base NCAA" at 9.
    fresh();
    for (uint8_t s = 0; s < RefSport::count(); s++) {
        RefSport::setIndex(s);
        char buf[RefMenu::ITEM_LABEL_MAX];
        RefMenu::itemValue(RefMenu::ITEM_SPORT, buf, sizeof(buf));
        ASSERT_TRUE(strlen(buf) <= 9);
    }
}

TEST(menu_items, an_unknown_row_yields_an_empty_value_too) {
    char buf[RefMenu::ITEM_LABEL_MAX];
    memset(buf, 'x', sizeof(buf));
    RefMenu::itemValue(RefMenu::ITEM_COUNT, buf, sizeof(buf));
    ASSERT_EQ(buf[0], '\0');
}

TEST(menu_items, set_time_is_near_the_top_where_it_can_be_found) {
    // The most important row in this menu and the least often used -- about
    // once a year, and again after every full discharge. Someone who has not
    // touched it in a year has to find it without hunting.
    ASSERT_TRUE(RefMenu::ITEM_SET_TIME <= RefMenu::ITEM_ABOUT + 1);
    ASSERT_TRUE(RefMenu::ITEM_SET_TIME < RefMenu::ITEM_SPORT);
}

TEST(menu_items, an_unknown_row_yields_an_empty_string_not_rubbish) {
    char buf[RefMenu::ITEM_LABEL_MAX];
    memset(buf, 'x', sizeof(buf));
    RefMenu::itemLabel(RefMenu::ITEM_COUNT, buf, sizeof(buf));
    ASSERT_EQ(buf[0], '\0');
}

TEST(menu_items, a_short_buffer_is_still_terminated) {
    fresh();
    char buf[4];
    memset(buf, 'x', sizeof(buf));
    RefMenu::itemLabel(RefMenu::ITEM_EDIT_CUSTOM, buf, sizeof(buf));
    ASSERT_EQ(buf[sizeof(buf) - 1], '\0');
}

int main() { return runAllTests(); }
