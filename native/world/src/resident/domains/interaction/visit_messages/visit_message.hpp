// 拜帖的三种显式本人动作；预览不锁人、不送信、不收费，也不成立预约。
#pragma once
#include "visit_message_catalog.hpp"
namespace hanmo::resident {
struct VisitMessageLifeContext;
class CurrentMomentRead;
struct LeaveVisitMessage {StableId policy_id;};
struct ReceiveVisitMessage {StableId message_fact_id;};
struct WithdrawVisitMessage {StableId message_fact_id;};
using VisitMessageCommand=std::variant<LeaveVisitMessage,ReceiveVisitMessage,WithdrawVisitMessage>;
struct VisitMessageQuote {
    AuthoredProviderSourceProof source;
    VisitMessageCommand command;
    StableId policy_id,message_fact_id,existing_result_fact_id;
};
// 只验本段具名原事实、原作者接待人和真实次序；后来缺席/死亡不抹旧领读历史。
Status validate_visit_messages(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionCatalog&,const CurrentMomentRead* actual_clock=nullptr);
// 原Common缓存键、实际历史区间和本次具名事实必须一致；裁掉更早历史不要求同人上次记录。
Status validate_visit_message_cached_source(const CommonStore&,const VisitMessageState&,const StableId& cache_key,
    const StableId& command_kind,const StableId& result_id,Integer tick,const StableId& fact_id,const CurrentMomentRead* actual_clock=nullptr);
Outcome<VisitMessageQuote> preview_visit_message(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor,const VisitMessageCommand&,const VisitMessageLifeContext* life=nullptr);
// 调用者须持原World候选/Common与新段保存点；末ID或事实失败由同一提交边界撤回。
Outcome<StableId> apply_visit_message_in_candidate(CommonStore&,VisitMessageState&,const VisitMessageCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor,const VisitMessageCommand&,const VisitMessageQuote&,const VisitMessageLifeContext* life=nullptr);
StableId visit_message_command_kind(const VisitMessageCommand&);
BoundaryValue encode_visit_message_command(const VisitMessageCommand&);
Outcome<VisitMessageCommand> decode_visit_message_command(const StableId& kind,const BoundaryValue& params);
}
