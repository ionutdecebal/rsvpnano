#include <cassert>
#include <cstdio>
#include "library/BookSettingsEditor.h"
#include "library/StorageManager.h"
#include "storage/fs/StoragePaths.h"

namespace fixture {
fs::FS disk;
BookLibrary::Listing books{{.path="/library/books/current.rsvp", .title="Current"},
                          {.path="/library/books/selected.rsvp", .title="Selected"}};
constexpr reading::BookIdentity identity{1024, 1234, 101};
bool indexed = true, canIndex = true;
}
namespace Board::Storage {
fs::FS& filesystem() { return fixture::disk; }
uint64_t cardSize() { return 1024 * 1024; }
void end() {}
}
namespace SdCard {
bool mount(bool& mounted, int* frequency) { mounted=true; if(frequency)*frequency=20000; return true; }
}
namespace StorageMigration { bool prepareLayout() { return true; } }
namespace BookLibrary {
void refresh(Listing& listing, bool, bool) { listing=fixture::books; }
void refreshMetadata(Entry&) {}
std::string_view displayName(const Entry& entry) { return entry.title; }
const Entry* at(const Listing& listing, size_t i) { return i<listing.size()?&listing[i]:nullptr; }
}
namespace IndexedBook {
bool readMetadata(std::string_view, BookMetadata& metadata, IndexedBookStore::Header* header) {
    if (!fixture::indexed) return false;
    metadata.locale="en"; metadata.scriptMask=UnicodeText::ScriptLatin;
    if(header)header->identity=fixture::identity;
    return true;
}
bool load(size_t index, BookLibrary::Listing& listing, IndexedBookStore& store, BookMetadata& metadata, const OpenRequest&) {
    if (!fixture::canIndex) return false;
    listing=fixture::books; // Force entry replacement, as import conversion can.
    IndexedBookStore::Header header;header.identity=fixture::identity;
    metadata.locale="fr"; metadata.scriptMask=UnicodeText::ScriptLatin;
    return store.open(listing[index].path,header);
}
}
bool IndexedBookStore::open(std::string_view path, const Header& header) {
    sourcePath_=path;identity_=header.identity;return true;
}
void IndexedBookStore::close() { sourcePath_.clear();identity_={}; }
bool IndexedBookStore::isOpen() const { return !sourcePath_.empty(); }
namespace StoragePaths {
std::string bookStatePathFor(std::string_view path) { return std::string{path}+".state"; }
}

int main() {
    StorageManager storage; assert(storage.begin());
    const auto current=fixture::books[0].path, selected=fixture::books[1].path;
    assert(ReadingProgress::writeBookStatePosition(current,fixture::identity,75));
    assert(ReadingProgress::writeBookStatePosition(selected,fixture::identity,30));
    const auto originalCurrent=fixture::disk.nodes.at(current+".state")->data;
    IndexedBookStore live;IndexedBookStore::Header header;header.identity=fixture::identity;live.open(current,header);
    ReadingSession session;session.bookStore=&live;session.state.wordIndex=75;
    BookSettingsEditor editor;assert(editor.load(storage,1));
    assert(editor.path==selected && editor.title=="Selected" && editor.metadata.locale=="en");
    editor.overrides.locale="de";editor.overrides.languageFonts={{"en","literata"}};
    assert(editor.save());
    auto saved=ReadingProgress::readBookState(selected,fixture::identity);
    assert(saved && saved->wordIndex==30 && saved->overrides==editor.overrides);
    assert(fixture::disk.nodes.at(current+".state")->data==originalCurrent);
    assert(session.sourcePath()==current && session.state.wordIndex==75 && !session.state.overrides.locale);
    editor.clear();assert(!editor.ready() && !editor.save());
    assert(editor.load(storage,1) && editor.overrides.locale=="de");
    const auto originalSelected=fixture::disk.nodes.at(selected+".state")->data;
    fixture::disk.failWrites=true;editor.overrides.locale="fr";assert(!editor.save());
    fixture::disk.failWrites=false;
    assert(fixture::disk.nodes.at(selected+".state")->data==originalSelected);
    fixture::disk.writeLimit=8;assert(!editor.save());fixture::disk.writeLimit=SIZE_MAX;
    assert(fixture::disk.nodes.at(selected+".state")->data==originalSelected);
    assert(!fixture::disk.nodes.contains(selected+".state.tmp"));
    assert(editor.load(storage,1) && editor.overrides.locale=="de");
    editor.overrides={};assert(editor.save());
    saved=ReadingProgress::readBookState(selected,fixture::identity);
    assert(saved && saved->wordIndex==30 && !saved->overrides.locale && saved->overrides.languageFonts.empty());
    std::puts("PASS: selected book edits/reset persist, preserve both positions and current book, and failed writes retain saved data.");

    fixture::indexed=false;assert(editor.load(storage,1) && editor.path==selected && editor.metadata.locale=="fr");
    assert(live.sourcePath()==current && live.identity()==fixture::identity);
    fixture::canIndex=false;assert(!editor.load(storage,1) && !editor.ready());
    fixture::indexed=true;assert(!editor.load(storage,2) && !editor.ready());
    assert(editor.load(storage,0));
    editor.overrides.locale="ro";assert(editor.save());
    saved=ReadingProgress::readBookState(current,fixture::identity);
    assert(saved && saved->wordIndex==75 && saved->overrides.locale=="ro");
    fixture::disk.nodes.erase(selected+".state");
    assert(editor.load(storage,1) && !editor.overrides.locale);
    editor.overrides.locale="en";assert(editor.save());
    saved=ReadingProgress::readBookState(selected,fixture::identity);
    assert(saved && saved->wordIndex==0 && saved->overrides.locale=="en");
    storage.end();assert(!editor.load(storage,0) && !editor.ready());
    std::puts("PASS: unopened/imported books, indexing failure, current book, absent sidecar, invalid selection and unmounted SD.");
}
