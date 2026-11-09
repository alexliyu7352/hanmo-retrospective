// 来访会话认领只保存原Talk实际来源；旧Interaction/Director visitor不编码本独立尾。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include <map>
namespace hanmo::resident {
inline constexpr const char* visit_reception_claim_module="visit_reception_claims";
inline constexpr const char* visit_reception_claim_kind="visit_reception_claimed";
struct VisitReceptionClaim {
    StableId fact_id,intent_id,policy_id,session_id,actor_id,provider_actor_id,role_id,dialogue_id,entry_node_id,place_id;
    Integer tick=0;
};
struct VisitReceptionState {
    Integer version=1;
    OrderedTable<VisitReceptionClaim> claims;
    // 唯一会话来源索引只重建，不能从任意Director.goal_ref生成授权。
    CheckpointMap<StableId,StableId> by_session;
};
void rebuild_visit_reception_indexes(VisitReceptionState&);
Status visit_visit_reception_checkpoint(CodecVisitor&,VisitReceptionState&);
BoundaryValue encode_visit_reception_claims(const VisitReceptionState&);
Outcome<VisitReceptionState> decode_visit_reception_claims(const BoundaryValue&);
}
