#include "StorageFixture.h"
#include "dictionary/Dictionary.h"
#include <canvas/Arduino_Canvas.h>
#include <cstdio>
#include <filesystem>
#include <cassert>
#include "ui/screens/ModernScreens.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/ChaptersScreen.h"
#include "ui/screens/ModernReaderChrome.h"
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/OnboardingScreen.h"
#include "settings/WifiProfiles.h"
#include "ui/screens/TouchCheckScreen.h"
#include "ui/screens/StandbyScreen.h"
#include <WiFi.h>
#include "ui/PocketLines.h"
#include "drivers/touch/axs15231b_touch/axs15231b_touch.h"
#include "ui/ClippedGfx.h"
#include "fonts/LiterataFallbackAlpha4.h"

extern int previewSaveCalls, previewPrefetchDelay;
#include <freertos/task.h>
extern size_t previewRuntimeSize;
namespace dictionary { extern std::string previewCase; }

namespace {
    ui::TouchContact contact;
    ui::TouchSampleResult pollResult = ui::TouchSampleResult::Contact;
    ui::TouchSampleResult poll(ui::TouchContact& out) { out = contact; return pollResult; }

    void verifyPhysicalBottomControls(){
        for(auto orientation:{ui::Orientation::LandscapeFlipped,ui::Orientation::Landscape}){
            Arduino_Canvas panel(172,640,nullptr);assert(panel.begin(GFX_SKIP_OUTPUT_BEGIN));
            ui::Context ui(panel);const auto theme=ui::themes::defaultTheme();ui.setTheme(theme);
            ui.setOrientation(orientation);ui.setModernPresentation(true);ui.setTouchSource({.surface={172,640},.poll=&poll});
            settings::ReadingSettings config;screens::ReaderScreen reader(panel,config);
            auto screen=screens::Screen::ReaderAppearance;uint32_t now=100;
            const auto frame=[&](bool down,int x,int y){
                const auto rawLong=orientation==ui::Orientation::LandscapeFlipped?639-x:x;
                const auto rawShort=orientation==ui::Orientation::LandscapeFlipped?171-y:y;
                const uint8_t packet[]{2,uint8_t(down),uint8_t(rawLong>>8),uint8_t(rawLong),0,uint8_t(rawShort),0,0};
                BoardDrivers::Touch::Sample sample;assert(Axs15231bTouch::decodePacket(packet,8,172,640,sample));
                contact={sample.touched,sample.physicalX,sample.physicalY,now+=25};
                ui.pollTouch(now+1000); // Rendering/queue latency cannot age the original tap.
                ui.beginFrame(uint8_t(screen));
                if(screen==screens::Screen::ReaderAppearance)reader.appearance(ui,screen);
                else screens::readingSettings(ui,config,screen);
                ui.endFrame();
            };
            const auto tap=[&](int x,int y){frame(true,x,y);frame(false,x,y);frame(false,x,y);};
            frame(false,0,0);
            for(int y:{135,149,164,168,171}){const int previous=config.typography.fontSizeIndex;tap(590,y);assert(int(config.typography.fontSizeIndex)==(previous+1)%4);}
            tap(50,107);const bool previous=config.phantomWords;tap(440,171);assert(config.phantomWords!=previous);
            screen=screens::Screen::ReadingSettings;frame(false,0,0);
            for(int y:{136,151,165,171}){
                config.pacing.punctuationDelayMs=0;frame(false,0,0);
                frame(true,335,y);frame(true,627,y);frame(false,627,y);assert(int(config.pacing.punctuationDelayMs)==600);
            }
        }
        std::puts("Physical touch path: gesture-bearing LCD packets reach bottom appearance controls and punctuation sliders, including y=171, in both orientations.");
    }

    void verifyPocketLinesMarks(Arduino_Canvas& panel){
        ui::Context ui(panel);const auto theme=ui::themes::defaultTheme();ui.setTheme(theme);ui.setModernPresentation(true);
        for(int size=6;size<=24;++size){
            panel.fillScreen(ui.color(ui::themes::Background));
            ui::pocketlines::mark(ui,panel,100,40,size,false,ui::themes::Accent);
            ui::pocketlines::mark(ui,panel,200,40,size,true,ui::themes::Accent);
            const auto* pixels=panel.getFramebuffer();
            for(int y=0;y<size;++y)for(int x=0;x<size;++x)assert(pixels[(40+y)*640+100+x]==pixels[(40+size-1-y)*640+200+x]);
            if(size>=12)assert(pixels[(40+size/2)*640+100+size/2]==ui.color(ui::themes::Background)); // Transparent pocket.
        }
        for(int size:{6,12,24})for(int gap:{0,4,12})for(int height:{24,48,76}){
            const int top=86-height/2,bottom=top+height-1;
            panel.fillScreen(ui.color(ui::themes::Background));
            ui::pocketlines::guides(ui,panel,{0,36,640,100},320,top,bottom,size,gap,ui::themes::Accent);
            const auto* pixels=panel.getFramebuffer();bool above=false,below=false;
            for(int y=0;y<172;++y)for(int x=0;x<640;++x)if(pixels[y*640+x]!=ui.color(ui::themes::Background)){
                assert(y>=36&&y<136&&!(y>=top&&y<=bottom));assert(x>=308&&x<332);
                above|=y<top;below|=y>bottom;
            }
            assert(above&&below);
        }
        std::puts("Pocket Lines: transparent SVG masks, mirrored pair, all sizes/gaps and large-font fitting stay outside text and within the reading area.");
    }

