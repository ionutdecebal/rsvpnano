#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <string>
#include <string_view>

#include "board/BoardDisplay.h"
#include "board/BoardPower.h"
#include "companion/http/CompanionApi.h"
#include "companion/serial/CompanionSerial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "input/Input.h"
#include "locales/LocaleCatalog.h"
#include "rss/RssFeeds.h"
#include "settings/SettingsStore.h"
#include "storage/StorageManager.h"
#include "ui/Ui.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/Screens.h"
#include "ui/screens/StandbyScreen.h"
#include "update/OtaUpdater.h"
#include "usb/UsbMassStorageManager.h"

class App {
public:
    void begin();
    void update(uint32_t nowMs);

private:
    enum class JobKind : uint8_t {
        None,
        Rss,
        StorageCheck,
        OtaCheck,
        OtaInstall,
        Book,
        Typography,
    };

    struct JobUpdate {
        bool complete = false;
        char title[24] = {};
        char line1[96] = {};
        char line2[96] = {};
        int progressPercent = -1;
        bool rebootRequired = false;
    };

    void renderScreen(uint32_t nowMs);
    void handleScreenAction(screens::Action action, uint32_t nowMs);
    void handleInput(Input::ActionMask actions, uint32_t nowMs);
    void handleTouch(uint32_t nowMs);
    void runRss();
    void runBookOpen(size_t index, uint32_t nowMs);
    bool requestTypographyRefresh();
    void updateBackgroundJob();
    bool startBackgroundJob(JobKind kind);
    static void backgroundJobEntry(void* context);
    void runBackgroundJob();
    void enqueueJobUpdate(JobUpdate update, bool mustSucceed = false);
    void showTransientStatus(std::string_view title, std::string_view line1, std::string_view line2,
                             uint32_t durationMs, screens::Screen destination, int progressPercent = -1);
    void applySettings();
    void loadAppearanceSettings();
    void reloadUiAssets();
    void enterUsbTransfer(uint32_t nowMs);
    void exitUsbTransfer(screens::Screen destination = screens::Screen::Reader);
    void runOtaCheck(bool install);
    void enterStandby(uint32_t nowMs);
    void exitStandby(uint32_t nowMs);
    void lightSleepFromStandby();
    void powerOff(uint32_t nowMs);
    static void renderStorageStatus(void* context, const char* title, const char* line1, const char* line2,
                                    int progressPercent);
    bool backgroundJobActive() const {
        return jobKind_ != JobKind::None;
    }
    bool typographyJobActive() const {
        return jobKind_ == JobKind::Typography;
    }

    ui::Context immediateUi_{Board::Display::gfx()};
    settings::SettingsStore settingsStore_;
    locales::Catalog localeCatalog_;
    Board::Power::BatteryState battery_;
    screens::ReaderScreen readerScreen_{Board::Display::gfx(), settingsStore_.settings().reading};
    screens::LibraryScreen libraryScreen_;
    screens::ChaptersScreen chaptersScreen_;
    screens::InterfaceScreen interfaceScreen_;
    screens::NetworkScreen networkScreen_;
    StorageManager storage_;
    Preferences prefs_;
    screens::FocusScreen focusScreen_;
    CompanionApi companionApi_{settingsStore_,   storage_,       localeCatalog_, immediateUi_, readerScreen_,
                               interfaceScreen_, networkScreen_, libraryScreen_, focusScreen_};
    UsbMassStorageManager usbTransfer_;
    CompanionSerial serialCompanion_{companionApi_, usbTransfer_};
    screens::StandbyScreen standbyScreen_;
    QueueHandle_t jobQueue_ = nullptr;
    JobKind jobKind_ = JobKind::None;
    size_t jobBookIndex_ = 0;
    bool jobBookLoaded_ = false;
    size_t pendingBookIndex_ = 0;
    bool bookOpenPending_ = false;
    bool typographyRefreshPending_ = false;
    bool typographyOpensBook_ = false;
    screens::Screen screen_ = screens::Screen::Status;
    screens::Screen statusDestination_ = screens::Screen::Reader;
    uint32_t bootMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    uint32_t standbyEnteredMs_ = 0;
    uint32_t statusUntilMs_ = 0;
    bool restartAfterStatus_ = false;
};
