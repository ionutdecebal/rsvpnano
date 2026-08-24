#include "app/App.h"
#include <esp_log.h>

#include <array>
#include <cstdio>
#include <esp_system.h>
#include <string>

#include "board/BoardAudio.h"
#include "board/BoardConfig.h"
#include "board/BoardInput.h"
#include "board/BoardPower.h"
#include "board/BoardStorage.h"
#include "board/BoardSystem.h"
#include "freertos/task.h"
#include "logging/Logger.h"
#include "rss/RssFeeds.h"
#include "settings/NvsSecurity.h"
#include "storage/index/ReadingProgress.h"
#include "storage/migration/Migration.h"
#include "update/OtaUpdater.h"

namespace {

    constexpr uint32_t kBootSplashMs = 650;
    constexpr std::array<uint32_t, 5> kStandbyMs = {
        0, 1UL * 60UL * 1000UL, 5UL * 60UL * 1000UL, 15UL * 60UL * 1000UL, 30UL * 60UL * 1000UL,
    };
    constexpr uint32_t kStandbyPowerOffMs = 5UL * 60UL * 1000UL;
    constexpr UBaseType_t kJobQueueLength = 8;
    constexpr uint32_t kJobStackBytes = 12288;
    constexpr UBaseType_t kJobPriority = 1;

    template<size_t Size>
    void copyText(char (&destination)[Size], const char* source) {
        std::snprintf(destination, Size, "%s", source == nullptr ? "" : source);
    }

    bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
        return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
    }

    void powerOffBoard() {
        if (!Board::Power::powerOff())
            ESP_LOGI("app", "hardware power off unavailable; entering light sleep");
        delay(1200);
        Board::System::lightSleep(0);
        esp_restart();
    }
} // namespace

void App::begin() {
    prefs_.begin(settings::kStateNvsNamespace);
    storage_.setStatusCallback(&App::renderStorageStatus, this);
    bootMs_ = millis();
    lastActivityMs_ = bootMs_;
    statusUntilMs_ = bootMs_ + kBootSplashMs;

    if (!Board::Display::begin()) {
        ESP_LOGE("app", "display init failed");
    }
    immediateUi_.setOrientation(Board::Display::defaultUiOrientation());
    immediateUi_.setTheme(interfaceScreen_.themes.resolve(settingsStore_.settings().interface.selectedThemeId));
    screens::status(immediateUi_, immediateUi_.text(UiText::Ready));
    if (!Input::begin())
        ESP_LOGE("input", "startup failed");
    immediateUi_.setTouchSource({.surface = Board::Input::touchSurface(), .poll = &Input::pollTouch});

    storage_.begin();
    Logger::startupCheckpoint("storage");
    fs::FS* filesystem = storage_.mounted() ? &Board::Storage::filesystem() : nullptr;
    if (auto result = settingsStore_.begin(filesystem); !result)
        ESP_LOGW("settings", "startup warning: %s", result.error().message.c_str());
    Logger::startupCheckpoint("settings");
    Logger::checkpoint("locale_catalog");
    localeCatalog_ = filesystem == nullptr
                       ? locales::Catalog{}
                       : locales::scanInstalled(*filesystem, static_cast<size_t>(UiText::Count));
    ESP_LOGI("languages", "catalog ready installed=%u", static_cast<unsigned>(localeCatalog_.size()));
    Logger::startupCheckpoint("locale_catalog");
    readerScreen_.fonts.loadFromSd();
    Logger::startupCheckpoint("fonts");
    Logger::checkpoint("ui_locale");
    loadAppearanceSettings();
    Logger::startupCheckpoint("locales");
    Board::Power::updateBattery(battery_, bootMs_, true);
    readerScreen_.begin(interfaceScreen_.themes.resolve(settingsStore_.settings().interface.selectedThemeId));
    networkScreen_.begin(settingsStore_);
    if (storage_.mounted())
        focusScreen_.begin(Board::Storage::filesystem());
    else
        focusScreen_.begin();
    readerScreen_.loadInitialBook(immediateUi_, storage_, prefs_, bootMs_);
    Logger::startupCheckpoint("book");
    libraryScreen_.invalidate();
    ESP_LOGI("startup", "ready");
}

