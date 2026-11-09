// 唯一有限Duty字段表生成，JSON/binary/shape共享；物理段选版仍由原owner批准。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_duty_contract.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/organization/authority_basis_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
namespace hanmo::resident::military_duty_codec {
template<class C>Status value(C&, SourceRef&);
template<class C>Status value(C&, SubjectRef&);
template<class C>Status value(C&, ScopeRef&);
template<class C>Status value(C&, AuthorityBasisRef&);
template<class C>Status value(C&, MomentOwnerSource&);
template<class C>Status value(C&, ActionCoreView&);
template<class C>Status value(C&, LegacyMilitaryOfficeRoot&);
template<class C>Status value(C&, OrganizationOfficeGrantRoot&);
template<class C>Status value(C&, PrivateServiceCommandRoot&);
template<class C>Status value(C&, MilitaryAuthorityRef&);
template<class C>Status value(C&, MilitaryParticipationRef&);
template<class C>Status value(C&, DutyTravelAction&);
template<class C>Status value(C&, DutyStationAction&);
template<class C>Status value(C&, DutyRegisteredMilitaryActivity&);
template<class C>Status value(C&, PassageBoundaryRef&);
template<class C>Status value(C&, MilitaryDutyTerms&);
template<class C>Status value(C&, DutyServiceWindowRef&);
template<class C>Status value(C&, CommonActualFactPrefix&);
template<class C>Status value(C&, CharacterConditionAppliedPrefix&);
template<class C>Status value(C&, ActorConditionHead&);
template<class C>Status value(C&, MilitaryDutyConditionConsumption&);
template<class C>Status value(C&, MilitaryDutyWorkPatrolRoute&);
template<class C>Status value(C&, MilitaryDutyAssignmentSource&);
template<class C>Status value(C&, MilitaryDutyQualification&,Integer);
template<class C>Status value(C&, MilitaryDutyServiceSegment&,Integer);
template<class C>Status value(C&, MilitaryDutyWindowState&,Integer);
template<class C>Status value(C&, MilitaryDutyQualification&);
template<class C>Status value(C&, MilitaryDutyServiceSegment&);
template<class C>Status value(C&, DutyWindowOpen&);
template<class C>Status value(C&, DutyWindowClosed&);
template<class C>Status value(C&, MilitaryDutyWindowState&);
template<class C>Status value(C&, MilitaryDutyPeriodFact2&);
template<class C>Status value(C&, MilitaryDutyRule&);
template<class C>Status value(C&, MilitaryDutyAcceptedSource&);
template<class C>Status value(C&, MilitaryDutyActualBindingSource&);
template<class C>Status value(C&, MilitaryDutyAuthorizationEnded&);
template<class C>Status value(C&, MilitaryDutyActualMovementEnded&);
template<class C>Status value(C&, MilitaryDutyMovementStopSource&);
template<class C>Status value(C&, MilitaryDutyExecution&);
template<class C>Status value(C&, MilitaryDutyControlSource&);
template<class C>Status value(C&, MilitaryStationActionSource&);
template<class C>Status value(C&, MilitaryStationTerminalSource&);
template<class C>Status value(C&, SubjectKind&);
template<class C>Status value(C&, ScopeRef::Kind&);
template<class C>Status value(C&, MilitaryAuthorityScope&);
template<class C>Status value(C&, MilitaryDutyKind&);
template<class C>Status value(C&, MilitaryDutyPhase&);
template<class C>Status value(C&, MilitaryDutyControlOperation&);
template<class C>Status value(C&, MilitaryDutyDisposition&);
template<class C>Status value(C&, PassageBoundaryKind&);
template<class C>Status value(C&, WorldMoment&);
template<class C>Status value(C&, MomentSpan&);
template<class C>Status value(C&, WorkUnits&);
template<class C>Status value(C&, WorldIdentity&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,bool& v){return c.field("value",v);}
template<class C,class T>Status value(C&,std::vector<T>&);
template<class C,class T>Status value(C&,std::optional<T>&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T>Status field(C& c,const char* name,std::vector<T>& v){return c.sequence(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status value(C& c,std::vector<T>& v){return field(c,"items",v);}
template<class C,class T>Status value(C& c,std::optional<T>& v){return field(c,"optional",v);}
template<std::size_t I=0,class C,class V>bool construct(C& c,V& v,Integer tag){if constexpr(I<std::variant_size_v<V>){if(tag==Integer(I)){if(!prepare_checkpoint_default_values<std::variant_alternative_t<I,V>>(c).ok)return true;v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}return false;}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){Integer tag=Integer(v.index());c.field("tag",tag);if(c.reading()&&c.status().ok&&!construct(c,v,tag))return c.reject("MilitaryDutyUnknownVariant");if(c.status().ok)return c.object("value",[&](auto& x){return std::visit([&](auto& item){return value(x,item);},v);});return c.status();}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,MomentSpan& v){field(c,"from",v.from);return field(c,"to",v.to);}
template<class C>Status value(C& c,WorkUnits& v){return exact_fields::work_units(c,v);}
template<class C>Status value(C& c,WorldIdentity& v){return authority_basis_codec_fields::world(c,v);}
template<class C>Status value(C& c,SubjectKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>2))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=SubjectKind(tag);return c.status();}
template<class C>Status value(C& c,ScopeRef::Kind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>4))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=ScopeRef::Kind(tag);return c.status();}
template<class C>Status value(C& c,MilitaryAuthorityScope& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=MilitaryAuthorityScope(tag);return c.status();}
template<class C>Status value(C& c,MilitaryDutyKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>2))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=MilitaryDutyKind(tag);return c.status();}
template<class C>Status value(C& c,MilitaryDutyPhase& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=MilitaryDutyPhase(tag);return c.status();}
template<class C>Status value(C& c,MilitaryDutyControlOperation& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=MilitaryDutyControlOperation(tag);return c.status();}
template<class C>Status value(C& c,MilitaryDutyDisposition& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>2))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=MilitaryDutyDisposition(tag);return c.status();}
template<class C>Status value(C& c,PassageBoundaryKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("MilitaryDutyUnknownEnum");if(c.reading()&&c.status().ok)v=PassageBoundaryKind(tag);return c.status();}
template<class C>Status value(C& c,SourceRef& v){field(c,"domain",v.domain);field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
template<class C>Status value(C& c,SubjectRef& v){field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
template<class C>Status value(C& c,ScopeRef& v){field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
template<class C>Status value(C& c,AuthorityBasisRef& v){field(c,"issued_source",v.issued_source);field(c,"source_schema",v.source_schema);return c.status();}
template<class C>Status value(C& c,MomentOwnerSource& v){field(c,"source_ref",v.source_ref);field(c,"source_schema",v.source_schema);return c.status();}
template<class C>Status value(C& c,ActionCoreView& v){field(c,"id",v.id);field(c,"kind",v.kind);field(c,"actor_id",v.actor_id);field(c,"participants",v.participants);field(c,"start_tick",v.start_tick);field(c,"remaining_ticks",v.remaining_ticks);return c.status();}
template<class C>Status value(C& c,LegacyMilitaryOfficeRoot& v){field(c,"profile_id",v.profile_id);field(c,"military_schema_version",v.military_schema_version);field(c,"military_office_id",v.military_office_id);field(c,"office_definition_id",v.office_definition_id);field(c,"holder_actor_id",v.holder_actor_id);field(c,"enabled",v.enabled);field(c,"original_appointment_id",v.original_appointment_id);field(c,"organization_id",v.organization_id);field(c,"garrison_id",v.garrison_id);field(c,"command_scope",v.command_scope);field(c,"source_world",v.source_world);field(c,"original_source_ref",v.original_source_ref);return c.status();}
template<class C>Status value(C& c,OrganizationOfficeGrantRoot& v){field(c,"office_id",v.office_id);field(c,"grant_id",v.grant_id);field(c,"authority_epoch",v.authority_epoch);field(c,"appointment_source_id",v.appointment_source_id);field(c,"organization_id",v.organization_id);field(c,"scope",v.scope);field(c,"authority_basis_ref",v.authority_basis_ref);return c.status();}
template<class C>Status value(C& c,PrivateServiceCommandRoot& v){field(c,"agreement_id",v.agreement_id);field(c,"accepted_terms_revision",v.accepted_terms_revision);field(c,"command_assignment_id",v.command_assignment_id);field(c,"assignment_revision",v.assignment_revision);field(c,"commander_actor_id",v.commander_actor_id);field(c,"principal_id",v.principal_id);field(c,"controlled_force_ids",v.controlled_force_ids);field(c,"valid_from_tick",v.valid_from_tick);field(c,"valid_until_tick",v.valid_until_tick);field(c,"accepted_source_id",v.accepted_source_id);return c.status();}
template<class C>Status value(C& c,MilitaryAuthorityRef& v){field(c,"principal",v.principal);field(c,"acting_actor_id",v.acting_actor_id);field(c,"source",v.source);field(c,"allowed_scope",v.allowed_scope);field(c,"allowed_target_ids",v.allowed_target_ids);return c.status();}
template<class C>Status value(C& c,MilitaryParticipationRef& v){field(c,"actor_id",v.actor_id);field(c,"owner_activity_id",v.owner_activity_id);field(c,"occupation_source_id",v.occupation_source_id);field(c,"occupation_version",v.occupation_version);return c.status();}
template<class C>Status value(C& c,DutyTravelAction& v){field(c,"action_id",v.action_id);field(c,"journey_ref",v.journey_ref);return c.status();}
template<class C>Status value(C& c,DutyStationAction& v){field(c,"action_id",v.action_id);return c.status();}
template<class C>Status value(C& c,DutyRegisteredMilitaryActivity& v){field(c,"activity_ref",v.activity_ref);field(c,"activity_schema_version",v.activity_schema_version);return c.status();}
template<class C>Status value(C& c,PassageBoundaryRef& v){field(c,"kind",v.kind);field(c,"id",v.id);field(c,"map_id",v.map_id);field(c,"from_site_id",v.from_site_id);field(c,"to_site_id",v.to_site_id);return c.status();}
template<class C>Status value(C& c,MilitaryDutyTerms& v){field(c,"duty_action_id",v.duty_action_id);field(c,"order_or_assignment_ref",v.order_or_assignment_ref);field(c,"authority_ref",v.authority_ref);field(c,"duty_kind",v.duty_kind);field(c,"leader_actor_id",v.leader_actor_id);field(c,"unit_ids",v.unit_ids);field(c,"named_guard_actor_ids",v.named_guard_actor_ids);field(c,"fleet_ids",v.fleet_ids);field(c,"execution_binding",v.execution_binding);field(c,"start_tick",v.start_tick);field(c,"until_tick",v.until_tick);field(c,"duty_rule_id",v.duty_rule_id);field(c,"duty_rule_version",v.duty_rule_version);field(c,"route_or_station_definition_ref",v.route_or_station_definition_ref);field(c,"boundary_refs",v.boundary_refs);field(c,"custody_source_refs",v.custody_source_refs);field(c,"supply_authorization_ref",v.supply_authorization_ref);field(c,"qualified_period_policy_ref",v.qualified_period_policy_ref);field(c,"report_recipient_ref",v.report_recipient_ref);field(c,"state",v.state);return c.status();}
template<class C>Status value(C& c,DutyServiceWindowRef& v){field(c,"duty_action_id",v.duty_action_id);field(c,"service_epoch",v.service_epoch);field(c,"ordinal",v.ordinal);field(c,"opened_at",v.opened_at);field(c,"planned_close_at",v.planned_close_at);return c.status();}
template<class C>Status value(C& c,CommonActualFactPrefix& v){return common_fact_prefix_fields::fields(c,v,[](auto& x,auto& source){return value(x,source);});}
template<class C>Status value(C& c,CharacterConditionAppliedPrefix& v){field(c,"applied_receipt_count",v.applied_receipt_count);c.optional("last_applied_receipt",v.last_applied_receipt,[](auto& x,auto& ref){return x.field("result_id",ref.result_id);});if(c.status().ok&&(v.applied_receipt_count<0||bool(v.applied_receipt_count)!=bool(v.last_applied_receipt)))return c.reject("MilitaryDutyInvalidConditionPrefix");return c.status();}
template<class C>Status value(C& c,ActorConditionHead& v){return actor_condition_codec::head(c,v);}
template<class C>Status value(C& c,MilitaryDutyConditionConsumption& v){field(c,"common_prefix",v.common_prefix);field(c,"condition_prefix",v.condition_prefix);return field(c,"consumed_heads",v.consumed_heads);}
template<class C>Status value(C& c,MilitaryDutyWorkPatrolRoute& v){field(c,"work_acceptance_ref",v.work_acceptance_ref);return field(c,"choice_ref",v.choice_ref);}
template<class C>Status value(C& c,MilitaryDutyAssignmentSource& v){field(c,"id",v.id);field(c,"issued_at",v.issued_at);field(c,"issuing_actor_id",v.issuing_actor_id);field(c,"duty_kind",v.duty_kind);field(c,"authority_ref",v.authority_ref);field(c,"authority_sources",v.authority_sources);field(c,"leader_actor_id",v.leader_actor_id);field(c,"unit_ids",v.unit_ids);field(c,"named_guard_actor_ids",v.named_guard_actor_ids);field(c,"fleet_ids",v.fleet_ids);field(c,"authorized_until_tick",v.authorized_until_tick);field(c,"duty_rule_id",v.duty_rule_id);field(c,"duty_rule_version",v.duty_rule_version);field(c,"qualified_period_policy_ref",v.qualified_period_policy_ref);field(c,"route_definition",v.route_definition);field(c,"supply_authorization",v.supply_authorization);return field(c,"original_work_assignment",v.original_work_assignment);}
template<class C>Status value(C& c,MilitaryDutyQualification& v,Integer selected){if(selected!=2&&selected!=3&&selected!=4)return c.reject("MilitaryDutySelectedSchemaRequired");if(selected!=4&&v.condition_consumption)return c.reject("MilitaryDutyConditionConsumptionRequires4");field(c,"actual_participant_refs",v.actual_participant_refs);field(c,"actual_member_ids",v.actual_member_ids);field(c,"eligible_force_member_ids",v.eligible_force_member_ids);field(c,"leader_present",v.leader_present);field(c,"authority_valid",v.authority_valid);field(c,"route_or_station_valid",v.route_or_station_valid);field(c,"supplied",v.supplied);field(c,"participant_basis_refs",v.participant_basis_refs);field(c,"route_or_station_sources",v.route_or_station_sources);field(c,"supply_sources",v.supply_sources);field(c,"authority_sources",v.authority_sources);field(c,"encounter_result_refs",v.encounter_result_refs);field(c,"unresolved_conflict_refs",v.unresolved_conflict_refs);field(c,"unavailable_cause",v.unavailable_cause);if(selected==4){if(c.reading()){if(!prepare_checkpoint_default_values<MilitaryDutyConditionConsumption>(c).ok)return c.status();v.condition_consumption.emplace();}if(!v.condition_consumption)return c.reject("MilitaryDutyConditionConsumptionRequired");c.object("condition_consumption",[&](auto& x){return value(x,*v.condition_consumption);});}return c.status();}
template<class C>Status value(C& c,MilitaryDutyQualification& v){return value(c,v,3);}
template<class C>Status value(C& c,MilitaryDutyServiceSegment& v,Integer selected){field(c,"span",v.span);return c.object("qualification",[&](auto& x){return value(x,v.qualification,selected);});}
template<class C>Status value(C& c,MilitaryDutyServiceSegment& v){return value(c,v,3);}
template<class C>Status value(C& c,DutyWindowOpen& v){return c.status();}
template<class C>Status value(C& c,DutyWindowClosed& v){field(c,"result_ref",v.result_ref);return c.status();}
template<class C>Status value(C& c,MilitaryDutyWindowState& v,Integer selected){field(c,"id",v.id);field(c,"window",v.window);field(c,"last_accounted_at",v.last_accounted_at);field(c,"actual_service_units",v.actual_service_units);field(c,"qualification_source_refs",v.qualification_source_refs);field(c,"min_eligible_force_members",v.min_eligible_force_members);field(c,"leader_present_throughout",v.leader_present_throughout);field(c,"conflict_source_refs",v.conflict_source_refs);field(c,"state",v.state);c.object("opening_qualification",[&](auto& x){return value(x,v.opening_qualification,selected);});c.sequence("actual_segments",v.actual_segments,[&](auto& x,auto& segment){return value(x,segment,selected);});c.optional("closing_qualification",v.closing_qualification,[&](auto& x,auto& q){return value(x,q,selected);});return c.status();}
template<class C>Status value(C& c,MilitaryDutyWindowState& v){return value(c,v,3);}
template<class C>Status value(C& c,MilitaryDutyPeriodFact2& v){field(c,"fact_id",v.fact_id);field(c,"window",v.window);field(c,"actual_until",v.actual_until);field(c,"execution_binding",v.execution_binding);field(c,"order_or_assignment_ref",v.order_or_assignment_ref);field(c,"duty_kind",v.duty_kind);field(c,"leader_actor_id",v.leader_actor_id);field(c,"leader_present",v.leader_present);field(c,"unit_ids",v.unit_ids);field(c,"actual_participant_refs",v.actual_participant_refs);field(c,"actual_participant_count",v.actual_participant_count);field(c,"eligible_force_member_count",v.eligible_force_member_count);field(c,"route_or_station_source_refs",v.route_or_station_source_refs);field(c,"participant_basis_refs",v.participant_basis_refs);field(c,"supply_fact_refs",v.supply_fact_refs);field(c,"encounter_result_refs",v.encounter_result_refs);field(c,"unresolved_conflict_refs",v.unresolved_conflict_refs);field(c,"provided_half_days",v.provided_half_days);field(c,"disposition",v.disposition);field(c,"reason_ref",v.reason_ref);field(c,"duty_rule_version",v.duty_rule_version);field(c,"authority_ref",v.authority_ref);return c.status();}
template<class C>Status value(C& c,MilitaryDutyRule& v){field(c,"id",v.id);field(c,"version",v.version);field(c,"kind",v.kind);field(c,"maximum_authorization_half_days",v.maximum_authorization_half_days);field(c,"minimum_eligible_force_members",v.minimum_eligible_force_members);field(c,"qualified_period_policy_ref",v.qualified_period_policy_ref);field(c,"leader_capability_purpose_ref",v.leader_capability_purpose_ref);field(c,"member_capability_purpose_ref",v.member_capability_purpose_ref);field(c,"requires_leader",v.requires_leader);field(c,"requires_supply",v.requires_supply);field(c,"rejects_conflict",v.rejects_conflict);return c.status();}
template<class C>Status value(C& c,MilitaryDutyAcceptedSource& v){field(c,"id",v.id);field(c,"duty_action_id",v.duty_action_id);field(c,"accepted_at",v.accepted_at);field(c,"original_assignment_source",v.original_assignment_source);field(c,"assignment_source_schema",v.assignment_source_schema);return c.status();}
template<class C>Status value(C& c,MilitaryDutyActualBindingSource& v){field(c,"id",v.id);field(c,"execution_binding",v.execution_binding);field(c,"original_execution_source",v.original_execution_source);field(c,"original_started_at",v.original_started_at);field(c,"bound_at",v.bound_at);return c.status();}
template<class C>Status value(C& c,MilitaryDutyAuthorizationEnded&){return c.status();}
template<class C>Status value(C& c,MilitaryDutyActualMovementEnded& v){return field(c,"actual_terminal_source",v.actual_terminal_source);}
template<class C>Status value(C& c,MilitaryDutyMovementStopSource& v){field(c,"id",v.id);field(c,"stopped_at",v.stopped_at);field(c,"reason",v.reason);return c.status();}
template<class C>Status value(C& c,MilitaryDutyExecution& v){field(c,"duty_action_id",v.duty_action_id);field(c,"accepted_source_ref",v.accepted_source_ref);field(c,"service_epoch",v.service_epoch);field(c,"next_period_ordinal",v.next_period_ordinal);field(c,"current_window_ref",v.current_window_ref);field(c,"last_result_ref",v.last_result_ref);field(c,"actual_binding",v.actual_binding);field(c,"movement_stop",v.movement_stop);return c.status();}
template<class C>Status value(C& c,MilitaryDutyControlSource& v){field(c,"id",v.id);field(c,"duty_action_id",v.duty_action_id);field(c,"operation",v.operation);field(c,"current_authority_source",v.current_authority_source);field(c,"occurred_at",v.occurred_at);field(c,"resulting_phase",v.resulting_phase);field(c,"safe_boundary_stop_source",v.safe_boundary_stop_source);field(c,"continuation_source",v.continuation_source);return c.status();}
template<class C>Status value(C& c,MilitaryStationActionSource& v){field(c,"id",v.id);field(c,"duty_action_id",v.duty_action_id);field(c,"actual_action",v.actual_action);field(c,"started_at",v.started_at);field(c,"actual_presence_sources",v.actual_presence_sources);return c.status();}
template<class C>Status value(C& c,MilitaryStationTerminalSource& v){field(c,"id",v.id);field(c,"station_source_ref",v.station_source_ref);field(c,"ended_at",v.ended_at);field(c,"original_cause",v.original_cause);field(c,"authorization_expired",v.authorization_expired);return c.status();}
struct DutyRowTransfer {
 template<class Target,class T>Status operator()(Target& destination,std::pair<StableId,T>& source)const{
  static_assert(std::is_same_v<T,MilitaryDutyTerms>||std::is_same_v<T,MilitaryDutyExecution>||
   std::is_same_v<T,MilitaryDutyAcceptedSource>||std::is_same_v<T,MilitaryDutyWindowState>||
   std::is_same_v<T,MilitaryDutyPeriodFact2>||std::is_same_v<T,MilitaryDutyControlSource>||
   std::is_same_v<T,MilitaryStationActionSource>||std::is_same_v<T,MilitaryStationTerminalSource>||
   std::is_same_v<T,MilitaryDutyAssignmentSource>);
  destination.emplace_back(std::move(source));return common_success();
 }
};
template<class T,class Visit>Status table(CodecVisitor& c,std::string_view name,OrderedTable<T>& rows,Visit visit){
 if(c.reading()&&!rows.empty())return c.reject("MilitaryDutyRootAlreadyPopulated",name);
 return common_checkpoint::table(c,name,rows,visit,DutyRowTransfer{});
}
template<class T,class Visit>Status table(ActorFoundationJsonVisitor& c,std::string_view name,OrderedTable<T>& rows,Visit visit){
 if(c.reading()&&!rows.empty())return c.reject("MilitaryDutyRootAlreadyPopulated",name);
 return c.table(name,rows,visit,DutyRowTransfer{});
}
template<class T,class Visit>Status table(ActorFoundationProjectionArchive& c,std::string_view name,OrderedTable<T>& rows,Visit visit){return c.table(name,rows,visit);}
template<class C,class T>Status table(C& c,std::string_view name,OrderedTable<T>& rows){return table(c,name,rows,[](auto& x,auto& v){return value(x,v);});}
// 按选定版本校验原布局并保留原拒绝码，不增删字段或变更版本。
template <class C> Status state(C &c, MilitaryDutyState &s, Integer selected) {
  if (selected != 2 && selected != 3 && selected != 4)
    return c.reject("MilitaryDutySelectedSchemaRequired");
  if (!c.reading()) {
    auto layout = validate_military_duty_selected_layout(s, selected);
    if (!layout.ok)
      return c.reject(layout.error.original.code);
  }
  table(c, "terms", s.terms);
  table(c, "executions", s.executions);
  table(c, "accepted_sources", s.accepted_sources);
  table(c, "windows", s.windows,
        [selected](auto &x, auto &v) { return value(x, v, selected); });
  table(c, "period_facts", s.period_facts);
  table(c, "controls", s.controls);
  table(c, "station_sources", s.station_sources);
  table(c, "station_terminals", s.station_terminals);
  if (selected == 4)
    table(c, "assignments", s.assignments);
  if (c.status().ok) {
    auto layout = validate_military_duty_selected_layout(s, selected);
    if (!layout.ok)
      return c.reject(layout.error.original.code);
  }
  return c.status();
}
template<class C>Status state(C& c,MilitaryDutyState& s){return state(c,s,3);}
}
