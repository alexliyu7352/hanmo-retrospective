// 原收件人本人回复，原发信人本人实领；接待者只保管，不替任何人同意会面。
#pragma once
#include "visit_reply_state.hpp"
#include "resident/domains/interaction/visit_messages/visit_message.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident {
struct VisitReplyAuthorshipRead;
struct VisitReplyLifeContext;
struct VisitReplyPolicy {StableId id,message_policy_id,reply_node_id,collect_node_id;};
struct VisitReplyCatalog {Integer version=1;std::vector<VisitReplyPolicy> policies;};
struct ReplyVisitMessage {StableId policy_id,message_fact_id,response;};
struct CollectVisitMessageReply {StableId reply_fact_id;};
using VisitReplyCommand=std::variant<ReplyVisitMessage,CollectVisitMessageReply>;
struct VisitReplyQuote {
    AuthoredProviderSourceProof source;
    VisitReplyCommand command;
    StableId policy_id,message_fact_id,received_fact_id,reply_fact_id;
    std::optional<WorldMoment> actual_quoted_at;
};
// 完整原拜帖/人物创建死亡来源先验后调用；今日远方状态不回填历史。
Status validate_visit_reply_catalog(const VisitReplyCatalog&,const VisitMessageCatalog&,const InteractionCatalog&);
Outcome<VisitReplyCatalog> decode_visit_reply_catalog(const BoundaryValue&,const VisitMessageCatalog&,const InteractionCatalog&);
BoundaryValue encode_visit_reply_catalog(const VisitReplyCatalog&);
Status emit_visit_reply_catalog_fields(ProjectionWriter&,const VisitReplyCatalog&);
Outcome<VisitReplyState> make_initial_visit_reply_state(const VisitReplyCatalog&,const VisitMessageCatalog&,const InteractionCatalog&);
Status validate_visit_replies(const CommonStore&,const CommonCatalog&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionCatalog&,const VisitReplyCatalog&,const VisitReplyState&,const FamilyDeathOriginRead&,const VisitReplyAuthorshipRead* authorship=nullptr,const VisitReplyLifeContext* life=nullptr);
Outcome<VisitReplyQuote> preview_visit_reply(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitReplyCatalog&,const VisitReplyState&,const StableId& actor,const VisitReplyCommand&,const VisitReplyLifeContext* life=nullptr);
// 同一World/Common/Interaction候选下写；末ID/写入失败不能留下半条回信或改变旧领读。
Outcome<StableId> apply_visit_reply_in_candidate(CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitReplyCatalog&,VisitReplyState&,const StableId& actor,const VisitReplyCommand&,const VisitReplyQuote&,const VisitReplyLifeContext* life=nullptr);
StableId visit_reply_command_kind(const VisitReplyCommand&);
BoundaryValue encode_visit_reply_command(const VisitReplyCommand&);
Outcome<VisitReplyCommand> decode_visit_reply_command(const StableId&,const BoundaryValue&);
}
