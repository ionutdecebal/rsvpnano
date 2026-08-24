#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "storage/fs/SdDiagnostics.h"

namespace StorageMigration {

    struct Report {
        bool healthy = true;
        size_t checked = 0;
        size_t moved = 0;
        size_t removed = 0;
        std::string diagnosticSummary;
        std::string diagnosticDetail;
        std::vector<std::string> actions;
        std::vector<std::string> issues;
    };

    bool prepareLayout();
    Report repair(bool mounted, SdDiagnostics::Inventory inventory);
    std::string currentPath(std::string_view path);

} // namespace StorageMigration
