// 外交v2专用载荷，最终嵌入同一DiplomacyState；不占另一World域或修改v1字节。
#pragma once
#include "diplomacy_report_contract.hpp"
namespace hanmo::resident {
struct DiplomaticMissionKnowledge {
    StableId id, delegation_policy_id, mission_authority_fact_id, envoy_actor_id, issuer_actor_id;
    StableId session_actor_id, session_id, site_id, role_id, provider_actor_id;
    std::optional<StableId> context_kind;
    Integer tick = 0;
};
struct DiplomaticReportRecord {
    DiplomaticEnvoyReportSource source;
    StableId delivery_fact_id, session_actor_id, provider_actor_id, role_id;
    std::optional<StableId> context_kind;
};
struct DiplomacyV2State {
    Integer version = 2;
    OrderedTable<DiplomaticMissionKnowledge> mission_knowledge;
    OrderedTable<DiplomaticReportRecord> reports;
};
BoundaryValue encode_diplomacy_v2_state(const DiplomacyV2State&);
Outcome<DiplomacyV2State> decode_diplomacy_v2_state(const BoundaryValue&);
Status visit_diplomacy_v2_checkpoint(CodecVisitor&, DiplomacyV2State&);
}
