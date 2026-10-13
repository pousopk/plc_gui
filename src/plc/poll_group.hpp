#pragma once
#include "config/config_types.hpp"
#include <vector>

// Snap7's hard cap on items per Cli_ReadMultiVars/WriteMultiVars call.
constexpr int kMaxItemsPerBatch = 20;
// Fallback when the negotiated PDU length can't be queried: the smallest
// classic S7 PDU size, so it is always safe.
constexpr int kDefaultPduLength = 240;
// Bytes reserved for the S7 header/parameter framing of each call.
constexpr int kPduCallOverheadBytes = 20;
// Conservative per-item overhead (covers request- and response-side framing).
constexpr int kPduPerItemOverheadBytes = 12;

using PollBatch = std::vector<TagConfig>;

// Tags sharing an update interval. NOT pre-chunked into per-call batches —
// the safe batch size depends on the PLC's actual negotiated PDU length,
// which is only known once connected (see buildByteSafeBatches).
struct PollGroup {
    int intervalMs;
    std::vector<TagConfig> tags;
};

std::vector<PollGroup> buildPollGroups(const std::vector<TagConfig>& tags);

// Splits `tags` into batches safe for one Cli_ReadMultiVars/WriteMultiVars
// call: each batch respects both Snap7's kMaxItemsPerBatch item-count cap
// and an estimated byte budget derived from `pduLength` (the PLC's actual
// negotiated PDU size), so a single call never exceeds what this specific
// PLC can transfer in one PDU — a hard per-model constraint, not something
// a fixed item-count cap alone can guarantee.
std::vector<PollBatch> buildByteSafeBatches(const std::vector<TagConfig>& tags, int pduLength);
