#pragma once
#include "config/config_types.hpp"
#include <vector>

constexpr int kMaxItemsPerBatch = 20; // Snap7 Cli_ReadMultiVars/WriteMultiVars hard item-count cap
constexpr int kDefaultPduLength = 240; // conservative fallback if the negotiated PDU can't be queried (smallest classic S7 PDU size)
constexpr int kPduCallOverheadBytes = 20; // reserved for S7 header/parameter framing per call
constexpr int kPduPerItemOverheadBytes = 12; // conservative per-item overhead (covers both request- and response-side item framing)

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
