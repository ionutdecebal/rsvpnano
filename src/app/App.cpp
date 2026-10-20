#include "app/App.h"
#include <esp_log.h>

#include <array>
#include <cstdio>
#include <esp_system.h>
#include <esp_random.h>
#include <string>

#include "board/BoardAudio.h"
#include "board/BoardConfig.h"
#include "board/BoardInput.h"
#include "board/BoardPower.h"
#include "board/BoardStorage.h"
#include "board/BoardSystem.h"
#include "freertos/task.h"
#include "logging/Logger.h"
#include "feeds/RssFeeds.h"
#include "settings/NvsSecurity.h"
#include "library/ReadingProgress.h"
#include "storage/migration/Migration.h"
#include "update/OtaUpdater.h"
#include "update/SwitchSettings.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include "dictionary/DictionaryInstaller.h"

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
    const bool modernStartup = immediateUi_.width() == 640 && immediateUi_.height() == 172;
    immediateUi_.setStartupPresentation(modernStartup);
    const auto startupStage = [&](UiText title) {
        if (modernStartup)
            screens::status(immediateUi_, immediateUi_.text(title), immediateUi_.text(UiText::Checking));
    };
    if (modernStartup)
        startupStage(UiText::Storage);
    else
        screens::status(immediateUi_, immediateUi_.text(UiText::Ready));
    if (!Input::begin())
        ESP_LOGE("input", "startup failed");
    immediateUi_.setTouchSource({.surface = Board::Input::touchSurface(), .poll = &Input::pollTouch});

    storage_.begin();
    if (storage_.mounted() && !dictionary::recoverInstall())
        ESP_LOGW("dictionary", "Interrupted dictionary install needs SD recovery");
    Logger::startupCheckpoint("storage");
    fs::FS* filesystem = storage_.mounted() ? &Board::Storage::filesystem() : nullptr;
    startupStage(UiText::Settings);
    if (auto result = settingsStore_.begin(filesystem); !result)
        ESP_LOGW("settings", "startup warning: %s", result.error().message.c_str());
    Logger::startupCheckpoint("settings");
    Logger::checkpoint("locale_catalog");
    startupStage(UiText::Language);
    localeCatalog_ = filesystem == nullptr
                       ? locales::Catalog{}
                       : locales::scanInstalled(*filesystem, static_cast<size_t>(UiText::Count));
    ESP_LOGI("languages", "catalog ready installed=%u", static_cast<unsigned>(localeCatalog_.size()));
    Logger::startupCheckpoint("locale_catalog");
    startupStage(UiText::FontSection);
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
    startupStage(UiText::Library);
    readerScreen_.loadInitialBook(immediateUi_, storage_, prefs_, bootMs_);
    Logger::startupCheckpoint("book");
    libraryScreen_.invalidate();
    // Leave the last real status (including failures) visible until the usual
    // transition. Do not introduce a second splash delay or fake a Ready state.
    if(modernStartup){
        statusDestination_=settingsStore_.settings().interface.onboardingComplete?screens::Screen::Read:screens::Screen::Onboarding;
        if(!settingsStore_.settings().interface.onboardingComplete)onboardingScreen_.begin(immediateUi_,settingsStore_.settings(),millis());
        if(settingsStore_.settings().reading.mode==settings::ReadingMode::page){
            settingsStore_.settings().reading.mode=settings::ReadingMode::rsvp;settingsStore_.acceptChanges();
        }
    }
    immediateUi_.setStartupPresentation(false);
    ESP_LOGI("startup", "ready");
}

