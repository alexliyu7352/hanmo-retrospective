// 显式NPC回复只消费原领读和真正认领的会话；独立来源不冒玩家空result。
#pragma once
#include "visit_reply_intents.hpp"
#include "visit_reply_control.hpp"
namespace hanmo::resident {
struct VisitReplyAuthorshipRead {
    const VisitReplyIntentCatalog& intents;const VisitReceptionCatalog& reception;
    const VisitReceptionState& claims;const InteractionState& interactions;
    VisitReplyPlayerHistoryRead player_before_fact;
    const VisitReplyLifeContext* life=nullptr;
};
// 只供原Director具名producer，外部JSON不开放此命令；actor始终取原NPC envelope。
struct AuthoredVisitReply {StableId intent_id,message_fact_id,received_fact_id,claim_fact_id;};
struct AuthoredVisitReplyQuote {AuthoredVisitReply command;VisitReplyQuote reply;};
inline constexpr const char* authored_visit_reply_command="reply_visit_message_authored";
Outcome<AuthoredVisitReplyQuote> preview_authored_visit_reply(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionCatalog&,const InteractionAccess&,const VisitReplyCatalog&,const VisitReplyState&,const VisitReplyAuthorshipRead&,const StableId& actor,const AuthoredVisitReply&);
Outcome<StableId> apply_authored_visit_reply_in_candidate(CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionCatalog&,const InteractionAccess&,const VisitReplyCatalog&,VisitReplyState&,const VisitReplyAuthorshipRead&,const StableId& actor,const AuthoredVisitReply&,const AuthoredVisitReplyQuote&);
// 完整原拜帖/claims后核每份NPC回复的永久原claim；不借当前Director.goal_ref倒造历史。
Status validate_visit_reply_authored_sources(const CommonStore&,const VisitMessageCatalog&,const InteractionCatalog&,const VisitReplyCatalog&,const VisitReplyState&,const VisitReplyAuthorshipRead*);
}
