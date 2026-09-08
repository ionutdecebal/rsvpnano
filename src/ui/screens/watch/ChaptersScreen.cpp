#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/watch/Layout.h"

namespace screens {
    Action ChaptersScreen::draw(ui::Context& ui, std::span<const ChapterMarker> chapters, ReadingSession& reader,
                                const settings::ReadingSettings& settings, uint32_t nowMs, Screen& screen) {
        (void) nowMs;
        detail::navigation(ui, Screen::Chapters, screen);
        const auto area = detail::tabContent(ui);
        if (chapters.empty()) {
            if (ui.card(area, ui.text(UiText::StartReading), {}, watch::textSize(ui))) {
                ReadingLoop::seekTo(reader, 0);
                return Action::Resume;
            }
            return Action::None;
        }
        if (source_.data() != chapters.data() || source_.size() != chapters.size()) {
            source_ = chapters;
            centeredIndex_ = 0;
            carouselGesture_ = {};
            while (centeredIndex_ + 1 < chapters.size()
                   && chapters[centeredIndex_ + 1].wordIndex <= reader.state.wordIndex)
                ++centeredIndex_;
        }
        centeredIndex_ = std::min(centeredIndex_, chapters.size() - 1);
        const int delta = carouselGesture_.update(ui.touch(), area) * (settings.chapterScrollReversed ? -1 : 1);
        if (delta) {
            centeredIndex_ = ui::rotateIndex(centeredIndex_, chapters.size(), delta);
            ui.invalidate();
        }
        const auto cards = watch::carousel(area);
        const auto previous = ui::rotateIndex(centeredIndex_, chapters.size(), -1);
        const auto next = ui::rotateIndex(centeredIndex_, chapters.size(), 1);
        const auto title = [&](size_t index) {
            return chapters[index].title.empty()
                     ? std::string{ui.text(UiText::Chapter)} + " " + std::to_string(index + 1)
                     : chapters[index].title;
        };
        if (ui.card(cards[0], title(previous), "<", 2, ui::themes::Accent, ui::Icon::None,
                    chapters.size() > 1 && !delta, 165)) {
            centeredIndex_ = previous;
            ui.invalidate();
        }
        if (ui.card(cards[1], title(centeredIndex_),
                    std::to_string(centeredIndex_ + 1) + "/" + std::to_string(chapters.size()), watch::textSize(ui),
                    ui::themes::Accent, ui::Icon::None, !delta)) {
            ReadingLoop::seekTo(reader, chapters[centeredIndex_].wordIndex);
            return Action::Resume;
        }
        if (ui.card(cards[2], title(next), ">", 2, ui::themes::Accent, ui::Icon::None, chapters.size() > 1 && !delta,
                    165)) {
            centeredIndex_ = next;
            ui.invalidate();
        }
        return Action::None;
    }
} // namespace screens
