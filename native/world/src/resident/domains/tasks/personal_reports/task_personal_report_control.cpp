// 原三类换控均零时；完成只可在严格from→to窗口读原from控制链，实际头的实例/Common仍双序核定。
#include "task_personal_report_control.hpp"
#include "task_personal_report_detail.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto at=id.rfind(':');return at==StableId::npos?std::optional<Integer>{}:personal_report_detail::serial(c,id,id.substr(0,at));}
bool kind(const StableId& k){return k==task_personal_report_started_kind||k==task_personal_report_interrupted_kind||k==task_personal_report_completed_kind;}
}
Outcome<StableId> task_personal_report_player_before(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact,const DynamicStepInput* phase){auto through=personal_message_detail::through(c,phase);if(!through.ok)return {false,{},through.error};const auto* h=c.state().facts.find(fact);const auto n=h?personal_report_detail::serial(c,fact,h->kind):std::optional<Integer>{};const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);if(!h||h->id!=fact||h->domain!=task_personal_report_module||!kind(h->kind)||!n||at==order.end()||tick<0||tick>through.value||(tick>c.tick()&&(!phase||h->kind==task_personal_report_started_kind)))return {false,{},common_failure("TaskPersonalMessageControlBoundaryRequired",fact).error};
 auto same_side=[&](const StableId& id,Integer changed)->Status{const auto serial=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!serial||where==order.end()||*serial==*n||((*serial<*n)!=(where<at))||changed<0||changed>c.tick()||(where<at?changed>tick:changed<tick))return common_failure("TaskPersonalMessageControlOrderMismatch",id);return common_success();};
 for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
 // phase的to没有合法控制写者；原链只读稳定from，以actual完成头为真正序边界。
 const auto observed=std::min(tick,c.tick());return legacy?family_player_before_fact_with_legacy(c,family,*legacy,observed,fact):family_player_before_fact(c,family,observed,fact);
}
}
