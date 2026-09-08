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
        TypographySettings,
        BookFonts,
        ReaderSettings,
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

    private:
        const locales::Catalog* languages_ = nullptr;
    };
    bool pacingSettings(ui::Context& ui, settings::PacingSettings& settings, Screen& screen);
    bool typographySettings(ui::Context& ui, settings::TypographySettings& config, FontCatalog& fonts, Screen& screen);
    bool bookFonts(ui::Context& ui, const BookMetadata& metadata, settings::ReadingOverrides& overrides,
                   const locales::Catalog& localeCatalog, FontCatalog& fonts, Screen& screen);
    bool readerSettings(ui::Context& ui, settings::ReadingSettings& settings, Screen& screen);
    class NetworkScreen {
    public:
        bool startupCheckPending = false;

        void begin(settings::SettingsStore& store);
        Action draw(ui::Context& ui, settings::SettingsStore& store, Screen& screen);
        void openWifiScan();
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

    private:
        Action drawTimers(ui::Context& ui, Screen& screen);
        void drawEditor(ui::Context& ui, Screen& screen);
        void drawNameEditor(ui::Context& ui, Screen& screen);
        bool drawSession(ui::Context& ui, uint32_t nowMs);
        void edit(size_t index, bool creating, Screen& screen);
        bool persist(const focus::Timers& timers);

        fs::FS* filesystem_ = nullptr;
        focus::Timers timers_;
        focus::Timer draft_;
        focus::Session session_;
        focus::OrientationReader orientation_;
        ui::KeyboardState keyboard_;
        size_t editIndex_ = 0;
        size_t activeIndex_ = 0;
        bool creating_ = false;
        bool deleteConfirm_ = false;
        bool writable_ = false;
        size_t selectedIndex_ = 0;
        ui::CarouselGesture carouselGesture_;
    };
    void status(ui::Context& ui, std::string_view title, std::string_view line1 = {}, std::string_view line2 = {},
                int progress = -1);

} // namespace screens
