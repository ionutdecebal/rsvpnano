#include "themes/ThemeStore.h"
#include <esp_log.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>

#include "board/BoardStorage.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

void ThemeStore::loadFromSd() {
    themes_ = {ui::themes::defaultTheme(), ui::modern::paperTheme()};

    File dir = Board::Storage::filesystem().open(StoragePaths::kThemesPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return;
    }

    std::vector<ui::themes::Theme> loaded;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }

        const std::string path = entry.path();
        if (!ui::themes::hasThemeExtension(path)) {
            entry.close();
            continue;
        }

        if (entry.size() > kMaximumFileBytes) {
            ESP_LOGW("theme", "skipped %s: file exceeds %u bytes", path.c_str(),
                     static_cast<unsigned>(kMaximumFileBytes));
            entry.close();
            continue;
        }
        std::string text(entry.size(), '\0');
        const size_t count = entry.read(reinterpret_cast<uint8_t*>(text.data()), text.size());
        entry.close();
        if (count != text.size()) {
            ESP_LOGW("theme", "skipped %s: incomplete read", path.c_str());
            continue;
        }
        auto parsed = ui::themes::decodeToml(text, ui::themes::themeIdFromPath(path));
        if (!parsed) {
            ESP_LOGW("theme", "skipped %s: %s", path.c_str(), parsed.error().message.c_str());
            continue;
        }

        loaded.push_back(std::move(*parsed));
    }
    dir.close();

    std::ranges::sort(loaded, {}, &ui::themes::Theme::id);
    const auto duplicates = std::ranges::unique(loaded, {}, &ui::themes::Theme::id);
    loaded.erase(duplicates.begin(), duplicates.end());

    const auto defaultTheme = std::ranges::find(loaded, ui::themes::kDefaultThemeId, &ui::themes::Theme::id);
    if (defaultTheme != loaded.end()) {
        defaultTheme->builtIn = true;
        themes_.front() = std::move(*defaultTheme);
        loaded.erase(defaultTheme);
    }
    const auto paper = std::ranges::find(loaded, "paper", &ui::themes::Theme::id);
    if (paper != loaded.end()) { themes_[1] = std::move(*paper); loaded.erase(paper); }
    themes_.insert(themes_.end(), std::make_move_iterator(loaded.begin()), std::make_move_iterator(loaded.end()));
}

std::expected<std::reference_wrapper<const ui::themes::Theme>, std::string> ThemeStore::
    install(std::string_view stagedPath, std::string_view finalPath) {
    const std::string staged{stagedPath};
    const std::string final{finalPath};
    const std::string id = ui::themes::themeIdFromPath(final);
    return StorageFiles::ensureDirectory(StoragePaths::kThemesPath)
        .transform_error([](std::error_code) {
            return std::string{"Theme folder could not be created"};
        })
        .and_then([&]() -> std::expected<ui::themes::Theme, std::string> {
            if (find(id) != nullptr)
                return std::unexpected("Theme already exists");
            return StorageFiles::readTextFile(Board::Storage::filesystem(), staged.c_str(), kMaximumFileBytes)
                .transform_error([](std::error_code) {
                    return std::string{"Theme file could not be read"};
                })
                .and_then([&](const std::string& text) -> std::expected<ui::themes::Theme, std::string> {
                    if (text.empty())
                        return std::unexpected("Theme file is empty");
                    return ui::themes::decodeToml(text, id).transform_error([](const auto& error) {
                        return error.message;
                    });
                });
        })
        .and_then([&](ui::themes::Theme installed)
                      -> std::expected<std::reference_wrapper<const ui::themes::Theme>, std::string> {
            auto replaced = StorageFiles::replaceFileAtomic(Board::Storage::filesystem(), final.c_str(), staged.c_str(),
                                                            (final + ".bak").c_str());
            if (!replaced)
                return std::unexpected("Theme file could not be installed");
            if (id == ui::themes::kDefaultThemeId) {
                installed.builtIn = true;
                themes_.front() = std::move(installed);
            } else {
                themes_.push_back(std::move(installed));
                std::ranges::sort(themes_.begin() + 1, themes_.end(), {}, &ui::themes::Theme::id);
            }
            return std::cref(resolve(id));
        });
}

std::expected<void, std::string> ThemeStore::remove(std::string_view id) {
    const auto theme = std::ranges::find(themes_, id, &ui::themes::Theme::id);
    if (theme == themes_.end())
        return std::unexpected("Theme not found");
    if (theme->builtIn)
        return std::unexpected("Built-in theme cannot be removed");

    File directory = Board::Storage::filesystem().open(StoragePaths::kThemesPath);
    while (directory && directory.isDirectory()) {
        File entry = directory.openNextFile();
        if (!entry)
            break;
        const std::string path = entry.path();
        const bool matches =
            !entry.isDirectory() && ui::themes::hasThemeExtension(path) && ui::themes::themeIdFromPath(path) == id;
        entry.close();
        if (!matches)
            continue;
        const bool removed = Board::Storage::filesystem().remove(path.c_str());
        directory.close();
        if (!removed)
            return std::unexpected("Theme file could not be removed");
        themes_.erase(theme);
        return {};
    }
    if (directory)
        directory.close();
    return std::unexpected("Theme file not found");
}

const ui::themes::Theme* ThemeStore::find(std::string_view id) const {
    const auto found = std::ranges::find(themes_, id, &ui::themes::Theme::id);
    return found == themes_.end() ? nullptr : &*found;
}

const ui::themes::Theme& ThemeStore::resolve(std::string_view id) const {
    const ui::themes::Theme* found = find(id);
    return found == nullptr ? themes_.front() : *found;
}

const ui::themes::Theme& ThemeStore::next(std::string_view id) const {
    const ui::themes::Theme* current = find(id);
    if (current == nullptr || current == &themes_.back())
        return themes_.front();
    return *std::next(current);
}

std::span<const ui::themes::Theme> ThemeStore::themes() const {
    return themes_;
}