void App::update(uint32_t nowMs) {
    serialCompanion_.update(nowMs);
    Input::ActionMask actions;
    while (Input::poll(actions)) {
        lastActivityMs_ = nowMs;
        handleInput(actions, nowMs);
        nowMs = millis();
    }
    while (immediateUi_.pollTouch(nowMs)) {
        lastActivityMs_ = nowMs;
        handleTouch(nowMs);
        nowMs = millis();
    }

    updateBackgroundJob();
    const bool preparingTypography = typographyJobActive();

    if (backgroundJobActive() && !preparingTypography)
        return;

    if (screen_ == screens::Screen::Status) {
        if (statusUntilMs_ == 0 || !deadlineReached(nowMs, statusUntilMs_))
            return;
        statusUntilMs_ = 0;
        if (restartAfterStatus_) {
            ESP.restart();
            return;
        }
        screen_ = statusDestination_;
        if (networkScreen_.startupCheckPending) {
            networkScreen_.startupCheckPending = false;
            runOtaCheck(false);
            return;
        }
    }

    if (screen_ == screens::Screen::Standby) {
        if (nowMs - standbyEnteredMs_ >= kStandbyPowerOffMs) {
            powerOff(nowMs);
            return;
        }
        standbyScreen_.update(immediateUi_, nowMs);
        return;
    }

    if (companionApi_.active() || serialCompanion_.active()) {
        settingsStore_.update(nowMs);
        Board::Power::updateBattery(battery_, nowMs);
        return;
    }

    if (!usbTransfer_.active())
        settingsStore_.update(nowMs);

    Board::Power::updateBattery(battery_, nowMs);
    if (!preparingTypography)
        readerScreen_.update(prefs_, nowMs);
    if (screen_ == screens::Screen::FocusSession) {
        if (focusScreen_.update(nowMs))
            Board::Audio::beep();
    }
    ReadingProgress::save(readerScreen_.session, prefs_, false, nowMs);

    renderScreen(nowMs);
    if (!preparingTypography && !readerScreen_.session.playing && !companionApi_.active() && !usbTransfer_.active()
        && screen_ != screens::Screen::FocusSession && screen_ != screens::Screen::Status
        && kStandbyMs[settingsStore_.settings().interface.standbyTimerIndex] > 0
        && nowMs - lastActivityMs_ >= kStandbyMs[settingsStore_.settings().interface.standbyTimerIndex]) {
        enterStandby(nowMs);
    }
}