    void verifyBannerWordmark(Arduino_Canvas& panel){
        auto custom=ui::modern::paperTheme();custom.id="custom";
        custom.definition.colors.background=ui::themes::rgb565(8,16,32);
        custom.definition.colors.foreground=ui::themes::rgb565(240,248,255);
        custom.definition.colors.accentBar=ui::themes::rgb565(0,192,255);
        ui::Context ui(panel);ui.setModernPresentation(true);
        for(const auto& theme:{ui::themes::defaultTheme(),ui::modern::paperTheme(),custom}){
            ui.setTheme(theme);ui.invalidate();
            const auto frame=[&](uint32_t time){ui.beginFrame(248);ui::modern::logo(ui,22,24,time);ui.endFrame();};
            frame(0);const std::vector<uint16_t> bright(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
            frame(40);assert(std::equal(bright.begin(),bright.end(),panel.getFramebuffer()));
            frame(960);const auto* dim=panel.getFramebuffer();bool foreground=false,accent=false,pulsed=false;
            for(int y=0;y<172;++y)for(int x=0;x<640;++x){
                const auto pixel=bright[y*640+x];
                if(x<22||x>=268||y<24||y>=129)assert(pixel==ui.color(ui::themes::Background));
                if(pixel==ui.color(ui::themes::Foreground)){foreground=true;assert(pixel==dim[y*640+x]);}
                accent|=pixel==ui.color(ui::themes::AccentBar);
                if(pixel!=dim[y*640+x]){assert(x>=22&&x<268&&y>=24&&y<129);pulsed=true;}
            }
            assert(foreground&&accent&&pulsed);
            // A fresh draw must match the same frame after an animation/theme change.
            const std::vector<uint16_t> animated(dim,dim+640*172);ui.invalidate();frame(960);
            assert(std::equal(animated.begin(),animated.end(),panel.getFramebuffer()));
        }
        std::puts("Banner wordmark: exact artwork fits its loading region; RSVP foreground stays static, the accent pulses, and light/dark/custom colours redraw cleanly.");
    }

    void verifyRelocatedControls(Arduino_Canvas& panel) {
        ui::Context ui(panel);ui.setModernPresentation(true);
        ui.setTouchSource({.surface={640,172},.poll=&poll});
        screens::InterfaceScreen display;settings::InterfaceSettings config;
        for (auto language : {"de", "ro", "en"}) {
            display.cycleLanguage(ui,config);
            assert(config.locale==language && ui.locale()==language);
        }
        ReadingSession session;dictionary::Dictionary dictionary;dictionary.open(session);
        auto screen=screens::Screen::Dictionary;uint32_t now=0;
        const auto frame=[&](bool down,int x,int y) {
            contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=25);
            ui.beginFrame(uint8_t(screen));
            if(screen==screens::Screen::Dictionary)dictionary.draw(ui,session,screen,now);
            else screens::modern::hub(ui,screen);
            ui.endFrame();
        };
        frame(false,0,0);
        const std::vector<uint16_t> lookup(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
        frame(true,336,20);frame(false,336,20);
        assert(screen==screens::Screen::DictionaryHelp);frame(false,0,0);
        frame(true,40,20);frame(false,40,20);assert(screen==screens::Screen::Dictionary);
        frame(false,0,0);
        assert(std::equal(lookup.begin(),lookup.end(),panel.getFramebuffer()));
        std::puts("Relocated controls: interface language cycles EN/DE/RO; Dictionary help returns without resetting the lookup.");
    }

    void verifyPageFontRestoration(Arduino_GFX& panel) {
        ui::Context ui(panel);ui.setModernReadingPalette(true);ui.setTouchSource({.surface={640,172},.poll=&poll});
        settings::SettingsStore settings;auto& config=settings.settings().reading;
        screens::ReaderScreen reader(panel,config);StorageManager storage;Preferences preferences;
        const std::vector<std::string> words{"Same", "word", "and", "the", "next", "sentence."};
        ReadingLoop::setWords(reader.session,words,0);auto screen=screens::Screen::Reader;uint32_t now=0;
        const auto frame=[&](bool down=false,int x=0,int y=0){
            contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=25);
            ui.beginFrame(uint8_t(screen),reader.pageBrowsing()?1:0);
            reader.handleModernControls(ui,now,preferences,settings,screen);
            FakeTask::now=now;reader.update(preferences,now);reader.draw(ui,storage,{},now);ui.endFrame();
        };
        for(auto mode:{settings::ReadingMode::rsvp,settings::ReadingMode::chunks}) {
            config.mode=mode;
            for(int size=0;size<4;++size) {
                config.typography.fontSizeIndex=size;
                for(int repeat=0;repeat<3;++repeat) {
                    frame();assert(previewRuntimeSize==size);assert(!reader.session.playing);
                    reader.openPage();frame();assert(previewRuntimeSize==2);
                    frame();assert(previewRuntimeSize==2);
                    frame(true,550,20);frame(false,550,20);
                    assert(!reader.pageBrowsing());assert(!reader.session.playing);
                    assert(reader.session.state.wordIndex==0);assert(previewRuntimeSize==size);
                    frame();assert(previewRuntimeSize==size);
                }
            }
        }
        std::puts("Real font-selection methods with a mutable strike: 24 page round trips restore all RSVP/chunk sizes while paused at the same word.");
    }

    void verifyRealPlaybackPause(Arduino_GFX& panel) {
        ui::Context ui(panel);ui.setModernReadingPalette(true);ui.setTouchSource({.surface={640,172},.poll=&poll});
        settings::SettingsStore settings;auto& config=settings.settings().reading;
        config.holdToRead=true;config.pauseMode=settings::PauseMode::sentenceEnd;config.wpm=1000;
        config.pacing.longWordDelayMs=0;config.pacing.complexWordDelayMs=0;config.pacing.punctuationDelayMs=0;
        screens::ReaderScreen reader(panel,config);const std::vector<std::string> words{"Wait","it.","Next","sentence."};
        ReadingLoop::setWords(reader.session,words,0);Preferences preferences;auto screen=screens::Screen::Reader;
        uint32_t now=1000;
        const auto touch=[&](bool down,uint32_t delta){
            contact={down,260,86};ui.pollTouch(now+=delta);FakeTask::now=now;
            reader.handleModernControls(ui,now,preferences,settings,screen);
        };
        touch(true,25);touch(true,350);assert(reader.session.playing);
        touch(false,25);assert(reader.session.playing); // release requests sentence end
        const auto saved=previewSaveCalls;
        FakeTask::now=now+250;reader.update(preferences,FakeTask::now);
        assert(!reader.session.playing && reader.session.state.wordIndex==1 && previewSaveCalls>saved);
        FakeTask::now+=500;reader.update(preferences,FakeTask::now);assert(!reader.session.playing);
        // Reproduce work crossing the deadline after the UI's pre-check.
        reader.resumeNow(3000);FakeTask::now=3059;reader.toggle(preferences,3059);
        assert(reader.session.playing);previewPrefetchDelay=2;reader.update(preferences,3059);previewPrefetchDelay=0;
        assert(!reader.session.playing && reader.session.state.wordIndex==1);
        // Immediate mode still stops on hold release without advancing.
        config.pauseMode=settings::PauseMode::instant;touch(true,100);touch(true,350);assert(reader.session.playing);
        touch(false,25);assert(!reader.session.playing && reader.session.state.wordIndex==1);
        std::puts("Real playback/control methods: hold release respects sentence/instant settings, delayed catch-up and prefetch deadlines pause correctly, save position and require manual resume.");
    }

