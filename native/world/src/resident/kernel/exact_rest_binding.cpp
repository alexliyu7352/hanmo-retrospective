// 首片FSL实际service/rest；legacy路径完全不调用本适配。
#include "exact_rest_binding.hpp"
#include "world_bundle.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "fixed_service_history.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 引用真实原源，schema外置，不新增Common Fact或报告。
SourceRef boundary_ref(const StableId& id){return {"growth","growth_fixed_service_boundary",id};}
SourceRef activity_ref(const StableId& id){return {"growth","life_activity",id};}
// 原条款字段以锁内角色及活动定位，不造另一个可调H规则。
LifeCatalogFieldRef role_field(const WorldBundle& w,const LifeActivityRecord& activity,const StableId& field){return {w.catalog->identity,w.catalog->actor_foundation->scenario_id,activity.context.role_id,field};}
// 所有资格由真实候选读取；调用者不能传自己的snapshot或desired body。
Outcome<LifeFixedServiceInputs> capture(const WorldBundle& w,const ActionCoreView& action,const WorldMoment& at){
 if(!w.actor_foundation||!w.catalog->actor_foundation)return {false,{},common_failure("ExactRestFoundationRequired").error};
 const auto* record=w.growth.life_activities.find(action.id);if(!record||record->view.kind!=LifeActivityKind::service||record->view.activity!="rest"||!std::holds_alternative<ServiceTerms>(record->terms))return {false,{},common_failure("ExactRestOriginalActivityRequired",action.id).error};
 LifeFixedServiceInputs out;ExactRestTimeRead times(w.growth);const auto& catalog=*w.catalog->actor_foundation;
 std::vector<std::pair<StableId,StableId>> actors={{record->view.actor_id,"recipient"},{record->view.provider_actor_id,"provider"}};std::sort(actors.begin(),actors.end());
 for(const auto& role:actors){const auto* actor=w.common.state().actors.find(role.first);if(!actor||!std::holds_alternative<SettlementLocation>(actor->location)||actor->action_id!=action.id)return {false,{},common_failure("ExactRestActualParticipationRequired",role.first).error};
  auto origin=read_authored_character_origin_v2(w.common.state(),*w.actor_foundation,catalog,role.first);if(!origin.ok)return {false,{},origin.error};
  const auto& purpose=role.second=="recipient"?catalog.rest_purpose:catalog.provider_purpose;
  auto frame=capture_authored_rest_capability(w.common.state(),*w.actor_foundation,catalog,role.first,purpose,at,&w.growth,&times);if(!frame.ok)return {false,{},frame.error};if(!frame.value.result.allowed)return {false,{},common_failure("ExactRestCapabilityDenied",role.first).error};
  const auto* authored=std::get_if<AuthoredCharacterSource>(&origin.value.source);if(!authored)return {false,{},common_failure("ExactRestAuthoredOriginRequired",role.first).error};
  LifeFixedActorInputs row;row.actor_id=role.first;row.role=role.second;row.capability_frames.push_back(std::move(frame.value));row.origin_actor_key=role.first;row.location_snapshot=actor->location;row.actual_location_source_refs.push_back(authored->actor_start_context_ref);row.participation_source_refs.push_back(LifeOwnedFieldRef{activity_ref(action.id),"view.participant_actor_ids"});row.occupation_source_refs.push_back(LifeOwnedFieldRef{activity_ref(action.id),"view.action_id"});out.actors.push_back(std::move(row));
 }
 out.locked_rule_refs={role_field(w,*record,"activities.rest")};
 out.accepted_permission_refs={{"service_terms",LifeOwnedFieldRef{activity_ref(action.id),"terms"},1},{"provider_role",role_field(w,*record,"role"),1}};
 out.resource_basis_refs={{LifeResourceBasisRef::Purpose::fee,LifeOwnedFieldRef{activity_ref(action.id),"cost_paid"},1},{LifeResourceBasisRef::Purpose::wage_reservation,LifeOwnedFieldRef{activity_ref(action.id),"terms.wage"},1}};
 auto& specific=out.specific.life_activity;specific.activity_id=action.id;specific.service_role_ref=role_field(w,*record,"role");specific.activity_ref=role_field(w,*record,"activities.rest");return {true,std::move(out),{}};
}
// ID来自原分配器；没有可持久的临时序号或技术slice记录。
Outcome<StableId> append_boundary(WorldBundle& w,LifeFixedServiceBoundary boundary){auto id=w.common.next_id("growth_fixed_service_boundary");if(!id.ok)return id;boundary.boundary_id=id.value;if(!w.growth.fixed_service_boundaries.insert(id.value,std::move(boundary)))return {false,{},common_failure("DuplicateGrowthFixedBoundary",id.value).error};return id;}
// S0输入索引只指原owner拥有的完整输入，不再拷贝其身体/费用深值。
std::vector<FixedServiceInputRef> inputs(const WorldBundle& w,const StableId& boundary,const LifeActivityRecord& activity){return {FixedAppliedInput{boundary_ref(boundary),1},FixedLockedRuleInput{w.catalog->identity,activity.context.role_id+"/activities/rest"}};}
// 真正Started仅一项；缺失不从已完成LifeActivity补历史。
const LifeFixedServiceBoundary* started(const GrowthState& growth,const StableId& action){const LifeFixedServiceBoundary* out=nullptr;for(const auto& row:growth.fixed_service_boundaries.rows())if(row.second.action_ref==action&&row.second.kind==LifeFixedBoundaryKind::started){if(out)return nullptr;out=&row.second;}return out;}
}
Outcome<WorldMoment> ExactRestTimeRead::completed_rest_at(const StableId& id)const{const LifeFixedServiceBoundary* found=nullptr;for(const auto& row:growth_.fixed_service_boundaries.rows())if(row.second.action_ref==id&&row.second.kind==LifeFixedBoundaryKind::completed){if(found)return {false,{},common_failure("DuplicateExactRestCompletion",id).error};found=&row.second;}if(!found)return {false,{},common_failure("MissingExactRestCompletion",id).error};return {true,found->occurred_at,{}};}
Status bind_exact_rest_started(WorldBundle& w,const StableId& actor,Integer seq,const CommitResult& result){
 if(!w.catalog->exact_time)return common_success();if(result.kind!="service")return common_success();if(exact_complete_selected(*w.catalog->exact_time))return common_failure("CompleteGrowthActionTimeWriterRequired");
 const auto* begun=result.details?std::get_if<ActionStarted>(&result.details->data):nullptr;const auto* action=begun?w.common.action(begun->action_id):nullptr;if(!action||action->actor_id!=actor||actor!=w.catalog->family->founder_actor_id||actor!=w.common.state().metadata.player_actor_id||!w.world_timeline||!w.action_elapsed)return common_failure("ExactRestActualStartRequired");
 if(started(w.growth,action->id))return common_failure("DuplicateExactRestStart",action->id);
 auto at=read_world_moment(w.common.tick(),*w.world_timeline);if(!at.ok)return {false,{},at.error};auto snapshot=capture(w,*action,at.value);if(!snapshot.ok)return {false,{},snapshot.error};const auto* activity=w.growth.life_activities.find(action->id);const auto& terms=std::get<ServiceTerms>(activity->terms);
 LifeFixedServiceBoundary boundary;boundary.action_ref=action->id;boundary.action_kind=action->kind;boundary.owner_ref={1,action->id,0};boundary.occurred_at=at.value;boundary.input_snapshot=std::move(snapshot.value);
 LifeActualCommand command;command.actor_id=actor;command.command_binding={result.kind,seq,result.result_id};command.route_basis=LifePlayerActorRoute{{w.catalog->identity,w.catalog->actor_foundation->scenario_id,actor,"player_actor"}};command.accepted_business_ref=activity_ref(action->id);boundary.cause=std::move(command);
 auto owner=append_boundary(w,std::move(boundary));if(!owner.ok)return {false,{},owner.error};auto elapsed=begin_fixed_elapsed(action->id,action->id,activity->context.role_id+"/activities/rest",action->id,at.value,terms.duration,w.world_timeline->scale_ref);if(!elapsed.ok)return {false,{},elapsed.error};w.action_elapsed->entries.push_back(std::move(elapsed.value));
 auto transition=w.common.next_id("fixed_service_transition");if(!transition.ok)return {false,{},transition.error};FixedServiceTransition row;row.transition_id=transition.value;row.action_ref=action->id;row.occurred_at=at.value;row.execution_source_ref=activity_ref(action->id);row.execution_source_schema=1;row.cause=FixedOwnerAdopted{boundary_ref(owner.value),1};row.input_basis_refs=inputs(w,owner.value,*activity);w.action_elapsed->fixed_service_transitions.push_back(std::move(row));return common_success();
}
Outcome<ActionCompletion> finish_exact_rest_in_candidate(WorldBundle& w,const ActionCoreView& action,const WorldMoment& at,std::vector<BoundAuthoredRestConditionWrite>& bindings){
 if(!w.actor_foundation||!w.catalog->actor_foundation||!w.action_elapsed)return {false,{},common_failure("ExactRestFoundationRequired").error};auto* first=started(w.growth,action.id);if(!first)return {false,{},common_failure("ExactRestStartedSourceRequired",action.id).error};
 const auto started_id=first->boundary_id;auto snapshot=capture(w,action,at);if(!snapshot.ok)return {false,{},snapshot.error};auto prepared=begin_authored_rest_condition_write(w.common,*w.actor_foundation,*w.catalog->actor_foundation,w.growth,action,at);if(!prepared.ok)return {false,{},prepared.error};
 // 明确传实际floor(M)，legacy缺省仍原common.tick()+1；原钱/身体公式只调用一次。
 auto finished=growth::finish_service_trade(w.common,w.growth,*w.catalog->growth,w.social,w.interaction,action,nullptr,at.half_day_tick);if(!finished.ok)return {false,{},finished.error};
 auto bound=bind_authored_rest_condition_write(w.common,*w.actor_foundation,*w.catalog->actor_foundation,w.growth,action,at,prepared.value);if(!bound.ok)return {false,{},bound.error};bindings.push_back(std::move(bound.value));
 LifeFixedServiceBoundary boundary;boundary.action_ref=action.id;boundary.action_kind=action.kind;boundary.owner_ref={1,action.id,0};boundary.boundary_ordinal=1;boundary.previous_boundary_ref=started_id;boundary.kind=LifeFixedBoundaryKind::completed;boundary.occurred_at=at;boundary.input_snapshot=std::move(snapshot.value);boundary.original_outcome_ref=LifeFixedOutcomeRef{LifeFixedOutcomeRef::Kind::life_activity,action.id,0,{}};
 LifeOriginalTerminalApplication terminal;terminal.trigger=LifeFixedCompletionGate{action.id,role_field(w,*w.growth.life_activities.find(action.id),"activities.rest.duration"),{}};terminal.actual_result_refs.push_back(*boundary.original_outcome_ref);boundary.cause=std::move(terminal);auto owner=append_boundary(w,std::move(boundary));if(!owner.ok)return {false,{},owner.error};
 auto* entry=find_fixed_elapsed(*w.action_elapsed,action.id);if(!entry)return {false,{},common_failure("MissingExactRestElapsed",action.id).error};entry->timing=ElapsedTerminal{action.id};
 const FixedServiceTransition* previous=nullptr;for(const auto& row:w.action_elapsed->fixed_service_transitions)if(row.action_ref==action.id)previous=&row;if(!previous||previous->kind!=FixedTransitionKind::started)return {false,{},common_failure("ExactRestUnsupportedHistory",action.id).error};
 auto transition=w.common.next_id("fixed_service_transition");if(!transition.ok)return {false,{},transition.error};FixedServiceTransition row;row.transition_id=transition.value;row.action_ref=action.id;row.transition_ordinal=previous->transition_ordinal+1;row.previous_transition_ref=previous->transition_id;row.kind=FixedTransitionKind::terminal;row.occurred_at=at;row.credited_at=entry->credited_units;row.execution_source_ref=activity_ref(action.id);row.execution_source_schema=1;row.cause=FixedOwnerTerminated{boundary_ref(owner.value),1};row.previous_input_basis_refs=previous->input_basis_refs;row.terminal_disposition=FixedTerminalDisposition::completed;w.action_elapsed->fixed_service_transitions.push_back(std::move(row));
 w.action_elapsed->terminal_bindings.push_back({action.id,action.kind,action.actor_id,action.participants,activity_ref(action.id),0,MomentOwnerSource{boundary_ref(started_id),1},MomentOwnerSource{boundary_ref(owner.value),1},action.id,{}, {}});
 ActionCompletion result;result.completed=true;result.success=finished.value.success;result.growth=std::move(finished.value);return {true,std::move(result),{}};
}
} // 真实源冷闭包在独立validator实现；writer技术失败由原WorldBundle候选整体撤回。
