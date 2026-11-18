#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include <cctype>

namespace screens {
namespace {
constexpr int16_t pageHeaderHeight = 44;
enum PageControl : size_t { menu, findPlace, dictionary, backToBook, previousLine, nextLine, pageText, none };
struct PageButton { ui::Rect rect; std::string_view label; };
constexpr PageButton pageButtons[]{
    {{8, 0, 144, 44}, "‹ Menu"}, {{168, 0, 144, 44}, "Find place"}, {{328, 0, 144, 44}, {}},
    {{488, 0, 144, 44}, "Back to book ↗"}, {{588, 46, 44, 48}, "↑"}, {{588, 98, 44, 48}, "↓"}
};
static_assert(std::size(pageButtons) == pageText);
std::string folded(std::string text) {
    for(auto& c:text) if(c>='A' && c<='Z') c+=('a'-'A');
    return text;
}
}
void ReaderScreen::openPage(bool hideControls) {
    ReadingLoop::pause(session);
    pageBrowsing_=true;pageSelected_=session.state.wordIndex;
    browse_ = {};
    browse_.first = pageSelected_;
    pageTouchControl_ = none;
    pageBarHidden_ = hideControls;
    pageScrollFinished_ = false;
    pageSelectionPinned_ = false;
    pageVerticalMoved_ = false;
    pageVerticalHighlight_ = false;
    pageScrubMoved_ = false;
    pageScrubFinished_ = false;
    pageHasScrolled_ = false;
    pageGesture_ = PageGesture::pending;
}
ui::Rect ReaderScreen::pageBody() const {
    const int16_t top = pageBarVisible() ? pageHeaderHeight : 0;
    return {0, top, 580, static_cast<int16_t>(172 - top)};
}
bool ReaderScreen::revealPageControls() {
    if (!pageBrowsing_ || pageBarVisible())
        return false;
    pageBarHidden_ = false;
    return true;
}
void ReaderScreen::openLocator(Screen& screen) {
    ReadingLoop::pause(session);
    locatorOrigin_=session.state.wordIndex;
    candidate_=pageBrowsing_?pageSelected_:session.state.wordIndex;
    screen=Screen::FindPlace;
}
void ReaderScreen::commitPlace(size_t index,Preferences& preferences,uint32_t nowMs) {
    if(index!=session.state.wordIndex) returnWord_=session.state.wordIndex;
    ReadingLoop::seekTo(session,index);finishPause(preferences,nowMs);
    pageSelected_=session.state.wordIndex;
}
void ReaderScreen::syncPagePosition(uint32_t nowMs) {
    if (!pageBrowsing_ || browse_.highlighted == SIZE_MAX
        || pageSelected_ == session.state.wordIndex)
        return;
    if (!pageHasScrolled_) {
        returnWord_ = session.state.wordIndex;
        pageHasScrolled_ = true;
    }
    ReadingLoop::seekTo(session, pageSelected_);
    pageProgressChangedAt_ = nowMs;
}
void ReaderScreen::drawPage(ui::Context& ui, uint32_t nowMs) {
    // Installed fonts keep their own glyphs and existing scaled sizes.
    constexpr uint8_t scales[]{PageReader::kNativeScale, 50, 64, 80, PageReader::kNativeScale};
    const auto size = settings_.pageTextSizeIndex();
    const bool builtInPageSize = size >= 1 && size <= 3 && fonts.families().size() == 1;
    const auto scale = builtInPageSize ? PageReader::kNativeScale : scales[size];
    const auto revision = ui::Context::combine(typographyRevision_, size);
    const bool verticalScroll = (pageGesture_ == PageGesture::vertical && pageVerticalMoved_)
                             || (pageScrollFinished_ && browse_.step == PageReader::ScrollStep::none);
    const size_t selected = verticalScroll && !pageSelectionPinned_ ? SIZE_MAX : pageSelected_;
    const auto highlightMode = !verticalScroll && !pageVerticalHighlight_ ? PageReader::HighlightMode::selection
                             : PageReader::HighlightMode::departingScrollLine;
    const bool follow = (pageGesture_ == PageGesture::horizontal && pageScrubMoved_) || pageScrubFinished_;
    PageReader::browse(browse_, ui, text_, [&](size_t index) { return pageTypeface(index, builtInPageSize); },
                       typography_, revision, session, pageBody(), selected, scale, follow, highlightMode, size <= 2);
    const bool gestureFinished = pageScrollFinished_ || pageScrubFinished_;
    pageScrubFinished_ = false;
    if (verticalScroll && browse_.highlighted != selected)
        pageSelectionPinned_ = false;
    if (browse_.highlighted != SIZE_MAX)
        pageSelected_ = browse_.highlighted;
    pageScrollFinished_ = false;
    if (gestureFinished)
        syncPagePosition(nowMs);
    for (size_t i = pageBarVisible() ? menu : previousLine; i < std::size(pageButtons); ++i) {
        const auto& item = pageButtons[i];
        const auto label = i == dictionary ? modern::copy(ui, modern::Copy::Dictionary) : item.label;
        const bool enabled = i == dictionary ? browse_.highlighted != SIZE_MAX : i == findPlace || i == backToBook;
        ui.label(item.rect, label, 13, enabled ? ui::themes::Accent : ui::themes::Muted, ui::TextAlign::Center);
    }
    if (pageBarVisible())
        ui::modern::rule(ui,{0,43,640,1});
    ui.label({584,148,52,24},std::to_string(ReadingProgress::percent(browse_.topLine == SIZE_MAX ? browse_.first : browse_.topLine, ReadingLoop::wordCount(session)))+"%",11,ui::themes::Muted,ui::TextAlign::Center);
}
void ReaderScreen::beginPageTouch(int x, int y) {
    resetTouch();
    pageGesture_ = PageGesture::pending;
    pageTouchX_ = x;
    pageScrollY_ = pageTouchY_ = pageLastTouchY_ = y;
    pageTouchDelta_ = 0;
    pagePendingTouchY_.reset();
    pageScrollDirection_ = 0;
    pageVerticalMoved_ = false;
    pageScrubMoved_ = false;
    pageTouchControl_ = ui::contains(pageBody(), x, y) ? pageText : none;
    for (size_t i = pageBarVisible() ? menu : previousLine; i < std::size(pageButtons); ++i) {
        if (ui::contains(pageButtons[i].rect, x, y)) {
            pageTouchControl_ = i;
            break;
        }
    }
}
bool ReaderScreen::handlePage(ui::Context& ui, uint32_t nowMs, Preferences& preferences, Screen& screen) {
    const auto* touch = ui.touch();
    if (!touch)
        return true;

    const auto hideBar = [&] {
        if (settings_.pageControlsAutoHide && !pageBarHidden_) {
            pageBarHidden_ = true;
            ui.invalidate();
        }
    };
    const auto finishScroll = [&] {
        pageScrollFinished_ = true;
        hideBar();
    };
    if (ui::hasTouch(*touch, ui::TouchCancel)) {
        if (pageGesture_ == PageGesture::vertical && pageVerticalMoved_)
            finishScroll();
        else if (pageGesture_ == PageGesture::horizontal)
            pageScrubFinished_ = pageScrubMoved_;
        pageGesture_ = PageGesture::pending;
        pageTouchControl_ = none;
        return true;
    }

    if (ui::hasTouch(*touch, ui::TouchStart))
        beginPageTouch(touch->x, touch->y);

    const int dx = int(touch->x) - pageTouchX_;
    const int dy = int(touch->y) - pageTouchY_;
    if (pageTouchControl_ == pageText && ui::hasTouch(*touch, ui::TouchMove)) {
        constexpr int verticalSlop = 6;
        constexpr int horizontalSlop = 8;
        constexpr int scrubStep = 16;
        constexpr int jitter = 2;
        if (pageGesture_ == PageGesture::pending) {
            if (std::abs(dx) >= horizontalSlop && 2 * std::abs(dx) > 3 * std::abs(dy)) {
                pageGesture_ = PageGesture::horizontal;
                pageVerticalHighlight_ = false;
                pageScrubX_ = pageTouchX_ - (dx > 0 ? 6 : -6);
            } else if (std::abs(dy) >= verticalSlop && std::abs(dy) >= std::abs(dx)) {
                pageGesture_ = PageGesture::vertical;
                pageScrollY_ += dy > 0 ? verticalSlop - jitter : jitter - verticalSlop;
            }
        }
        if (pageGesture_ == PageGesture::vertical) {
            const int rawDelta = int(touch->y) - pageLastTouchY_;
            const int jump = std::max(8, 2 * std::abs(pageTouchDelta_) + 4);
            // At slow speed, require a second reading before acting on an
            // isolated jump. Small changes still track the finger immediately.
            if (pageVerticalMoved_ && std::abs(rawDelta) > jump) {
                if (!pagePendingTouchY_ || ((rawDelta > 0) != (*pagePendingTouchY_ > pageLastTouchY_))) {
                    pagePendingTouchY_ = touch->y;
                    return true;
                }
            }
            pagePendingTouchY_.reset();
            pageTouchDelta_ = rawDelta;
            pageLastTouchY_ = touch->y;
            const int delta = pageScrollY_ - int(touch->y);
            const int direction = delta < 0 ? -1 : 1;
            // Small reversals around a stationary finger need more travel
            // than continuing a drag. Rebase to avoid a catch-up jump.
            const bool reverses = pageScrollDirection_ != 0 && pageScrollDirection_ != direction;
            constexpr int reverseSlop = 5;
            const int slop = reverses ? reverseSlop : jitter;
            if (std::abs(delta) > slop) {
                browse_.scrollBy(delta - direction * slop);
                pageScrollY_ = int(touch->y) + direction * jitter;
                pageScrollDirection_ = direction;
                pageVerticalMoved_ = true;
                pageVerticalHighlight_ = true;
            }
        } else if (pageGesture_ == PageGesture::horizontal) {
            while (std::abs(int(touch->x) - pageScrubX_) >= scrubStep) {
                const bool right = touch->x > pageScrubX_;
                const size_t word = PageReader::scrubWord(browse_, pageSelected_, right);
                if (word == SIZE_MAX) {
                    pageScrubX_ = touch->x;
                    break;
                }
                pageSelected_ = word;
                pageSelectionPinned_ = true;
                pageScrubMoved_ = true;
                pageScrubX_ += right ? scrubStep : -scrubStep;
            }
        }
    }

    if (!ui::hasTouch(*touch, ui::TouchRelease))
        return true;
    const auto control = pageTouchControl_;
    const auto gesture = pageGesture_;
    pageTouchControl_ = none;
    pageGesture_ = PageGesture::pending;
    if (gesture == PageGesture::vertical) {
        if (pageVerticalMoved_)
            finishScroll();
        return true;
    }
    if (gesture == PageGesture::horizontal) {
        pageScrubFinished_ = pageScrubMoved_;
        hideBar();
        return true;
    }
    if (!ui::hasTouch(*touch, ui::TouchTap)
        || (control < pageText && !ui::contains(pageButtons[control].rect, touch->x, touch->y)))
        return true;
    switch (control) {
    case menu: screen = Screen::Read; break;
    case dictionary:
        if (browse_.highlighted != SIZE_MAX)
            screen = Screen::Dictionary;
        break;
    case findPlace: openLocator(screen); break;
    case backToBook:
        if (pageSelected_ != session.state.wordIndex)
            pageProgressChangedAt_ = nowMs;
        commitPlace(pageSelected_, preferences, nowMs);
        pageBrowsing_ = false;
        break;
    case previousLine:
    case nextLine: {
        const bool down = control == nextLine;
        browse_.step = down ? PageReader::ScrollStep::nextPage : PageReader::ScrollStep::previousPage;
        browse_.stepWord = PageReader::edgeWord(browse_, down);
        pageSelectionPinned_ = false;
        finishScroll();
        break;
    }
    case pageText: {
        const size_t word = PageReader::wordAtPoint(browse_, touch->x, touch->y);
        if (word != SIZE_MAX) {
            pageVerticalHighlight_ = false;
            pageSelected_ = word;
            pageSelectionPinned_ = true;
        }
        break;
    }
    default: break;
    }
    return true;
}
std::string ReaderScreen::placeExcerpt(size_t index,size_t maxWords) const {
    std::string result;
    for(size_t i=index;i<ReadingLoop::wordCount(session)&&i<index+maxWords;++i) {
        const std::string word{ReadingLoop::wordAt(session,i)};
        if(result.size()+word.size()>512) break;
        if(!result.empty()) result+=' ';
        result+=word;
    }
    return result;
}
void ReaderScreen::updateSearch() {
    if(!searchRunning_ || searchQuery_.empty()) return;
    const auto query=folded(searchQuery_);
    const size_t stop=std::min(ReadingLoop::wordCount(session),searchCursor_+48);
    const uint32_t started=millis();
    while(searchCursor_<stop && searchResults_.size()<64) {
        const size_t index=searchCursor_++;
        std::string window{ReadingLoop::wordAt(session,index)};
        const size_t firstSize=window.size();
        for(size_t i=index+1;i<ReadingLoop::wordCount(session) && i<index+64 && window.size()<firstSize+query.size();++i) {
            if(std::ranges::binary_search(session.metadata.paragraphStarts,i)) break;
            window+=' ';window+=ReadingLoop::wordAt(session,i);
        }
        const auto at=folded(std::move(window)).find(query);
        if(at!=std::string::npos && at<firstSize) searchResults_.push_back(index);
        if(millis()-started>=8) break;
    }
    if(searchCursor_>=ReadingLoop::wordCount(session) || searchResults_.size()>=64) searchRunning_=false;
}
void ReaderScreen::drawLocator(ui::Context& ui,Screen& screen,Preferences& preferences,uint32_t nowMs) {
    using ui::modern::button;using ui::modern::Style;
    if(candidate_==SIZE_MAX) candidate_=browse_.first;
    if(screen==Screen::BookSearchKeyboard) {
        const auto result=ui.keyboard({0,0,640,172},searchDraft_,64,searchKeyboard_,"Find in book");
        if(result==ui::KeyboardAction::Cancel) screen=Screen::BookSearch;
        if(result==ui::KeyboardAction::Submit) {
            searchQuery_=searchDraft_;searchResults_.clear();searchCursor_=0;searchPage_=0;
            searchRunning_=!searchQuery_.empty();screen=Screen::BookSearch;
        }
        return;
    }
    if(screen==Screen::BookSearch) {
        updateSearch();
        const int swipe=locatorSwipe_.update(ui,{12,48,550,120});
        if(swipe>0 && (searchPage_+1)*2<searchResults_.size()) ++searchPage_;
        if(swipe<0 && searchPage_>0) --searchPage_;
        if(button(ui,{8,0,102,44},"‹ Find place",Style::Plain)) screen=Screen::FindPlace;
        ui.label({120,0,286,44},searchQuery_.empty()?"Search this book":searchQuery_,15);
        if(button(ui,{468,0,164,44},"Edit query",Style::Accent)) {searchDraft_=searchQuery_;searchKeyboard_={};screen=Screen::BookSearchKeyboard;}
        for(size_t row=0;row<2;++row) {
            const size_t result=searchPage_*2+row;
            if(result>=searchResults_.size()) break;
            const size_t word=searchResults_[result];
            const auto* chapter=session.metadata.chapterAt(word);
            if(button(ui,{12,int16_t(48+row*60),550,54},placeExcerpt(word,16),Style::Secondary,true,
                      std::to_string(ReadingProgress::percent(word,ReadingLoop::wordCount(session)))+"% · "+(chapter?chapter->title:"Book"),13,ui::TextAlign::Start)) {candidate_=word;screen=Screen::FindPlace;}
        }
        if(searchResults_.empty()) ui.label({16,60,534,92},searchRunning_?"Searching…":searchQuery_.empty()?"Search words or a phrase.\nThe keyboard opens on its own screen.":"No matches found. Try another word or phrase.",17,ui::themes::Muted,ui::TextAlign::Start,3);
        if(button(ui,{576,48,52,50},"↑",Style::Plain,searchPage_>0)) --searchPage_;
        const bool hasNext=(searchPage_+1)*2<searchResults_.size();
        if(button(ui,{576,104,52,50},hasNext?"↓":"More",Style::Plain,hasNext || (!searchRunning_ && searchCursor_<ReadingLoop::wordCount(session) && !searchQuery_.empty()),{},hasNext?20:11)) {
            if(hasNext) ++searchPage_;
            else {searchResults_.clear();searchPage_=0;searchRunning_=true;}
        }
        ui.progress({0,169,640,3},ReadingProgress::percent(searchCursor_,ReadingLoop::wordCount(session)));
        return;
    }
    if(screen==Screen::FindChapters) {
        if(button(ui,{8,0,102,44},"‹ Find place",Style::Plain)) screen=Screen::FindPlace;
        ui.label({124,0,364,44},"Choose a chapter to preview",16);
        const auto& chapters=session.metadata.chapters;
        const size_t pages=std::max<size_t>(1,(chapters.size()+1)/2);
        const int swipe=locatorSwipe_.update(ui,{12,48,550,120});
        if(swipe>0 && findChapterPage_+1<pages) ++findChapterPage_;
        if(swipe<0 && findChapterPage_>0) --findChapterPage_;
        findChapterPage_=std::min(findChapterPage_,pages-1);
        for(size_t row=0;row<2 && findChapterPage_*2+row<chapters.size();++row) {
            const auto index=findChapterPage_*2+row;const auto& chapter=chapters[index];
            if(button(ui,{12,int16_t(48+row*60),550,54},std::to_string(index+1)+"  "+chapter.title,Style::Secondary,true,
                      std::to_string(ReadingProgress::percent(chapter.wordIndex,ReadingLoop::wordCount(session)))+"% through book",14,ui::TextAlign::Start)) {candidate_=chapter.wordIndex;screen=Screen::FindPlace;}
        }
        if(chapters.empty()) ui.label({16,58,540,80},"This book has no chapter markers.\nUse the book slider or search to find a place.",17,ui::themes::Muted,ui::TextAlign::Start,2);
        if(button(ui,{576,48,52,50},"↑",Style::Plain,findChapterPage_>0)) --findChapterPage_;
        if(button(ui,{576,108,52,50},"↓",Style::Plain,findChapterPage_+1<pages)) ++findChapterPage_;
        return;
    }
    if(button(ui,{8,0,90,44},"‹ Cancel",Style::Plain)) screen=Screen::Reader;
    if (returnWord_ == SIZE_MAX) ui.label({108,0,186,44},"Find your place",16);
    else if (button(ui,{108,0,178,44},"↶ Previous place",Style::Plain,true,{},13)) candidate_=returnWord_;
    if(button(ui,{296,0,92,44},"Search",Style::Accent)) {screen=searchQuery_.empty()?Screen::BookSearchKeyboard:Screen::BookSearch;searchDraft_=searchQuery_;}
    if(button(ui,{394,0,100,44},"Chapters",Style::Accent)) screen=Screen::FindChapters;
    if(button(ui,{504,0,128,44},"Read here ↗",Style::Primary,ReadingLoop::wordCount(session)>0)) {
        commitPlace(candidate_,preferences,nowMs);pageBrowsing_=false;screen=Screen::Reader;
    }
    ui::modern::rule(ui,{0,43,640,1});
    const auto* chapter=session.metadata.chapterAt(candidate_);
    ui.label({16,49,324,24},chapter?chapter->title:"Book preview",12,ui::themes::Accent);
    ui.label({16,77,324,83},placeExcerpt(candidate_),17,ui::themes::Foreground,ui::TextAlign::Start,3,session.metadata.localeAt(candidate_));
    int progress=int((uint64_t(candidate_)*100)/std::max<size_t>(1,ReadingLoop::wordCount(session)-1));
    if(ui.slider({362,47,262,46},"Through book",progress,0,100,1,"%")) candidate_=(uint64_t(progress)*std::max<size_t>(1,ReadingLoop::wordCount(session)-1))/100;
    const auto& starts=session.metadata.paragraphStarts;
    if(button(ui,{362,102,126,44},"‹ Paragraph",Style::Secondary,candidate_>0,{},12)) {
        auto it=std::ranges::lower_bound(starts,candidate_);candidate_=it==starts.begin()?0:*std::prev(it);
    }
    if(button(ui,{498,102,126,44},"Paragraph ›",Style::Secondary,candidate_+1<ReadingLoop::wordCount(session),{},12)) {
        auto it=std::ranges::upper_bound(starts,candidate_);candidate_=it==starts.end()?ReadingLoop::wordCount(session)-1:*it;
    }
    ui.label({362,148,262,24},"Preview only · your place is saved",11,ui::themes::Muted,ui::TextAlign::Center);
}
} // namespace screens
