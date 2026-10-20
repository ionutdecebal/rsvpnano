#include <cassert>
#include <cstdio>
#include "settings/SettingsStore.h"
#include "library/StorageManager.h"
#include "ui/screens/LibraryScreen.h"
#include "storage/fs/StoragePaths.h"

namespace fixture {
fs::FS disk;
BookLibrary::Listing books{{.path="/library/books/one.txt", .title="One"},
                          {.path="/library/books/two.txt", .title="Two"}};
size_t metadataReads = 0, progressReads = 0;
bool loadSucceeds = true;
void resetSettings() {
    blobs.clear(); integers.clear(); failingKey.clear(); blobWrites = 0; now = 0;
}
void tick(settings::SettingsStore& store, uint32_t time) { now = time; store.update(time); }
}

// Hardware/catalog fixtures. The persistence, cache and invalidation code under
// test are the production SettingsStore, LibraryScreen and StorageManager.
namespace Board::Storage {
fs::FS& filesystem() { return fixture::disk; }
uint64_t cardSize() { return 1024 * 1024; }
void end() {}
}
namespace SdCard {
bool mount(bool& mounted, int* frequency) { mounted = true; if (frequency) *frequency = 20000; return true; }
}
namespace StorageMigration { bool prepareLayout() { return true; } }
namespace BookLibrary {
void refresh(Listing& listing, bool, bool) { listing = fixture::books; }
void refreshMetadata(Entry&) {}
int indexOfPath(const Listing& listing, std::string_view path) {
    for (size_t i=0; i<listing.size(); ++i) if (listing[i].path == path) return int(i);
    return -1;
}
const Entry* at(const Listing& listing, size_t index) { return index < listing.size() ? &listing[index] : nullptr; }
}
namespace IndexedBook {
bool readMetadata(std::string_view, BookMetadata& metadata, IndexedBookStore::Header* header) {
    ++fixture::metadataReads; metadata.chapters = {{"Opening", 0}, {"Later", 50}};
    if (header) header->identity.wordCount = 101;
    return true;
}
bool load(size_t, BookLibrary::Listing& listing, IndexedBookStore&, BookMetadata&, const OpenRequest&) {
    listing = fixture::books; // Conversion may replace entries even if it fails.
    return fixture::loadSucceeds;
}
}
bool IndexedBookStore::open(std::string_view path, const Header& header) {
    sourcePath_ = path; identity_ = header.identity; return true;
}
bool IndexedBookStore::isOpen() const { return !sourcePath_.empty(); }
size_t IndexedBookStore::wordCount() const { return identity_.wordCount; }
namespace ReadingLoop {
size_t wordCount(const ReadingSession& session) { return session.stored() ? session.bookStore->wordCount() : session.words.size(); }
}
namespace ReadingProgress {
std::expected<uint32_t, std::error_code> readBookStatePosition(std::string_view, const reading::BookIdentity&) {
    ++fixture::progressReads; return 30;
}
uint8_t percent(uint32_t word, uint32_t count) { return count <= 1 ? 0 : std::min<uint64_t>(100, uint64_t(word)*100/(count-1)); }
}
namespace StoragePaths {
std::string bookStatePathFor(std::string_view path) { return std::string{path} + ".state"; }
std::string indexedIndexPathFor(std::string_view path) { return std::string{path} + ".index"; }
std::string indexedDataPathFor(std::string_view path) { return std::string{path} + ".data"; }
}

void testSettingsRetryAndRecovery() {
    fixture::resetSettings(); fs::FS disk; settings::SettingsStore store;
    assert(store.begin(&disk)); const auto previousWrites = fixture::blobWrites;
    fixture::now = 100; store.settings().reading.wpm = 420; store.acceptChanges();
    store.secrets().wifiPassword = "fixture-only"; store.acceptSecretChanges();
    disk.failWrites = true;
    fixture::tick(store, 1599); assert(fixture::blobWrites == previousWrites);
    fixture::tick(store, 1600); const auto failedWrites = fixture::blobWrites;
    // Even though the SD mirror failed, credentials reached NVS.
    auto secrets = settings::codec::decodeSecrets(fixture::blobs.at("secrets"), settings::SettingsSource::Nvs);
    assert(secrets && secrets->wifiPassword == "fixture-only");
    for (uint32_t t=1601; t<3100; ++t) fixture::tick(store, t);
    assert(fixture::blobWrites == failedWrites);
    fixture::tick(store, 3100); assert(fixture::blobWrites == failedWrites+1);
    disk.failWrites = false; fixture::tick(store, 4600);
    const auto restored = settings::codec::decodeToml(disk.nodes.at(StoragePaths::kSettingsConfigPath)->data, settings::SettingsSource::Sd);
    assert(restored && restored->reading.wpm == 420);
    const auto doneWrites = fixture::blobWrites;
    fixture::tick(store, 10000); assert(fixture::blobWrites == doneWrites);
    std::puts("PASS: SD failure throttles retries, saves independent credentials and recovers pending settings");
}

