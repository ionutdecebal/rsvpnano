#pragma once
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
    void open(ReadingSession&);
    screens::Action draw(ui::Context&, ReadingSession&, screens::Screen&, uint32_t);
    void lookupOnline(settings::SettingsStore&);
    std::string message;
private:
    void lookup();
    std::string query_, draft_;
    Definition definition_;
    size_t sense_ = 0;
    std::vector<size_t> definitionOffsets_{0};
    uint32_t lookupWord_ = 0;
    ui::KeyboardState keyboard_;
};
}
