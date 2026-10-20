#pragma once

#include <Arduino.h>
#include <FS.h>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>

#include "library/BookMetadata.h"
#include "themes/ThemeStore.h"
#include "fonts/FontCatalog.h"
#include "localization/LocaleCatalog.h"
#include "reader/ReadingLoop.h"
#include "settings/NvsSecurity.h"
#include "settings/SettingsModel.h"
#include "settings/SettingsStore.h"
#include "screensavers/ScreensaverTypes.h"
#include "focus/FocusOrientation.h"
#include "focus/FocusSession.h"
#include "focus/FocusTimers.h"
#include "ui/Ui.h"
#include "ui/Modern.h"
#include "ui/Layouts.h"

namespace screens {

    enum class Screen : uint8_t {
        Read,
        Library,
        Chapters,
        Settings,
        ReadingSettings,
        InterfaceSettings,
        PacingSettings,
        ReaderAppearance,
        BookFonts,
        NetworkSettings,
        WifiScan,
        WifiConnect,
        NetworkEdit,
        Device,
        StorageEncryption,
        Sync,
        Ota,
        FocusTimers,
        FocusEditor,
        FocusNameEdit,
        FocusSession,
        Reader,
        Usb,
        Status,
        Standby,
        ReadingHub,
        DisplayHub,
        Connections,
        Help,
        StorageHub,
        PowerHub,
        FocusPresets,
        FindPlace,
        FindChapters,
        BookSearch,
        BookSearchKeyboard,
        QuickControls,
        Onboarding,
        ButtonMap,
        AddBooks,
        TouchCheck,
        Dictionary,
        DictionarySearch,
        DictionaryHelp,
    };

    enum class Action : uint8_t {
        None,
        OpenBook,
        Resume,
        PowerOff,
        CompanionSync,
        RssRefresh,
        UsbTransfer,
        StorageStatus,
        EnableStorageEncryption,
        OtaCheck,
        OtaInstall,
        FocusRead,
        OpenPage,
        ReplayIntroduction,
        FinishIntroduction,
        SkipIntroduction,
        CheckOnboardingStorage,
        OpenDictionary,
        OnlineDictionary,
        OpenBookSettings,
        CycleLanguage,
    };

    Action read(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress, Screen& screen);
    Action settings(ui::Context& ui, Screen& screen);
    bool readingSettings(ui::Context& ui, settings::ReadingSettings& settings, Screen& screen);
    class InterfaceScreen {
    public:
        ThemeStore themes;

        bool begin(ui::Context& ui, settings::InterfaceSettings& settings, const locales::Catalog& languages,
                   void (*setBrightness)(uint8_t));
        bool draw(ui::Context& ui, settings::InterfaceSettings& settings, std::span<const uint32_t> standbyDurations,
                  void (*setBrightness)(uint8_t), Screen& screen);
        void cycleLanguage(ui::Context& ui, settings::InterfaceSettings& settings);

    private:
        const locales::Catalog* languages_ = nullptr;
    };
    bool pacingSettings(ui::Context& ui, settings::PacingSettings& settings, Screen& screen);
    bool bookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                   const locales::Catalog& localeCatalog, FontCatalog& fonts, Screen& screen,
                   Screen parent = Screen::Settings, std::string_view bookTitle = {});
    void resetBookFonts();
    class NetworkScreen {
    public:
        bool startupCheckPending = false;

        void begin(settings::SettingsStore& store);
        Action draw(ui::Context& ui, settings::SettingsStore& store, Screen& screen);
        void openWifiScan();
        void openSavedWifi(){closeWifi();savedNetworks_=true;}
        void closeWifi();
        void drawWifiScan(ui::Context& ui, settings::SettingsStore& store, Screen& screen);
        bool drawWifiConnect(ui::Context& ui, settings::SettingsStore& store, Screen& screen);
        void drawEdit(ui::Context& ui, settings::SettingsStore& store, Screen& screen);

    private:
        struct WifiNetwork {
            std::string ssid;
            int32_t rssi = 0;
            bool secured = false;
        };

        enum class WifiScanState : uint8_t {
            Idle,
            Scanning,
            Complete,
            Failed,
        };

        enum class EditField : uint8_t {
            Owner,
            Tag,
        };

        void saveNetwork(settings::SettingsStore& store, std::string_view ssid);
        void updateWifiScan();
        bool savedNetworks_=false;

        std::array<WifiNetwork, 8> networks_;
        size_t networkCount_ = 0;
        size_t selectedNetworkIndex_ = networks_.size();
        std::string password_;
        std::string editValue_;
        ui::KeyboardState keyboard_;
        EditField editField_ = EditField::Owner;
        WifiScanState scanState_ = WifiScanState::Idle;
        bool connectionFailed_ = false;
    };
    Action device(ui::Context& ui, bool storageReady, size_t bookCount, settings::NvsEncryptionState encryptionState,
                  Screen& screen);
    Action storageEncryption(ui::Context& ui, settings::NvsEncryptionState encryptionState, Screen& screen);
    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen);
    class FocusScreen {
    public:
        void begin();
        void begin(fs::FS& filesystem);
        bool update(uint32_t nowMs);
        Action draw(ui::Context& ui, uint32_t nowMs, Screen& screen);
        void setTimers(focus::Timers timers);
        const focus::Timers& timers() const {
            return timers_;
        }
        void close();
        bool manual() const { return manualActive_; }
        bool active() const { return manualActive_; }
        uint8_t frameSurface(Screen screen) const {
            // Phase changes rearrange controls on the same screen. Rebuild its
            // slots once, including cancelling a press on a disappearing button.
            return screen == Screen::FocusSession && manualActive_ ? static_cast<uint8_t>(session_.phase()) + 1 : 0;
        }
        bool breakVisible() const { return manualActive_ && session_.phase() != focus::Phase::Focus
                                                    && session_.phase() != focus::Phase::PausedFocus; }
        std::string badge(uint32_t nowMs) const;


    private:
        Action drawModern(ui::Context& ui, uint32_t nowMs, Screen& screen);
        Action drawTimers(ui::Context& ui, Screen& screen);
        void drawEditor(ui::Context& ui, Screen& screen);
        void drawNameEditor(ui::Context& ui, Screen& screen);
        bool drawSession(ui::Context& ui, uint32_t nowMs);
        void edit(size_t index, bool creating, Screen& screen);
        bool persist(const focus::Timers& timers);

        fs::FS* filesystem_ = nullptr;
        focus::Timers timers_;
        focus::Timer draft_;
        focus::Timer activeTimer_;
        bool manualActive_ = false;
        bool readWithFocus_ = false;
        size_t presetPage_ = 0;
        ui::modern::ListSwipe presetSwipe_;
        focus::Session session_;
        focus::OrientationReader orientation_;
        ui::KeyboardState keyboard_;
        size_t editIndex_ = 0;
        size_t activeIndex_ = 0;
        bool creating_ = false;
        bool deleteConfirm_ = false;
        bool exitConfirm_ = false;
        bool writable_ = false;
        size_t selectedIndex_ = 0;
        ui::CarouselGesture carouselGesture_;
    };
    void rememberStatus(std::string_view title, std::string_view line1, std::string_view line2, int progress);
    void animateStatus(ui::Context& ui, uint32_t nowMs);
    void status(ui::Context& ui, std::string_view title, std::string_view line1 = {}, std::string_view line2 = {},
                int progress = -1);

} // namespace screens
