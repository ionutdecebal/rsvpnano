#include "ui/screens/watch/Layout.h"

namespace screens {
    bool pacingSettings(ui::Context& ui, settings::PacingSettings& config, Screen& screen) {
        auto area = watch::header(ui, ui.text(UiText::WordPacing), Screen::Settings, screen);
        auto grid = ui.pagedGrid(area, 4, 1, 60);
        bool changed = watch::stepper(ui, grid.item(0), UiText::LongWords, config.longWordDelayMs, " ms");
        changed |= watch::stepper(ui, grid.item(1), UiText::Complexity, config.complexWordDelayMs, " ms");
        changed |= watch::stepper(ui, grid.item(2), UiText::Punctuation, config.punctuationDelayMs, " ms");
        if (ui.card(grid.item(3), ui.text(UiText::Reset), {}, watch::textSize(ui))) {
            changed |= config != settings::PacingSettings{};
            config = {};
        }
        return changed;
    }
} // namespace screens
