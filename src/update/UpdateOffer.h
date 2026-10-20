#pragma once
#include "update/ChannelCatalog.h"
namespace OtaUpdater {
enum class Target { Current, Other };
struct Offer {
    update::Release release;
    std::string repository, version, message;
    bool ready = false;
    bool switching = false;
};
}
