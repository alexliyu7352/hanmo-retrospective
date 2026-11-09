// 新回复只认实际提交前后唯一新增和已分配的Common结果；不借首history裁剪后的旧源推断。
#pragma once
#include "visit_reply_state.hpp"
#include "resident/domains/interaction/visit_messages/visit_authored_command.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<StableId> visit_reply_new_player_source(const VisitReplyState* before,const VisitReplyState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_visit_reply_player_result(CommonStore&,VisitReplyState&,const StableId& source_fact_id,const StableId& actor,Integer request_sequence,const CommitResult&);
Status validate_visit_reply_cached_results(const CommonStore&,const VisitReplyState*,const VisitAuthoredCommandRead* actual_commands=nullptr);
// Real before/after delta, before Common seal and adoption. Does not write a player binding.
Outcome<MomentOwnerSource> read_visit_reply_director_command_effect(
 const CommonStore&,const VisitReplyState& before,const VisitReplyState& after,
 const StableId& actor,const ReceiptDetails&,const WorldMoment& actual_at);
Status validate_visit_reply_director_actor_effect(const CommonStore&,const VisitReplyState&,
 const VisitAuthoredCommandRead&,const StableId& actor,const SourceRef&);
}
