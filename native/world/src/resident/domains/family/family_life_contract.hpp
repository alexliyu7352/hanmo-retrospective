// 人物生命唯一可信读口；实际FamilyDeathState及其cause先验，消息与alive布尔不能替代。
#pragma once
#include "death_ref.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct VerifiedDeathView {DeathRef death_ref;StableId actor_id;ActorFoundationAt effective_at,recorded_at;StableId cause_kind;SourceRef cause_ref;Integer source_schema=1,source_ordinal=0;WorldId world_id{};StableId scenario_id;};
class VerifiedDeathRead {public:virtual ~VerifiedDeathRead()=default;virtual Outcome<VerifiedDeathView> by_ref(const DeathRef&,const ReadStamp&)const=0;virtual Outcome<std::optional<VerifiedDeathView>> by_actor(const StableId&,const ReadStamp&)const=0;};
}
