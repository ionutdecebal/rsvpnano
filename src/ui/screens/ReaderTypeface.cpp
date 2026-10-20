#include "ui/screens/ReaderScreen.h"
#include "ui/screens/ReaderLayout.h"

namespace screens {
    void ReaderScreen::activateFace(const FontCatalog::Face& face) {
        text_.setFont(face.raster.get());
    }

    void ReaderScreen::refreshTypeface() {
        // FontCatalog keeps one mutable strike per family. Page browsing can
        // replace the object referenced by face_ without changing the word.
        if (pageFontActive_) {
            pageFontActive_ = false;
            pageState_ = {};
            browse_.layout = {};
            loadedWordIndex_ = SIZE_MAX;
            loadedFontSizeIndex_ = 0xFF;
        }
        if (loadedWordIndex_ == session.state.wordIndex && loadedFontSizeIndex_ == typography_.fontSizeIndex) {
            activateFace(face_);
            return;
        }
        loadedWordIndex_ = session.state.wordIndex;
        const size_t family = fontChoice(session.state.wordIndex);
        if (loadedFamilyIndex_ != family || loadedFontSizeIndex_ != typography_.fontSizeIndex) {
            loadedFamilyIndex_ = family;
            loadedFontSizeIndex_ = typography_.fontSizeIndex;
            ++fontRevision_;
        }
        face_ = fonts.loadFace(family, typography_.fontSizeIndex);
        activateFace(face_);
    }

    FontCatalog::Face ReaderScreen::pageTypeface(size_t wordIndex) {
        pageFontActive_ = true;
        // Smooth Small for the modern browser. The legacy playing page mode
        // keeps its compact strike; it is not exposed in this preview UI.
        const size_t size = pageBrowsing_ ? RFont4::sizeIndexForId("small") : readerLayout::pageStrikeIndex();
        return fonts.loadFace(fontChoice(wordIndex), size);
    }
} // namespace screens