void testSecretFailureAndRecovery() {
    fixture::resetSettings(); settings::SettingsStore store; assert(store.begin(nullptr));
    store.secrets().wifiPassword = "second-fixture"; store.acceptSecretChanges();
    fixture::failingKey = "secrets"; fixture::tick(store, 1500);
    const auto attempts = fixture::blobWrites;
    fixture::tick(store, 1501); fixture::tick(store, 2999); assert(fixture::blobWrites == attempts);
    fixture::failingKey.clear(); fixture::tick(store, 3000);
    auto restored = settings::codec::decodeSecrets(fixture::blobs.at("secrets"), settings::SettingsSource::Nvs);
    assert(restored && restored->wifiPassword == "second-fixture");
    std::puts("PASS: NVS credential failure remains pending and recovers after retry interval");
}

void testDebounceWrapAndForcedFlush() {
    fixture::resetSettings(); settings::SettingsStore store; assert(store.begin(nullptr));
    fixture::now = UINT32_MAX - 500; store.settings().reading.wpm = 510; store.acceptChanges();
    const auto writes = fixture::blobWrites;
    fixture::tick(store, 998); assert(fixture::blobWrites == writes);
    fixture::tick(store, 999); assert(fixture::blobWrites == writes+1);
    fixture::now = 1000; store.settings().reading.wpm = 520; store.acceptChanges();
    fixture::now = 2000; store.settings().reading.wpm = 530; store.acceptChanges();
    fixture::tick(store, 3499); assert(fixture::blobWrites == writes+1);
    assert(store.flush()); assert(fixture::blobWrites == writes+2); // shutdown need not wait
    fixture::tick(store, 3500); assert(fixture::blobWrites == writes+2);
    std::puts("PASS: debounce coalesces edits, survives millis wrap and allows immediate shutdown flush");
}

void testLibraryCache() {
    StorageManager storage; assert(storage.begin());
    IndexedBookStore book; IndexedBookStore::Header header; header.identity.wordCount = 101;
    book.open(fixture::books[0].path, header);
    ReadingSession session; session.bookStore = &book; session.metadata.chapters = {{"Opening",0},{"Later",50}};
    screens::LibraryScreen library; assert(library.needsItems(storage));
    const auto& first = library.items(storage, book, session);
    assert(first.size()==2 && first[0].progress==0 && first[1].progress==30);
    const auto reads = fixture::metadataReads, progress = fixture::progressReads;
    library.reset(); session.state.wordIndex=75;
    assert(!library.needsItems(storage));
    const auto& again = library.items(storage, book, session);
    assert(again[0].progress==75 && again[0].chapter=="Later");
    session.metadata.chapters.clear(); library.items(storage, book, session); assert(again[0].chapter.empty());
    assert(fixture::metadataReads==reads && fixture::progressReads==progress);
    std::puts("PASS: repeated library visits update active progress without SD metadata or position reads");

    fixture::books[1] = {.path="/library/books/replacement.txt", .title="Replacement"};
    storage.refreshBooks(); assert(library.needsItems(storage));
    const auto& replaced = library.items(storage, book, session);
    assert(replaced[1].book->title=="Replacement" && fixture::metadataReads==reads+2);
    library.invalidate(); assert(library.needsItems(storage)); library.items(storage, book, session);
    std::puts("PASS: same-count replacement and explicit invalidation rebuild cached book references");

    BookMetadata metadata;
    assert(storage.loadIndexedBook(0, book, metadata)); assert(library.needsItems(storage));
    library.items(storage, book, session); fixture::loadSucceeds=false;
    assert(!storage.loadIndexedBook(0, book, metadata)); assert(library.needsItems(storage));
    library.items(storage, book, session);
    std::puts("PASS: successful and failed conversion/open invalidate potentially replaced entries");

    fixture::disk.open(fixture::books[1].path.c_str(), FILE_WRITE);
    assert(storage.removeBook(fixture::books[1].path)); assert(library.needsItems(storage));
    assert(library.items(storage, book, session).size()==1);
    storage.end(); assert(library.needsItems(storage)); assert(library.items(storage, book, session).empty());
    std::puts("PASS: deletion and storage unmount invalidate the library cache");
}

int main() {
    testSettingsRetryAndRecovery(); testSecretFailureAndRecovery(); testDebounceWrapAndForcedFlush(); testLibraryCache();
    std::puts("7 storage regression groups passed (production code, injected hardware/catalog I/O).");
}
