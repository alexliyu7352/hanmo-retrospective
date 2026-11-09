// 原收帖人的显式回复意向；目录本身不构成已领读或真实会话来源。
#pragma once
#include "visit_reply.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_claims.hpp"
namespace hanmo::resident {
inline constexpr const char* visit_reply_intent_module="visit_reply_intents";
struct VisitReplyIntent {StableId id,reception_intent_id,reply_policy_id,response;std::vector<VisitReceptionStep> route;};
struct VisitReplyIntentCatalog {Integer version=1;std::vector<VisitReplyIntent> intents;};
const VisitReplyIntent* visit_reply_intent(const VisitReplyIntentCatalog&,const StableId&);
const VisitReplyIntent* visit_reply_reception_intent(const VisitReplyIntentCatalog&,const StableId& reception_intent_id);
Status validate_visit_reply_intents(const VisitReplyIntentCatalog&,const VisitReplyCatalog&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const InteractionCatalog&);
Outcome<VisitReplyIntentCatalog> decode_visit_reply_intents(const BoundaryValue&,const VisitReplyCatalog&,const VisitReceptionCatalog&,const VisitMessageCatalog&,const InteractionCatalog&);
BoundaryValue encode_visit_reply_intents(const VisitReplyIntentCatalog&);
Outcome<Integer> decode_visit_reply_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
}
