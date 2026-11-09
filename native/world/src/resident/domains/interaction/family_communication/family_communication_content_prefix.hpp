// 仅旁支三源共用的公开前缀投影；不是独立版本、迁移或获知授权。
#pragma once
#include "family_communication_history.hpp"
namespace hanmo::resident::family_content_detail {
Outcome<FamilyCommunicationContent1> read_collateral_prefix_current(
    const FamilyCommunicationContentReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const ReadStamp &,
    CheckpointMemoryCharge *returned_memory = nullptr,
    bool include_field_order = true);
Outcome<FamilyCommunicationContent1> read_collateral_prefix_before_fact(
    const FamilyCommunicationHistoryReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const SourceRef &,
    const ReadStamp &, CheckpointMemoryCharge *returned_memory = nullptr,
    bool include_field_order = true);
} // namespace hanmo::resident::family_content_detail
