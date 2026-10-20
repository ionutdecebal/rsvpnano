#pragma once
#include "ui/screens/Screens.h"
#include "storage/fs/OnboardingStorage.h"

namespace screens {
// Owns an isolated sample; never seeks, replaces or saves the open book.
class OnboardingScreen {
public:
    void begin(ui::Context& ui,const settings::DeviceSettings& settings,uint32_t nowMs);
    void close();
    bool active() const {return active_;}
    uint8_t step() const {return step_;}
    uint8_t surface() const {return storageVisible_ ? 64+uint8_t(storage_.state)*2+storageBusy_ : step_*2+(controlsFound_&&step_==2);}
    void next(int delta=1);
    void play(uint32_t nowMs,bool paced);
    void pause();
    void update(uint32_t nowMs);
    void hardwarePlay(uint32_t nowMs);
    void hardwareControls(){if(step_==2&&!storageVisible_){pause();controlsFound_=true;}}
    Action draw(ui::Context& ui,settings::DeviceSettings& settings,uint32_t nowMs);
    bool takeSettingsChanged(){return std::exchange(settingsChanged_,false);}
    const settings::ReadingSettings& draft() const{return draft_;}
    const ReadingSession& sample() const{return sample_;}
    static Action help(ui::Context& ui,const settings::ReadingSettings& reading,Screen& screen);
    void buttons(ui::Context& ui);
    void books(ui::Context& ui);
    void beginStorageCheck(uint32_t nowMs);
    void acceptStorageProgress(uint32_t request, OnboardingStorage::Progress progress, uint32_t nowMs);
    void setStorageBusy(bool busy) {storageBusy_=busy;}
    bool storageVisible() const {return storageVisible_;}
    bool storageChecking() const {return storage_.state==OnboardingStorage::State::Checking;}
    uint32_t storageRequest() const {return storageRequest_;}
    OnboardingStorage::Progress storageProgress() const {return storage_;}
    bool canComplete() const {return !storageAttempted_ || storage_.state==OnboardingStorage::State::Ready;}
    void refreshDraft(const settings::ReadingSettings& reading) {draft_=reading;draft_.mode=settings::ReadingMode::rsvp;}
    void waitForStorage() {cancelStorageCheck();pause();storageVisible_=storageAttempted_=true;}
private:
    void loadSample(ui::Context& ui,uint32_t nowMs);
    void touchPractice(ui::Context& ui,const settings::ReadingSettings& settings,uint32_t nowMs);
    void drawWord(ui::Context& ui,ui::Rect area);
    void explorer(ui::Context& ui,bool settings);
    void pageLesson(ui::Context& ui);
    void setPageTool(ui::Context& ui,uint8_t tool);
    Action storageCheck(ui::Context& ui,uint32_t nowMs);
    void cancelStorageCheck();
    settings::ReadingSettings draft_,timing_;
    ReadingSession sample_;
    std::vector<std::string> words_;
    uint8_t step_=0,topic_=0,settingsTopic_=0,pageTool_=0,focusKind_=0,button_=3;
    uint8_t pacingHint_=0;
    bool active_=false,settingsChanged_=false,started_=false,paused_=false,controlsFound_=false,done_=false,paced_=false;
    bool touching_=false,moved_=false,holding_=false,pressedPlaying_=false;
    uint16_t touchX_=0,touchY_=0;
    uint32_t pressedAt_=0;
    int brightness_=70;
    size_t pageStart_=0,selectedWord_=0;
    OnboardingStorage::Progress storage_;
    uint32_t storageRequest_=0,storageStartedMs_=0;
    bool storageVisible_=false,storageAttempted_=false,storageBusy_=false,storageStartPending_=false;
    bool uploadUsb_=false;
};
}
