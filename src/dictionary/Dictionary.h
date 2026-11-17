#pragma once
#include <optional>
#include <string>
#include <vector>
#include "reader/ReadingSession.h"
#include "settings/SettingsStore.h"
#include "ui/screens/Screens.h"

namespace dictionary {
struct Sense {
    std::string pos, definition, example;
    std::vector<std::string> synonyms;
};
struct Definition {
    std::string word, source, license, url;
    std::vector<Sense> senses;
};

// Owns only dictionary state. No notes, radio service, pairing or background
// polling. Network lookup runs on App's existing exclusive worker.
class Dictionary {
public:
    void open(ReadingSession&, std::optional<size_t> word = {},
              screens::Screen returnScreen = screens::Screen::QuickControls);
    screens::Screen returnScreen() const { return returnScreen_; }
    screens::Action draw(ui::Context&, ReadingSession&, screens::Screen&, uint32_t);
    void lookupOnline(settings::SettingsStore&);
    std::string message;
private:
    void lookup();
    std::string query_, draft_;
    Definition definition_;
    size_t sense_ = 0;
    std::vector<size_t> definitionOffsets_{0};
    size_t lookupWord_ = 0;
    screens::Screen returnScreen_ = screens::Screen::QuickControls;
    ui::KeyboardState keyboard_;
};
}
