#include "resident/domains/growth/growth_model.hpp"
// 保留唯一原LifeActivityRecord，绝不另写费用、恢复条款或今日身体快照。
#include "growth_service_condition_source.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
constexpr Integer maximum=9007199254740991LL;
bool allocated(const CommonState& c,const StableId& id){if(id.rfind("action:",0)!=0)return false;const auto tail=id.substr(7);Integer n=0;auto r=std::from_chars(tail.data(),tail.data()+tail.size(),n);return r.ec==std::errc{}&&r.ptr==tail.data()+tail.size()&&n>0&&n<c.next_instance_id&&std::to_string(n)==tail;}
bool same(const ServiceTerms& a,const ServiceTerms& b){return a.duration==b.duration&&a.cost==b.cost&&a.wage==b.wage&&a.hp_gain==b.hp_gain&&a.fatigue_delta==b.fatigue_delta&&a.wallet_id==b.wallet_id;}
Status source_shape(const CommonState& c,const GrowthState& state,const GrowthCatalog& catalog,const StableId& id){
 const auto* h=c.facts.find(id);const auto* row=state.life_activities.find(id);if(!catalog.regional_life||!allocated(c,id)||!h||h->id!=id||h->kind!="life_activity"||h->domain!="growth"||!row)return common_failure("GrowthServiceConditionOriginalRequired",id);
 const auto& v=row->view;const auto* terms=std::get_if<ServiceTerms>(&row->terms);const auto* role=catalog.roles.find(row->context.role_id);const auto* original=role?role->activities.find(v.activity):nullptr;
 if(v.kind!=LifeActivityKind::service||(v.activity!="rest"&&v.activity!="work")||!terms||!original||!same(*terms,*original)||v.fact_id!=id||v.action_id!=id||v.actor_id.empty()||v.actor_id==v.provider_actor_id||!c.actors.find(v.actor_id)||!c.actors.find(v.provider_actor_id)||row->context.provider_actor_id!=v.provider_actor_id||v.participant_actor_ids!=std::vector<StableId>{v.actor_id,v.provider_actor_id}||terms->duration<1||terms->duration>720||v.start_tick<0||v.start_tick>maximum-terms->duration)return common_failure("GrowthServiceConditionOriginalInvalid",id);return common_success();
}
}
Outcome<GrowthServiceConditionSource> read_growth_service_condition_source(const CommonState& c,const GrowthState& state,const GrowthCatalog& catalog,const StableId& id,const DynamicStepInput* phase){
 auto valid=source_shape(c,state,catalog,id);if(!valid.ok)return {false,{},valid.error};Integer through=c.identity.tick;if(through<0||through>maximum)return {false,{},common_failure("GrowthServiceConditionClockInvalid",id).error};if(phase){if(through==maximum||phase->from_tick!=through||phase->to_tick!=through+1)return {false,{},common_failure("GrowthServiceConditionPhaseInvalid",id).error};through=phase->to_tick;}
 const auto& record=*state.life_activities.find(id);const auto& v=record.view;const auto& terms=std::get<ServiceTerms>(record.terms);
 if(v.phase!=LifeActivityPhase::completed||!v.success||!v.code.empty()||v.completed_tick!=v.start_tick+terms.duration||v.completed_tick>through||record.cost_paid!=terms.cost||record.wage_paid!=terms.wage)return {false,{},common_failure("GrowthServiceConditionNotCompleted",id).error};
 if(v.completed_tick>c.identity.tick&&(!phase||std::find(phase->existing_action_ids.begin(),phase->existing_action_ids.end(),id)==phase->existing_action_ids.end()))return {false,{},common_failure("GrowthServiceConditionPhaseActionRequired",id).error};
 GrowthServiceConditionSource out;out.cause.source_identity=c.identity;out.cause.business={GrowthPersonalConditionBusiness::service_completion,id,v.completed_tick,1};out.cause.actual_fact_id=id;out.cause.actor_ids={v.actor_id};out.activity=v.activity;out.original_terms=terms;return {true,std::move(out),{}};
}
Outcome<std::vector<StableId>> read_growth_condition_service_pins(const CommonState& c,const GrowthState& state,const GrowthCatalog& catalog){
 if(!catalog.regional_life)return {false,{},common_failure("GrowthServiceConditionOriginalRequired").error};for(const auto& row:c.facts.rows())if(row.second.kind=="life_activity"&&!state.life_activities.find(row.first))return {false,{},common_failure("GrowthServiceConditionOriginalRequired",row.first).error};
 std::vector<StableId> out;for(const auto& row:state.life_activities.rows())if(row.second.view.kind==LifeActivityKind::service){auto valid=source_shape(c,state,catalog,row.first);if(!valid.ok)return {false,{},valid.error};out.push_back(row.first);}const auto& order=c.facts.order();std::sort(out.begin(),out.end(),[&](const auto& a,const auto& b){return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);});return {true,std::move(out),{}};
}
}
