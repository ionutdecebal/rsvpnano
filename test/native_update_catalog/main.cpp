#include "update/ChannelCatalog.h"
#include <glaze/glaze.hpp>
#include <cassert>
#include <cstdio>

int main() {
    assert(std::string_view(update::kCurrentChannel) == "classic");
    assert(std::string_view(update::kOtherChannel) == "new-ui");
    assert(update::supportsNewUi(update::kNewUiBoard));
    assert(update::supportsNewUi(update::kNewUiRev2Board));
    assert(!update::supportsNewUi("waveshare_esp32s3_touch_amoled_1_8"));
    update::Release rev1{"new-ui",update::kNewUiBoard,"v0.2.0",std::string(40,'a'),
        "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",std::string(64,'b'),3400000};
    auto rev2=rev1; rev2.board=update::kNewUiRev2Board; rev2.tag="v0.2.0-rev2.1";
    rev2.asset="rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin";
    auto classic1=rev1; classic1.channel="classic"; classic1.tag="v0.1.5";
    auto classic2=rev2; classic2.channel="classic"; classic2.tag="v0.1.5";
    const auto json=glz::write_json(update::Catalog{1,{classic1,classic2,rev1,rev2}}).value();
    for (const auto& expected : {classic1,classic2,rev1,rev2}) {
        auto result=update::selectRelease(json,expected.channel,expected.board,expected.asset);
        assert(result && result->tag==expected.tag && result->asset==expected.asset);
        const auto wrongAsset=expected.board==rev2.board ? rev1.asset : rev2.asset;
        assert(!update::selectRelease(json,expected.channel,expected.board,wrongAsset));
    }
    auto only1=glz::write_json(update::Catalog{1,{rev1}}).value();
    assert(!update::selectRelease(only1,"new-ui",rev2.board,rev2.asset));
    auto duplicate=glz::write_json(update::Catalog{1,{rev2,rev2}}).value();
    assert(!update::selectRelease(duplicate,"new-ui",rev2.board,rev2.asset));
    for (int field=0;field<4;++field) {
        auto bad=rev2;
        if(field==0) bad.sha256="bad";
        if(field==1) bad.commit="bad";
        if(field==2) bad.bytes=0;
        if(field==3) bad.asset=rev1.asset;
        assert(!update::selectRelease(glz::write_json(update::Catalog{1,{bad}}).value(),
                                     "new-ui",rev2.board,rev2.asset));
    }
    puts("Classic selector: both LCD revisions/channels, unsupported hardware, wrong assets, missing/duplicate and invalid entries passed.");
}
