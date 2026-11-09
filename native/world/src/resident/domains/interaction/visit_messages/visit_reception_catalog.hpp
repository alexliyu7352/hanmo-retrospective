// NPC只按明确作者来访意向走原有限对白；不能由未获知的远方拜帖触发寻人。
#pragma once
#include "visit_message_catalog.hpp"
namespace hanmo::resident {
struct VisitReceptionStep {StableId node_id,option_id;};
struct VisitReceptionIntent {
    StableId id,actor_id,message_policy_id,purpose;
    Integer cooldown_ticks=4;
    std::vector<VisitReceptionStep> route;
};
struct VisitReceptionCatalog {Integer version=1;std::vector<VisitReceptionIntent> intents;};
const VisitReceptionIntent* visit_reception_intent(const VisitReceptionCatalog&,const StableId&);
const VisitReceptionIntent* visit_reception_actor_intent(const VisitReceptionCatalog&,const StableId& actor_id);
Status validate_visit_reception_catalog(const VisitReceptionCatalog&,const VisitMessageCatalog&,const InteractionCatalog&);
Outcome<VisitReceptionCatalog> decode_visit_reception_catalog(const BoundaryValue&,const VisitMessageCatalog&,const InteractionCatalog&);
BoundaryValue encode_visit_reception_catalog(const VisitReceptionCatalog&);
// 目录与claims尾严格同启用；执行进度仍由原Director计划及真实Interaction会话保存。
Outcome<bool> decode_visit_reception_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_visit_reception_layout(const std::vector<std::pair<StableId,StableId>>&,const VisitReceptionCatalog*,const VisitMessageCatalog*);
}