void App::renderScreen(uint32_t nowMs) {
    if (serialCompanion_.active()) {
        screens::status(immediateUi_, "USB companion", "Connected", "Keep the browser open");
        return;
    }
    const screens::Screen renderedScreen = screen_;
    screens::Action action = screens::Action::None;
    switch (screen_) {
    case screens::Screen::Status:
        screens::status(immediateUi_, immediateUi_.text(UiText::Ready));
        return;
    case screens::Screen::Reader:
        if (typographyJobActive()) {
            screens::status(immediateUi_, immediateUi_.text(UiText::FontSection), immediateUi_.text(UiText::Checking));
            return;
        }
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        readerScreen_.draw(immediateUi_, storage_, battery_, nowMs);
        immediateUi_.endFrame();
        return;
    case screens::Screen::Library: {
        const auto& items = libraryScreen_.items(storage_, readerScreen_.store, readerScreen_.session);
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        const screens::Action result = libraryScreen_.draw(immediateUi_, items, nowMs, screen_);
        immediateUi_.endFrame();
        if (result == screens::Action::OpenBook) {
            runBookOpen(libraryScreen_.selectedIndex(), nowMs);
        } else {
            handleScreenAction(result, nowMs);
        }
        return;
    }
    case screens::Screen::Usb:
        screens::status(immediateUi_, "USB", usbTransfer_.statusMessage(), immediateUi_.text(UiText::HoldPowerToExit));
        return;
    case screens::Screen::Standby:
        standbyScreen_.draw(immediateUi_);
        return;
    case screens::Screen::Read:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        {
            const int bookIndex = storage_.findBook(readerScreen_.session.sourcePath());
            const BookLibrary::Entry* book = bookIndex < 0 ? nullptr : storage_.book(static_cast<size_t>(bookIndex));
            action = screens::read(immediateUi_, ReadingProgress::title(readerScreen_.session, storage_),
                                   book == nullptr ? std::string_view{} : std::string_view{book->author},
                                   ReadingProgress::percent(readerScreen_.session.state.wordIndex,
                                                            ReadingLoop::wordCount(readerScreen_.session)),
                                   screen_);
        }
        break;
    case screens::Screen::Chapters:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = chaptersScreen_.draw(immediateUi_, readerScreen_.session.metadata.chapters, readerScreen_.session,
                                      settingsStore_.settings().reading, nowMs, screen_);
        break;
    case screens::Screen::Settings:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::settings(immediateUi_, screen_);
        break;
    case screens::Screen::ReadingSettings: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        const settings::ReadingMode mode = settingsStore_.settings().reading.mode;
        if (screens::readingSettings(immediateUi_, settingsStore_.settings().reading, screen_)) {
            settingsStore_.acceptChanges();
            if (mode != settingsStore_.settings().reading.mode)
                requestTypographyRefresh();
        }
        break;
    }
    case screens::Screen::InterfaceSettings: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        const std::string locale = settingsStore_.settings().interface.locale;
        if (interfaceScreen_.draw(immediateUi_, settingsStore_.settings().interface, kStandbyMs,
                                  &Board::Display::setBrightness, screen_)) {
            settingsStore_.acceptChanges();
            if (locale != settingsStore_.settings().interface.locale)
                reloadUiAssets();
            readerScreen_
                .applyTheme(interfaceScreen_.themes.resolve(settingsStore_.settings().interface.selectedThemeId));
        }
        break;
    }
    case screens::Screen::PacingSettings: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (screens::pacingSettings(immediateUi_, settingsStore_.settings().reading.pacing, screen_))
            settingsStore_.acceptChanges();
        break;
    }
    case screens::Screen::TypographySettings: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (screens::typographySettings(immediateUi_, settingsStore_.settings().reading.typography, readerScreen_.fonts,
                                        screen_)) {
            settingsStore_.acceptChanges();
            requestTypographyRefresh();
        }
        break;
    }
    case screens::Screen::BookFonts: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (screens::bookFonts(immediateUi_, readerScreen_.session.metadata, readerScreen_.session.state.overrides,
                               localeCatalog_, readerScreen_.fonts, screen_)) {
            ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
            requestTypographyRefresh();
        }
        break;
    }
    case screens::Screen::ReaderSettings:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (screens::readerSettings(immediateUi_, settingsStore_.settings().reading, screen_))
            settingsStore_.acceptChanges();
        break;
    case screens::Screen::NetworkSettings:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = networkScreen_.draw(immediateUi_, settingsStore_, screen_);
        break;
    case screens::Screen::WifiScan:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        networkScreen_.drawWifiScan(immediateUi_, settingsStore_, screen_);
        break;
    case screens::Screen::WifiConnect:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (networkScreen_.drawWifiConnect(immediateUi_, settingsStore_, screen_))
            return;
        break;
    case screens::Screen::NetworkEdit:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        networkScreen_.drawEdit(immediateUi_, settingsStore_, screen_);
        break;
    case screens::Screen::Device:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::device(immediateUi_, storage_.mounted(), storage_.books().size(),
                                 settings::nvsEncryptionState(), screen_);
        break;
    case screens::Screen::StorageEncryption:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::storageEncryption(immediateUi_, settings::nvsEncryptionState(), screen_);
        break;
    case screens::Screen::Sync:
        screens::status(immediateUi_, immediateUi_.text(UiText::Sync), companionApi_.statusLine1(),
                        companionApi_.statusLine2());
        return;
    case screens::Screen::Ota: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::ota(immediateUi_, OtaUpdater::currentVersion().data(), screen_);
        break;
    }
    case screens::Screen::FocusTimers:
    case screens::Screen::FocusEditor:
    case screens::Screen::FocusNameEdit:
    case screens::Screen::FocusSession: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = focusScreen_.draw(immediateUi_, nowMs, screen_);
        immediateUi_.endFrame();
        if (action != screens::Action::None) {
            handleScreenAction(action, nowMs);
            return;
        }
        if (renderedScreen == screens::Screen::FocusSession && screen_ != screens::Screen::FocusSession)
            focusScreen_.close();
        if (screen_ != renderedScreen)
            renderScreen(nowMs);
        return;
    }
    }
    immediateUi_.endFrame();
    if (screen_ != renderedScreen) {
        if (screen_ == screens::Screen::Library)
            libraryScreen_.reset();
    }
    handleScreenAction(action, nowMs);
}

void App::handleScreenAction(screens::Action action, uint32_t nowMs) {
    if (backgroundJobActive() && action != screens::Action::None)
        return;
    switch (action) {
    case screens::Action::None:
    case screens::Action::OpenBook:
        return;
    case screens::Action::Resume:
        ReadingLoop::pause(readerScreen_.session);
        screen_ = screens::Screen::Reader;
        renderScreen(nowMs);
        return;
    case screens::Action::PowerOff:
        powerOff(nowMs);
        return;
    case screens::Action::CompanionSync:
        screen_ = screens::Screen::Sync;
        immediateUi_.invalidate();
        screens::status(immediateUi_, immediateUi_.text(UiText::CompanionSync), immediateUi_.text(UiText::Connecting));
        companionApi_.begin();
        renderScreen(nowMs);
        return;
    case screens::Action::RssRefresh:
        runRss();
        return;
    case screens::Action::UsbTransfer:
        enterUsbTransfer(nowMs);
        return;
    case screens::Action::StorageStatus:
        screen_ = screens::Screen::Status;
        statusUntilMs_ = 0;
        screens::status(immediateUi_, immediateUi_.text(UiText::Storage), immediateUi_.text(UiText::Checking));
        if (!startBackgroundJob(JobKind::StorageCheck))
            showTransientStatus(immediateUi_.text(UiText::Storage), immediateUi_.text(UiText::CouldNotStart), {}, 1200,
                                screens::Screen::Device);
        return;
    case screens::Action::EnableStorageEncryption:
        screens::status(immediateUi_, immediateUi_.text(UiText::StorageEncryption),
                        immediateUi_.text(UiText::EnablingEncryption));
        ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
        ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
        if (!storage_.mounted() || !settings::enableNvsEncryption(prefs_, settingsStore_)) {
            showTransientStatus(immediateUi_.text(UiText::StorageEncryption), immediateUi_.text(UiText::Unavailable),
                                {}, 1200, screens::Screen::Device);
        }
        return;
    case screens::Action::OtaCheck:
        runOtaCheck(false);
        return;
    case screens::Action::OtaInstall:
        runOtaCheck(true);
        return;
    }
}

