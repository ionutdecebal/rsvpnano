#pragma once
#include "settings/SettingsStore.h"
#include "update/UpdateOffer.h"
#include <expected>
#include <string>
namespace OtaUpdater {
// Called on the UI task after verification to commit settings and select the boot slot.
std::expected<void, std::string> finalizeInstall(settings::SettingsStore& store, const Offer& offer);
}
