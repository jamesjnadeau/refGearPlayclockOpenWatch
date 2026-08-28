// A test framework small enough to read in one sitting.
//
// Deliberately not gtest or Unity: this harness has to compile with nothing
// but g++ on a machine that may never have seen an embedded toolchain, and
// the whole point of it is to run before any hardware exists.

#pragma once

#include <cstdio>
#include <cstring>
#include <vector>

struct TestCase {
    const char *suite;
    const char *name;
    void (*fn)();
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int &failures() {
    static int n = 0;
    return n;
}

inline const char *&current() {
    static const char *name = "";
    return name;
}

struct Registrar {
    Registrar(const char *suite, const char *name, void (*fn)()) {
        registry().push_back({suite, name, fn});
    }
};

#define TEST(suite, name)                                                     \
    static void suite##_##name();                                             \
    static Registrar suite##_##name##_reg(#suite, #name, suite##_##name);     \
    static void suite##_##name()

#define FAIL_AT(fmt, ...)                                                     \
    do {                                                                      \
        std::printf("  FAIL %s\n    %s:%d: " fmt "\n", current(),             \
                    __FILE__, __LINE__, __VA_ARGS__);                         \
        failures()++;                                                         \
    } while (0)

#define ASSERT_EQ(a, b)                                                       \
    do {                                                                      \
        if ((a) != (b)) {                                                     \
            FAIL_AT("%s == %s, but %d != %d", #a, #b, (int)(a), (int)(b));    \
        }                                                                     \
    } while (0)

#define ASSERT_NE(a, b)                                                       \
    do {                                                                      \
        if ((a) == (b)) {                                                     \
            FAIL_AT("%s != %s, but both are %d", #a, #b, (int)(a));           \
        }                                                                     \
    } while (0)

#define ASSERT_TRUE(a)                                                        \
    do {                                                                      \
        if (!(a)) {                                                           \
            std::printf("  FAIL %s\n    %s:%d: %s is false\n", current(),      \
                        __FILE__, __LINE__, #a);                              \
            failures()++;                                                     \
        }                                                                     \
    } while (0)

inline int runAllTests() {
    const char *suite = "";
    for (const TestCase &t : registry()) {
        if (std::strcmp(suite, t.suite) != 0) {
            suite = t.suite;
            std::printf("%s\n", suite);
        }
        current() = t.name;
        int before = failures();
        t.fn();
        if (failures() == before) {
            std::printf("  ok   %s\n", t.name);
        }
    }
    std::printf("\n%zu test(s), %d failure(s)\n", registry().size(),
                failures());
    return failures() == 0 ? 0 : 1;
}