void App::handleInput(Input::ActionMask actions, uint32_t nowMs) {
    if (serialCompanion_.active()) {
        if (Input::hasAction(actions, Input::ActionBack) || Input::hasAction(actions, Input::ActionOpenMenu)) {
            serialCompanion_.close();
            renderScreen(nowMs);
        }
        return;
    }
    if (screen_ == screens::Screen::Standby) {
        exitStandby(nowMs);
        return;
    }
    if (backgroundJobActive() || screen_ == screens::Screen::Status)
        return;
    if (usbTransfer_.active() && Input::hasAction(actions, Input::ActionPowerOff)) {
        exitUsbTransfer();
        return;
    }
    if (usbTransfer_.active() && Input::hasAction(actions, Input::ActionOpenMenu)) {
        exitUsbTransfer();
        return;
    }
    if (usbTransfer_.active() && Input::hasAction(actions, Input::ActionBack)) {
        exitUsbTransfer(screens::Screen::Read);
        return;
    }
    if (Input::hasAction(actions, Input::ActionOpenMenu)) {
        if (screen_ == screens::Screen::Reader) {
            ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
            ReadingLoop::pause(readerScreen_.session);
            libraryScreen_.invalidate();
            screen_ = screens::Screen::Read;
        } else {
            if (companionApi_.active()) {
                companionApi_.end();
            } else {
                networkScreen_.closeWifi();
            }
            if (screen_ == screens::Screen::FocusSession)
                focusScreen_.close();
            ReadingLoop::pause(readerScreen_.session);
            screen_ = screens::Screen::Reader;
        }
        renderScreen(nowMs);
        return;
    }
    if (screen_ == screens::Screen::FocusSession
        && (Input::hasAction(actions, Input::ActionBack) || Input::hasAction(actions, Input::ActionSelect))) {
        focusScreen_.close();
        screen_ = screens::Screen::FocusTimers;
        renderScreen(nowMs);
        return;
    }
    if (Input::hasAction(actions, Input::ActionPowerOff)) {
        powerOff(nowMs);
        return;
    }
    if (Input::hasAction(actions, Input::ActionStandby)) {
        enterStandby(nowMs);
        return;
    }
    if (Input::hasAction(actions, Input::ActionBack)) {
        if (companionApi_.active()) {
            companionApi_.end();
            screen_ = screens::Screen::Device;
            renderScreen(nowMs);
        } else if (screen_ != screens::Screen::Reader) {
            if (screen_ == screens::Screen::Read) {
                ReadingLoop::pause(readerScreen_.session);
                screen_ = screens::Screen::Reader;
            } else if (screen_ == screens::Screen::StorageEncryption || screen_ == screens::Screen::Sync
                       || screen_ == screens::Screen::Ota) {
                screen_ = screens::Screen::Device;
            } else if (screen_ == screens::Screen::WifiConnect) {
                networkScreen_.closeWifi();
                screen_ = screens::Screen::WifiScan;
            } else if (screen_ == screens::Screen::WifiScan || screen_ == screens::Screen::NetworkEdit) {
                if (screen_ == screens::Screen::WifiScan)
                    networkScreen_.closeWifi();
                screen_ = screens::Screen::NetworkSettings;
            } else if (screen_ == screens::Screen::ReadingSettings || screen_ == screens::Screen::InterfaceSettings
                       || screen_ == screens::Screen::PacingSettings || screen_ == screens::Screen::TypographySettings
                       || screen_ == screens::Screen::ReaderSettings || screen_ == screens::Screen::NetworkSettings) {
                screen_ = screens::Screen::Settings;
            } else if (screen_ == screens::Screen::BookFonts) {
                screen_ = screens::Screen::Read;
            } else if (screen_ == screens::Screen::FocusNameEdit) {
                screen_ = screens::Screen::FocusEditor;
            } else if (screen_ == screens::Screen::FocusEditor) {
                screen_ = screens::Screen::FocusTimers;
            } else {
                screen_ = screens::Screen::Read;
            }
            renderScreen(nowMs);
        } else {
            ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
            ReadingLoop::pause(readerScreen_.session);
            libraryScreen_.invalidate();
            screen_ = screens::Screen::Read;
            renderScreen(nowMs);
        }
        return;
    }
    if (Input::hasAction(actions, Input::ActionSelect) || Input::hasAction(actions, Input::ActionPlayPause)) {
        if (screen_ == screens::Screen::Reader && !typographyJobActive()) {
            readerScreen_.toggle(prefs_, nowMs);
        }
    }
}