void App::update(uint32_t nowMs) {
    if (focusScreen_.active()) {
        if (focusScreen_.update(nowMs)) Board::Audio::beep();
        if (focusScreen_.breakVisible()) {
            if (!backgroundJobActive() && readerScreen_.session.playing)
                readerScreen_.pauseImmediately(prefs_, nowMs);
            screen_ = screens::Screen::FocusSession;
        }
    }
    if(onboardingJobActive()){
        onboardingScreen_.update(nowMs);
        if(screen_!=screens::Screen::Onboarding || !onboardingScreen_.active()
            || !onboardingScreen_.storageChecking() || onboardingScreen_.storageRequest()!=onboardingJobRequest_)
            onboardingStorageCancel_.store(true);
    }
    immediateUi_.setBackgroundBusy(backgroundJobActive()&&!(onboardingJobActive()&&screen_==screens::Screen::Onboarding));
    immediateUi_.setFocusBadge(focusScreen_.badge(nowMs));
    if(!backgroundJobActive())serialCompanion_.update(nowMs);
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
    immediateUi_.setBackgroundBusy(backgroundJobActive()&&!(onboardingJobActive()&&screen_==screens::Screen::Onboarding));
    const bool preparingTypography = typographyJobActive();
    if (focusScreen_.breakVisible()) {
        screen_ = screens::Screen::FocusSession;
        immediateUi_.setModernPresentation(true);
        immediateUi_.setModernReadingPalette(false);
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_), focusScreen_.frameSurface(screen_));
        const auto action = focusScreen_.draw(immediateUi_, nowMs, screen_);
        immediateUi_.endFrame();
        // A loading worker owns the book; a manual resume waits until it is finished.
        handleScreenAction(action, nowMs);
        lastActivityMs_ = nowMs;
        return;
    }

    if (backgroundJobActive() && !preparingTypography) {
        if(onboardingJobActive()&&screen_==screens::Screen::Onboarding)renderScreen(nowMs);
        else screens::animateStatus(immediateUi_, nowMs);
        return;
    }

    if (screen_ == screens::Screen::Status) {
        if (statusUntilMs_ == 0 || !deadlineReached(nowMs, statusUntilMs_))
            return;
        statusUntilMs_ = 0;
        if (restartAfterStatus_) {
            ESP.restart();
            return;
        }
        screen_ = statusDestination_;
        if (networkScreen_.startupCheckPending && screen_ != screens::Screen::Onboarding) {
            networkScreen_.startupCheckPending = false;
            runOtaCheck(false, false, true);
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
        companionApi_.renderStatus(serialCompanion_.active());
        settingsStore_.update(nowMs);
        Board::Power::updateBattery(battery_, nowMs);
        return;
    }

    // Finish/Skip saves setup explicitly. Do not let an automatic settings
    // retry block the practice/error screens on the card they just rejected.
    if (!usbTransfer_.active() && screen_ != screens::Screen::Onboarding)
        settingsStore_.update(nowMs);

    Board::Power::updateBattery(battery_, nowMs);
    if (!preparingTypography)
        readerScreen_.update(prefs_, nowMs);
    if (screen_ == screens::Screen::FocusSession && !focusScreen_.manual()) {
        if (focusScreen_.update(nowMs))
            Board::Audio::beep();
    }
    ReadingProgress::save(readerScreen_.session, prefs_, false, nowMs);

    renderScreen(nowMs);
    if (!preparingTypography && !readerScreen_.session.playing && !companionApi_.active() && !usbTransfer_.active()
        && !focusScreen_.active() && screen_ != screens::Screen::FocusSession && screen_ != screens::Screen::Status
        && screen_ != screens::Screen::Onboarding
        && kStandbyMs[settingsStore_.settings().interface.standbyTimerIndex] > 0
        && nowMs - lastActivityMs_ >= kStandbyMs[settingsStore_.settings().interface.standbyTimerIndex]) {
        enterStandby(nowMs);
    }
}

