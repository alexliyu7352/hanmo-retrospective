// 原形成、真实接任、接受军职、辞职与占领前序共同约束围城命令；未知停战来源不能默许。
#include "military_siege_lifecycle_authority.hpp"
#include "military_authority_history.hpp"
#include "military_siege_office_boundary.hpp"
#include "military_field_command_source.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
std::optional<std::size_t> position(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();auto it=std::find(order.begin(),order.end(),id);return it==order.end()?std::nullopt:std::optional<std::size_t>{std::size_t(it-order.begin())};}
// 已形成统帅的不可用写者现仅原军事死/俘/逃；出生未成年仅新身份，不能把本口扩大为通用alive历史读。
Outcome<bool> military_available_before(const CommonStore& c,const MilitaryState& m,const StableId& actor,const StableId& before,Integer tick){if(!c.actor(actor))return {false,{},common_failure("UnknownActor",actor).error};auto end=position(c,before);if(!end)return {false,{},common_failure("MilitarySiegeLifecycleFactMissing",before).error};bool available=true;for(const auto& f:m.casualty_facts){auto at=position(c,f.id);if(!at)return {false,{},common_failure("MilitarySiegeLifecycleCauseRequired",f.id).error};if(*at>=*end)continue;for(const auto& change:f.actor_changes)if(change.actor_id==actor&&(change.status=="dead"||change.status=="captured"||change.status=="deserted")){if(f.tick>tick)return {false,{},common_failure("MilitarySiegeLifecycleCauseTimeMismatch",f.id).error};available=false;}}return {true,available,{}};}
Outcome<bool> office_ready_before(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const CampaignRecord& campaign,const StableId& actor,const MilitarySiegeLifecycleFact& f,const DynamicStepInput* phase){
 // 只枚举原helper实际消费的这一个office来源，再严格核实例序与真实头序一致。
 std::vector<MilitarySiegeOfficeSourceRef> refs;
 for(const auto& row:m.action_facts){const auto* terms=std::get_if<OfficeTerms>(&row.terms);if(row.command=="office_accept"&&row.phase=="completed"&&row.success&&terms&&terms->office_id==campaign.office_id)refs.push_back({row.id,"military_action"});}
 for(const auto& row:m.resignation_facts)if(row.office_id==campaign.office_id)refs.push_back({row.id,"office_resignation"});
 for(const auto& row:m.occupation_facts)if(row.assets_applied.value_or(false)&&row.disabled_office_ids&&std::find(row.disabled_office_ids->begin(),row.disabled_office_ids->end(),campaign.office_id)!=row.disabled_office_ids->end())refs.push_back({row.id,"military_occupation"});
 for(const auto& row:m.integration_facts)for(const auto& ended:row.ended_appointments)if(ended.id==campaign.office_id)refs.push_back({row.id,"military_integration"});
 auto ordered=validate_military_siege_office_boundary(c.state(),f.id,refs);if(!ordered.ok)return {false,{},ordered.error};
 const auto* definition=find(cat.offices,campaign.office_id);if(!definition)return {false,{},common_failure("MilitarySiegeLifecycleOfficeRequired",f.id).error};auto office=phase?military_office_before(c,m,cat,campaign.office_id,f.tick,f.id,*phase):military_office_before(c,m,cat,campaign.office_id,f.tick,f.id);if(!office.ok)return {false,{},office.error};bool permitted=office.value.enabled&&office.value.holder_actor_id==actor;
 // 原失权阶段只检查原军职失效；开围/战略还额外要求原旗号营地产权。
 if(f.kind==MilitarySiegeLifecycleKind::terminated)return {true,permitted,{}};auto end=position(c,f.id);
 const auto* camp=find(cat.garrisons,definition->garrison_id);if(!camp)return {false,{},common_failure("MilitarySiegeLifecycleOfficeRequired",f.id).error};StableId owner=camp->organization_id;std::vector<const MilitaryOccupationFact*> occupations;for(const auto& hold:m.occupation_facts)if(hold.garrison_id==camp->id&&hold.assets_applied.value_or(false)){auto at=position(c,hold.id);if(!at||!end)return {false,{},common_failure("MilitarySiegeLifecycleCauseRequired",hold.id).error};if(*at<*end){if(hold.tick>f.tick)return {false,{},common_failure("MilitarySiegeLifecycleCauseTimeMismatch",hold.id).error};occupations.push_back(&hold);}}std::sort(occupations.begin(),occupations.end(),[&](const auto* a,const auto* b){return *position(c,a->id)<*position(c,b->id);});for(const auto* occupation:occupations)owner=occupation->to_organization_id;return {true,permitted&&owner==definition->organization_id,{}};
}
}
Status validate_military_siege_lifecycle_authority(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitarySiegeLifecycleState& state,const DynamicStepInput* phase){
 Integer limit=c.tick();if(phase){if(phase->from_tick<0||phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return common_failure("MilitarySiegeLifecyclePhaseMismatch");limit=phase->to_tick;}
 for(const auto& f:state.facts){if(f.tick<0||f.tick>limit)return common_failure("MilitarySiegeLifecyclePhaseMismatch",f.id);const auto* siege=find(m.sieges,f.siege_id);if(!siege)return common_failure("UnknownSiege",f.siege_id);
  if(f.kind==MilitarySiegeLifecycleKind::created||f.kind==MilitarySiegeLifecycleKind::joined){const auto* arrival=find(m.arrival_facts,f.cause_id);const auto* campaign=arrival?find(m.campaigns,arrival->campaign_id):nullptr;const auto* order=campaign?find(cat.orders,campaign->template_id):nullptr;if(!arrival||!campaign||!order||campaign->actor_id!=f.actor_id||arrival->tick>f.tick||(f.kind==MilitarySiegeLifecycleKind::created&&order->objective_kind!="occupy"))return common_failure("MilitarySiegeLifecycleArrivalMismatch",f.id);auto allowed=office_ready_before(c,m,cat,*campaign,f.actor_id,f,phase);if(!allowed.ok)return {false,{},allowed.error};auto alive=military_available_before(c,m,f.actor_id,f.id,f.tick);if(!alive.ok)return {false,{},alive.error};if(!allowed.value||!alive.value)return common_failure("MilitarySiegeLifecycleAuthorityRequired",f.id);
  }else if(f.kind==MilitarySiegeLifecycleKind::strategic){if(!f.before||!f.strategic_command)return common_failure("MilitarySiegeLifecycleCauseShape",f.id);auto alive=military_available_before(c,m,f.actor_id,f.id,f.tick);if(!alive.ok)return {false,{},alive.error};if(!alive.value)return common_failure("MilitarySiegeLifecycleAuthorityRequired",f.id);bool allowed=false;for(const auto& member:f.before->armies){if(std::find(f.before->withdrawn_army_ids.begin(),f.before->withdrawn_army_ids.end(),member.first)!=f.before->withdrawn_army_ids.end())continue;if(f.strategic_command->kind=="withdraw"&&f.strategic_command->army_id!=member.first)continue;auto commander=query_military_army_command_source_before(c,m,member.first,f.id);if(!commander.ok)return {false,{},commander.error};if(commander.value.commander_actor_id!=f.actor_id)continue;if(f.strategic_command->kind=="withdraw"){allowed=true;break;}const auto* army=find(m.armies,member.first);const auto* campaign=army?find(m.campaigns,army->order_id):nullptr;if(!campaign)return common_failure("MilitarySiegeLifecycleAuthorityRequired",f.id);auto office=office_ready_before(c,m,cat,*campaign,f.actor_id,f,phase);if(!office.ok)return {false,{},office.error};if(office.value){allowed=true;break;}}if(!allowed)return common_failure("MilitarySiegeLifecycleAuthorityRequired",f.id);
  }else if(f.kind==MilitarySiegeLifecycleKind::terminated){
   const auto* campaign=find(m.campaigns,siege->campaign_id);if(!campaign||!f.termination_reason)return common_failure("MilitarySiegeLifecycleCauseRequired",f.id);auto command=query_military_army_command_source_before(c,m,campaign->army_id,f.id);if(!command.ok)return {false,{},command.error};const bool field=command.value.command_source.kind==MilitaryFieldCommandSourceKind::field_assignment;const auto responsible=field?command.value.commander_actor_id:campaign->actor_id;auto alive=military_available_before(c,m,responsible,f.id,f.tick);if(!alive.ok)return {false,{},alive.error};const auto& reason=*f.termination_reason;
   if(reason=="responsible_actor_lost"){if(alive.value)return common_failure("MilitarySiegeLifecycleActorLossSourceRequired",f.id);}
   else if(reason=="authority_lost"){if(!alive.value||field)return common_failure("MilitarySiegeLifecycleAuthorityLossMismatch",f.id);auto office=office_ready_before(c,m,cat,*campaign,responsible,f,phase);if(!office.ok)return {false,{},office.error};if(office.value)return common_failure("MilitarySiegeLifecycleAuthorityLossMismatch",f.id);}
   else if(reason=="cancelled"){
    if(!alive.value||field||!campaign->objective_result||campaign->objective_result->tick>f.tick)return common_failure("MilitarySiegeLifecycleCancellationSourceRequired",f.id);const auto& outcome=*campaign->objective_result;
    // 原campaign_step在同次release之前按不可变deadline判过期；零时取消/撤回则必须已在较早稳定半日成立。
    if(outcome.kind=="expired"){if(outcome.tick<=campaign->deadline_tick||outcome.tick<campaign->accepted_tick)return common_failure("MilitarySiegeLifecycleCancellationSourceRequired",f.id);}
    else if((outcome.kind!="cancelled"&&outcome.kind!="withdrawn")||outcome.tick>=f.tick)return common_failure("MilitarySiegeLifecycleCancellationSourceRequired",f.id);
   }
   else return common_failure("MilitarySiegeLifecycleTerminationSourceUnavailable",f.id);
  }
 }
 return common_success();
}
}