void App::handleTouch(uint32_t nowMs) {
    if (serialCompanion_.active())
        return;
    const ui::Touch* touch = immediateUi_.touch();
    if (touch == nullptr)
        return;
    if (screen_ == screens::Screen::Standby) {
        exitStandby(nowMs);
        return;
    }
    if (companionApi_.active() || usbTransfer_.active() || backgroundJobActive() || screen_ == screens::Screen::Status)
        return;
    if (screen_ == screens::Screen::Reader) {
        readerScreen_.handleTouch(immediateUi_, nowMs, prefs_, settingsStore_);
    } else {
        renderScreen(nowMs);
    }
}

void App::showTransientStatus(std::string_view title, std::string_view line1, std::string_view line2,
                              uint32_t durationMs, screens::Screen destination, int progressPercent) {
    screen_ = screens::Screen::Status;
    statusDestination_ = destination;
    statusUntilMs_ = millis() + durationMs;
    restartAfterStatus_ = false;
    screens::status(immediateUi_, title, line1, line2, progressPercent);
}

void App::updateBackgroundJob() {
    JobUpdate update;
    while (jobQueue_ != nullptr && xQueueReceive(jobQueue_, &update, 0) == pdTRUE) {
        if (!update.complete) {
            screens::status(immediateUi_, update.title, update.line1, update.line2, update.progressPercent);
            continue;
        }

        const JobKind completed = jobKind_;
        jobKind_ = JobKind::None;
        vQueueDelete(jobQueue_);
        jobQueue_ = nullptr;
        Logger::checkpoint("running");
        if (completed == JobKind::Typography) {
            if (bookOpenPending_) {
                const size_t index = pendingBookIndex_;
                bookOpenPending_ = false;
                typographyRefreshPending_ = false;
                typographyOpensBook_ = false;
                runBookOpen(index, millis());
                return;
            }
            if (typographyRefreshPending_) {
                typographyRefreshPending_ = false;
                requestTypographyRefresh();
                return;
            }
            immediateUi_.invalidate();
            if (typographyOpensBook_) {
                typographyOpensBook_ = false;
                ReadingLoop::pause(readerScreen_.session);
                screen_ = screens::Screen::Reader;
                statusUntilMs_ = 0;
            }
            renderScreen(millis());
            return;
        }
        if (completed == JobKind::Book) {
            const int loadedIndex = storage_.findBook(readerScreen_.store.sourcePath());
            const BookLibrary::Entry* book =
                storage_.book(jobBookLoaded_ && loadedIndex >= 0 ? static_cast<size_t>(loadedIndex) : jobBookIndex_);
            const std::string_view bookName = book == nullptr ? std::string_view{} : BookLibrary::displayName(*book);
            if (jobBookLoaded_) {
                readerScreen_.finishBookOpen(prefs_, millis());
                ReadingLoop::pause(readerScreen_.session);
                typographyOpensBook_ = true;
                screens::status(immediateUi_, immediateUi_.text(UiText::OpeningBook), bookName, {}, 85);
                if (!requestTypographyRefresh()) {
                    typographyOpensBook_ = false;
                    screen_ = screens::Screen::Reader;
                    statusUntilMs_ = 0;
                    renderScreen(millis());
                }
            } else {
                showTransientStatus(immediateUi_.text(UiText::BookFailed), bookName,
                                    immediateUi_.text(UiText::CheckSdCard), 1200, screens::Screen::Library);
            }
            return;
        }
        if (completed == JobKind::Rss) {
            libraryScreen_.invalidate();
            showTransientStatus("RSS", update.line1, update.line2, 1400, screens::Screen::Reader);
            return;
        }
        if (completed == JobKind::StorageCheck) {
            showTransientStatus(immediateUi_.text(UiText::Storage), update.line1, update.line2, 1800,
                                screens::Screen::Device);
            return;
        }

        const bool reboot = completed == JobKind::OtaInstall && update.rebootRequired;
        showTransientStatus("OTA", update.line1, update.line2, reboot ? 500 : 1400, screens::Screen::Reader);
        restartAfterStatus_ = reboot;
        return;
    }
}