void App::renderScreen(uint32_t nowMs) {
    if (dictionaryWifiReturn_ && (screen_ == screens::Screen::Connections
        || screen_ == screens::Screen::Read || screen_ == screens::Screen::Reader)) {
        dictionaryWifiReturn_ = false; screen_ = screens::Screen::DictionaryInstall;
    }
    if (dictionarySetup_.onboarding && (screen_ == screens::Screen::Read || screen_ == screens::Screen::Reader))
        screen_ = screens::Screen::Onboarding;
    if (screen_ == screens::Screen::Onboarding) dictionarySetup_.onboarding = false;
    if (focusScreen_.breakVisible()) screen_ = screens::Screen::FocusSession;
    if (!backgroundJobActive() && bookSettings_.ready()
        && screen_ != screens::Screen::BookFonts && screen_ != screens::Screen::Status)
        bookSettings_.clear();
    if (screen_ != screens::Screen::Reader) readerScreen_.cancelModernControls();
    if (screen_ != screens::Screen::Onboarding && onboardingScreen_.active()
        && !dictionarySetup_.onboarding) onboardingScreen_.close();
    // Presentation only: the real RSVP/page renderer keeps its font and geometry paths.
    immediateUi_.setModernPresentation(screens::modern::enabled(immediateUi_.width(), immediateUi_.height(), screen_));
    immediateUi_.setModernReadingPalette(immediateUi_.width() == 640 && immediateUi_.height() == 172
                                        && screen_ == screens::Screen::Reader);
    if (serialCompanion_.active()) {
        companionApi_.renderStatus(true);
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
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_), readerScreen_.pageBrowsing());
        readerScreen_.draw(immediateUi_, storage_, battery_, nowMs);
        immediateUi_.endFrame();
        return;
    case screens::Screen::FindPlace:
    case screens::Screen::FindChapters:
    case screens::Screen::BookSearch:
    case screens::Screen::BookSearchKeyboard:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        readerScreen_.drawLocator(immediateUi_, screen_, prefs_, nowMs);
        immediateUi_.endFrame();
        break;
    case screens::Screen::Library: {
        if (libraryScreen_.needsItems(storage_)) {
            screen_ = screens::Screen::Status;
            statusUntilMs_ = 0;
            screens::status(immediateUi_, "Loading library", "Getting your books ready", {});
            if (!startBackgroundJob(JobKind::Library))
                showTransientStatus("Library unavailable", "Please try again", {}, 1200, screens::Screen::Read);
            return;
        }
        const auto& items = libraryScreen_.items(storage_, readerScreen_.store, readerScreen_.session);
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        const screens::Action result = libraryScreen_.draw(immediateUi_, items, nowMs, screen_);
        immediateUi_.endFrame();
        if (result == screens::Action::OpenBook) {
            runBookOpen(libraryScreen_.selectedIndex(), nowMs);
        } else if (result == screens::Action::OpenBookSettings) {
            readerScreen_.pauseImmediately(prefs_, nowMs);
            jobBookIndex_ = libraryScreen_.selectedIndex();
            bookSettings_.clear();
            screens::resetBookFonts();
            screen_ = screens::Screen::Status;
            statusUntilMs_ = 0;
            screens::status(immediateUi_, "Book settings", "Loading language and fonts");
            if (!startBackgroundJob(JobKind::BookSettings))
                showTransientStatus("Settings unavailable", "Please try again", {}, 1200, screens::Screen::Library);
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
            const auto title = ReadingProgress::title(readerScreen_.session, storage_);
            const auto author = book == nullptr ? std::string_view{} : std::string_view{book->author};
            const auto progress = ReadingProgress::percent(readerScreen_.session.state.wordIndex,
                                                            ReadingLoop::wordCount(readerScreen_.session));
            if (immediateUi_.modernPresentation()) {
                auto& interface = settingsStore_.settings().interface;
                const uint8_t brightness = interface.brightnessPercent;
                const auto& chapters = readerScreen_.session.metadata.chapters;
                const auto chapter = std::ranges::upper_bound(chapters, readerScreen_.session.state.wordIndex,
                                                              {}, &ChapterMarker::wordIndex) - chapters.begin();
                action = screens::modern::menu(immediateUi_, title, author, progress,
                                               ReadingLoop::wordCount(readerScreen_.session) > 0, interface, screen_, chapter);
                if (brightness != interface.brightnessPercent) {
                    Board::Display::setBrightness(interface.brightnessPercent);
                    settingsStore_.acceptChanges();
                }
            } else {
                action = screens::read(immediateUi_, title,
                                   book == nullptr ? std::string_view{} : std::string_view{book->author},
                                   progress, screen_);
            }
        }
        break;
    case screens::Screen::QuickControls: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        auto& config=settingsStore_.settings();
        const auto speed=config.reading.wpm;const auto brightness=config.interface.brightnessPercent;
        action=screens::modern::quickControls(immediateUi_,config.reading,config.interface,screen_);
        if(speed!=config.reading.wpm || brightness!=config.interface.brightnessPercent){
            if(brightness!=config.interface.brightnessPercent)Board::Display::setBrightness(config.interface.brightnessPercent);
            settingsStore_.acceptChanges();
        }
        break;
    }
    case screens::Screen::Dictionary:
    case screens::Screen::DictionarySearch:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action=dictionary_.draw(immediateUi_,readerScreen_.session,screen_,nowMs);
        break;
    case screens::Screen::Onboarding: {
        if(!onboardingScreen_.active())onboardingScreen_.begin(immediateUi_,settingsStore_.settings(),nowMs);
        onboardingScreen_.setStorageBusy(onboardingJobActive());
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_),onboardingScreen_.surface());
        const auto locale=settingsStore_.settings().interface.locale;
        action=onboardingScreen_.draw(immediateUi_,settingsStore_.settings(),nowMs);
        if(onboardingScreen_.takeSettingsChanged())settingsStore_.acceptChanges();
        if(locale!=settingsStore_.settings().interface.locale){
            if(onboardingJobActive()||onboardingScreen_.storageProgress().state!=OnboardingStorage::State::Ready)onboardingReloadUiPending_=true;
            else reloadUiAssets();
        }
        if(onboardingJobActive()&&(!onboardingScreen_.storageChecking()||onboardingScreen_.storageRequest()!=onboardingJobRequest_))
            onboardingStorageCancel_.store(true);
        break;
    }
    case screens::Screen::TouchCheck:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action=touchCheckScreen_.draw(immediateUi_,screen_);
        break;
    case screens::Screen::Help:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action=screens::OnboardingScreen::help(immediateUi_,settingsStore_.settings().reading,screen_);
        break;
    case screens::Screen::AddBooks:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action=screens::modern::header(immediateUi_,screens::modern::copy(immediateUi_,screens::modern::Copy::AddBooks),screen_,screens::Screen::Help);
        if(action==screens::Action::None)onboardingScreen_.books(immediateUi_);
        break;
    case screens::Screen::ButtonMap:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action=screens::modern::header(immediateUi_,screens::modern::copy(immediateUi_,screens::modern::Copy::PhysicalButtons),screen_,screens::Screen::Help);
        onboardingScreen_.buttons(immediateUi_);
        break;
    case screens::Screen::Connections:
    case screens::Screen::DictionaryHelp:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::modern::hub(immediateUi_, screen_);
        break;
    case screens::Screen::DictionaryInstall:
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        action = screens::modern::dictionarySetup(immediateUi_, dictionarySetup_, screen_);
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
    case screens::Screen::ReadingHub:
    case screens::Screen::ReadingSettings: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        const settings::ReadingMode mode = settingsStore_.settings().reading.mode;
        const bool leftHanded = settingsStore_.settings().reading.leftHanded;
        if (screens::readingSettings(immediateUi_, settingsStore_.settings().reading, screen_)) {
            settingsStore_.acceptChanges();
            if (mode != settingsStore_.settings().reading.mode)
                requestTypographyRefresh();
            if (leftHanded != settingsStore_.settings().reading.leftHanded) {
                immediateUi_.endFrame();
                immediateUi_.setOrientation(settingsStore_.settings().reading.leftHanded
                                                ? Board::Display::rotatedUiOrientation()
                                                : Board::Display::defaultUiOrientation());
                renderScreen(nowMs);
                return;
            }
        }
        break;
    }
    case screens::Screen::DisplayHub:
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
    case screens::Screen::ReaderAppearance: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (readerScreen_.appearance(immediateUi_, screen_)) {
            settingsStore_.acceptChanges();
        }
        break;
    }
    case screens::Screen::BookFonts: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_));
        if (bookSettings_.ready()) {
            const auto previous = bookSettings_.overrides;
            if (screens::bookFonts(immediateUi_, bookSettings_.metadata, bookSettings_.overrides,
                                   localeCatalog_, readerScreen_.fonts, screen_, screens::Screen::Library,
                                   bookSettings_.title)) {
                if (!bookSettings_.save()) {
                    bookSettings_.overrides = previous;
                    showTransientStatus("Settings not saved", "Check the SD card and try again", {}, 1800,
                                        screens::Screen::BookFonts);
                } else if (bookSettings_.path == readerScreen_.session.sourcePath()) {
                    readerScreen_.session.state.overrides = bookSettings_.overrides;
                    requestTypographyRefresh();
                }
            }
        } else if (screens::bookFonts(immediateUi_, readerScreen_.session.metadata, readerScreen_.session.state.overrides,
                               localeCatalog_, readerScreen_.fonts, screen_)) {
            ReadingProgress::mirror(readerScreen_.session, readerScreen_.store);
            requestTypographyRefresh();
        }
        break;
    }
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
    case screens::Screen::StorageHub:
    case screens::Screen::PowerHub:
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
        action = screens::ota(immediateUi_, OtaUpdater::currentVersion().data(), screen_, &otaOffer_);
        break;
    }
    case screens::Screen::FocusPresets:
    case screens::Screen::FocusTimers:
    case screens::Screen::FocusEditor:
    case screens::Screen::FocusNameEdit:
    case screens::Screen::FocusSession: {
        immediateUi_.beginFrame(static_cast<uint8_t>(screen_), focusScreen_.frameSurface(screen_));
        action = focusScreen_.draw(immediateUi_, nowMs, screen_);
        immediateUi_.endFrame();
        if (action != screens::Action::None) {
            handleScreenAction(action, nowMs);
            return;
        }
        if (!focusScreen_.manual() && renderedScreen == screens::Screen::FocusSession && screen_ != screens::Screen::FocusSession)
            focusScreen_.close();
        if (screen_ != renderedScreen)
            renderScreen(nowMs);
        return;
    }
    }
    immediateUi_.endFrame();
    if (screen_ != renderedScreen) {
        if (renderedScreen == screens::Screen::DictionaryInstall && screen_ == screens::Screen::NetworkSettings)
            dictionaryWifiReturn_ = true;
        if (screen_ == screens::Screen::Library)
            libraryScreen_.reset();
    }
    handleScreenAction(action, nowMs);
}