    void verifyReaderControlRoutes(Arduino_GFX& panel) {
        ui::Context ui(panel);ui.setModernReadingPalette(true);ui.setTouchSource({.surface={640,172},.poll=&poll});
        settings::SettingsStore settings;auto& config=settings.settings().reading;config.pauseMode=settings::PauseMode::instant;screens::ReaderScreen reader(panel,config);
        const std::vector<std::string> words(10,"word");ReadingLoop::setWords(reader.session,words,0);
        Preferences preferences;auto screen=screens::Screen::Reader;uint32_t now=0;
        const auto sample=[&](bool down,int x,int y,uint32_t elapsed=25){contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=elapsed);return reader.handleModernControls(ui,now,preferences,settings,screen);};
        const auto tap=[&](int x,int y){assert(sample(true,x,y));assert(sample(false,x,y));};
        tap(260,86);assert(reader.session.playing);tap(260,86);assert(!reader.session.playing);
        tap(50,155);assert(screen==screens::Screen::Reader);assert(!reader.session.playing);
        const auto index=reader.session.state.wordIndex;const auto speed=config.wpm;
        for(auto [x,y]:{std::pair{390,86},std::pair{130,86},std::pair{260,135}}){
            sample(true,260,86);sample(true,x,y);sample(false,x,y);
            assert(screen==screens::Screen::Reader && reader.session.state.wordIndex==index && config.wpm==speed && !reader.pageBrowsing());
        }
        sample(true,260,115);sample(true,260,45);sample(false,260,45);assert(screen==screens::Screen::Reader);assert(!reader.session.playing);
        screen=screens::Screen::Reader;config.chapterVisibility=settings::Visibility::always;tap(100,18);assert(screen==screens::Screen::FindPlace);
        screen=screens::Screen::Reader;tap(370,18);assert(screen==screens::Screen::Reader);
        ui.setFocusBadge("Focus 12:00");tap(370,18);assert(screen==screens::Screen::FocusSession);
        screen=screens::Screen::Reader;ui.setFocusBadge("");const auto footer=config.footerMetric;tap(600,155);assert(config.footerMetric!=footer);
        reader.session.metadata.writingMode=WritingMode::verticalRl;assert(!sample(true,260,86));assert(!sample(false,260,86));
        std::puts("Quiet reader: text playback, no reader Menu target and ignored reader swipes, clean inactive chrome, saved book/WPM during other drags, Focus and page routes verified.");
    }
    void verifyReaderTouchComfort(Arduino_GFX& panel) {
        ui::Context ui(panel);ui.setModernReadingPalette(true);ui.setTouchSource({.surface={640,172},.poll=&poll});
        settings::SettingsStore settings;auto& config=settings.settings().reading;config.holdToRead=true;config.pauseMode=settings::PauseMode::instant;
        screens::ReaderScreen reader(panel,config);const std::vector<std::string> words(10,"word");ReadingLoop::setWords(reader.session,words,0);
        Preferences preferences;auto screen=screens::Screen::Reader;uint32_t now=100000;
        const auto sample=[&](bool down,int x,int y,uint32_t elapsed=25){contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=elapsed);return reader.handleModernControls(ui,now,preferences,settings,screen);};
        sample(true,260,86);sample(false,260,86);assert(!reader.session.playing);
        sample(true,260,86);sample(true,263,87,349);assert(!reader.session.playing);
        sample(true,263,87,1);assert(reader.session.playing);sample(false,263,87);assert(!reader.session.playing);
        sample(true,260,86);sample(true,290,86,400);sample(false,290,86);assert(!reader.session.playing);
        sample(true,260,86);sample(true,260,86,350);assert(reader.session.playing);
        pollResult=ui::TouchSampleResult::Reset;sample(false,260,86);pollResult=ui::TouchSampleResult::Contact;assert(!reader.session.playing);
        config.holdToRead=false;sample(true,260,86);reader.cancelModernControls();sample(false,260,86);assert(!reader.session.playing);
        reader.session.playing=true;sample(true,260,86);ReadingLoop::pause(reader.session);sample(false,260,86,750);assert(!reader.session.playing);
        sample(true,260,86);ReadingLoop::start(reader.session,now);sample(false,260,86);assert(reader.session.playing);
        sample(true,260,86);sample(false,260,86);assert(!reader.session.playing);
        std::puts("Touch: hold threshold/release, motion rejection, controller cancellation, screen changes and preserved play/pause intent verified.");
    }

    void verifyAppearanceAndFocusRoutes(Arduino_Canvas& panel) {
        ui::Context ui(panel);
        const auto theme = ui::themes::defaultTheme();
        ui.setTheme(theme); ui.setModernPresentation(true);
        ui.setTouchSource({.surface={640, 172}, .poll=&poll});
        settings::ReadingSettings config;
        screens::ReaderScreen reader(panel, config);
        screens::FocusScreen focus; focus.begin();
        auto screen = screens::Screen::ReaderAppearance;
        uint32_t now = 10000, focusNow = 0;
        const auto frame = [&](bool down, int x, int y) {
            contact = {down, static_cast<uint16_t>(x), static_cast<uint16_t>(y)}; ui.pollTouch(now += 25);
            ui.beginFrame(static_cast<uint8_t>(screen), focus.frameSurface(screen));
            if (screen == screens::Screen::ReaderAppearance) reader.appearance(ui, screen);
            else focus.draw(ui, focusNow, screen);
            ui.endFrame();
        };
        const auto tap = [&](int x, int y) { frame(true, x, y); frame(false, x, y); frame(false, x, y); };
        const auto verifyPhaseRedraw = [&] {
            const std::vector<uint16_t> incremental(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
            ui.invalidate();frame(false,0,0);
            assert(std::equal(incremental.begin(),incremental.end(),panel.getFramebuffer()));
        };
        frame(false, 0, 0);
        const int size = config.typography.fontSizeIndex;
        tap(590, 150); assert(config.typography.fontSizeIndex != size);
        tap(50, 107);
        const bool phantoms = config.phantomWords;
        tap(470, 150); assert(config.phantomWords != phantoms);
        const bool highlight=config.typography.focusHighlight;
        tap(575,150);assert(config.typography.focusHighlight!=highlight);
        tap(575,150);assert(config.typography.focusHighlight==highlight);
        config.mode=settings::ReadingMode::chunks;frame(false,0,0);
        const auto guideSize=config.typography.guideSize;const auto guideGap=config.typography.guideTextGap;
        tap(600,65);tap(600,100);
        assert(config.typography.guideSize==guideSize && config.typography.guideTextGap==guideGap);
        tap(575,150);assert(config.typography.focusHighlight==highlight);
        config.mode=settings::ReadingMode::rsvp;
        tap(50,150);
        const auto pausedChapter=config.chapterVisibility;
        tap(470,70);assert(settings::visible(config.batteryLabelVisibility,true));
        assert(settings::visible(config.chapterVisibility,false)==settings::visible(pausedChapter,false));
        tap(470,70);assert(settings::visible(config.chapterVisibility,true));
        const bool readingChapter=settings::visible(config.chapterVisibility,true);
        tap(470,120);assert(settings::visible(config.chapterVisibility,true)==readingChapter);
        tap(587, 20); assert(screen == screens::Screen::Reader);
        screen = screens::Screen::FocusTimers; ui.invalidate(); frame(false, 0, 0);
        tap(500, 68); assert(screen == screens::Screen::Reader && focus.active());
        screen=screens::Screen::FocusSession;focusNow=1000;frame(false,0,0);
        tap(420,135);assert(focus.badge(focusNow)=="Paused 24:59");
        verifyPhaseRedraw();
        focusNow+=30000;focus.update(focusNow);
        assert(focus.badge(focusNow)=="Paused 24:59");
        tap(500,76);assert(screen==screens::Screen::Reader && focus.badge(focusNow)=="Focus 24:59");
        // Running timer survives leaving the timer screen and ignores orientation.
        screen=screens::Screen::FocusSession;frame(false,0,0);
        frame(true,420,135); // Pause becomes part of End Focus when a break begins.
        focusNow=25*60000+30001;focus.update(focusNow);assert(focus.breakVisible());
        frame(false,420,135);assert(focus.active() && focus.breakVisible());
        frame(false,0,0);
        verifyPhaseRedraw();
        focusNow=30*60000+30001;focus.update(focusNow);assert(focus.breakVisible());
        frame(false,0,0);verifyPhaseRedraw();
        tap(500,76);assert(!focus.breakVisible() && focus.active());
        screen=screens::Screen::FocusTimers;ui.invalidate();frame(false,0,0);
        tap(500,140);assert(!focus.active());
        screen=screens::Screen::FocusTimers;ui.invalidate();frame(false,0,0);
        tap(540,20);assert(screen==screens::Screen::FocusPresets);
        tap(490,74);assert(screen==screens::Screen::FocusEditor);
        screen=screens::Screen::FocusTimers;focus.close();ui.invalidate();frame(false,0,0);
        tap(500,140);assert(screen==screens::Screen::FocusSession && focus.active());
        tap(420,135);tap(500,76);assert(screen==screens::Screen::FocusSession && focus.active());
        focus.close();
        std::puts("Appearance and Focus: visibility, paused badge/timing, complete phase redraws, press cancellation at break, manual next round and presets verified.");
    }

    void verifyNavigation(Arduino_Canvas& panel,const std::filesystem::path& output) {
        ui::Context ui(panel);auto theme=ui::themes::defaultTheme();ui.setTheme(theme);
        ui.setTouchSource({.surface={640,172},.poll=&poll});
        settings::SettingsStore settings;Preferences preferences;
        screens::ReaderScreen reader(panel,settings.settings().reading);
        std::vector<std::string> words;
        const std::vector<std::string> paragraph{"She","opened","the","blue","door","and","stepped","outside.","Beyond","the","garden,","a","quiet","path","led","towards","the","river."};
        for(int i=0;i<25;++i) words.insert(words.end(),paragraph.begin(),paragraph.end());
        ReadingLoop::setWords(reader.session,words,0);
        for(size_t i=0;i<words.size();i+=paragraph.size())reader.session.metadata.paragraphStarts.push_back(i);
        reader.session.metadata.chapters={{"The garden",0},{"A quiet path",180}};
        reader.openPage();auto screen=screens::Screen::Reader;uint32_t now=100000;
        const auto frame=[&](bool down,int x,int y) {
            contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=25);
            ui.setModernPresentation(screen!=screens::Screen::Reader);ui.setModernReadingPalette(screen==screens::Screen::Reader);
            if(screen==screens::Screen::Reader)reader.handleModernControls(ui,now,preferences,settings,screen);
            ui.beginFrame(static_cast<uint8_t>(screen),screen==screens::Screen::Reader && reader.pageBrowsing());
            if(screen==screens::Screen::Reader) {
                if(reader.pageBrowsing()) reader.drawPage(ui);
                else screens::modern::readerChrome(ui,{.chapter="The garden",.footer="42%",.batteryLabel="84%",.canRead=true},settings.settings().reading,{{true,3.9f,84},0,false});
            }
            else reader.drawLocator(ui,screen,preferences,now);
            ui.endFrame();
        };
        const auto tap=[&](int x,int y){frame(true,x,y);frame(false,x,y);frame(false,x,y);};
        const auto save=[&](const char* name) {
            auto* file=std::fopen((output/(std::string{name}+".ppm")).c_str(),"wb");assert(file);
            std::fprintf(file,"P6\n640 172\n255\n");
            for(int i=0;i<640*172;++i) {const auto pixel=panel.getFramebuffer()[i];const unsigned char rgb[]{static_cast<unsigned char>(((pixel>>11)&31)*255/31),static_cast<unsigned char>(((pixel>>5)&63)*255/63),static_cast<unsigned char>((pixel&31)*255/31)};std::fwrite(rgb,1,3,file);}std::fclose(file);
        };
        frame(false,0,0);save("page");
        frame(true,200,145);frame(true,200,65);frame(false,200,65);frame(false,0,0);
        assert(reader.session.state.wordIndex==0);save("page-scrolled");
        settings.settings().reading.mode=settings::ReadingMode::chunks;
        tap(552,22);assert(!reader.pageBrowsing()&&!reader.session.playing);
        assert(settings.settings().reading.mode==settings::ReadingMode::chunks);
        const std::vector<uint16_t> restored(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
        ui.invalidate();frame(false,0,0);
        assert(std::equal(restored.begin(),restored.end(),panel.getFramebuffer()));
        save("reader-after-page");
        ReadingLoop::seekTo(reader.session,0);reader.openPage();frame(false,0,0);
        frame(true,200,145);frame(true,200,65);frame(false,200,65);frame(false,0,0);
        tap(360,20);assert(screen==screens::Screen::FindPlace);save("find-place");
        tap(552,22);assert(screen==screens::Screen::Reader&&!reader.session.playing);assert(reader.session.state.wordIndex>0);
        const auto committed=reader.session.state.wordIndex;
        reader.openPage();frame(false,0,0);tap(360,20);tap(180,20);tap(552,22);assert(reader.session.state.wordIndex==0);
        reader.openLocator(screen);frame(false,0,0);tap(340,20);assert(screen==screens::Screen::BookSearchKeyboard);save("search-keyboard");
        // Type 'the', using the production 44-pixel keyboard keys.
        tap(253,60);tap(309,104);tap(141,60);tap(602,150);
        assert(screen==screens::Screen::BookSearch);frame(false,0,0);save("search-results");
        tap(150,74);assert(screen==screens::Screen::FindPlace);assert(reader.session.state.wordIndex==0);
        tap(40,20);assert(screen==screens::Screen::Reader&&reader.session.state.wordIndex==0);
        screen=screens::Screen::FindChapters;frame(false,0,0);save("find-chapters");
        screen=screens::Screen::Reader;reader.openPage();theme=ui::modern::paperTheme();ui.setTheme(theme);frame(false,0,0);save("page-paper");
        assert(committed>0);std::puts("Page, locator and search: simple header, mode preserved, chrome fully restored, direct drag, explicit jump, previous place, keyboard, results preview and cancel verified.");
    }

    void verifyBookSettings(Arduino_Canvas& panel,const std::filesystem::path& output) {
        ui::Context ui(panel);auto theme=ui::themes::defaultTheme();ui.setTheme(theme);ui.setModernPresentation(true);
        ui.setTouchSource({.surface={640,172},.poll=&poll});
        BookMetadata metadata;metadata.locale="en";metadata.scriptMask=UnicodeText::ScriptLatin;
        settings::ReadingOverrides overrides;FontCatalog fonts;locales::Catalog catalog;
        auto screen=screens::Screen::BookFonts;uint32_t now=500000;
        const auto frame=[&](bool down,int x,int y) {
            contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(now+=25);ui.beginFrame(static_cast<uint8_t>(screen));
            screens::bookFonts(ui,metadata,overrides,catalog,fonts,screen,screens::Screen::Library,"Selected book");ui.endFrame();
        };
        const auto tap=[&](int x,int y){frame(true,x,y);frame(false,x,y);frame(false,x,y);};
        const auto save=[&](const char* name) {
            auto* file=std::fopen((output/(std::string{name}+".ppm")).c_str(),"wb");assert(file);
            std::fprintf(file,"P6\n640 172\n255\n");
            for(int i=0;i<640*172;++i) {const auto pixel=panel.getFramebuffer()[i];const unsigned char rgb[]{static_cast<unsigned char>(((pixel>>11)&31)*255/31),static_cast<unsigned char>(((pixel>>5)&63)*255/63),static_cast<unsigned char>((pixel&31)*255/31)};std::fwrite(rgb,1,3,file);}std::fclose(file);
        };
        screens::resetBookFonts();frame(false,0,0);save("book-font-language");save("library-details");tap(120,76);save("book-language-picker");
        tap(520,76);assert(overrides.locale && *overrides.locale=="fr");
        tap(480,138);assert(overrides.languageFonts.size()==1 && overrides.languageFonts[0].fontId=="literata");
        frame(false,0,0);save("book-font-overrides");
        tap(140,138);assert(!overrides.locale && overrides.languageFonts.empty());
        metadata.textRuns={{0,"en",TextDirection::ltr,UnicodeText::ScriptLatin},{20,"fr",TextDirection::ltr,UnicodeText::ScriptLatin}};
        frame(false,0,0);tap(480,76);tap(480,138);assert(overrides.languageFonts.size()==1 && overrides.languageFonts[0].locale=="fr");
        tap(140,138);metadata.scriptMask|=UnicodeText::ScriptMath;frame(false,0,0);tap(480,76);frame(false,0,0);save("book-math-font");
        tap(40,22);assert(screen==screens::Screen::Library);
        screen=screens::Screen::BookFonts;tap(120,76); // Leave a picker open for another book.
        screens::resetBookFonts();ui.invalidate();frame(false,0,0);
        tap(40,22);assert(screen==screens::Screen::Library);
        screen=screens::Screen::WifiConnect;ui::KeyboardState keyboard;std::string password="sample-password";
        const auto keyFrame=[&]{ui.beginFrame(static_cast<uint8_t>(screen));ui.keyboard({8,8,624,156},password,63,keyboard,"Home Wi-Fi",true);ui.endFrame();};
        keyFrame();save("wifi-keyboard");keyboard.passwordVisible=true;keyFrame();save("wifi-keyboard-visible");
        theme=ui::modern::paperTheme();ui.setTheme(theme);keyFrame();save("wifi-keyboard-paper");
        std::puts("Book settings: language choice, per-language font, reset and math target verified; shared password keyboard rendered in both themes.");
    }

    void verifyChunkFrames(Arduino_Canvas& panel) {
        ui::Context ui(panel);const auto theme=ui::themes::defaultTheme();ui.setTheme(theme);ui.setModernReadingPalette(true);
        const std::vector<std::string> words{"She","opened","the","blue","door","and","stepped","outside.","A","watering","can","stood","nearby."};
        StorageManager storage;
        for(int size=0;size<4;++size)for(int tracking : {-2,0,3}) {
            settings::ReadingSettings config;config.mode=settings::ReadingMode::chunks;config.typography.fontSizeIndex=size;config.typography.tracking=tracking;
            screens::ReaderScreen reader(panel,config);ReadingLoop::setWords(reader.session,words,0);
            reader.session.metadata.locale="en";
            size_t index=0;
            while(index<words.size()) {
                ReadingLoop::seekTo(reader.session,index);
                ui.beginFrame(250);reader.draw(ui,storage,{},0);ui.endFrame();
                const auto count=ReadingLoop::displayWordCount(reader.session);assert(count>=1&&count<=3&&index+count<=words.size());
                std::string phrase=words[index];for(size_t i=1;i<count;++i)phrase+=" "+words[index+i];
                ui::fonts::AlphaTextRenderer<640> measure(panel);measure.setFont(*ui::fonts::LiterataFallbackAlpha4_Sizes[size]);
                assert(count==1||measure.textAdvance(phrase,tracking)<=608);
                index+=count;
            }
            assert(index==words.size());
            reader.session.metadata.locale="fr";ReadingLoop::seekTo(reader.session,0);
            ui.beginFrame(250);reader.draw(ui,storage,{},0);ui.endFrame();assert(ReadingLoop::displayWordCount(reader.session)==1);
        }
        std::puts("Native chunk frames: all four font sizes, tracking limits, exact word coverage, measured fit and non-English fallback verified.");
    }

    void verifyChunkCenter(Arduino_Canvas& panel,const std::filesystem::path& output) {
        StorageManager storage;
        for (int size=0;size<4;++size) for (int tracking : {-2,0,3}) for (int count=1;count<=3;++count) {
            std::vector<uint16_t> reference;
            for (int anchor : {30,35,40}) {
                ui::Context ui(panel);const auto theme=ui::themes::defaultTheme();ui.setTheme(theme);ui.setModernReadingPalette(true);
                settings::ReadingSettings config;config.mode=settings::ReadingMode::chunks;
                config.typography.anchor=anchor;config.typography.fontSizeIndex=size;config.typography.tracking=tracking;
                screens::ReaderScreen reader(panel,config);
                const std::vector<std::string> words=count==1?std::vector<std::string>{"the"}:count==2?std::vector<std::string>{"the","sky"}:std::vector<std::string>{"the","red","sky"};
                ReadingLoop::setWords(reader.session,words,0);
                ui.beginFrame(250);reader.draw(ui,storage,{},0);ui.endFrame();
                const auto* pixels=panel.getFramebuffer();
                const auto foreground=ui.color(ui::themes::Foreground),accent=ui.readingFocusColor();
                int left=640,right=-1;
                for (int y=36;y<136;++y) for (int x=0;x<640;++x) {
                    assert(pixels[y*640+x]!=accent);
                    if (pixels[y*640+x]==foreground) {left=std::min(left,x);right=std::max(right,x);}
                }
                // Font side bearings can offset ink from advance bounds by a few pixels.
                assert(right>left && std::abs(left+right-640)<=8);
                if (anchor==30) reference.assign(pixels,pixels+640*172);
                else assert(std::equal(reference.begin(),reference.end(),pixels));
                if (size==0 && tracking==0) assert(ReadingLoop::displayWordCount(reader.session)==size_t(count));
                if (anchor==30 && size==1 && tracking==0) {
                    const auto name="chunk-centred-"+std::to_string(count);
                    auto* file=std::fopen((output/(name+".ppm")).c_str(),"wb");assert(file);std::fprintf(file,"P6\n640 172\n255\n");
                    for(int i=0;i<640*172;++i) {const auto pixel=pixels[i];const unsigned char rgb[]{static_cast<unsigned char>(((pixel>>11)&31)*255/31),static_cast<unsigned char>(((pixel>>5)&63)*255/63),static_cast<unsigned char>((pixel&31)*255/31)};std::fwrite(rgb,1,3,file);}std::fclose(file);
                }
                config.typography.focusHighlight=false;config.typography.guideSize=0;ui.invalidate();ui.beginFrame(250);reader.draw(ui,storage,{},0);ui.endFrame();
                assert(std::equal(reference.begin(),reference.end(),panel.getFramebuffer()));
            }
        }
        std::puts("Chunk centre: one/two/three-word ink centred, identical output across three word-anchor preferences, four sizes and all tracking limits; no accent letter or marker.");
    }

    void verifyFilledSliders(Arduino_Canvas& panel) {
        for (const auto& theme : {ui::themes::defaultTheme(),ui::modern::paperTheme()}) {
            ui::Context ui(panel);ui.setTheme(theme);ui.setModernPresentation(true);
            for (const int value : {0,300,600}) {
                int scalar=value;ui.invalidate();ui.beginFrame(249);
                ui.slider({12,49,198,50},"Long words",scalar,0,600,50," ms");ui.endFrame();
                const auto* pixels=panel.getFramebuffer();
                const auto surface=ui.color(ui::themes::SurfaceMuted),fill=ui.color(ui::themes::AccentBar);
                // The fill occupies the tile's height, not a small rail at its bottom.
                for (const int y : {56,92}) {
                    assert(pixels[y*640+26]==(value==0?surface:fill));
                    assert(pixels[y*640+195]==(value==600?fill:surface));
                }
                bool foreground=false,onFill=false;
                for (int y=0;y<172;++y) for (int x=0;x<640;++x) {
                    if (x<12 || x>=210 || y<49 || y>=99) assert(pixels[y*640+x]==ui.color(ui::themes::Background));
                    else {
                        foreground|=pixels[y*640+x]==ui.color(ui::themes::Foreground);
                        onFill|=pixels[y*640+x]==ui.color(ui::themes::OnAccent);
                    }
                }
                if(value==300)assert(foreground&&onFill); // both label and value retain contrast
            }
            for (const int value : {5,52,100}) {
                int scalar=value;ui.invalidate();ui.beginFrame(249);
                ui.verticalSlider({584,8,48,156},scalar,5,100);ui.endFrame();
                const auto* pixels=panel.getFramebuffer();
                const auto surface=ui.color(ui::themes::SurfaceMuted),fill=ui.color(ui::themes::AccentBar);
                assert(pixels[70*640+592]==(value==100?fill:surface));
                assert(pixels[116*640+592]==(value==5?surface:fill));
                for(int y=0;y<172;++y)for(int x=0;x<640;++x)
                    if(x<584||x>=632||y<8||y>=164)assert(pixels[y*640+x]==ui.color(ui::themes::Background));
            }
        }
        std::puts("Filled sliders: full tile coverage, empty/half/full values, split text contrast, and bounded vertical brightness in Ink and Paper passed.");
    }

    bool verifyNativeFontClipping(Arduino_Canvas& panel) {
        constexpr ui::Rect clip{16, 52, 328, 76};
        constexpr uint16_t untouched = 0x1234;
        ui::ClippedGfx viewport(panel, clip);
        ui::fonts::AlphaTextRenderer<640> text(viewport);
        text.begin();
        for (const auto* font : ui::fonts::LiterataFallbackAlpha4_Sizes) {
            panel.fillScreen(untouched);
            text.setFont(*font); text.setTextColor(0xffff, untouched);
            for (const auto x : {-30, 0, 300, 340}) text.drawString("reading", x, 105, 3);
            size_t changed = 0;
            for (int y = 0; y < 172; ++y) for (int x = 0; x < 640; ++x) {
                const auto pixel = panel.getFramebuffer()[y * 640 + x];
                if (!ui::contains(clip, x, y) && pixel != untouched) return false;
                changed += pixel != untouched;
            }
            if (!changed) return false;
        }
        std::puts("Native RFont4: all four sizes stay inside the preview viewport.");
        return true;
    }
}

