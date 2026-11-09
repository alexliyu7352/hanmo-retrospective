// 零时销约读取实际头前唯一控制人；原Family/Legacy完整来源仍是调用前置。
#include "task_personal_cancellation_control.hpp"
#include "task_personal_cancellation_detail.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto at=id.rfind(':');return at==StableId::npos?std::optional<Integer>{}:personal_cancellation_detail::serial(c,id,id.substr(0,at));}
bool kind(const StableId& k){return k==task_personal_cancelled_kind;}
}
Outcome<StableId> task_personal_cancellation_player_before(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){auto through=personal_message_detail::through(c,nullptr);if(!through.ok)return {false,{},through.error};const auto* h=c.state().facts.find(fact);const auto n=h?personal_cancellation_detail::serial(c,fact,h->kind):std::optional<Integer>{};const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);if(!h||h->id!=fact||h->domain!=task_personal_cancellation_module||!kind(h->kind)||!n||at==order.end()||tick<0||tick>through.value)return {false,{},common_failure("TaskPersonalCancellationControlBoundaryRequired",fact).error};
 auto same_side=[&](const StableId& id,Integer changed)->Status{const auto serial=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!serial||where==order.end()||*serial==*n||((*serial<*n)!=(where<at))||changed<0||changed>c.tick()||(where<at?changed>tick:changed<tick))return common_failure("TaskPersonalCancellationControlOrderMismatch",id);return common_success();};
 for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
 // 只读稳定原刻与实际取消头，不用当前player回填早先本人。
 const auto observed=std::min(tick,c.tick());return legacy?family_player_before_fact_with_legacy(c,family,*legacy,observed,fact):family_player_before_fact(c,family,observed,fact);
}
}