bool App::startBackgroundJob(JobKind kind) {
    if (backgroundJobActive())
        return false;
    if (jobQueue_ == nullptr)
        jobQueue_ = xQueueCreate(kJobQueueLength, sizeof(JobUpdate));
    if (jobQueue_ == nullptr)
        return false;
    xQueueReset(jobQueue_);
    jobKind_ = kind;
    if (xTaskCreate(backgroundJobEntry, "background", kJobStackBytes, this, kJobPriority, nullptr) != pdPASS) {
        jobKind_ = JobKind::None;
        vQueueDelete(jobQueue_);
        jobQueue_ = nullptr;
        return false;
    }
    return true;
}

void App::backgroundJobEntry(void* context) {
    ESP_LOGI("background", "started task=%s core=%d", pcTaskGetName(nullptr), xPortGetCoreID());
    static_cast<App*>(context)->runBackgroundJob();
    ESP_LOGI("background", "finished task=%s core=%d", pcTaskGetName(nullptr), xPortGetCoreID());
    vTaskDelete(nullptr);
}

void App::runBackgroundJob() {
    JobUpdate complete;
    switch (jobKind_) {
    case JobKind::Rss: {
        Logger::checkpoint("rss_update");
        Preferences preferences;
        RssFeeds::Result result;
        if (!preferences.begin(settings::kStateNvsNamespace)) {
            result.summary = "RSS failed";
            result.detail = "Could not open state";
        } else {
            result = RssFeeds::check(preferences, settingsStore_.settings(), settingsStore_.secrets(),
                                     &App::renderStorageStatus, this);
            preferences.end();
        }
        storage_.refreshBooks();
        copyText(complete.line1, result.summary.c_str());
        copyText(complete.line2, result.detail.c_str());
        break;
    }
    case JobKind::StorageCheck: {
        Logger::checkpoint("storage_check");
        const StorageMigration::Report report =
            StorageMigration::repair(storage_.mounted(), {
                                                            .libraryItems = storage_.books().size(),
                                                            .fonts = readerScreen_.fonts.families().size() - 1,
                                                            .themes = interfaceScreen_.themes.themes().size() - 1,
                                                        });
        if (storage_.mounted()) {
            storage_.refreshBooks();
            readerScreen_.fonts.loadFromSd();
            interfaceScreen_.themes.loadFromSd();
            localeCatalog_ =
                locales::scanInstalled(Board::Storage::filesystem(), static_cast<size_t>(UiText::Count));
        }
        const std::string resultDetail = report.issues.empty()
                                           ? "Checked " + std::to_string(report.checked) + ", moved "
                                                 + std::to_string(report.moved) + ", cleaned "
                                                 + std::to_string(report.removed)
                                           : report.issues.front();
        copyText(complete.line1,
                 report.healthy ? report.diagnosticSummary.c_str() : "Storage needs attention");
        copyText(complete.line2, resultDetail.c_str());
        break;
    }
    case JobKind::OtaCheck: {
        Logger::checkpoint("ota_check");
        const OtaUpdater::Result result =
            OtaUpdater::checkOnly(settingsStore_.settings(), settingsStore_.secrets(), &App::renderStorageStatus, this);
        copyText(complete.line1, result.summary.c_str());
        copyText(complete.line2, result.detail.c_str());
        break;
    }
    case JobKind::OtaInstall: {
        Logger::checkpoint("ota_install");
        const OtaUpdater::Result result =
            OtaUpdater::checkAndInstall(settingsStore_.settings(), settingsStore_.secrets(), &App::renderStorageStatus,
                                        this);
        copyText(complete.line1, result.summary.c_str());
        copyText(complete.line2, result.detail.c_str());
        complete.rebootRequired = result.rebootRequired;
        break;
    }
    case JobKind::Book: {
        Logger::checkpoint("book_open");
        jobBookLoaded_ = storage_.loadIndexedBook(jobBookIndex_, readerScreen_.store, readerScreen_.session.metadata);
        break;
    }
    case JobKind::Typography:
        Logger::checkpoint("typography");
        readerScreen_.refreshTypography(settingsStore_.settings().reading, readerScreen_.session.state.overrides);
        break;
    case JobKind::None:
        break;
    }

    complete.complete = true;
    enqueueJobUpdate(complete, true);
}

bool App::requestTypographyRefresh() {
    if (typographyJobActive()) {
        typographyRefreshPending_ = true;
        return true;
    }
    if (backgroundJobActive())
        return false;

    if (startBackgroundJob(JobKind::Typography))
        return true;

    ESP_LOGW("reader", "typography task unavailable; preparing synchronously");
    readerScreen_.refreshTypography(settingsStore_.settings().reading, readerScreen_.session.state.overrides);
    return false;
}

void App::enqueueJobUpdate(JobUpdate update, bool mustSucceed) {
    if (mustSucceed) {
        xQueueSend(jobQueue_, &update, portMAX_DELAY);
        return;
    }
    if (xQueueSend(jobQueue_, &update, 0) == pdTRUE)
        return;
    JobUpdate discarded;
    xQueueReceive(jobQueue_, &discarded, 0);
    xQueueSend(jobQueue_, &update, 0);
}

