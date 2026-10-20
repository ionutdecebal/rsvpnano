#pragma once
#include <string>
#include "settings/SettingsModel.h"
#include "text/AsciiText.h"
namespace update {
inline constexpr char kDefaultUiReleaseTag[]="preview-ui-dictionary-20261020.4";
struct ReleaseSource {std::string owner,repo,tag;};
    inline bool splitOwnerRepo(std::string_view value, std::string& owner, std::string& repo) {
        const std::string_view trimmed = AsciiText::trim(value);
        const size_t slash = trimmed.find('/');
        if (slash == 0 || slash == std::string_view::npos || slash + 1 >= trimmed.length()) {
            return false;
        }

        // value may alias owner/repo; copy both before changing either string.
        const std::string parsedOwner{AsciiText::trim(trimmed.substr(0, slash))};
        const std::string parsedRepo{AsciiText::trim(trimmed.substr(slash + 1))};
        owner = parsedOwner;
        repo = parsedRepo;
        return !owner.empty() && !repo.empty();
    }

    inline ReleaseSource releaseSourceForSettings(const settings::UpdateSettings& settings) {
        ReleaseSource source{settings.repositoryOwner.empty() ? std::string{settings::kDefaultRepositoryOwner}
                                                              : std::string{AsciiText::trim(settings.repositoryOwner)},
                             "rsvpnano", std::string{AsciiText::trim(settings.releaseTag)}};

        splitOwnerRepo(source.owner, source.owner, source.repo);
        splitOwnerRepo(source.repo, source.owner, source.repo);

        const size_t at = source.tag.find('@');
        if (at != std::string::npos && at > 0 && at + 1 < source.tag.length()) {
            std::string repoPart{AsciiText::trim(std::string_view{source.tag}.substr(0, at))};
            source.tag = std::string{AsciiText::trim(std::string_view{source.tag}.substr(at + 1))};
            if (!splitOwnerRepo(repoPart, source.owner, source.repo) && !repoPart.empty()) {
                source.repo = repoPart;
            }
        }

        if (source.tag.empty()) source.tag = kDefaultUiReleaseTag;
        return source;
    }

}
