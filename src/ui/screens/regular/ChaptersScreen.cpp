#include "ui/screens/ChaptersScreen.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>

#include "ui/screens/ScreenCommon.h"

namespace screens {

    namespace {

        constexpr int16_t kHeaderHeight = 32;
        constexpr int16_t kRowStep = 30;
        constexpr int16_t kDragThreshold = 5;
        constexpr int32_t kMaximumVelocity = 400'000;
        constexpr uint32_t kAccelerationMs = 450;
        constexpr int32_t kScrollScale = 1'000'000;

    } // namespace

    Action ChaptersScreen::draw(ui::Context& ui, std::span<const ChapterMarker> chapters, ReadingSession& reader,
                                const settings::ReadingSettings& settings, uint32_t nowMs, Screen& screen) {
        if (const Action action = detail::navigation(ui, Screen::Chapters, screen); action != Action::None)
            return action;

        const ui::Rect content = detail::tabContent(ui);
        if (ui.button({content.x, content.y, 64, kHeaderHeight}, "<<")) {
            screen = Screen::Read;
            dragging_ = false;
            return Action::None;
        }

        size_t readingIndex = 0;
        while (readingIndex + 1 < chapters.size() && chapters[readingIndex + 1].wordIndex <= reader.state.wordIndex) {
            ++readingIndex;
        }

        if (source_.data() != chapters.data() || source_.size() != chapters.size()) {
            source_ = chapters;
            centeredIndex_ = readingIndex;
            offset_ = 0;
            dragging_ = false;
        }

        char position[24];
        std::snprintf(position, sizeof(position), "%u / %u",
                      static_cast<unsigned>(chapters.empty() ? 0 : centeredIndex_ + 1),
                      static_cast<unsigned>(chapters.size()));
        const int16_t positionWidth = std::min<int16_t>(84, content.w / 4);
        ui.label({static_cast<int16_t>(content.x + 68), content.y,
                  static_cast<int16_t>(std::max<int16_t>(0, content.w - positionWidth - 72)), kHeaderHeight},
                 ui.text(UiText::Chapters), 2, ui::themes::ColorRole::Foreground, ui::TextAlign::Center);
        ui.label({static_cast<int16_t>(content.x + content.w - positionWidth), content.y, positionWidth, kHeaderHeight},
                 position, 1, ui::themes::ColorRole::Muted, ui::TextAlign::Right);

        const ui::Rect viewport{content.x, static_cast<int16_t>(content.y + kHeaderHeight + 4), content.w,
                                static_cast<int16_t>(content.h - kHeaderHeight - 4)};
        if (chapters.empty()) {
            if (ui.button(viewport, ui.text(UiText::StartReading))) {
                ReadingLoop::seekTo(reader, 0);
                return Action::Resume;
            }
            return Action::None;
        }

        const ui::Touch* touch = ui.touch();
        if (touch != nullptr && ui::hasTouch(*touch, ui::TouchStart) && ui::contains(viewport, touch->x, touch->y)) {
            dragging_ = true;
            dragStartIndex_ = centeredIndex_;
            lastY_ = touch->y;
            dragDistance_ = 0;
            lastTickMs_ = nowMs;
            velocity_ = 0;
            scrollRemainder_ = 0;
        }
        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchMove)) {
            const int16_t delta = static_cast<int16_t>(touch->y) - static_cast<int16_t>(lastY_);
            dragDistance_ = static_cast<uint16_t>(std::min<int>(UINT16_MAX, dragDistance_ + std::abs(delta)));
            lastY_ = touch->y;
        }

        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchRelease)
            && ui::hasTouch(*touch, ui::TouchTap) && ui::contains(viewport, touch->x, touch->y)) {
            dragging_ = false;
            offset_ = 0;
            velocity_ = 0;
            scrollRemainder_ = 0;
            centeredIndex_ = dragStartIndex_;

            const size_t tapCenter = centeredIndex_;
            const size_t tapFirst = tapCenter > 4 ? tapCenter - 4 : 0;
            const size_t tapLast = std::min(chapters.size(), tapCenter + 5);
            size_t tappedIndex = tapCenter;
            int closestDistance = INT_MAX;
            for (size_t index = tapFirst; index < tapLast; ++index) {
                const int raw = (static_cast<int>(index) - static_cast<int>(tapCenter)) * kRowStep;
                const int magnitude = std::min<int>(std::abs(raw), viewport.h);
                const int curved = raw * (2 * viewport.h - magnitude) / (2 * viewport.h);
                const int rowY = viewport.y + viewport.h / 2 + curved;
                const int distance = std::abs(rowY - touch->y);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    tappedIndex = index;
                }
            }

            centeredIndex_ = tappedIndex;
            ReadingLoop::seekTo(reader, chapters[centeredIndex_].wordIndex);
            return Action::Resume;
        }

        if (dragging_) {
            const uint32_t elapsed = std::min<uint32_t>(nowMs - lastTickMs_, 100);
            lastTickMs_ = nowMs;
            const int32_t dragRate =
                ui::centeredDragRate(lastY_, viewport.y, viewport.h, kRowStep / 2, kMaximumVelocity);
            if (dragDistance_ > kDragThreshold && dragRate != 0) {
                const int32_t direction = settings.chapterScrollReversed ? 1 : -1;
                const int32_t target = direction * dragRate;
                velocity_ += static_cast<int32_t>(static_cast<int64_t>(target - velocity_) * elapsed / kAccelerationMs);
                scrollRemainder_ += static_cast<int32_t>(static_cast<int64_t>(velocity_) * elapsed);
                const int16_t pixels = static_cast<int16_t>(scrollRemainder_ / kScrollScale);
                scrollRemainder_ %= kScrollScale;
                offset_ = static_cast<int16_t>(offset_ + pixels);
            } else {
                velocity_ = 0;
                scrollRemainder_ = 0;
                if (dragDistance_ > kDragThreshold)
                    offset_ = 0;
            }
        }

        while (offset_ <= -kRowStep / 2 && centeredIndex_ + 1 < chapters.size()) {
            ++centeredIndex_;
            offset_ = static_cast<int16_t>(offset_ + kRowStep);
        }
        while (offset_ >= kRowStep / 2 && centeredIndex_ > 0) {
            --centeredIndex_;
            offset_ = static_cast<int16_t>(offset_ - kRowStep);
        }
        if (centeredIndex_ == 0) {
            offset_ = std::min<int16_t>(offset_, 0);
            if (velocity_ > 0)
                velocity_ = scrollRemainder_ = 0;
        }
        if (centeredIndex_ + 1 == chapters.size()) {
            offset_ = std::max<int16_t>(offset_, 0);
            if (velocity_ < 0)
                velocity_ = scrollRemainder_ = 0;
        }

        if (dragging_ && touch != nullptr && ui::hasTouch(*touch, ui::TouchRelease)) {
            dragging_ = false;
            offset_ = 0;
            velocity_ = 0;
            scrollRemainder_ = 0;
        }

        uint32_t state = ui::Context::combine(static_cast<uint32_t>(chapters.size()), centeredIndex_);
        state = ui::Context::combine(state, static_cast<uint16_t>(offset_));
        state = ui::Context::combine(state, static_cast<uint32_t>(readingIndex));
        const size_t first = centeredIndex_ > 4 ? centeredIndex_ - 4 : 0;
        const size_t last = std::min(chapters.size(), centeredIndex_ + 5);
        const int16_t centerY = static_cast<int16_t>(viewport.y + viewport.h / 2);
        for (size_t index = first; index < last; ++index)
            state =
                ui::Context::signature(chapters[index].title,
                                       ui::Context::combine(state, static_cast<uint32_t>(chapters[index].wordIndex)));

        if (ui.redraw(viewport, state)) {
            Arduino_GFX& gfx = ui.gfx();
            const int16_t halfHeight = std::max<int16_t>(1, viewport.h / 2);
            const int16_t maximumWidth = std::max<int16_t>(40, static_cast<int16_t>(viewport.w - 20));
            const uint16_t background = ui.color(ui::themes::ColorRole::Background);
            for (size_t index = first; index < last; ++index) {
                const int raw = (static_cast<int>(index) - static_cast<int>(centeredIndex_)) * kRowStep + offset_;
                const int magnitude = std::min<int>(std::abs(raw), viewport.h);
                const int curved = raw * (2 * viewport.h - magnitude) / (2 * viewport.h);
                const int16_t y = static_cast<int16_t>(centerY + curved);
                const bool centered = index == centeredIndex_;
                const uint8_t alpha =
                    centered ? 255 : static_cast<uint8_t>(std::max(48, 220 - std::abs(curved) * 172 / halfHeight));
                const int16_t height = centered ? 28 : 18;
                const int16_t width = centered ? maximumWidth
                                               : static_cast<int16_t>(maximumWidth
                                                                      - std::min<int>(std::abs(curved), halfHeight)
                                                                            * (maximumWidth / 3) / halfHeight);
                const int16_t x = static_cast<int16_t>(viewport.x + (viewport.w - width) / 2);
                const int16_t top = static_cast<int16_t>(y - height / 2);
                if (top < viewport.y || top + height > viewport.y + viewport.h)
                    continue;
                const int16_t right = static_cast<int16_t>(x + width - 1);
                const int16_t notch = std::min<int16_t>(10, height / 2);
                const uint16_t surface = centered ? ui.color(ui::themes::ColorRole::SurfaceActive)
                                                  : ui.blend(ui::themes::ColorRole::SurfaceMuted, alpha);
                const uint16_t outline = centered ? ui.color(ui::themes::ColorRole::Outline)
                                                  : ui.blend(ui::themes::ColorRole::Outline, alpha);
                gfx.fillRoundRect(x, top, width, height, 5, surface);
                if (index == readingIndex) {
                    const int16_t tailLeft = static_cast<int16_t>(right - notch - 6);
                    gfx.fillRect(tailLeft, top, static_cast<int16_t>(right - tailLeft + 1), height,
                                 centered ? ui.color(ui::themes::ColorRole::Accent)
                                          : ui.blend(ui::themes::ColorRole::Accent, alpha));
                }
                gfx.drawRoundRect(x, top, width, height, 5, outline);
                gfx.fillTriangle(static_cast<int16_t>(right - notch), y, right, top, right,
                                 static_cast<int16_t>(top + height - 1), background);
                gfx.drawLine(static_cast<int16_t>(right - notch), y, right, top, outline);
                gfx.drawLine(static_cast<int16_t>(right - notch), y, right, static_cast<int16_t>(top + height - 1),
                             outline);
                if (centered)
                    gfx.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(top + 3), 3,
                                 static_cast<int16_t>(height - 6), ui.color(ui::themes::ColorRole::Accent));

                char fallback[24];
                const std::string_view chapter = ui.text(UiText::Chapter);
                std::snprintf(fallback, sizeof(fallback), "%.*s %u", static_cast<int>(chapter.size()), chapter.data(),
                              static_cast<unsigned>(index + 1));
                const std::string_view title = chapters[index].title.empty() ? std::string_view{fallback}
                                                                             : std::string_view{chapters[index].title};
                ui.drawText({static_cast<int16_t>(x + 10), top, static_cast<int16_t>(width - notch - 24), height},
                            title, centered ? 2 : 1, ui.blend(ui::themes::ColorRole::Foreground, alpha),
                            ui::TextAlign::Center, 1, reader.metadata.localeAt(chapters[index].wordIndex));
            }
        }
        return Action::None;
    }

} // namespace screens