void App::runRss() {
    ReadingLoop::pause(readerScreen_.session);
    screen_ = screens::Screen::Status;
    statusUntilMs_ = 0;
    screens::status(immediateUi_, "RSS", immediateUi_.text(UiText::CheckingFeeds));
    if (!startBackgroundJob(JobKind::Rss))
        showTransientStatus("RSS", immediateUi_.text(UiText::CouldNotStart), {}, 1200, screens::Screen::Reader);
}

void App::runBookOpen(size_t index, uint32_t nowMs) {
    if (!storage_.mounted() || index >= storage_.books().size())
        return;
    const BookLibrary::Entry& book = storage_.books()[index];
    if (typographyJobActive()) {
        pendingBookIndex_ = index;
        bookOpenPending_ = true;
        screens::status(immediateUi_, immediateUi_.text(UiText::OpeningBook), BookLibrary::displayName(book), {}, 5);
        screen_ = screens::Screen::Status;
        statusUntilMs_ = 0;
        return;
    }
    if (backgroundJobActive())
        return;

    jobBookIndex_ = index;
    jobBookLoaded_ = false;
    screens::status(immediateUi_, immediateUi_.text(UiText::OpeningBook), BookLibrary::displayName(book), {}, 5);
    screen_ = screens::Screen::Status;
    statusUntilMs_ = 0;
    readerScreen_.prepareBookOpen(prefs_, nowMs);
    if (!startBackgroundJob(JobKind::Book))
        showTransientStatus(immediateUi_.text(UiText::BookFailed), BookLibrary::displayName(book),
                            immediateUi_.text(UiText::CheckSdCard), 1200, screens::Screen::Library);
}

void App::enterUsbTransfer(uint32_t nowMs) {
#if RSVP_USB_TRANSFER_ENABLED
    if (serialCompanion_.active())
        return;
    ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
    ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
    settingsStore_.flush();
    if (!usbTransfer_.begin(true)) {
        showTransientStatus("USB", immediateUi_.text(UiText::CouldNotStart), usbTransfer_.statusMessage(), 1200,
                            screens::Screen::Reader);
        return;
    }
    ReadingLoop::pause(readerScreen_.session);
    screen_ = screens::Screen::Usb;
    renderScreen(nowMs);
#else
    showTransientStatus("USB", immediateUi_.text(UiText::Unavailable), {}, 1000, screens::Screen::Reader);
#endif
}

void App::exitUsbTransfer(screens::Screen destination) {
    usbTransfer_.end();
    storage_.refreshBooks();
    localeCatalog_ =
        locales::scanInstalled(Board::Storage::filesystem(), static_cast<size_t>(UiText::Count));
    readerScreen_.fonts.loadFromSd();
    applySettings();
    libraryScreen_.invalidate();
    screen_ = destination;
    renderScreen(millis());
}

void App::applySettings() {
    loadAppearanceSettings();
    readerScreen_.applyTheme(interfaceScreen_.themes.resolve(settingsStore_.settings().interface.selectedThemeId));
    requestTypographyRefresh();
    networkScreen_.begin(settingsStore_);
    networkScreen_.startupCheckPending = false;
}

void App::loadAppearanceSettings() {
    auto& current = settingsStore_.settings();
    bool corrected = false;
    immediateUi_.setLanguageCatalog(storage_.mounted() ? &Board::Storage::filesystem() : nullptr, &localeCatalog_,
                                    &locales::loadUiFont);
    if (!readerScreen_.fonts.find(current.reading.typography.fontId)) {
        current.reading.typography.fontId = settings::TypographySettings{}.fontId;
        corrected = true;
    }
    if (current.interface.locale != Localization::kDefaultLocale
        && !locales::findPackForLocale(localeCatalog_, current.interface.locale)) {
        current.interface.locale = Localization::kDefaultLocale;
        corrected = true;
    }

    reloadUiAssets();
    corrected |=
        interfaceScreen_.begin(immediateUi_, current.interface, localeCatalog_, &Board::Display::setBrightness);
    if (corrected)
        settingsStore_.acceptChanges();
}

void App::reloadUiAssets() {
    locales::UiAssets assets;
    if (storage_.mounted()) {
        auto loaded =
            locales::loadUiAssets(Board::Storage::filesystem(), localeCatalog_,
                                  settingsStore_.settings().interface.locale, static_cast<size_t>(UiText::Count));
        if (loaded)
            assets = std::move(*loaded);
        else
            ESP_LOGW("languages", "selected UI pack rejected: %s", loaded.error().c_str());
    }
    immediateUi_.setLanguageAssets(std::move(assets));
}

