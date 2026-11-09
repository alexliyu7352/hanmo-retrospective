// 不从今天技能/兵额补过去准备；工程只落真实原siege_step，旧版本完全不调用本链。
#include "military_siege_engineering_history.hpp"
#include "military_siege_timeout_source.hpp"
#include "military_siege_lifecycle_authority.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
std::optional<std::size_t> position(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();auto at=std::find(order.begin(),order.end(),id);return at==order.end()?std::nullopt:std::optional<std::size_t>{std::size_t(at-order.begin())};}
class ParticipationRead final:public MilitarySiegeParticipationRead {
 const CommonStore& common_;const MilitarySiegeLifecycleState& state_;const MilitarySiegeEngineeringPreparation& p_;const DynamicStepInput* phase_;
public:
 ParticipationRead(const CommonStore& c,const MilitarySiegeLifecycleState& s,const MilitarySiegeEngineeringPreparation& p,const DynamicStepInput* phase):common_(c),state_(s),p_(p),phase_(phase){}
 Outcome<MilitarySiegeParticipationObservation> participation_before_engineering(const StableId& siege,const StableId& preparation)const override {if(siege!=p_.siege_id||preparation!=p_.id)return {false,{},common_failure("MilitarySiegeEngineeringParticipationRequired",preparation).error};return read_military_siege_participation_for_engineering(common_,state_,p_,phase_);}
};
}
Outcome<MilitarySiegeEngineeringPreparation> prepare_military_siege_engineering(CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const SiegeRecord& siege,MilitarySiegeEngineeringState& state,const MilitarySiegeEngineeringSkillRead& skills,const DynamicStepInput& phase){
 auto prepared=preview_military_siege_engineering_preparation(c,m,cat,siege,skills,phase);if(!prepared.ok)return prepared;auto& p=prepared.value;const MilitarySiegeEngineeringPreparation* previous=nullptr;for(const auto& f:state.preparations)if(f.siege_id==siege.id)previous=&f;
 if(previous){if(previous->topology_id!=siege.topology_id||previous->to_tick!=phase.from_tick||previous->round+1!=siege.round)return {false,{},common_failure("MilitarySiegeEngineeringStepRepeatedOrSkipped",siege.id).error};p.previous_preparation_id=previous->id;}else if(siege.round!=1||siege.started_tick!=phase.from_tick)return {false,{},common_failure("MilitarySiegeEngineeringHistoryMissing",siege.id).error};
 auto id=c.next_id(military_siege_engineering_fact_prefix);if(!id.ok)return {false,{},id.error};p.id=id.value;auto appended=c.append_fact({p.id,military_siege_engineering_fact_kind,"military"});if(!appended.ok)return {false,{},appended.error};state.preparations.push_back(p);return prepared;
}
Status validate_military_siege_engineering_history(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitarySiegeEngineeringPolicy& policy,const MilitarySiegeEngineeringState& engineering,const MilitarySiegeLifecycleState& lifecycle,const DynamicStepInput* phase,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog){
 auto checked=validate_military_siege_participation_history(c,m,cat,policy,lifecycle,engineering,phase,population,population_catalog);if(!checked.ok)return checked;checked=validate_military_siege_lifecycle_authority(c,m,cat,lifecycle,phase);if(!checked.ok)return checked;std::set<StableId> ids;std::map<StableId,const MilitarySiegeEngineeringPreparation*> previous;std::optional<std::size_t> last_order;
 auto fields=engineering.field_order;std::sort(fields.begin(),fields.end());if(!fields.empty()&&fields!=FieldOrder{"preparations"})return common_failure("MilitarySiegeEngineeringStateShape");
 for(const auto& p:engineering.preparations){auto at=position(c,p.id);if(!at||(last_order&&*at<=*last_order)||!ids.insert(p.id).second||std::find(policy.topology_ids.begin(),policy.topology_ids.end(),p.topology_id)==policy.topology_ids.end())return common_failure("MilitarySiegeEngineeringSourceOrder",p.id);last_order=at;ParticipationRead reader(c,lifecycle,p,phase);checked=validate_military_siege_engineering_bindings(c,m,cat,p,reader,phase,population,population_catalog);if(!checked.ok)return checked;auto prior=previous.find(p.siege_id);
  if(prior==previous.end()){if(!p.previous_preparation_id.empty()||p.round!=1)return common_failure("MilitarySiegeEngineeringHistoryMissing",p.id);}else if(p.previous_preparation_id!=prior->second->id||p.from_tick!=prior->second->to_tick||p.topology_id!=prior->second->topology_id||p.round!=prior->second->round+1)return common_failure("MilitarySiegeEngineeringHistoryMismatch",p.id);
  unsigned consumers=0;for(const auto& f:lifecycle.facts)if(f.kind==MilitarySiegeLifecycleKind::advanced&&f.cause_id==p.id){auto used=position(c,f.id);if(!used||*used<=*at||f.siege_id!=p.siege_id||f.tick!=p.to_tick||f.topology_id!=p.topology_id)return common_failure("MilitarySiegeEngineeringExecutionMismatch",p.id);++consumers;}if(consumers!=1)return common_failure("MilitarySiegeEngineeringExecutionRequired",p.id);previous[p.siege_id]=&p;
 }
 for(const auto& row:c.state().facts.rows())if(row.second.kind==military_siege_engineering_fact_kind&&row.second.domain=="military"&&!ids.count(row.first))return common_failure("MilitarySiegeEngineeringPayloadMissing",row.first);
 return validate_military_siege_timeout_releases(c.state(),m,cat,phase);
}
Status validate_military_siege_engineering_history_actor_sources(const CommonStore& c,const MilitarySiegeEngineeringState& state,const MilitarySiegeEngineeringSkillRead& skills){for(const auto& p:state.preparations){auto checked=validate_military_siege_engineering_actor_sources(c,p,skills);if(!checked.ok)return checked;}return common_success();}
}
