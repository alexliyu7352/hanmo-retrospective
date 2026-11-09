#include "resident/domains/military/military_model.hpp"
// 原习得必须实际早于接敌，入场后教学即使同tick也不能使在途部队获得战法。
#include "military_tactic_entry_source.hpp"
#include "military_command_resources_source.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
std::size_t position(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();return std::find(order.begin(),order.end(),id)-order.begin();}
Outcome<MilitaryTacticEntryState> collect(const CommonStore& c,const CommonCatalog& common,const MilitaryCatalog& cat,const MilitaryTacticLearningState& learning,const MilitaryBattleRecord& b,const StableId& boundary,const DynamicStepInput* phase){
 using R=MilitaryTacticEntryState;if(!cat.battle_catalog.tactic_learning)return fail<R>("MilitaryTacticLearningProfileRequired",b.id);const auto& definitions=*cat.battle_catalog.tactic_learning;R out;out.entry_tick=b.created_tick;
 for(const auto& u:b.units){const auto* troop=find(cat.battle_catalog.troop_types,u.troop_type_id);if(!troop)return fail<R>("UnknownMilitaryDefinition",u.troop_type_id);MilitaryTacticEntryUnit row;row.unit_id=u.unit_id;row.commander_actor_id=u.commander_actor_id;
  for(const auto& t:troop->tactics){auto checked=validate_military_learned_tactic_binding(t,definitions);if(!checked.ok)return {false,{},checked.error};auto q=read_military_learned_tactic(c,common,definitions,learning,u.commander_actor_id,t.learned_tactic_id,b.created_tick,boundary,phase);if(!q.ok)return {false,{},q.error};if(q.value){if(position(c,q.value->acquisition_fact_id)>=position(c,b.source_id))return fail<R>("MilitaryTacticAcquisitionAfterContact",q.value->acquisition_fact_id);row.tactic_acquisition_ids.emplace_back(t.learned_tactic_id,q.value->acquisition_fact_id);}}
  std::sort(row.tactic_acquisition_ids.begin(),row.tactic_acquisition_ids.end());out.units.push_back(std::move(row));
 }
 std::sort(out.units.begin(),out.units.end(),[](const auto& a,const auto& b){return a.unit_id<b.unit_id;});auto checked=validate_military_tactic_entry_bindings(b,cat.battle_catalog,out);return checked.ok?Outcome<R>{true,std::move(out),{}}:Outcome<R>{false,{},checked.error};
}
}
Outcome<MilitaryTacticEntryState> prepare_military_tactic_entry(const CommonStore& c,const CommonCatalog& common,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryTacticLearningState& learning,const MilitaryBattleRecord& b,const DynamicStepInput& phase){
 using R=MilitaryTacticEntryState;if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1||b.created_tick!=phase.to_tick)return fail<R>("MilitaryTacticEntryPhaseMismatch",b.id);
 if(!cat.command_resources_policy||!b.command_resources||b.phase!="planning"||b.round!=1||!b.ai_plans.empty()||b.plan_submitted||!b.pending_orders.empty()||!b.round_receipt_ids.empty()||b.learned_tactics)return fail<R>("MilitaryTacticEntryRequired",b.id);const auto* contact=find(s.contact_facts,b.source_id);const auto* head=c.state().facts.find(b.source_id);if(!contact||contact->siege_id||contact->node_id||contact->tick!=phase.to_tick||!head||head->id!=b.source_id||head->kind!="military_contact"||head->domain!="military"||position(c,b.source_id)==c.state().facts.size())return fail<R>("MilitaryTacticContactRequired",b.source_id);return collect(c,common,cat,learning,b,{},&phase);
}
Status validate_military_tactic_entry_history(const CommonStore& c,const CommonCatalog& common,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryTacticLearningState& learning,const MilitaryRoutEntryFact& e,const DynamicStepInput* phase){
 const auto& b=e.initial;if(!b.learned_tactics||!b.command_resources)return common_failure("MilitaryTacticEntryRequired",e.id);auto checked=validate_military_command_resources_bindings(c,s,cat,e,*b.command_resources,phase);if(!checked.ok)return checked;const auto* head=c.state().facts.find(e.id);const auto* contact=find(s.contact_facts,b.source_id);if(!head||head->kind!="military_rout_entry"||head->domain!="military"||!contact||contact->tick!=e.tick||e.tick!=b.created_tick||position(c,b.source_id)>=position(c,e.id))return common_failure("MilitaryTacticEntrySourceMismatch",e.id);checked=validate_military_tactic_entry_bindings(b,cat.battle_catalog,*b.learned_tactics);if(!checked.ok)return checked;auto expected=collect(c,common,cat,learning,b,e.id,phase);if(!expected.ok)return {false,{},expected.error};const auto& actual=*b.learned_tactics;if(actual.units.size()!=expected.value.units.size())return common_failure("MilitaryTacticEntryHistoryMismatch",e.id);for(std::size_t i=0;i<actual.units.size();++i){const auto& x=actual.units[i];const auto& y=expected.value.units[i];if(x.unit_id!=y.unit_id||x.commander_actor_id!=y.commander_actor_id||x.tactic_acquisition_ids!=y.tactic_acquisition_ids)return common_failure("MilitaryTacticEntryHistoryMismatch",x.unit_id);}return common_success();
}
}