void App::handleScreenAction(screens::Action action, uint32_t nowMs) {
    if(onboardingJobActive()&&(action==screens::Action::FinishIntroduction||action==screens::Action::SkipIntroduction)){
        // Let the user practise in RAM while a slow filesystem returns, but never
        // open the library or a second storage operation over an unfinished job.
        onboardingStorageCancel_.store(true);onboardingScreen_.waitForStorage();renderScreen(nowMs);return;
    }
    if (backgroundJobActive() && action != screens::Action::None)
        return;
    switch (action) {
    case screens::Action::None:
    case screens::Action::OpenBook:
    case screens::Action::OpenBookSettings:
        return;
    case screens::Action::CycleLanguage:
        interfaceScreen_.cycleLanguage(immediateUi_, settingsStore_.settings().interface);
        settingsStore_.acceptChanges();
        reloadUiAssets();
        renderScreen(nowMs);
        return;
    case screens::Action::OpenDictionary:
        readerScreen_.pauseImmediately(prefs_,nowMs);
        dictionary_.open(readerScreen_.session);
        screen_=screens::Screen::Dictionary;
        renderScreen(nowMs);return;
    case screens::Action::OnlineDictionary:
        readerScreen_.pauseImmediately(prefs_,nowMs);
        screen_=screens::Screen::Status;statusUntilMs_=0;
        screens::status(immediateUi_,"Looking up word","Your book stays paused");
        if(!startBackgroundJob(JobKind::OnlineDictionary))
            showTransientStatus("Please try again","Worker could not start",{},1200,screens::Screen::Dictionary);
        return;
    case screens::Action::OpenDictionarySetup:
        dictionarySetup_ = {};
        dictionarySetup_.installed = dictionary::installedPack();
        dictionarySetup_.onboarding = screen_ == screens::Screen::Onboarding;
        screen_ = screens::Screen::DictionaryInstall;
        renderScreen(nowMs); return;
    case screens::Action::InstallDictionary:
        readerScreen_.pauseImmediately(prefs_, nowMs);
        dictionaryCancel_.store(false);
        screen_ = screens::Screen::Status; statusUntilMs_ = 0;
        screens::status(immediateUi_, "Installing dictionary", "Middle button cancels", {}, 0);
        if (!startBackgroundJob(JobKind::DictionaryInstall))
            showTransientStatus("Please try again", "Worker could not start", {}, 1200, screens::Screen::DictionaryInstall);
        return;
    case screens::Action::CheckOnboardingStorage:
        ReadingLoop::pause(readerScreen_.session);
        onboardingStorageCancel_.store(false);
        onboardingJobRequest_=onboardingScreen_.storageRequest();onboardingJobStartedMs_=nowMs;
        onboardingJobWasMounted_=storage_.mounted();
        if(!startBackgroundJob(JobKind::OnboardingStorage))
            onboardingScreen_.acceptStorageProgress(onboardingJobRequest_,{OnboardingStorage::State::TimedOut,0},nowMs);
        renderScreen(nowMs);return;
    case screens::Action::OpenPage:
        readerScreen_.pauseImmediately(prefs_,nowMs);readerScreen_.openPage();screen_=screens::Screen::Reader;
        renderScreen(nowMs);return;
    case screens::Action::ReplayIntroduction:
        readerScreen_.pauseImmediately(prefs_,nowMs);onboardingScreen_.begin(immediateUi_,settingsStore_.settings(),nowMs);
        screen_=screens::Screen::Onboarding;renderScreen(nowMs);return;
    case screens::Action::FinishIntroduction:
    case screens::Action::SkipIntroduction: {
        const bool complete=onboardingScreen_.canComplete();
        bool saved=true;
        if(complete){
            if(onboardingReloadUiPending_){onboardingReloadUiPending_=false;reloadUiAssets();}
            settingsStore_.settings().interface.onboardingComplete=true;settingsStore_.acceptChanges();
            if(auto result=settingsStore_.flush();!result){saved=false;ESP_LOGW("settings","introduction save pending: %s",result.error().message.c_str());}
        }
        onboardingScreen_.close();
        screen_=complete&&action==screens::Action::FinishIntroduction&&ReadingLoop::wordCount(readerScreen_.session)>0?screens::Screen::Reader:screens::Screen::Read;
        if(!saved){showTransientStatus(screens::modern::copy(immediateUi_,screens::modern::Copy::SdSavePending),
            screens::modern::copy(immediateUi_,screens::modern::Copy::SdSaveRetry),{},1800,screen_);return;}
        renderScreen(nowMs);return;
    }
    case screens::Action::FocusRead:
        if (!focusScreen_.breakVisible()) readerScreen_.resumeNow(nowMs);
        screen_ = screens::Screen::Reader;
        renderScreen(nowMs);
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
    case screens::Action::OtaSwitchCheck:
        runOtaCheck(false, true);
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
    if(onboardingJobActive()&&Input::hasAction(actions,Input::ActionPowerOff)){
        // Only a disposable probe may be open. Do not flush settings through the
        // same potentially stalled filesystem while powering down.
        onboardingStorageCancel_.store(true);powerOffBoard();return;
    }
    if (jobKind_ == JobKind::DictionaryInstall && (Input::hasAction(actions, Input::ActionOpenMenu)
        || Input::hasAction(actions, Input::ActionBack))) {
        dictionaryCancel_.store(true);
        return;
    }
    if ((backgroundJobActive()&&!(onboardingJobActive()&&screen_==screens::Screen::Onboarding)) || screen_ == screens::Screen::Status)
        return;
    if (focusScreen_.breakVisible() && !Input::hasAction(actions, Input::ActionPowerOff)) return;
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
    if(screen_==screens::Screen::Onboarding){
        if(Input::hasAction(actions,Input::ActionPowerOff)){onboardingScreen_.pause();powerOff(nowMs);}
        else if(Input::hasAction(actions,Input::ActionStandby)){if(!onboardingJobActive()){onboardingScreen_.pause();enterStandby(nowMs);}}
        else if(Input::hasAction(actions,Input::ActionOpenMenu)){onboardingScreen_.hardwareControls();renderScreen(nowMs);}
        else if(Input::hasAction(actions,Input::ActionBack)){onboardingScreen_.next(-1);renderScreen(nowMs);}
        else if(Input::hasAction(actions,Input::ActionSelect)||Input::hasAction(actions,Input::ActionPlayPause)){
            onboardingScreen_.hardwarePlay(nowMs);renderScreen(nowMs);
        }
        return;
    }
    if (Input::hasAction(actions, Input::ActionOpenMenu)) {
        if (screen_ == screens::Screen::Reader) {
            ReadingProgress::save(readerScreen_.session, prefs_, true, nowMs);
            ReadingLoop::pause(readerScreen_.session);
            screen_ = immediateUi_.modernReadingPalette() ? screens::Screen::QuickControls : screens::Screen::Read;
        } else {
            if (companionApi_.active()) {
                companionApi_.end();
            } else {
                networkScreen_.closeWifi();
            }
            if (screen_ == screens::Screen::FocusSession && !focusScreen_.manual())
                focusScreen_.close();
            ReadingLoop::pause(readerScreen_.session);
            screen_ = screens::Screen::Reader;
        }
        renderScreen(nowMs);
        return;
    }
    if (screen_ == screens::Screen::FocusSession && !focusScreen_.manual()
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
            } else if (screen_ == screens::Screen::BookSearchKeyboard) {
                screen_ = screens::Screen::BookSearch;
            } else if (screen_ == screens::Screen::BookSearch || screen_ == screens::Screen::FindChapters) {
                screen_ = screens::Screen::FindPlace;
            } else if (screen_ == screens::Screen::FindPlace) {
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
                       || screen_ == screens::Screen::PacingSettings || screen_ == screens::Screen::ReaderAppearance
                       || screen_ == screens::Screen::NetworkSettings) {
                screen_ = screen_ == screens::Screen::NetworkSettings && immediateUi_.modernPresentation()
                          ? screens::Screen::Connections : screens::Screen::Settings;
            } else if (screen_ == screens::Screen::BookFonts) {
                screens::resetBookFonts();
                screen_ = immediateUi_.modernPresentation() ? screens::Screen::Library : screens::Screen::Read;
            } else if (screen_ == screens::Screen::ReadingHub || screen_ == screens::Screen::DisplayHub
                       || screen_ == screens::Screen::Connections) {
                screen_ = screens::Screen::Settings;
            } else if (screen_ == screens::Screen::TouchCheck) {
                screen_ = screens::Screen::InterfaceSettings;
            } else if ((screen_ == screens::Screen::ButtonMap || screen_ == screens::Screen::AddBooks)) {
                screen_ = screens::Screen::Help;
            } else if (screen_ == screens::Screen::DictionaryHelp) {
                screen_ = screens::Screen::Dictionary;
            } else if (screen_ == screens::Screen::Help) {
                screen_ = screens::Screen::Settings;
            } else if (screen_ == screens::Screen::Dictionary || screen_ == screens::Screen::DictionarySearch) {
                screen_ = screens::Screen::QuickControls;
            } else if (screen_ == screens::Screen::QuickControls) {
                screen_ = screens::Screen::Reader;
            } else if (screen_ == screens::Screen::StorageHub || screen_ == screens::Screen::PowerHub) {
                screen_ = screens::Screen::Device;
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
    if (companionApi_.active() || usbTransfer_.active()
        || (backgroundJobActive()&&!(onboardingJobActive()&&screen_==screens::Screen::Onboarding)) || screen_ == screens::Screen::Status)
        return;
    if (screen_ == screens::Screen::Reader) {
        if (!readerScreen_.handleModernControls(immediateUi_, nowMs, prefs_, settingsStore_, screen_))
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
            if(onboardingJobActive()){
                if(screen_==screens::Screen::Onboarding)onboardingScreen_.acceptStorageProgress(update.storageRequest,update.storage,millis());
            }else screens::status(immediateUi_, update.title, update.line1, update.line2, update.progressPercent);
            continue;
        }

        const JobKind completed = jobKind_;
        jobKind_ = JobKind::None;
        vQueueDelete(jobQueue_);
        jobQueue_ = nullptr;
        Logger::checkpoint("running");
        if (completed == JobKind::BookSettings) {
            if (bookSettings_.ready()) {
                if (bookSettings_.path == readerScreen_.session.sourcePath())
                    bookSettings_.overrides = readerScreen_.session.state.overrides;
                screen_ = screens::Screen::BookFonts;
                statusUntilMs_ = 0;
                immediateUi_.cancelTouchUntilRelease();
                immediateUi_.invalidate();
                renderScreen(millis());
            } else {
                showTransientStatus("Settings unavailable", "Could not read this book", "Check the SD card", 1600,
                                    screens::Screen::Library);
            }
            return;
        }
        if(completed==JobKind::OnlineDictionary){
            screen_=screens::Screen::Dictionary;statusUntilMs_=0;
            immediateUi_.cancelTouchUntilRelease();immediateUi_.invalidate();renderScreen(millis());return;
        }
        if (completed == JobKind::DictionaryInstall) {
            dictionarySetup_.installed = dictionary::installedPack();
            dictionarySetup_.confirming = false;
            dictionarySetup_.message = update.line1;
            if (update.line2[0]) dictionarySetup_.message += std::string{" · "} + update.line2;
            screen_ = screens::Screen::DictionaryInstall; statusUntilMs_ = 0;
            immediateUi_.cancelTouchUntilRelease(); immediateUi_.invalidate(); renderScreen(millis()); return;
        }
        if (completed == JobKind::OtaCheck) {
            if (otaAutomatic_ && !otaOffer_.ready) {
                otaAutomatic_ = false;
                showTransientStatus("OTA", update.line1, update.line2, 1800, screens::Screen::Reader);
                return;
            }
            otaAutomatic_ = false;
            screen_ = screens::Screen::Ota; statusUntilMs_ = 0;
            immediateUi_.cancelTouchUntilRelease(); immediateUi_.invalidate(); renderScreen(millis()); return;
        }
        if(completed==JobKind::OnboardingStorage){
            onboardingScreen_.setStorageBusy(false);
            if(!onboardingJobWasMounted_&&storage_.mounted())onboardingStorageNeedsRecovery_=true;
            if(screen_==screens::Screen::Onboarding)onboardingScreen_.acceptStorageProgress(update.storageRequest,update.storage,millis());
            if(update.storage.state==OnboardingStorage::State::Ready&&onboardingStorageNeedsRecovery_
                &&onboardingScreen_.active()&&onboardingScreen_.storageVisible()
                &&onboardingScreen_.storageRequest()==update.storageRequest&&onboardingScreen_.canComplete()){
                recoverOnboardingStorage();onboardingStorageNeedsRecovery_=false;
            }
            if(onboardingReloadUiPending_&&onboardingScreen_.storageProgress().state==OnboardingStorage::State::Ready){
                onboardingReloadUiPending_=false;reloadUiAssets();
            }
            immediateUi_.cancelTouchUntilRelease();immediateUi_.invalidate();renderScreen(millis());return;
        }
        if (completed == JobKind::Library) {
            immediateUi_.cancelTouchUntilRelease();
            libraryScreen_.reset();
            screen_ = screens::Screen::Library;
            statusUntilMs_ = 0;
            renderScreen(millis());
            return;
        }
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

        bool reboot = completed == JobKind::OtaInstall && update.rebootRequired;
        if (reboot) {
            if (auto saved = OtaUpdater::finalizeInstall(settingsStore_, otaOffer_); !saved) {
                reboot = false;
                copyText(update.line1, "Switch cancelled");
                copyText(update.line2, saved.error().c_str());
            }
        }
        showTransientStatus("OTA", update.line1, update.line2, reboot ? 500 : 2500, screens::Screen::Ota);
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
    case JobKind::OnboardingStorage: {
        const auto stop=[](void* ctx){auto& app=*static_cast<App*>(ctx);return app.onboardingStorageCancel_.load()
            || uint32_t(millis()-app.onboardingJobStartedMs_)>=OnboardingStorage::kTimeoutMs;};
        const auto report=[](void* ctx,OnboardingStorage::Progress progress){
            auto& app=*static_cast<App*>(ctx);JobUpdate update;update.storage=progress;
            update.storageRequest=app.onboardingJobRequest_;app.enqueueJobUpdate(update);
        };
        complete.storageRequest=onboardingJobRequest_;
        const bool mounted=!stop(this)&&storage_.mountForCheck();
        complete.storage=OnboardingStorage::check(Board::Storage::filesystem(),mounted,esp_random(),report,stop,this);
        if(complete.storage.state==OnboardingStorage::State::Cancelled&&uint32_t(millis()-onboardingJobStartedMs_)>=OnboardingStorage::kTimeoutMs)
            complete.storage.state=OnboardingStorage::State::TimedOut;
        if(complete.storage.state==OnboardingStorage::State::Ready&&(!onboardingJobWasMounted_||onboardingStorageNeedsRecovery_))
            storage_.refreshBooks(false);
        if(stop(this))complete.storage.state=uint32_t(millis()-onboardingJobStartedMs_)>=OnboardingStorage::kTimeoutMs
            ?OnboardingStorage::State::TimedOut:OnboardingStorage::State::Cancelled;
        break;
    }
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
            OtaUpdater::checkOnly(settingsStore_.settings(), settingsStore_.secrets(), &App::renderStorageStatus, this,
                                  otaTarget_, &otaOffer_);
        copyText(complete.line1, result.summary.c_str());
        copyText(complete.line2, result.detail.c_str());
        break;
    }
    case JobKind::OtaInstall: {
        Logger::checkpoint("ota_install");
        const OtaUpdater::Result result =
            OtaUpdater::checkAndInstall(settingsStore_.settings(), settingsStore_.secrets(), &App::renderStorageStatus,
                                        this, &otaOffer_);
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
    case JobKind::OnlineDictionary:
        dictionary_.lookupOnline(settingsStore_);
        break;
    case JobKind::DictionaryInstall: {
        const auto result = dictionary::installEnglish(settingsStore_.settings(), settingsStore_.secrets(),
            dictionarySetup_.confirming, [&](const char* line1, const char* line2, int percent) {
                renderStorageStatus(this, "Dictionary", line1, line2, percent);
            }, [&] { return dictionaryCancel_.load(); });
        copyText(complete.line1, result ? "Dictionary ready" : result.error().c_str());
        copyText(complete.line2, result ? "Available offline" : "Try again from dictionary setup");
        break;
    }
    case JobKind::BookSettings:
        Logger::checkpoint("book_settings");
        bookSettings_.load(storage_, jobBookIndex_);
        break;
    case JobKind::Library:
        Logger::checkpoint("library_load");
        libraryScreen_.items(storage_, readerScreen_.store, readerScreen_.session);
        break;
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
    if (auto result = settingsStore_.flush(); !result) {
        showTransientStatus("USB", immediateUi_.text(UiText::CouldNotStart), result.error().message, 1200,
                            screens::Screen::Reader);
        return;
    }
    ReadingLoop::pause(readerScreen_.session);
    readerScreen_.store.close();
    storage_.end();
    if (!usbTransfer_.begin(true)) {
        const std::string failure = usbTransfer_.statusMessage();
        exitUsbTransfer();
        showTransientStatus("USB", immediateUi_.text(UiText::CouldNotStart), failure, 1200, screens::Screen::Reader);
        return;
    }
    screen_ = screens::Screen::Usb;
    renderScreen(nowMs);
#else
    showTransientStatus("USB", immediateUi_.text(UiText::Unavailable), {}, 1000, screens::Screen::Reader);
#endif
}

void App::exitUsbTransfer(screens::Screen destination) {
    usbTransfer_.end();
    const bool mounted = storage_.begin();
    fs::FS* filesystem = mounted ? &Board::Storage::filesystem() : nullptr;
    if (auto result = settingsStore_.begin(filesystem); !result)
        ESP_LOGW("settings", "USB transfer reload warning: %s", result.error().message.c_str());
    localeCatalog_ = mounted ? locales::scanInstalled(*filesystem, static_cast<size_t>(UiText::Count))
                             : locales::Catalog{};
    if (mounted) {
        readerScreen_.fonts.loadFromSd();
        focusScreen_.begin(*filesystem);
        readerScreen_.loadInitialBook(immediateUi_, storage_, prefs_, millis());
    } else {
        focusScreen_.begin();
    }
    applySettings();
    libraryScreen_.invalidate();
    screen_ = destination;
    renderScreen(millis());
}

void App::recoverOnboardingStorage() {
    // The worker has returned and verified the newly mounted card. Reload only
    // now, so the UI never races a mount or a filesystem probe.
    auto& disk=Board::Storage::filesystem();
    const auto chosenLocale=settingsStore_.settings().interface.locale;
    if(auto result=settingsStore_.begin(&disk);!result)
        ESP_LOGW("settings","card recovery warning: %s",result.error().message.c_str());
    if(settingsStore_.settings().interface.locale!=chosenLocale){
        settingsStore_.settings().interface.locale=chosenLocale;settingsStore_.acceptChanges();
    }
    localeCatalog_=locales::scanInstalled(disk,static_cast<size_t>(UiText::Count));
    readerScreen_.fonts.loadFromSd();
    if(!focusScreen_.active())focusScreen_.begin(disk);
    loadAppearanceSettings();libraryScreen_.invalidate();
    if(onboardingScreen_.active())onboardingScreen_.refreshDraft(settingsStore_.settings().reading);
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
    immediateUi_.setOrientation(current.reading.leftHanded ? Board::Display::rotatedUiOrientation()
                                                           : Board::Display::defaultUiOrientation());
    immediateUi_.setLanguageCatalog(storage_.mounted() ? &Board::Storage::filesystem() : nullptr, &localeCatalog_,
                                    &locales::loadUiFont);
    if (!readerScreen_.fonts.find(current.reading.typography.fontId)) {
        current.reading.typography.fontId = settings::TypographySettings{}.fontId;
        corrected = true;
    }
    // The modern introduction ships these translations in flash. An empty SD
    // locale directory must not undo the language selected on Welcome/reboot.
    const bool bundledModernLocale=screens::modern::enabled(immediateUi_.width(),immediateUi_.height(),screens::Screen::Onboarding)
        &&(current.interface.locale=="de"||current.interface.locale=="ro");
    if (!bundledModernLocale && current.interface.locale != Localization::kDefaultLocale
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

void App::runOtaCheck(bool install, bool switching, bool automatic) {
    otaAutomatic_ = automatic;
    if (install && !otaOffer_.ready) return;
    if (!install) { otaTarget_ = switching ? OtaUpdater::Target::Other : OtaUpdater::Target::Current; otaOffer_ = {}; }

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
    if (focusScreen_.active()) return;
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
    // The sampler reinitializes touch on resume; finish the wake-contact reads first.
    Input::resume();
}

void App::powerOff(uint32_t nowMs) {

    if (companionApi_.active())
        companionApi_.end();
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
    if(app.onboardingJobActive())return; // The setup screen owns its three-stage progress.
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
