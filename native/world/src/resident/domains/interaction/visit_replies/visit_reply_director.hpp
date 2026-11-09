// 同候选借用原已领读与显式回复目录；不新增Director模板或持久计划字段。
#pragma once
#include "visit_reply_authored.hpp"
#include "visit_reply_read.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_director.hpp"
namespace hanmo::resident {
struct VisitReplyNpcRead {const VisitReplyCatalog& catalog;const VisitReplyState& state;const VisitReplyIntentCatalog& intents;const VisitReplyReadContext* original=nullptr;};
Outcome<bool> visit_reply_reception_pending(const VisitReceptionRead&,const VisitReplyNpcRead&,const StableId& actor,const StableId& reception_intent_id);
Outcome<std::optional<DecisionProposal>> propose_received_visit_reply(const VisitReceptionRead&,const VisitReplyNpcRead&,const DirectorState&,const KnownSubjectView&,const StableId& reception_intent_id,const DecisionProposal&);
}
