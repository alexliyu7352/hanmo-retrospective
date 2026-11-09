// 同一World候选的拜帖薄接头；状态仍嵌原Interaction保存集，旧Interaction编码不携新段。
#pragma once
#include "visit_message_view.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
bool visit_message_command_kind(const StableId&);
Status validate_visit_message_world(const WorldBundle&,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<VisitMessageQuote> prepare_visit_message_world(const WorldBundle&,const StableId&,const VisitMessageCommand&,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<InteractionEffect> commit_visit_message_world(WorldBundle&,const StableId&,const VisitMessageCommand&,const VisitMessageQuote&,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<BoundaryValue> query_visit_message_world(const WorldBundle&,Integer cursor=0,Integer offer_cursor=0,const CompleteExactWorldReadBindings* actual=nullptr);
BoundaryValue encode_visit_message_quote(const VisitMessageQuote&);
Status apply_visit_message_death_world_in_candidate(WorldBundle&,const DeathRef&,const CompleteExactWorldReadBindings* actual=nullptr);
}
