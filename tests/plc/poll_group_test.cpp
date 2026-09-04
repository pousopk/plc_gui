#include <doctest/doctest.h>
#include "plc/poll_group.hpp"
#include <algorithm>

namespace {
TagConfig makeTag(std::string id, int intervalMs, PlcType type = PlcType::Int) {
    TagConfig t;
    t.id = std::move(id);
    t.db = 1;
    t.type = type;
    t.offset = 0;
    t.updateIntervalMs = intervalMs;
    return t;
}
} // namespace

TEST_CASE("buildPollGroups groups tags sharing the same interval") {
    std::vector<TagConfig> tags = {makeTag("a", 100), makeTag("b", 100), makeTag("c", 500)};
    auto groups = buildPollGroups(tags);

    REQUIRE(groups.size() == 2);
    auto g100 = std::find_if(groups.begin(), groups.end(), [](auto& g) { return g.intervalMs == 100; });
    auto g500 = std::find_if(groups.begin(), groups.end(), [](auto& g) { return g.intervalMs == 500; });
    REQUIRE(g100 != groups.end());
    REQUIRE(g500 != groups.end());
    CHECK(g100->tags.size() == 2);
    CHECK(g500->tags.size() == 1);
}

TEST_CASE("buildPollGroups returns no groups for an empty tag list") {
    CHECK(buildPollGroups({}).empty());
}

TEST_CASE("buildByteSafeBatches respects the 20-item Snap7 cap even with a huge PDU") {
    std::vector<TagConfig> tags;
    for (int i = 0; i < 25; ++i) tags.push_back(makeTag("t" + std::to_string(i), 100, PlcType::Bool));

    auto batches = buildByteSafeBatches(tags, /*pduLength=*/8192);

    REQUIRE(batches.size() == 2);
    CHECK(batches[0].size() == 20);
    CHECK(batches[1].size() == 5);
}

TEST_CASE("buildByteSafeBatches splits a batch that would exceed a small PDU, even under the 20-item cap") {
    // Reproduces the real bug: 20 tags sharing an interval, well under the
    // item-count cap, still overflowed a real PLC's negotiated PDU and
    // failed with "total data exceeds the PDU size" — because nothing
    // ever accounted for the actual byte size of the batch.
    std::vector<TagConfig> tags;
    for (int i = 0; i < 20; ++i) tags.push_back(makeTag("t" + std::to_string(i), 100, PlcType::Bool));

    auto batches = buildByteSafeBatches(tags, /*pduLength=*/240);

    REQUIRE(batches.size() > 1); // must NOT be a single 20-item batch
    for (const auto& batch : batches) {
        CHECK(batch.size() <= 20);
    }
    size_t total = 0;
    for (const auto& batch : batches) total += batch.size();
    CHECK(total == 20); // every tag still gets read, just spread across more calls
}

TEST_CASE("buildByteSafeBatches always puts at least one item in a batch, even if it alone exceeds budget") {
    std::vector<TagConfig> tags = {makeTag("only", 100, PlcType::Real)};
    auto batches = buildByteSafeBatches(tags, /*pduLength=*/1); // absurdly small, smaller than any single item

    REQUIRE(batches.size() == 1);
    CHECK(batches[0].size() == 1);
}

TEST_CASE("buildByteSafeBatches returns no batches for an empty tag list") {
    CHECK(buildByteSafeBatches({}, 240).empty());
}