void App::runOtaCheck(bool install) {
    ReadingLoop::pause(readerScreen_.session);
    if (install) {
        const uint32_t nowMs = millis();
        ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
        ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
        settingsStore_.flush();
    }
    screen_ = screens::Screen::Status;
    statusUntilMs_ = 0;
    screens::status(immediateUi_, "OTA", immediateUi_.text(UiText::Checking));
    if (!startBackgroundJob(install ? JobKind::OtaInstall : JobKind::OtaCheck))
        showTransientStatus("OTA", immediateUi_.text(UiText::CouldNotStart), {}, 1200, screens::Screen::Reader);
}

void App::enterStandby(uint32_t nowMs) {
    if (companionApi_.active()) {
        companionApi_.end();
    } else {
        networkScreen_.closeWifi();
    }
    ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
    ReadingLoop::pause(readerScreen_.session);
    if (settingsStore_.settings().interface.screensaver == standby::Kind::screenOff) {
        screen_ = screens::Screen::Standby;
        lightSleepFromStandby();
        return;
    }
    Board::Display::wake();
    immediateUi_.invalidate();
    const int bookIndex = storage_.findBook(readerScreen_.session.sourcePath());
    standbyScreen_.begin(immediateUi_, nowMs, bookIndex < 0 ? 0 : static_cast<size_t>(bookIndex),
                         readerScreen_.session.state.wordIndex, settingsStore_.settings().interface.screensaver);
    standbyEnteredMs_ = nowMs;
    screen_ = screens::Screen::Standby;
    renderScreen(nowMs);
}

void App::exitStandby(uint32_t nowMs) {
    standbyScreen_.reset();
    Board::Display::wake();
    lastActivityMs_ = nowMs;
    screen_ = screens::Screen::Reader;
    renderScreen(nowMs);
}

void App::lightSleepFromStandby() {
    ESP_LOGI("app", "screen-off standby; entering light sleep");
    ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
    standbyScreen_.reset();
    if (usbTransfer_.active())
        usbTransfer_.end();
    settingsStore_.flush();
    Board::Display::sleep();
    Input::cancel();

    bool wokeByTouch = false;
    const uint32_t sleepStartedAtMs = millis();
    while (true) {
        const uint32_t elapsedMs = millis() - sleepStartedAtMs;
        if (elapsedMs >= kStandbyPowerOffMs) {
            ESP_LOGI("app", "screen-off standby expired; powering off");
            powerOff(millis());
            return;
        }

        switch (Board::System::lightSleep(kStandbyPowerOffMs - elapsedMs)) {
        case EspLightSleep::WakeReason::timer:
            ESP_LOGI("app", "screen-off standby expired; powering off");
            powerOff(millis());
            return;
        case EspLightSleep::WakeReason::input:
            if constexpr (Board::Config::HAS_LIGHT_SLEEP_TOUCH_IRQ) {
                ui::TouchContact contact = {};
                wokeByTouch = Board::Input::touchReady() && Board::Input::readTouch(contact) && contact.touched;
                const ::Input::PressActions controls = Board::Input::currentActions();
                const bool powerPressed = ::Input::hasAction(controls.longPress, ::Input::ActionPowerOff);
                if (!wokeByTouch && !powerPressed)
                    continue;
            }
            break;
        case EspLightSleep::WakeReason::error:
            break;
        }
        break;
    }

    const uint32_t wokeAtMs = millis();
    Input::resume();
    exitStandby(wokeAtMs);

    if (wokeByTouch) {
        const uint32_t releaseWaitStartedMs = millis();
        ui::TouchContact contact = {.touched = true};
        while (contact.touched && millis() - releaseWaitStartedMs < 1000) {
            delay(10);
            if (!Board::Input::readTouch(contact))
                break;
        }
    }
}

void App::powerOff(uint32_t nowMs) {
    if (companionApi_.active())
        companionApi_.end();
    if (screen_ == screens::Screen::FocusSession)
        focusScreen_.close();
    ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
    ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
    ReadingLoop::pause(readerScreen_.session);
    screens::status(immediateUi_, immediateUi_.text(UiText::Off), immediateUi_.text(UiText::ReleasePower));
    delay(250);
    settingsStore_.flush();
    Board::Display::sleep();
    readerScreen_.store.close();
    storage_.end();
    Input::end();
    powerOffBoard();
}

void App::renderStorageStatus(void* context, const char* title, const char* line1, const char* line2,
                              int progressPercent) {
    if (context == nullptr)
        return;

    App& app = *static_cast<App*>(context);
    if (app.backgroundJobActive()) {
        JobUpdate update;
        copyText(update.title, title == nullptr ? "SD" : title);
        copyText(update.line1, line1);
        copyText(update.line2, line2);
        update.progressPercent = progressPercent;
        app.enqueueJobUpdate(update);
        return;
    }
    screens::status(app.immediateUi_, title == nullptr ? "SD" : title, line1 == nullptr ? "" : line1,
                    line2 == nullptr ? "" : line2, progressPercent);
}
