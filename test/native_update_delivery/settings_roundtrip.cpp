#include "settings/SettingsCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv) {
#ifdef GENERATE_NEW_UI
    settings::DeviceSettings config;
    config.reading.mode=settings::ReadingMode::chunks;
    config.reading.wpm=467;
    config.network.ssid="Reading room";
    config.interface.onboardingComplete=true;
    auto text=settings::codec::encodeToml(config,settings::SettingsSource::Programmatic);assert(text);
    std::cout<<*text;
#else
    assert(argc==2);
    std::ifstream input(argv[1]);std::string text{std::istreambuf_iterator<char>{input},{}};
    auto config=settings::codec::decodeToml(text,settings::SettingsSource::Sd);assert(config);
    assert(config->reading.wpm==467);
    assert(config->network.ssid=="Reading room");
    assert(config->reading.mode==settings::ReadingMode::rsvp);
    auto encoded=settings::codec::encodeToml(*config,settings::SettingsSource::Programmatic);assert(encoded);
    auto decoded=settings::codec::decodeToml(*encoded,settings::SettingsSource::Sd);assert(decoded);
    assert(decoded->reading.mode==settings::ReadingMode::rsvp);
    std::cout<<*encoded;
#endif
}
