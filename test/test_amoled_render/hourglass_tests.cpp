#include <cstdio>
#include <unity.h>
#include "Panel.h"
#include "ui/Ui.h"

namespace {
    void test_hourglass_pixels_and_outline_work() {
        // Pixel hashes captured before hoisting geometry or rejecting off-strip outline segments.
        constexpr uint32_t expected[]{3633032551U, 2330481835U, 3893509525U, 240183267U,  607977099U,  3948901819U,
                                      1477843077U, 1187747883U, 2011822209U, 3368656621U, 2918503029U, 650440771U,
                                      2559327735U, 170734571U,  3456941917U, 655999027U,  3930342283U, 4264805595U,
                                      3659012633U, 3421996811U, 1434021205U, 3835014161U, 933572217U,  3839381707U};
        size_t sample = 0;
        for (const auto rotation: {ui::Orientation::Portrait, ui::Orientation::Landscape}) {
            for (const bool wide: {false, true}) {
                for (const int state: {0, 1, 2, 3, 4, 5}) {
                    testgfx::Panel panel{160, 180};
                    ui::Context ui{panel};
                    const auto theme = ui::themes::defaultTheme();
                    ui.setTheme(theme);
                    ui.setOrientation(rotation);
                    const ui::Rect rect{8, 8, static_cast<int16_t>(wide ? 120 : 80),
                                        static_cast<int16_t>(wide ? 80 : 120)};
                    constexpr uint16_t progress[]{0, 1, 333, 500, 999, 1000};
                    ui.beginFrame(1);
                    Arduino_GFX::allDrawLines = 0;
                    ui.hourglass(rect, progress[state], state == 2, state == 5, ui::themes::Accent, state == 3,
                                 "12:34");
                    ui.endFrame();
                    uint32_t hash = Fnv1a::kOffsetBasis;
                    for (const uint16_t pixel: panel.pixels) {
                        hash = (hash ^ (pixel & 0xff)) * Fnv1a::kPrime;
                        hash = (hash ^ (pixel >> 8)) * Fnv1a::kPrime;
                    }
                    std::printf("HOURGLASS %d %d %d %u %llu\n", static_cast<int>(rotation), wide, state, hash,
                                static_cast<unsigned long long>(Arduino_GFX::allDrawLines));
                    TEST_ASSERT_EQUAL_UINT32(expected[sample++], hash);
                    if (wide)
                        TEST_ASSERT_LESS_THAN_UINT64(1500, Arduino_GFX::allDrawLines);
                    TEST_ASSERT_EQUAL(0, panel.invalidWindows);
                }
            }
        }
    }
} // namespace

void runAmoledHourglassTests() {
    RUN_TEST(test_hourglass_pixels_and_outline_work);
}
