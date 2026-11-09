// 作者精确声明接待本人愿意托留、交还与撤回的say来源，不赋予替人接受预约的权力。
#pragma once
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "visit_message_state.hpp"
namespace hanmo::resident {
struct VisitMessagePolicy {
    StableId id,sender_actor_id,recipient_actor_id,custodian_actor_id,role_id,dialogue_id;
    StableId deposit_node_id,receive_node_id,withdraw_node_id;
    // /3 explicit original service declaration; /1 and fixed life /2 keep their policy bytes.
    bool report_custody_included_in_contact=false;
};
struct VisitMessageCatalog {Integer version=1;std::vector<VisitMessagePolicy> policies;};
Outcome<VisitMessageState> make_initial_visit_message_state(const VisitMessageCatalog&,const InteractionCatalog&);
Status validate_visit_message_catalog(const VisitMessageCatalog&,const InteractionCatalog&);
Outcome<VisitMessageCatalog> decode_visit_message_catalog(const BoundaryValue&,const InteractionCatalog&);
BoundaryValue encode_visit_message_catalog(const VisitMessageCatalog&);
Status emit_visit_message_catalog_fields(ProjectionWriter&,const VisitMessageCatalog&);
// 显式profile与独立冷段成对，未启用旧Interaction字节与含义均不追加。
Outcome<Integer> decode_visit_message_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<bool> decode_visit_message_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_visit_message_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>& modules,const VisitMessageCatalog*,const VisitMessageState*);
}
