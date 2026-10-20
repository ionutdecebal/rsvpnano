#include "ui/screens/ReaderScreen.h"
#include "reader/ContextChunks.h"
#include "ui/ClippedGfx.h"

namespace screens {
void ReaderScreen::prepareChunk() {
    if (settings_.mode != settings::ReadingMode::chunks) {
        session.displayWordCount = 1;
        session.displayWordIndex = SIZE_MAX;
        chunkLayoutReady_ = false;
        return;
    }
    const auto index = session.state.wordIndex;
    if (session.displayWordIndex == index && chunkRevision_ == typographyRevision_) return;
    session.displayWordIndex = index;
    session.displayWordCount = 1;
    chunkRevision_ = typographyRevision_;
    chunkLayoutReady_ = false;
    chunkText_ = ReadingLoop::wordAt(session, index);
    if (chunkText_.empty() || ReadingLoop::pacingMode(session) == settings::ReadingPacing::cjkPhrase
        || session.metadata.writingMode != WritingMode::horizontalTb
        || session.metadata.requiresBidi(index, index+1)) return;

    // One-word frames share this layout too. Unsupported scripts/directions still
    // use the normal shaping path, with the highlight disabled for chunk mode.
    refreshTypeface();
    activateFace(face_);
    chunkLayoutReady_ = true;
    const auto sourceLocale = session.metadata.localeAt(index);
    const auto locale = session.localeAt(index);
    if (!locale.empty() && locale != "en" && !locale.starts_with("en-")) return;
    const auto family = fontChoice(index);
    std::array<std::string, ContextChunks::kLookahead> words;
    size_t count = 0;
    for (size_t at=index; at<ReadingLoop::wordCount(session) && count<words.size(); ++at) {
        if (at>index && (std::ranges::binary_search(session.metadata.paragraphStarts, at)
            || session.metadata.chapterAt(at) != session.metadata.chapterAt(index)
            || session.metadata.localeAt(at) != sourceLocale || fontChoice(at) != family
            || session.metadata.requiresBidi(at, at+1))) break;
        words[count] = ReadingLoop::wordAt(session, at);
        if (!ContextChunks::englishToken(words[count])) break;
        ++count;
        if (ContextChunks::boundary(words[count-1])) break;
    }
    if (count<2) return;
    const auto fits = [&](std::string_view phrase) {
        return text_.textAdvance(phrase, typography_.tracking) <= width_ - 32;
    };
    auto n = ContextChunks::count(std::span{words}.first(count), fits);
    // Check shaped widths too, retaining equal space on both sides of the chunk.
    for (;n>1;--n) {
        chunkText_ = words[0];
        for (size_t i=1;i<n;++i) { chunkText_ += ' '; chunkText_ += words[i]; }
        if (face_.shaper) {
            std::vector<ui::fonts::PositionedGlyph> glyphs;
            const auto width = face_.shaper->shape(chunkText_,0,chunkText_.size(),false,locale,text_,glyphs);
            if (!width || *width > width_ - 32) continue;
        }
        break;
    }
    if (n==1) chunkText_ = words[0];
    session.displayWordCount = n;
}

void ReaderScreen::drawChunk(ui::Context& ui, ui::Rect area) {
    const auto signature = ui::Context::combine(frameSignature(chunkText_,false,false,settings_),session.displayWordCount);
    if (!ui.redraw(area,signature,true)) return;
    activateFace(face_);
    text_.setTextColor(ui.color(ui::themes::Foreground),ui.color(ui::themes::Background));
    bool shaped = false;
    int16_t chunkWidth = text_.textAdvance(chunkText_, typography_.tracking);
    rsvpGlyphs_.clear();
    if (face_.shaper) {
        const auto result = face_.shaper->shape(chunkText_,0,chunkText_.size(),false,session.localeAt(session.state.wordIndex),text_,rsvpGlyphs_);
        shaped = result.has_value();
        if (result) chunkWidth = *result;
    }
    if (shaped) text_.prepare(std::span<const ui::fonts::PositionedGlyph>{rsvpGlyphs_});
    else text_.prepare(chunkText_);
    const auto& raster = face_.raster.get();
    const int16_t baseline = area.y+(area.h-(raster.wordInkBottom-raster.wordInkTop+1))/2-raster.wordInkTop;
    const int16_t anchor = ui.width()/2;
    const int16_t x = anchor-chunkWidth/2;
    ui.paint(area,[&](Arduino_GFX& gfx,ui::Rect translated) {
        ui::ClippedGfx clipped(gfx,translated);
        auto& previous = text_.setOutput(clipped);
        gfx.fillRect(translated.x,translated.y,translated.w,translated.h,ui.color(ui::themes::Background));
        const int16_t dx=translated.x-area.x,dy=translated.y-area.y;
        drawGuides(ui,clipped,anchor+dx,baseline+dy,translated);
        if (shaped) text_.drawGlyphs(rsvpGlyphs_,x+dx,baseline+dy);
        else text_.drawString(chunkText_,x+dx,baseline+dy,typography_.tracking);
        text_.setOutput(previous);
    });
}
} // namespace screens
