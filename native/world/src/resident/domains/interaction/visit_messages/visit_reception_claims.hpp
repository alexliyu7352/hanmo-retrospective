// 原Talk采用子事务写入真实来源；冷读只承认该来源和当前同ID会话，不改写后来角色历史。
#pragma once
#include "visit_reception_state.hpp"
#include "visit_reception_catalog.hpp"
namespace hanmo::resident {
class CurrentMomentRead;class NativeActorSocialOccupancyRead;
Status validate_visit_reception_claims(const CommonStore&,const VisitReceptionState&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const InteractionCatalog&,const InteractionState&,const CurrentMomentRead* actual_clock=nullptr,const NativeActorSocialOccupancyRead* actual_occupancy=nullptr);
const VisitReceptionClaim* read_visit_reception_claim(const VisitReceptionState&,const StableId& session_id,const StableId& actor_id,const StableId& intent_id);
Status record_visit_reception_claim_in_candidate(CommonStore&,VisitReceptionState&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const InteractionCatalog&,const InteractionState&,const StableId& actor_id,const StableId& intent_id,const CurrentMomentRead* actual_clock=nullptr,const NativeActorSocialOccupancyRead* actual_occupancy=nullptr);
}
