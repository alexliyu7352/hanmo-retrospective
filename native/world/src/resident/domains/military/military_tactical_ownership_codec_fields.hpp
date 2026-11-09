// Single finite field table. Original accepted-intent/order, Common prefix,
// identity and WorldMoment codecs stay their sole owners.
#pragma once
#include "military_tactical_ownership_model.hpp"
#include "military_tactical_intent_codec_fields.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "resident/domains/organization/authority_basis_codec_fields.hpp"
namespace hanmo::resident::military_tactical_ownership_fields {
template<class C>Status value(C&,MilitaryBattlefieldDestination3&);
template<class C>Status value(C&,MilitaryWorldDestination3&);
template<class C>Status value(C&,MilitaryMoveTo3&);
template<class C>Status value(C&,MilitaryHoldPosition3&);
template<class C>Status value(C&,MilitaryEngageKnownForce3&);
template<class C>Status value(C&,MilitarySupportFormation3&);
template<class C>Status value(C&,MilitaryWithdrawToward3&);
template<class C>Status value(C&,MilitaryRescueEmergency3&);
template<class C>Status value(C&,MilitaryFormationWorld3&);
template<class C>Status value(C&,MilitaryFormationInBattle3&);
template<class C>Status value(C&,MilitaryFormationPlayer3&);
template<class C>Status value(C&,MilitaryFormationPrebattle3&);
template<class C>Status value(C&,MilitaryFormationDispatched3&);
template<class C>Status value(C&,MilitaryFormationAutonomous3&);
template<class C>Status value(C&,MilitaryTacticalFormationBirth3&);
template<class C>Status value(C&,MilitaryTacticalFormation3&);
template<class C>Status value(C&,MilitaryForceWorldContext3&);
template<class C>Status value(C&,MilitaryForceBattleContext3&);
template<class C>Status value(C&,MilitaryForceCommandStamp3&);
template<class C>Status value(C&,DirectForceIntent3&);
template<class C>Status value(C&,DirectForceIntentReceipt3&);
template<class C>Status value(C&,MilitaryMissionOriginalExecution3&);
template<class C>Status value(C&,MilitaryTacticalMissionRecord3&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,MomentOwnerSource& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,WorldIdentity& v){return authority_basis_codec_fields::world(c,v);}
template<class C>Status value(C& c,HexPoint& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,MilitaryAcceptedIntent3& v){return military_tactical_intent_fields::value(c,v);}
template<class C>Status value(C& c,CommonActualFactPrefix& v){
 return common_fact_prefix_fields::fields(c,v,[](auto& fields,auto& ref){return exact_fields::source(fields,ref);});
}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){
 using V=std::variant<T...>;
 static_assert(std::is_same_v<V,MilitaryCommandDestination3>||std::is_same_v<V,MilitaryTacticalMission3>||
  std::is_same_v<V,MilitaryFormationSpatial3>||std::is_same_v<V,MilitaryFormationAutonomousOrigin3>||
  std::is_same_v<V,MilitaryFormationControl3>||std::is_same_v<V,MilitaryForceCommandContext3>);
 Integer tag=Integer(v.index());c.field("tag",tag);
 if(c.status().ok&&(tag<0||std::size_t(tag)>=sizeof...(T)))return c.reject("UnknownMilitaryTacticalOwnershipVariant");
 if(c.reading()&&c.status().ok&&!dynamic_action_codec::construct(c,v,tag))return c.reject("UnknownMilitaryTacticalOwnershipVariant");
 if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& row){return value(x,row);},v);});
 return c.status();
}
template<class C>Status value(C& c,MilitaryBattlefieldDestination3& v){
 field(c,"arena_ref",v.arena_ref);field(c,"geometry_profile_ref",v.geometry_profile_ref);
 field(c,"position",v.position);field(c,"heading",v.heading);return c.status();
}
template<class C>Status value(C& c,MilitaryWorldDestination3& v){field(c,"registered_s1_destination_ref",v.registered_s1_destination_ref);return c.status();}
template<class C>Status value(C& c,MilitaryMoveTo3& v){field(c,"destination",v.destination);return c.status();}
template<class C>Status value(C& c,MilitaryHoldPosition3& v){field(c,"destination",v.destination);return c.status();}
template<class C>Status value(C& c,MilitaryEngageKnownForce3& v){field(c,"known_target_ref",v.known_target_ref);return c.status();}
template<class C>Status value(C& c,MilitarySupportFormation3& v){field(c,"allied_formation_ref",v.allied_formation_ref);return c.status();}
template<class C>Status value(C& c,MilitaryWithdrawToward3& v){field(c,"approved_exit_ref",v.approved_exit_ref);return c.status();}
template<class C>Status value(C& c,MilitaryRescueEmergency3& v){field(c,"known_emergency_ref",v.known_emergency_ref);field(c,"rescue_plan_ref",v.rescue_plan_ref);return c.status();}
template<class C>Status value(C& c,MilitaryFormationWorld3& v){field(c,"s1_force_location_ref",v.s1_force_location_ref);field(c,"binding_revision",v.binding_revision);return c.status();}
template<class C>Status value(C& c,MilitaryFormationInBattle3& v){
 field(c,"session_id",v.session_id);field(c,"session_generation",v.session_generation);
 field(c,"entry_source_ref",v.entry_source_ref);field(c,"binding_revision",v.binding_revision);return c.status();
}
template<class C>Status value(C& c,MilitaryFormationPlayer3& v){field(c,"controller_actor_id",v.controller_actor_id);field(c,"control_epoch",v.control_epoch);return c.status();}
template<class C>Status value(C& c,MilitaryFormationPrebattle3& v){field(c,"assignment_ref",v.assignment_ref);return c.status();}
template<class C>Status value(C& c,MilitaryFormationDispatched3& v){field(c,"dispatch_fact_ref",v.dispatch_fact_ref);return c.status();}
template<class C>Status value(C& c,MilitaryFormationAutonomous3& v){field(c,"origin",v.origin);field(c,"active_mission_ref",v.active_mission_ref);return c.status();}
template<class C>Status value(C& c,MilitaryTacticalFormationBirth3& v){
 field(c,"original_admission_source",v.original_admission_source);field(c,"created_at",v.created_at);
 field(c,"actual_before_common_prefix",v.actual_before_common_prefix);field(c,"original_source_refs",v.original_source_refs);return c.status();
}
template<class C>Status value(C& c,MilitaryTacticalFormation3& v){
 field(c,"formation_id",v.formation_id);field(c,"source_army_ref",v.source_army_ref);field(c,"roster_revision",v.roster_revision);
 field(c,"naval_carrier_ref",v.naval_carrier_ref);field(c,"spatial_binding",v.spatial_binding);
 field(c,"member_unit_refs",v.member_unit_refs);field(c,"officer_assignment_ref",v.officer_assignment_ref);
 field(c,"superior_formation_ref",v.superior_formation_ref);field(c,"mission_terms_ref",v.mission_terms_ref);
 field(c,"control",v.control);field(c,"active_intent_ref",v.active_intent_ref);field(c,"last_applied_order_ordinal",v.last_applied_order_ordinal);
 field(c,"control_revision",v.control_revision);field(c,"command_binding_revision",v.command_binding_revision);field(c,"intent_revision",v.intent_revision);
 field(c,"birth",v.birth);return c.status();
}
template<class C>Status value(C& c,MilitaryForceWorldContext3&){return c.status();}
template<class C>Status value(C& c,MilitaryForceBattleContext3& v){field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);return c.status();}
template<class C>Status value(C& c,MilitaryForceCommandStamp3& v){
 field(c,"world",v.world);field(c,"moment",v.moment);field(c,"formation_id",v.formation_id);
 field(c,"control_revision",v.control_revision);field(c,"spatial_binding_revision",v.spatial_binding_revision);
 field(c,"player_control_epoch",v.player_control_epoch);field(c,"context",v.context);return c.status();
}
template<class C>Status value(C& c,DirectForceIntent3& v){
 field(c,"stamp",v.stamp);field(c,"formation_id",v.formation_id);field(c,"expected_intent_revision",v.expected_intent_revision);
 field(c,"intent",v.intent);return c.status();
}
template<class C>Status value(C& c,DirectForceIntentReceipt3& v){
 field(c,"formation_id",v.formation_id);field(c,"mission_ref",v.mission_ref);field(c,"previous_intent_revision",v.previous_intent_revision);
 field(c,"intent_revision",v.intent_revision);field(c,"accepted_at",v.accepted_at);field(c,"accepted_intent_refs",v.accepted_intent_refs);return c.status();
}
template<class C>Status value(C& c,MilitaryMissionOriginalExecution3& v){field(c,"unit_ref",v.unit_ref);field(c,"execution_source",v.execution_source);return c.status();}
template<class C>Status value(C& c,MilitaryTacticalMissionRecord3& v){
 field(c,"id",v.id);field(c,"formation_ref",v.formation_ref);field(c,"intent_revision",v.intent_revision);field(c,"adopted_at",v.adopted_at);
 field(c,"previous_intent_ref",v.previous_intent_ref);field(c,"original_direct_request",v.original_direct_request);
 field(c,"actual_before_common_prefix",v.actual_before_common_prefix);field(c,"control_binding_source",v.control_binding_source);
 field(c,"original_solver_definition_ref",v.original_solver_definition_ref);field(c,"original_solver_definition_version",v.original_solver_definition_version);
 field(c,"original_scope_sources",v.original_scope_sources);field(c,"original_executions",v.original_executions);
 field(c,"accepted_intents",v.accepted_intents);field(c,"actual_command_source",v.actual_command_source);return c.status();
}
}
