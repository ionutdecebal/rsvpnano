#include "update/ChannelCatalog.h"
#include "dictionary/PackTransaction.h"
#include "FS.h"
#include <glaze/glaze.hpp>
#include <cassert>
#include <cstdio>
#include <stdexcept>

std::string contents(fs::FS& fs, const std::string& path) {
    auto file = fs.open(path.c_str()); if (!file) return {};
    std::string text(file.size(), '\0');
    file.read(reinterpret_cast<uint8_t*>(text.data()), text.size()); return text;
}
void put(fs::FS& fs, const std::string& path, std::string_view text) {
    auto file = fs.open(path.c_str(), "w");
    assert(file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size()) == text.size());
}
struct FaultFS : fs::FS {
    int calls = 0, failAt = -1;
    bool crash = false;
    bool fail() {
        if (++calls != failAt) return false;
        if (crash) throw std::runtime_error("power loss");
        return true;
    }
    bool remove(const char* path) { if (fail()) return false; return fs::FS::remove(path); }
    bool rename(const char* from, const char* to) { if (fail()) return false; return fs::FS::rename(from, to); }
};
void seed(FaultFS& fs, bool existing) {
    for (const auto* name : dictionary::kPackNames) {
        if (existing) put(fs, dictionary::packPath(name), std::string{"original:"} + name);
        put(fs, dictionary::packPath(name, ".download"), std::string{"new:"} + name);
    }
    put(fs, "/library/book.rstate.toml", "word=9376");
    put(fs, "/dictionaries/custom-extra", "leave alone");
}
void verifyGeneration(FaultFS& fs, bool existing, bool committed) {
    for (const auto* name : dictionary::kPackNames)
        assert(contents(fs, dictionary::packPath(name)) == (committed ? std::string{"new:"} + name
            : existing ? std::string{"original:"} + name : std::string{}));
    assert(contents(fs, "/library/book.rstate.toml") == "word=9376");
    assert(contents(fs, "/dictionaries/custom-extra") == "leave alone");
}
int main() {
    update::Release release{"new-ui", update::kNewUiBoard, "v0.2.0", std::string(40, 'a'),
        "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin", std::string(64, 'b'), 3400000};
    auto classic = release; classic.channel = "classic"; classic.tag = "v0.1.3";
    auto select = [&](const update::Catalog& catalog, std::string_view channel = "new-ui") {
        return update::selectRelease(glz::write_json(catalog).value(), channel, update::kNewUiBoard, release.asset);
    };
    assert(select({1,{classic,release}})->tag == "v0.2.0");
    assert(select({1,{classic,release}}, "classic")->tag == "v0.1.3");
    assert(!select({2,{release}}));
    assert(!select({1,{classic}}));
    assert(!select({1,{release,release}}));
    for (int field = 0; field < 7; ++field) {
        auto bad = release;
        switch (field) {
        case 0: bad.sha256[0] = 'g'; break;
        case 1: bad.commit.clear(); break;
        case 2: bad.asset = "wrong-ota.bin"; break;
        case 3: bad.bytes = 9000000; break;
        case 4: bad.tag = "../v0.2.0"; break;
        case 5: bad.board = "other"; break;
        case 6: bad.bytes = 0; break;
        }
        assert(!select({1,{bad}}));
    }
    assert(!update::selectRelease(std::string(16385, 'x'),"new-ui",update::kNewUiBoard,release.asset));
    assert(!update::selectRelease("{","new-ui",update::kNewUiBoard,release.asset));
    auto rev2 = release;
    rev2.board = update::kNewUiRev2Board;
    rev2.asset = "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin";
    auto rev2Classic = rev2; rev2Classic.channel = "classic"; rev2Classic.tag = "v0.1.4";
    const auto both = glz::write_json(update::Catalog{1, {classic,release,rev2Classic,rev2}}).value();
    for (const auto& expected : {classic,release,rev2Classic,rev2}) {
        auto selected = update::selectRelease(both, expected.channel, expected.board, expected.asset);
        assert(selected && selected->tag == expected.tag && selected->asset == expected.asset);
        // A catalogue entry for another revision must never be used as fallback.
        const auto otherAsset = expected.board == rev2.board ? release.asset : rev2.asset;
        assert(!update::selectRelease(both, expected.channel, expected.board, otherAsset));
    }
    assert(!update::selectRelease(glz::write_json(update::Catalog{1,{release}}).value(),
                                 "new-ui",rev2.board,rev2.asset));
    assert(!update::selectRelease(glz::write_json(update::Catalog{1,{rev2}}).value(),
                                 "new-ui",release.board,release.asset));
    assert(!update::selectRelease(glz::write_json(update::Catalog{1,{rev2,rev2}}).value(),
                                 "new-ui",rev2.board,rev2.asset));
    auto unsupported = rev2; unsupported.board = "waveshare_esp32s3_touch_amoled_1_8";
    assert(!update::validRelease(unsupported, unsupported.board, unsupported.asset));
    puts("Catalog: variant selection, missing/duplicate entries, hardware, asset, size and checksum bounds passed.");
    puts("Rev2: four board/channel choices, cross-revision rejection, unpublished and duplicate entries passed.");
    int scenarios = 0;
    for (bool existing : {false, true}) for (bool crash : {false, true}) for (int fault = 1; fault <= 15; ++fault) {
        FaultFS fs; seed(fs, existing); fs.failAt = fault; fs.crash = crash;
        bool complete = false;
        try { complete = dictionary::promotePack(fs); } catch (const std::runtime_error&) {}
        const bool pending = dictionary::packExists(fs, dictionary::kInstallJournal);
        const bool newFiles = contents(fs,dictionary::packPath("english.oewn")) == "new:english.oewn";
        fs.failAt = -1;
        assert(dictionary::recoverPack(fs));
        assert(dictionary::recoverPack(fs)); // Recovery itself is repeatable.
        verifyGeneration(fs, existing, complete || (!pending && newFiles));
        ++scenarios;
    }
    FaultFS partial; seed(partial, true); partial.writeLimit = 1;
    assert(!dictionary::promotePack(partial)); verifyGeneration(partial, true, false);
    FaultFS readOnly; seed(readOnly, true); readOnly.failWrites = true;
    assert(!dictionary::promotePack(readOnly)); verifyGeneration(readOnly, true, false);
    printf("Dictionary: %d rename/delete failure and reboot scenarios, short journal writes and read-only SD passed.\n", scenarios);
}
