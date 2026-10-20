#include "update/ReleaseSource.h"
#include <cassert>
#include <iostream>
int main() {
    settings::UpdateSettings settings;
    auto source=update::releaseSourceForSettings(settings);
    assert(source.owner=="ionutdecebal" && source.repo=="rsvpnano");
    assert(source.tag==update::kDefaultUiReleaseTag);
    settings.releaseTag=" preview-ui-next ";
    source=update::releaseSourceForSettings(settings);
    assert(source.tag=="preview-ui-next" && source.repo=="rsvpnano");
    settings.repositoryOwner="alice/reader-fork";
    source=update::releaseSourceForSettings(settings);
    assert(source.owner=="alice" && source.repo=="reader-fork");
    settings.releaseTag="bob/other@preview-2";
    source=update::releaseSourceForSettings(settings);
    assert(source.owner=="bob" && source.repo=="other" && source.tag=="preview-2");
    settings.releaseTag="newrepo@preview-3";
    source=update::releaseSourceForSettings(settings);
    assert(source.owner=="alice" && source.repo=="newrepo" && source.tag=="preview-3");
    settings.repositoryOwner="";settings.releaseTag="v0.1.2";
    source=update::releaseSourceForSettings(settings);
    assert(source.repo=="rsvpnano" && source.tag=="v0.1.2"); // explicit Classic selection
    settings.releaseTag=" ";
    assert(update::releaseSourceForSettings(settings).tag==update::kDefaultUiReleaseTag);
    std::cout<<"Release source: preview default, explicit tags, owner/repo aliases and intentional Classic selection passed\n";
}
