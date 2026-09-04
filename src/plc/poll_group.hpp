#pragma once
#include "config/config_types.hpp"
#include <vector>

constexpr int kMaxItemsPerBatch = 20; // Snap7 Cli_ReadMultiVars/WriteMultiVars item cap

using PollBatch = std::vector<TagConfig>;

struct PollGroup {
    int intervalMs;
    std::vector<PollBatch> batches;
};

std::vector<PollGroup> buildPollGroups(const std::vector<TagConfig>& tags);
