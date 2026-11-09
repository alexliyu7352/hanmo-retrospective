// 首条真实rest的反向全扫；当前值/整数tick/两张互引表都不能替代原采用和身体来源。
#include "exact_rest_binding.hpp"
#include "world_bundle.hpp"
#include "fixed_service_history.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/actor_foundation_codec.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 原分配器规范ID；不能把另一world的任意字面数当已经真实发生。
bool allocated(const StableId& id,const char* prefix,Integer next){StableId p=prefix;if(id.compare(0,p.size(),p)!=0)return false;Integer value=0;const auto* begin=id.data()+p.size();auto r=std::from_chars(begin,id.data()+id.size(),value);return r.ec==std::errc{}&&r.ptr==id.data()+id.size()&&value>0&&value<next&&std::to_string(value)==id.substr(p.size());}
bool catalog_equal(const CatalogIdentity& a,const CatalogIdentity& b){return std::tie(a.content_id,a.content_version,a.content_digest,a.ruleset_id,a.schema_sha256,a.pack_set_digest,a.module_versions,a.artifact_sha256)==std::tie(b.content_id,b.content_version,b.content_digest,b.ruleset_id,b.schema_sha256,b.pack_set_digest,b.module_versions,b.artifact_sha256);}
bool activity(const SourceRef& ref,const StableId& id){return ref.domain=="growth"&&ref.kind=="life_activity"&&ref.id==id;}
bool boundary_source(const SourceRef& ref,const StableId& id){return ref.domain=="growth"&&ref.kind=="growth_fixed_service_boundary"&&ref.id==id;}
bool catalog_field(const WorldBundle& w,const LifeCatalogFieldRef& ref,const StableId& entity,const StableId& field){return catalog_equal(ref.catalog_identity,w.catalog->identity)&&ref.scenario_id==w.catalog->actor_foundation->scenario_id&&ref.entity_id==entity&&ref.field_selector==field;}
bool owned_field(const LifeOriginalRef& ref,const StableId& id,const StableId& field){const auto* r=std::get_if<LifeOwnedFieldRef>(&ref);return r&&activity(r->owner_ref,id)&&r->field_selector==field;}
// 条款/接受/费用只反读原LifeActivity中的真实拥有值，不把临时recipe包装成永久来源。
Status inputs(const WorldBundle& w,const LifeActivityRecord& record,const LifeFixedServiceBoundary& row,const ExactRestTimeRead& times){
 const auto& s=row.input_snapshot;const auto& id=record.view.action_id;const auto& role=record.context.role_id;const auto& definition=*w.catalog->actor_foundation;
 if(s.actors.size()!=2||s.locked_rule_refs.size()!=1||!catalog_field(w,s.locked_rule_refs[0],role,"activities.rest")||s.accepted_permission_refs.size()!=2||!s.facility_use_refs.empty()||s.resource_basis_refs.size()!=2||s.specific.kind!=1)return common_failure("ExactRestInputSetMismatch",id);
 const auto& terms=s.accepted_permission_refs[0];const auto& provider=s.accepted_permission_refs[1];const auto* provider_ref=std::get_if<LifeCatalogFieldRef>(&provider.original_ref);
 if(terms.permission_kind!="service_terms"||terms.source_schema!=1||!owned_field(terms.original_ref,id,"terms")||provider.permission_kind!="provider_role"||provider.source_schema!=1||!provider_ref||!catalog_field(w,*provider_ref,role,"role"))return common_failure("ExactRestPermissionSourceMismatch",id);
 if(s.resource_basis_refs[0].purpose!=LifeResourceBasisRef::Purpose::fee||s.resource_basis_refs[0].source_schema!=1||!owned_field(s.resource_basis_refs[0].original_source_ref,id,"cost_paid")||s.resource_basis_refs[1].purpose!=LifeResourceBasisRef::Purpose::wage_reservation||s.resource_basis_refs[1].source_schema!=1||!owned_field(s.resource_basis_refs[1].original_source_ref,id,"terms.wage"))return common_failure("ExactRestResourceSourceMismatch",id);
 const auto& specific=s.specific.life_activity;if(specific.activity_id!=id||specific.terms_kind!=0||!specific.service_role_ref||!catalog_field(w,*specific.service_role_ref,role,"role")||!specific.activity_ref||!catalog_field(w,*specific.activity_ref,role,"activities.rest")||specific.recovery_service_ref||specific.original_quiet_or_rest_basis||specific.trade_settlement_ref)return common_failure("ExactRestSpecificInputMismatch",id);
 std::vector<std::pair<StableId,StableId>> people={{record.view.actor_id,"recipient"},{record.view.provider_actor_id,"provider"}};std::sort(people.begin(),people.end());
 for(std::size_t i=0;i<people.size();++i){const auto& input=s.actors[i];const auto* original=definition.actors.find(people[i].first);if(!original||input.actor_id!=people[i].first||input.role!=people[i].second||input.origin_actor_key!=input.actor_id||input.origin_schema!=2||!input.skill_qualification_views.empty()||!input.effective_attribute_inputs.empty()||input.age_view||input.capability_frames.size()!=1||input.actual_location_source_refs.size()!=1||input.participation_source_refs.size()!=1||input.occupation_source_refs.size()!=1||!owned_field(input.participation_source_refs[0],id,"view.participant_actor_ids")||!owned_field(input.occupation_source_refs[0],id,"view.action_id"))return common_failure("ExactRestActorInputMismatch",id);
  const auto* origin=std::get_if<ScenarioOriginRef>(&input.actual_location_source_refs[0]);const auto* location=std::get_if<SettlementLocation>(&input.location_snapshot);const auto* place=w.catalog->common->places.find(original->placement.place_ref);if(!origin||!actor_foundation_origin_equal(*origin,original->scenario_origin_ref)||!location||!place||location->place_id!=original->placement.place_ref||location->settlement_id!=place->settlement_id)return common_failure("ExactRestLocationSourceMismatch",id);
  const auto& frozen=input.capability_frames[0];const auto& purpose=input.role=="recipient"?definition.rest_purpose:definition.provider_purpose;auto at=compare_world_moments(frozen.captured_at,row.occurred_at);if(!at.ok)return {false,{},at.error};if(at.value!=0||frozen.actor_id!=input.actor_id||!actor_foundation_rule_equal(frozen.purpose_ref,purpose))return common_failure("ExactRestCapabilitySourceMismatch",id);
  auto expected=read_authored_rest_capability_at_head(w.common.state(),*w.actor_foundation,definition,frozen.condition_head_snapshot,purpose,row.occurred_at,&w.growth,&times);if(!expected.ok)return {false,{},expected.error};auto encoded=encode_frozen_rest_capability_inputs(frozen);if(!encoded.ok)return {false,{},encoded.error};auto canonical=encode_frozen_rest_capability_inputs(expected.value);if(!canonical.ok)return {false,{},canonical.error};if(!compare_boundary(encoded.value,canonical.value).exact()||!frozen.result.allowed)return common_failure("ExactRestHistoricalCapabilityMismatch",id);
 }
 return common_success();
}
}
Status validate_exact_rest_sources(const WorldBundle& w){
 if(!w.actor_foundation||!w.catalog->actor_foundation||!w.action_elapsed||!w.world_timeline)return common_failure("ExactRestFoundationRequired");ExactRestTimeRead times(w.growth);auto foundation=validate_authored_actor_foundation(w.common.state(),*w.actor_foundation,*w.catalog->actor_foundation,&w.growth,&times);if(!foundation.ok)return foundation;
 if(!allocated(w.world_timeline->period_id,"world_period:",w.common.state().next_instance_id))return common_failure("ExactPeriodIdentityInvalid");
 // 同一M仍按真实owner插入序读before；不能拿刚完成的post-image冒充Completed输入。
 std::map<StableId,ActorConditionHead> expected_heads;
 for(const auto& pair:w.actor_foundation->character_condition_extensions.actor_condition_heads.rows()){auto head=pair.second;head.condition_revision=0;head.latest_receipt_ref.reset();expected_heads.emplace(pair.first,std::move(head));}
 std::optional<WorldMoment> previous_at;Integer previous_id=0;std::set<StableId> consumed_receipts;
 for(const auto& pair:w.growth.fixed_service_boundaries.rows()){
  const auto& row=pair.second;if(!allocated(row.boundary_id,"growth_fixed_service_boundary:",w.common.state().next_instance_id))return common_failure("InvalidExactRestBoundary",pair.first);
  Integer number=0;const auto offset=StableId{"growth_fixed_service_boundary:"}.size();std::from_chars(row.boundary_id.data()+offset,row.boundary_id.data()+row.boundary_id.size(),number);if(number<=previous_id)return common_failure("ExactRestBoundaryAllocationOrder",pair.first);previous_id=number;
  if(previous_at){auto order=compare_world_moments(*previous_at,row.occurred_at);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("ExactRestBoundaryTimeOrder",pair.first);}previous_at=row.occurred_at;
  for(const auto& actor:row.input_snapshot.actors){if(actor.capability_frames.size()!=1)return common_failure("ExactRestCapabilitySourceMismatch",pair.first);const auto expected=expected_heads.find(actor.actor_id);const auto& actual=actor.capability_frames.front().condition_head_snapshot;if(expected==expected_heads.end()||actual.actor_id!=expected->second.actor_id||actual.condition_revision!=expected->second.condition_revision||actual.initial_condition_seed_ref.seed_key!=expected->second.initial_condition_seed_ref.seed_key||bool(actual.latest_receipt_ref)!=bool(expected->second.latest_receipt_ref)||(actual.latest_receipt_ref&&actual.latest_receipt_ref->result_id!=expected->second.latest_receipt_ref->result_id))return common_failure("ExactRestBoundaryBeforeHeadMismatch",pair.first);}
  if(row.kind==LifeFixedBoundaryKind::completed){const auto* original=w.growth.life_activities.find(row.action_ref);if(!original)return common_failure("ExactRestOriginalSourceMismatch",row.action_ref);const ConditionReceiptV2* receipt=nullptr;
   for(const auto& item:w.actor_foundation->character_condition_extensions.applied_receipts.rows())if(const auto* business=std::get_if<RestConditionBusinessKey>(&item.second.business_key))if(business->action_id==row.action_ref&&business->actor_id==original->view.actor_id){if(receipt)return common_failure("ExactRestConditionReceiptDuplicate",row.action_ref);receipt=&item.second;}
   if(!receipt||!consumed_receipts.insert(receipt->result_id).second)return common_failure("ExactRestConditionReceiptMissing",row.action_ref);auto head=expected_heads.find(original->view.actor_id);if(head==expected_heads.end()||receipt->condition_revision_before!=head->second.condition_revision)return common_failure("ExactRestConditionReceiptOrder",row.action_ref);head->second.condition_revision=receipt->condition_revision_after;head->second.latest_receipt_ref=ConditionReceiptRefV2{receipt->result_id};
  }
 }
 if(consumed_receipts.size()!=w.actor_foundation->character_condition_extensions.applied_receipts.size())return common_failure("ExactRestConditionReceiptCoverage");
 std::map<StableId,std::vector<const LifeFixedServiceBoundary*>> sources;std::set<StableId> result_ids;std::set<std::pair<StableId,Integer>> once;
 for(const auto& pair:w.growth.fixed_service_boundaries.rows()){const auto& row=pair.second;if(pair.first!=row.boundary_id||!allocated(row.boundary_id,"growth_fixed_service_boundary:",w.common.state().next_instance_id)||row.source_schema!=1||row.action_kind!="service"||row.owner_ref.kind!=1||row.owner_ref.life_activity_id!=row.action_ref||row.owner_ref.terms_kind!=0)return common_failure("InvalidExactRestBoundary",pair.first);sources[row.action_ref].push_back(&row);}
 if(sources.size()!=w.growth.life_activities.size()||sources.size()!=w.action_elapsed->entries.size())return common_failure("ExactRestSourceCoverageMismatch");std::size_t active_count=0;
 for(const auto& variant:w.action_elapsed->entries){const auto* fixed=std::get_if<ActionElapsedState>(&variant);if(!fixed)return common_failure("UnexpectedSharedWorkInRestProfile");const auto& entry=*fixed;const auto* record=w.growth.life_activities.find(entry.action_ref);const auto found=sources.find(entry.action_ref);if(!record||found==sources.end()||record->view.activity!="rest"||record->view.kind!=LifeActivityKind::service||record->selected_trade_settlement||record->trade_terminal_time||!std::holds_alternative<ServiceTerms>(record->terms))return common_failure("ExactRestOriginalSourceMismatch",entry.action_ref);const auto& chain=found->second;const auto& first=*chain.front();const bool terminal=std::holds_alternative<ElapsedTerminal>(entry.timing);if(chain.size()!=(terminal?2u:1u)||first.kind!=LifeFixedBoundaryKind::started||first.boundary_ordinal!=0||first.previous_boundary_ref||first.original_outcome_ref)return common_failure("ExactRestBoundaryChainMismatch",entry.action_ref);
   const auto *command = std::get_if<LifeActualCommand>(&first.cause);
   const auto *route =
       command ? std::get_if<LifePlayerActorRoute>(&command->route_basis)
               : nullptr;
   const auto *accepted =
       command ? std::get_if<SourceRef>(&command->accepted_business_ref)
               : nullptr;
   const auto *high =
       command ? w.common.state().request_sequences.find(command->actor_id)
               : nullptr;
   const auto *control = route ? std::get_if<LifeCatalogFieldRef>(
                                     &route->original_control_source_ref)
                               : nullptr;
   if (!command || !route || !accepted ||
       !activity(*accepted, entry.action_ref) ||
       command->actor_id != record->view.actor_id ||
       command->actor_id != w.catalog->family->founder_actor_id ||
       (!control ||
        !catalog_field(w, *control, command->actor_id, "player_actor")) ||
       command->command_binding.command_kind != "service" ||
       command->command_binding.request_seq < 1 || !high ||
       command->command_binding.request_seq > *high ||
       !allocated(command->command_binding.result_id,
                  "result:", w.common.state().next_instance_id) ||
       !result_ids.insert(command->command_binding.result_id).second ||
       !once.emplace(command->actor_id, command->command_binding.request_seq)
            .second)
     return common_failure("ExactRestCommandSourceMismatch", entry.action_ref);
   if (const auto *cached = w.common.state().results.find(
           command->actor_id + ":" +
           std::to_string(command->command_binding.request_seq))) {
     const auto *begun =
         (*cached)->details
             ? std::get_if<ActionStarted>(&(*cached)->details->data)
             : nullptr;
     if ((*cached)->result_id != command->command_binding.result_id ||
         (*cached)->kind != "service" || !begun ||
         begun->action_id != entry.action_ref)
       return common_failure("ExactRestCachedResultMismatch", entry.action_ref);
   }
  auto began=compare_world_moments(first.occurred_at,entry.started_at);if(!began.ok)return {false,{},began.error};const auto& terms=std::get<ServiceTerms>(record->terms);auto required=fixed_duration_work_units(terms.duration,w.world_timeline->scale_ref.units_per_half_day);if(!required.ok)return {false,{},required.error};auto required_equal=compare_fractions(required.value.value,entry.required_units.value);if(!required_equal.ok)return {false,{},required_equal.error};if(began.value||required_equal.value||record->view.start_tick!=first.occurred_at.half_day_tick||entry.execution_source_ref!=entry.action_ref||entry.owner_cost_state_ref!=entry.action_ref||entry.duration_definition_ref!=record->context.role_id+"/activities/rest"||entry.owner_work_remainder!=0)return common_failure("ExactRestElapsedOriginalMismatch",entry.action_ref);
  for(const auto* source:chain){auto checked=inputs(w,*record,*source,times);if(!checked.ok)return checked;}
  std::vector<const FixedServiceTransition*> transitions;for(const auto& row:w.action_elapsed->fixed_service_transitions)if(row.action_ref==entry.action_ref)transitions.push_back(&row);if(transitions.size()!=chain.size())return common_failure("ExactRestTransitionCoverageMismatch",entry.action_ref);
  for(std::size_t i=0;i<chain.size();++i){const auto& transition=*transitions[i];const auto& source=*chain[i];auto same=compare_world_moments(transition.occurred_at,source.occurred_at);if(!same.ok)return {false,{},same.error};if(!allocated(transition.transition_id,"fixed_service_transition:",w.common.state().next_instance_id)||same.value||!activity(transition.execution_source_ref,entry.action_ref)||transition.execution_source_schema!=1)return common_failure("ExactRestTransitionSourceMismatch",entry.action_ref);if(i==0){const auto* cause=std::get_if<FixedOwnerAdopted>(&transition.cause);const auto* input=transition.input_basis_refs.size()==2?std::get_if<FixedAppliedInput>(&transition.input_basis_refs[0]):nullptr;const auto* rule=transition.input_basis_refs.size()==2?std::get_if<FixedLockedRuleInput>(&transition.input_basis_refs[1]):nullptr;if(!cause||!boundary_source(cause->owner_source_ref,source.boundary_id)||cause->owner_source_schema!=1||!input||!boundary_source(input->source_ref,source.boundary_id)||input->source_schema!=1||!rule||!catalog_equal(rule->catalog_identity,w.catalog->identity)||rule->rule_ref!=entry.duration_definition_ref)return common_failure("ExactRestTransitionInputMismatch",entry.action_ref);}else{const auto* cause=std::get_if<FixedOwnerTerminated>(&transition.cause);if(transition.terminal_disposition!=FixedTerminalDisposition::completed||!cause||!boundary_source(cause->owner_result_ref,source.boundary_id)||cause->owner_result_schema!=1)return common_failure("ExactRestTransitionTerminalMismatch",entry.action_ref);}}
  if(!terminal){++active_count;const auto* action=w.common.action(entry.action_ref);auto remaining=fixed_elapsed_remaining_ticks(entry,w.world_timeline->scale_ref);if(!remaining.ok)return {false,{},remaining.error};if(!action||action->kind!="service"||action->actor_id!=record->view.actor_id||action->participants!=record->view.participant_actor_ids||action->start_tick!=record->view.start_tick||action->remaining_ticks!=remaining.value||record->view.phase!=LifeActivityPhase::active)return common_failure("ExactRestActionProjectionMismatch",entry.action_ref);}
  else{const auto& end=*chain.back();const auto* applied=std::get_if<LifeOriginalTerminalApplication>(&end.cause);const auto* gate=applied?std::get_if<LifeFixedCompletionGate>(&applied->trigger):nullptr;if(end.kind!=LifeFixedBoundaryKind::completed||end.boundary_ordinal!=1||!end.previous_boundary_ref||*end.previous_boundary_ref!=first.boundary_id||!end.original_outcome_ref||end.original_outcome_ref->kind!=LifeFixedOutcomeRef::Kind::life_activity||end.original_outcome_ref->id!=entry.action_ref||!applied||applied->disposition!=LifeOriginalTerminalApplication::Disposition::completed||!gate||gate->action_ref!=entry.action_ref||gate->last_actual_period_ref||!catalog_field(w,gate->original_required_rule_ref,record->context.role_id,"activities.rest.duration")||applied->actual_result_refs.size()!=1||applied->actual_result_refs[0].kind!=LifeFixedOutcomeRef::Kind::life_activity||applied->actual_result_refs[0].id!=entry.action_ref||!applied->remaining_responsibility_refs.empty()||record->view.phase!=LifeActivityPhase::completed||!record->view.success||record->view.completed_tick!=end.occurred_at.half_day_tick||w.common.action(entry.action_ref)||w.growth.actions.find(entry.action_ref))return common_failure("ExactRestActualTerminalMismatch",entry.action_ref);
   auto binding=std::find_if(w.action_elapsed->terminal_bindings.begin(),w.action_elapsed->terminal_bindings.end(),[&](const auto& row){return row.action_ref==entry.action_ref;});if(std::get<ElapsedTerminal>(entry.timing).outcome_ref!=entry.action_ref||binding==w.action_elapsed->terminal_bindings.end()||binding->terminal_result_ref!=entry.action_ref||binding->actor_id!=record->view.actor_id||binding->action_kind!="service"||binding->participant_actor_ids!=record->view.participant_actor_ids||!activity(binding->original_execution_source_ref,entry.action_ref))return common_failure("ExactRestTerminalIdentityMismatch",entry.action_ref);
  }
 }
 if(active_count!=w.common.action_order().size())return common_failure("ExactRestActionCoverageMismatch");return common_success();
}
}
