#include "plc/poll_group.hpp"
#include "plc/plc_value.hpp"
#include <algorithm>
#include <map>

std::vector<PollGroup> buildPollGroups(const std::vector<TagConfig>& tags) {
    std::map<int, std::vector<TagConfig>> byInterval;
    for (const auto& tag : tags) byInterval[tag.updateIntervalMs].push_back(tag);

    std::vector<PollGroup> groups;
    for (auto& [interval, groupTags] : byInterval) {
        groups.push_back(PollGroup{interval, std::move(groupTags)});
    }
    return groups;
}

std::vector<PollBatch> buildByteSafeBatches(const std::vector<TagConfig>& tags, int pduLength) {
    int budget = pduLength - kPduCallOverheadBytes;
    if (budget < kPduPerItemOverheadBytes) budget = kPduPerItemOverheadBytes; // always allow at least one item

    std::vector<PollBatch> batches;
    PollBatch current;
    int currentBytes = 0;

    for (const auto& tag : tags) {
        int itemBytes = kPduPerItemOverheadBytes + plcTypeByteSize(tag.type);
        bool wouldExceedBytes = currentBytes + itemBytes > budget;
        bool wouldExceedCount = static_cast<int>(current.size()) >= kMaxItemsPerBatch;
        if (!current.empty() && (wouldExceedBytes || wouldExceedCount)) {
            batches.push_back(std::move(current));
            current = PollBatch{};
            currentBytes = 0;
        }
        current.push_back(tag);
        currentBytes += itemBytes;
    }
    if (!current.empty()) batches.push_back(std::move(current));
    return batches;
}
