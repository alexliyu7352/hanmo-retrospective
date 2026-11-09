// 原Common/Interaction候选中的真实回信；所有借用限当前栈，实际结果绑定在统一提交尾。
#pragma once
#include "visit_reply_boundary.hpp"
#include "visit_reply_authored.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
Status validate_visit_reply_world(const WorldBundle&,const VisitReplyReadContext* actual=nullptr);
Outcome<VisitReplyQuote> prepare_visit_reply_world(const WorldBundle&,const StableId& actor,const VisitReplyCommand&,const VisitReplyReadContext* actual=nullptr);
Outcome<InteractionEffect> commit_visit_reply_world(WorldBundle&,const StableId& actor,const VisitReplyCommand&,const VisitReplyQuote&,const VisitReplyReadContext* actual=nullptr);
// NPC执行仍经过原认领与实际当面声明，不使用玩家请求结果。
Outcome<AuthoredVisitReplyQuote> prepare_authored_visit_reply_world(const WorldBundle&,const StableId& actor,const AuthoredVisitReply&,const VisitReplyReadContext* actual=nullptr,const CompleteExactWorldReadBindings* actual_world=nullptr);
Outcome<InteractionEffect> commit_authored_visit_reply_world(WorldBundle&,const StableId& actor,const AuthoredVisitReply&,const AuthoredVisitReplyQuote&,const VisitReplyReadContext* actual=nullptr,const CompleteExactWorldReadBindings* actual_world=nullptr);
Outcome<BoundaryValue> query_visit_reply_world(const WorldBundle&,Integer cursor=0,const VisitReplyReadContext* actual=nullptr);
Outcome<StableId> visit_reply_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& kind,const ReceiptDetails&);
Status apply_visit_reply_death_world_in_candidate(WorldBundle&,const DeathRef&,const VisitReplyReadContext* actual=nullptr);
Status bind_visit_reply_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer sequence,const CommitResult&);
Outcome<MomentOwnerSource> read_visit_reply_director_command_effect_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const ReceiptDetails&,const VisitReplyReadContext* actual=nullptr);
Status validate_visit_reply_director_actor_effect_world(const WorldBundle&,const StableId& actor,const SourceRef&,const VisitReplyReadContext* actual=nullptr);
Status finalize_visit_reply_director_actor_result_world(const WorldBundle&,const DirectorAdoptionResult2&,const VisitReplyReadContext* actual=nullptr);
}
