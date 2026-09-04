#include <doctest/doctest.h>
#include "plc/poll_group.hpp"
#include <algorithm>

namespace {
TagConfig makeTag(std::string id, int intervalMs) {
    TagConfig t;
    t.id = std::move(id);
    t.db = 1;
    t.type = PlcType::Int;
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
    REQUIRE(g100->batches.size() == 1);
    CHECK(g100->batches[0].size() == 2);
    REQUIRE(g500->batches.size() == 1);
    CHECK(g500->batches[0].size() == 1);
}

TEST_CASE("buildPollGroups chunks a group larger than 20 tags into multiple batches") {
    std::vector<TagConfig> tags;
    for (int i = 0; i < 25; ++i) tags.push_back(makeTag("t" + std::to_string(i), 100));

    auto groups = buildPollGroups(tags);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].batches.size() == 2);
    CHECK(groups[0].batches[0].size() == 20);
    CHECK(groups[0].batches[1].size() == 5);
}

TEST_CASE("buildPollGroups returns no groups for an empty tag list") {
    CHECK(buildPollGroups({}).empty());
}
