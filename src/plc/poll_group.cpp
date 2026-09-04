#include "plc/poll_group.hpp"
#include <algorithm>
#include <map>

std::vector<PollGroup> buildPollGroups(const std::vector<TagConfig>& tags) {
    std::map<int, std::vector<TagConfig>> byInterval;
    for (const auto& t : tags) byInterval[t.updateIntervalMs].push_back(t);

    std::vector<PollGroup> groups;
    for (auto& [interval, groupTags] : byInterval) {
        PollGroup group;
        group.intervalMs = interval;
        for (size_t i = 0; i < groupTags.size(); i += kMaxItemsPerBatch) {
            size_t end = std::min(i + static_cast<size_t>(kMaxItemsPerBatch), groupTags.size());
            group.batches.emplace_back(groupTags.begin() + static_cast<long>(i),
                                        groupTags.begin() + static_cast<long>(end));
        }
        groups.push_back(std::move(group));
    }
    return groups;
}
