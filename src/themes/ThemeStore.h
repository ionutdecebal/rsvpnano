#pragma once

#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/Theme.h"
#include "ui/Modern.h"

class ThemeStore {
public:
    static constexpr size_t kMaximumFileBytes = 4096;

    void loadFromSd();
    std::expected<std::reference_wrapper<const ui::themes::Theme>, std::string> install(std::string_view stagedPath,
                                                                                        std::string_view finalPath);
    std::expected<void, std::string> remove(std::string_view id);
    const ui::themes::Theme* find(std::string_view id) const;
    const ui::themes::Theme& resolve(std::string_view id) const;
    const ui::themes::Theme& next(std::string_view id) const;
    std::span<const ui::themes::Theme> themes() const;

private:
    std::vector<ui::themes::Theme> themes_{ui::themes::defaultTheme(), ui::modern::paperTheme()};
};