// Metadata fixtures only: layout, rasterization and selection use firmware code.
namespace BookLibrary {
    std::string_view displayName(const Entry& book) { return book.title; }
    bool isArticle(const Entry& book) { return std::string_view{book.path}.starts_with("/library/articles/"); }
}

// Catalog fixtures; no SD-card access is performed by the host preview.
const ui::themes::Theme& ThemeStore::resolve(std::string_view) const { return themes_.front(); }
const ui::themes::Theme& ThemeStore::next(std::string_view) const { return themes_.front(); }
namespace locales {
    std::string_view localeName(const Catalog&, std::string_view locale) { return locale == "en" ? "English" : locale; }
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::filesystem::create_directories(argv[1]);
    Arduino_Canvas panel(640, 172, nullptr);
    if (!panel.begin(GFX_SKIP_OUTPUT_BEGIN)) return 1;
    if (!verifyNativeFontClipping(panel)) return 2;
    verifyPhysicalBottomControls();
    verifyPocketLinesMarks(panel);
    verifyBannerWordmark(panel);
    verifyRealPlaybackPause(panel);
    verifyPageFontRestoration(panel);
    verifyRelocatedControls(panel);
    verifyReaderControlRoutes(panel);
    verifyReaderTouchComfort(panel);
    verifyAppearanceAndFocusRoutes(panel);
    verifyNavigation(panel,std::filesystem::path{argv[1]});
    verifyBookSettings(panel,std::filesystem::path{argv[1]});
    verifyFilledSliders(panel);
    verifyChunkFrames(panel);
    verifyChunkCenter(panel,std::filesystem::path{argv[1]});
    ui::Context ui(panel);
    const auto theme = ui::themes::defaultTheme();
    ui.setTheme(theme);
    ui.setModernPresentation(true);
    ui.setTouchSource({.surface = {640, 172}, .poll = &poll});
    const BookLibrary::Entry book{.path = "/library/book.epub", .title = "The Left Hand of Darkness",
                                  .author = "Ursula K. Le Guin"};
    const BookLibrary::Entry longBook{.title = "The Discreet Charm of Long Titles and Other Unexpected Adventures",
                                      .author = "A writer with a rather long name"};
    const std::vector<screens::LibraryItem> books{{&book, "The Place Inside the Blizzard", 42},
                                                {&longBook, "An unexpected chapter", 17}};
    screens::LibraryScreen library;
    screens::ChaptersScreen chapters;
    ReadingSession reader;
    dictionary::Dictionary dictionary;
    settings::InterfaceSettings interface;
    screens::InterfaceScreen display;
    settings::ReadingSettings reading;
    screens::ReaderScreen appearance(panel, reading);
    screens::FocusScreen focus;
    focus.begin();
    const std::array<ChapterMarker, 3> markers{{{"A Parade in Erhenrang", 0},
        {"The Place Inside the Blizzard", 5}, {"The Escape", 10}}};
    const std::pair<screens::Screen, const char*> views[]{
        {screens::Screen::Dictionary,"dictionary"}, {screens::Screen::Dictionary,"dictionary-paper"},
        {screens::Screen::DictionaryHelp,"dictionary-setup"},
        {screens::Screen::Read, "menu"}, {screens::Screen::Read, "menu-paper"}, {screens::Screen::QuickControls,"quick-controls"}, {screens::Screen::Library, "library"},
        {screens::Screen::Library, "library-long"}, {screens::Screen::Library, "library-empty"},
        {screens::Screen::Settings, "settings"}, {screens::Screen::Chapters, "chapters"},
        {screens::Screen::ReadingHub, "reading"}, {screens::Screen::DisplayHub, "display"},
        {screens::Screen::Connections, "connections"}, {screens::Screen::Device, "device"},
        {screens::Screen::ReadingSettings, "reading-controls"}, {screens::Screen::PacingSettings, "pacing"}, {screens::Screen::PacingSettings, "pacing-paper"},
        {screens::Screen::Ota, "updates"}, {screens::Screen::Help, "help"},
        {screens::Screen::Status, "startup"}, {screens::Screen::Status, "startup-paper"}, {screens::Screen::Status, "startup-book"},
        {screens::Screen::Status, "startup-error"}, {screens::Screen::Status, "library-loading"},
        {screens::Screen::Reader, "reader-paused"}, {screens::Screen::Reader, "reader-playing"},
        {screens::Screen::Reader, "reader-pausing"}, {screens::Screen::Reader, "reader-pressed"},
        {screens::Screen::InterfaceSettings, "display-controls"},
        {screens::Screen::StorageHub, "storage"}, {screens::Screen::PowerHub, "power"},
        {screens::Screen::ReaderAppearance, "appearance-type"},
        {screens::Screen::ReaderAppearance, "appearance-type-chunks"},
        {screens::Screen::ReaderAppearance, "appearance-guides"},
        {screens::Screen::ReaderAppearance, "appearance-guides-paper"},
        {screens::Screen::ReaderAppearance, "appearance-guides-chunks"},
        {screens::Screen::ReaderAppearance, "appearance-reading"},
        {screens::Screen::ReaderAppearance, "appearance-paused"},
        {screens::Screen::ReaderAppearance, "appearance-more"},
        {screens::Screen::FocusTimers, "focus"}, {screens::Screen::FocusTimers, "focus-session"},
        {screens::Screen::FocusSession, "focus-paused"},
        {screens::Screen::FocusSession, "focus-exit"},
        {screens::Screen::StorageEncryption,"encryption"}, {screens::Screen::StorageEncryption,"encryption-confirm"},
    };
    for (const auto& [view, name] : views) {
        const auto viewTheme = std::string_view{name}.ends_with("-paper") ? ui::modern::paperTheme() : theme;
        ui.setTheme(viewTheme);
        auto screen = view;
        reading.mode=std::string_view{name}.ends_with("-chunks")?settings::ReadingMode::chunks:settings::ReadingMode::rsvp;
        ui.setModernPresentation(screen != screens::Screen::Reader);
        ui.setModernReadingPalette(screen == screens::Screen::Reader);
        ui.invalidate();
        if (screen != screens::Screen::Status) ui.beginFrame(static_cast<uint8_t>(screen),focus.frameSurface(screen));
        switch (screen) {
        case screens::Screen::Reader: {
            const bool playing = std::string_view{name} == "reader-playing" || std::string_view{name} == "reader-pausing";
            // One fixture word, rasterized with the real device face. This is
            // a chrome review, not a simulation of the reading/phantom engine.
            ui::fonts::AlphaTextRenderer<640> word(panel);
            word.begin(); word.setFont(*ui::fonts::LiterataFallbackAlpha4_Sizes[1]);
            word.setTextColor(ui.color(ui::themes::Foreground), ui.color(ui::themes::Background));
            word.drawString("reading", 230, 112, 0);
            screens::modern::readerChrome(ui, {.vertical=false, .chapter="Chapter 4 · Martha", .footer="42%",
                .batteryLabel="84%", .reading=playing, .topState=0, .bottomState=0, .percent=42,
                .pressedControl=std::string_view{name} == "reader-pressed" ? static_cast<uint8_t>(screens::modern::ReaderControl::Footer) : uint8_t{0},
                .pausePending=std::string_view{name} == "reader-pausing"},
                reading, {{true, 3.9f, 84}, 0, false});
            break;
        }
        case screens::Screen::ReadingHub: screens::readingSettings(ui, reading, screen); break;
        case screens::Screen::DisplayHub: case screens::Screen::InterfaceSettings:
            display.draw(ui, interface, std::array<uint32_t, 5>{0, 60000, 300000, 600000, 1800000}, nullptr, screen); break;
        case screens::Screen::Status:
            ui.setStartupPresentation(true);
            if (std::string_view{name} == "library-loading") {
                ui.setStartupPresentation(false);
                screens::status(ui, "Loading library", "Getting your books ready", {});
            } else if (std::string_view{name} == "startup-book")
                screens::status(ui, ui.text(UiText::OpeningBook), longBook.title, "Reading your saved position", 42);
            else if (std::string_view{name} == "startup-error")
                screens::status(ui, ui.text(UiText::BookFailed), book.title, ui.text(UiText::CheckSdCard));
            else
                screens::status(ui, ui.text(UiText::Storage), ui.text(UiText::Checking));
            ui.setStartupPresentation(false);
            break;
        case screens::Screen::Dictionary:
            dictionary.open(reader);dictionary.draw(ui,reader,screen,0);break;
        case screens::Screen::QuickControls:screens::modern::quickControls(ui,reading,interface,screen);break;
        case screens::Screen::Help:screens::OnboardingScreen::help(ui,reading,screen);break;
        case screens::Screen::Read:
            screens::modern::menu(ui, book.title, book.author, 42, true, interface, screen); break;
        case screens::Screen::Library:
            if (std::string_view{name} == "library-long") library.draw(ui, {{&longBook, {}, 17}}, 0, screen);
            else if (std::string_view{name} == "library-empty") library.draw(ui, {}, 0, screen);
            else library.draw(ui, books, 0, screen);
            break;
        case screens::Screen::Settings: screens::settings(ui, screen); break;
        case screens::Screen::Chapters: chapters.draw(ui, markers, reader, reading, 0, screen); break;
        case screens::Screen::Device: case screens::Screen::StorageHub: case screens::Screen::PowerHub:
            screens::device(ui, true, 18, settings::NvsEncryptionState::Available, screen); break;
        case screens::Screen::ReaderAppearance: appearance.appearance(ui, screen); break;
        case screens::Screen::StorageEncryption: screens::storageEncryption(ui,settings::NvsEncryptionState::Available,screen);break;
        case screens::Screen::FocusTimers: case screens::Screen::FocusSession: focus.draw(ui, 0, screen); break;
        case screens::Screen::ReadingSettings: screens::readingSettings(ui, reading, screen); break;
        case screens::Screen::PacingSettings: screens::pacingSettings(ui, reading.pacing, screen); break;
        case screens::Screen::Ota: screens::ota(ui, "preview-ui-dictionary-20261020.3", screen); break;
        default: screens::modern::hub(ui, screen); break;
        }
        if (screen != screens::Screen::Status) ui.endFrame();
        const auto tap = [&](uint16_t x, uint16_t y, auto draw) {
            uint32_t now = 1000;
            for (const bool down : {true, false, false}) {
                contact = {down, x, y}; ui.pollTouch(now += 25);
                ui.beginFrame(static_cast<uint8_t>(screen),focus.frameSurface(screen)); draw(); ui.endFrame();
            }
        };
        if (screen == screens::Screen::ReaderAppearance) {
            const auto draw = [&] { appearance.appearance(ui, screen); };
            if (std::string_view{name}.starts_with("appearance-guides") || std::string_view{name} == "appearance-more") tap(50, 107, draw);
            if (std::string_view{name} == "appearance-reading") tap(50,150,draw);
            if (std::string_view{name} == "appearance-paused") {tap(50,150,draw);tap(470,120,draw);}
        }
        if (std::string_view{name}=="encryption-confirm") tap(550,20,[&]{screens::storageEncryption(ui,settings::NvsEncryptionState::Available,screen);});
        if (std::string_view{name} == "focus-session") {
            tap(500,68,[&]{focus.draw(ui,0,screen);});screen=screens::Screen::FocusSession;
            ui.invalidate();ui.beginFrame(static_cast<uint8_t>(screen),focus.frameSurface(screen));focus.draw(ui,0,screen);ui.endFrame();
        }
        if (std::string_view{name} == "focus-exit") {
            // The prior screenshot pauses the session; resume before the break.
            tap(500,76,[&]{focus.draw(ui,0,screen);});screen=screens::Screen::FocusSession;
            focus.update(25*60000+1);screen=screens::Screen::FocusSession;
            ui.invalidate();ui.beginFrame(static_cast<uint8_t>(screen),focus.frameSurface(screen));focus.draw(ui,25*60000+1,screen);ui.endFrame();
        }
        if (std::string_view{name} == "focus-paused") {
            tap(420,135,[&]{focus.draw(ui,0,screen);});
            assert(focus.badge(0)=="Paused 25:00");
            ui.beginFrame(static_cast<uint8_t>(screen),focus.frameSurface(screen));focus.draw(ui,0,screen);ui.endFrame();
        }
        const auto path = std::filesystem::path{argv[1]} / (std::string{name} + ".ppm");
        auto* file = std::fopen(path.c_str(), "wb");
        if (!file) return 1;
        std::fprintf(file, "P6\n640 172\n255\n");
        for (int i = 0; i < 640 * 172; ++i) {
            const auto pixel = panel.getFramebuffer()[i];
            const unsigned char rgb[]{static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
                static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63),
                static_cast<unsigned char>((pixel & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, file);
        }
        std::fclose(file);
    }
    // Native screenshots of every lesson, plus replay/hold/swipe interaction.
    screens::OnboardingScreen intro;settings::DeviceSettings onboarding;
    const auto snapshot=[&](std::string name){
        const auto path=std::filesystem::path{argv[1]}/(name+".ppm");auto* file=std::fopen(path.c_str(),"wb");assert(file);
        std::fprintf(file,"P6\n640 172\n255\n");
        for(int i=0;i<640*172;++i){const auto p=panel.getFramebuffer()[i];const unsigned char rgb[]{static_cast<unsigned char>(((p>>11)&31)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};std::fwrite(rgb,1,3,file);}std::fclose(file);
    };
    uint32_t introNow=0;ui.setModernReadingPalette(false);ui.setModernPresentation(true);
    const auto introFrame=[&](bool down,int x,int y,uint32_t elapsed=25){
        contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(introNow+=elapsed);
        ui.beginFrame(static_cast<uint8_t>(screens::Screen::Onboarding),intro.surface());
        const auto result=intro.draw(ui,onboarding,introNow);ui.endFrame();return result;
    };
    for(const auto locale:{"en","de","ro"}){
        ui.setTheme(theme);ui.setLocale(locale);onboarding.interface.locale=locale;intro.begin(ui,onboarding,0);
        for(int step=0;step<12;++step){introFrame(false,0,0);
            const std::vector<uint16_t> prior(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
            ui.invalidate();introFrame(false,0,0);
            if(!std::equal(prior.begin(),prior.end(),panel.getFramebuffer()))std::fprintf(stderr,"Intro incremental mismatch %s %d\n",locale,step);
            snapshot(std::string{"intro-"}+locale+"-"+std::to_string(step));intro.next();}
    }
    ui.setLocale("en");onboarding.interface.locale="en";onboarding.reading.holdToRead=true;intro.begin(ui,onboarding,introNow);intro.next();
    introFrame(false,0,0);introFrame(true,410,100);introFrame(true,410,100,349);assert(!intro.sample().playing);
    introFrame(true,410,100,1);assert(intro.sample().playing);introFrame(false,410,100);assert(!intro.sample().playing);
    intro.next();introFrame(false,0,0);intro.hardwareControls();introFrame(false,0,0);
    snapshot("intro-controls-found");
    assert(intro.surface()==5);
    introFrame(true,606,20);introFrame(false,606,20);assert(onboarding.interface.brightnessPercent==70);
    assert(onboarding.reading.wpm==300);
    ui.setTheme(ui::modern::paperTheme());intro.next();introFrame(false,0,0);snapshot("intro-speed-paper");
    intro.next();introFrame(false,0,0);snapshot("intro-pacing-paper");intro.close();
    std::puts("Introduction: translated lessons, hold threshold/release, middle-button controls discovery, independent practice settings and themes rendered.");

    // Render production storage states and QR pixels in each built-in language/theme.
    using S=OnboardingStorage::State;
    int paletteIndex=0;
    for(const auto palette:{theme,ui::modern::paperTheme()}){
        ui.setTheme(palette);
        for(const auto locale:{"en","de","ro"}){
            ui.setLocale(locale);onboarding.interface.locale=locale;
            const auto prefix=std::string{"setup-"}+std::to_string(paletteIndex)+"-"+locale;
            for(const auto state:{S::Checking,S::Ready,S::Unavailable,S::FoldersFailed,S::SaveFailed,S::TimedOut}){
                intro.begin(ui,onboarding,introNow);intro.beginStorageCheck(introNow);
                intro.acceptStorageProgress(intro.storageRequest(),{state,uint8_t(state==S::Unavailable?0:state==S::FoldersFailed?1:2)},introNow);
                introFrame(false,0,0);
                if(state!=S::Checking){
                    const std::vector<uint16_t> prior(panel.getFramebuffer(),panel.getFramebuffer()+640*172);
                    ui.invalidate();introFrame(false,0,0);
                    assert(std::equal(prior.begin(),prior.end(),panel.getFramebuffer()));
                }
                snapshot(prefix+"-sd-"+std::to_string(int(state)));
            }
            intro.begin(ui,onboarding,introNow);intro.next(10);introFrame(false,0,0);snapshot(prefix+"-books-wifi");
            introFrame(true,250,70);introFrame(false,250,70);introFrame(false,0,0);snapshot(prefix+"-books-usb");
            intro.close();
        }
        ++paletteIndex;
    }
    std::puts("Storage: six production states and both upload methods rendered in three languages and both built-in themes; stable states match fresh redraws.");

    ui.setTheme(theme);ui.setLocale("en");screens::TouchCheckScreen touchCheck;auto checkScreen=screens::Screen::TouchCheck;
    const auto checkFrame=[&](bool down,int x,int y){contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(introNow+=25);ui.beginFrame(uint8_t(checkScreen));touchCheck.draw(ui,checkScreen);ui.endFrame();};
    for(int y:{55,105,171})for(int x:{0,160,267,373,480,639})checkFrame(true,x,y);
    checkFrame(false,639,171);snapshot("touch-check-edges");

    // Reach real screen states through their public controls. Wi-Fi and storage
    // responses are synthetic; this checks layout, not radio or flash hardware.
    settings::SettingsStore reviewStore;
    screens::NetworkScreen network;network.begin(reviewStore);
    auto reviewScreen=screens::Screen::NetworkSettings;
    const auto frame=[&](auto draw){contact={};ui.pollTouch(introNow+=25);ui.invalidate();ui.beginFrame(uint8_t(reviewScreen));draw();ui.endFrame();};
    const auto tap=[&](int x,int y,auto draw){for(bool down:{true,false,false}){contact={down,uint16_t(x),uint16_t(y)};ui.pollTouch(introNow+=25);ui.beginFrame(uint8_t(reviewScreen));draw();ui.endFrame();}};
    const auto networkDraw=[&]{network.draw(ui,reviewStore,reviewScreen);};
    frame(networkDraw);snapshot("wifi-settings");
    tap(390,105,networkDraw);assert(reviewScreen==screens::Screen::NetworkEdit);
    frame([&]{network.drawEdit(ui,reviewStore,reviewScreen);});snapshot("ota-owner");
    reviewScreen=screens::Screen::NetworkSettings;frame(networkDraw);
    tap(540,105,networkDraw);frame([&]{network.drawEdit(ui,reviewStore,reviewScreen);});snapshot("ota-tag");
    for(auto [result,name]:{std::pair{-1,"wifi-scanning"},{-2,"wifi-failed"},{0,"wifi-empty"},{3,"wifi-scan"}}){
        WiFi.found=result;network.openWifiScan();reviewScreen=screens::Screen::WifiScan;
        frame([&]{network.drawWifiScan(ui,reviewStore,reviewScreen);});snapshot(name);
    }
    tap(240,70,[&]{network.drawWifiScan(ui,reviewStore,reviewScreen);});
    assert(reviewScreen==screens::Screen::WifiConnect);
    frame([&]{network.drawWifiConnect(ui,reviewStore,reviewScreen);});snapshot("wifi-password");
    settings::wifi::remember(reviewStore.settings().network,reviewStore.secrets(),"Home network","sample-only");
    settings::wifi::remember(reviewStore.settings().network,reviewStore.secrets(),"Pixel hotspot","sample-only");
    network.openSavedWifi();reviewScreen=screens::Screen::WifiScan;
    frame([&]{network.drawWifiScan(ui,reviewStore,reviewScreen);});snapshot("wifi-saved");
    tap(80,80,[&]{network.drawWifiScan(ui,reviewStore,reviewScreen);});
    assert(reviewScreen==screens::Screen::NetworkSettings);
    assert(reviewStore.settings().network.ssid=="Pixel hotspot");
    frame(networkDraw);snapshot("wifi-settings-saved");
    reviewScreen=screens::Screen::ButtonMap;
    frame([&]{screens::modern::header(ui,"Physical buttons",reviewScreen,screens::Screen::Help);intro.buttons(ui);});snapshot("button-map");
    reviewScreen=screens::Screen::AddBooks;
    frame([&]{screens::modern::header(ui,"Add books",reviewScreen,screens::Screen::Help);intro.books(ui);});snapshot("add-books");
    for(const auto mode:{"missing-pack","missing-word","online-error","long"}){
        dictionary::previewCase=mode;dictionary.open(reader);reviewScreen=screens::Screen::Dictionary;
        frame([&]{dictionary.draw(ui,reader,reviewScreen,introNow);});snapshot(std::string("dictionary-")+mode);
    }
    reviewScreen=screens::Screen::DictionarySearch;
    frame([&]{dictionary.draw(ui,reader,reviewScreen,introNow);});snapshot("dictionary-search");
    focus.close();focus.begin();reviewScreen=screens::Screen::FocusPresets;
    const auto focusDraw=[&]{focus.draw(ui,introNow,reviewScreen);};
    frame(focusDraw);snapshot("focus-presets");
    tap(500,75,focusDraw);assert(reviewScreen==screens::Screen::FocusEditor);frame(focusDraw);snapshot("focus-editor");
    tap(200,25,focusDraw);assert(reviewScreen==screens::Screen::FocusNameEdit);frame(focusDraw);snapshot("focus-name");
    reviewScreen=screens::Screen::FocusEditor;frame(focusDraw);tap(490,25,focusDraw);frame(focusDraw);snapshot("focus-delete-confirm");
    focus.close();focus.begin();reviewScreen=screens::Screen::FocusTimers;frame(focusDraw);
    tap(500,135,focusDraw);assert(reviewScreen==screens::Screen::FocusSession);frame(focusDraw);snapshot("focus-timer-only");
    introNow+=25*60000;focus.update(introNow);frame(focusDraw);snapshot("focus-break");
    introNow+=5*60000;focus.update(introNow);frame(focusDraw);snapshot("focus-waiting");
    screens::StandbyScreen standby;standby.begin(ui,introNow,0,20,standby::Kind::life);standby.update(ui,introNow);snapshot("standby");
    for(auto [name,title,line1,line2]:{
        std::tuple{"companion","Sync","Connected: Reading room","192.0.2.1"},
        {"usb-transfer","USB","Mounted read/write","hold PWR to exit"},
        {"status-storage","Storage","SD card ready","30 books · 48 MB dictionary"},
        {"dictionary-loading","Looking up word","Your book stays paused",""},
        {"ota-error","Update failed","Could not reach GitHub","Check Wi-Fi and try again"},
        {"power-off","Off","Release PWR",""}
    }) {screens::status(ui,title,line1,line2);snapshot(name);}
    std::puts("Review states: real Wi-Fi controls/keyboards, Focus editors/break, dictionary variants, help and standby captured with synthetic hardware responses.");
}
