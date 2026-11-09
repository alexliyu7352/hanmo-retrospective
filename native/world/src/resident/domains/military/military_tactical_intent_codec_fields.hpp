// One accepted-intent field table; source/order/M tables remain original owners.
#pragma once
#include "military_tactical_intent_model.hpp"
#include "dynamic_action_codec_fields.hpp"
namespace hanmo::resident::military_tactical_intent_fields {
template<class C>Status value(C&,MilitaryPlayerDirectIntent3&);
template<class C>Status value(C&,MilitaryCommittedDecisionIntent3&);
template<class C>Status value(C&,MilitaryDeliveredOrderIntent3&);
template<class C>Status value(C&,MilitaryIntentAdoption3&);
template<class C>Status value(C&,MilitaryAcceptedIntent3&);
template<class C>Status value(C& c,MomentOwnerSource& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,MilitaryOrder& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status value(C& c,MilitaryPlayerDirectIntent3& v){
 field(c,"controller_actor_id",v.controller_actor_id);field(c,"control_epoch",v.control_epoch);
 field(c,"actual_command_source",v.actual_command_source);return c.status();
}
template<class C>Status value(C& c,MilitaryCommittedDecisionIntent3& v){
 field(c,"decision_source_ref",v.decision_source_ref);field(c,"controller_ref",v.controller_ref);
 field(c,"control_binding_generation",v.control_binding_generation);return c.status();
}
template<class C>Status value(C& c,MilitaryDeliveredOrderIntent3& v){
 field(c,"order_ref",v.order_ref);field(c,"actual_response_source",v.actual_response_source);return c.status();
}
template<class C>Status value(C& c,MilitaryIntentAdoption3& v){
 Integer tag=Integer(v.index());c.field("tag",tag);
 if(c.status().ok&&(tag<0||tag>2))return c.reject("UnknownMilitaryIntentAdoption");
 if(c.reading()&&c.status().ok&&!dynamic_action_codec::construct(c,v,tag))return c.reject("UnknownMilitaryIntentAdoption");
 if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& row){return value(x,row);},v);});
 return c.status();
}
template<class C>Status value(C& c,MilitaryAcceptedIntent3& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);
 field(c,"formation_ref",v.formation_ref);field(c,"mission_ref",v.mission_ref);field(c,"unit_ref",v.unit_ref);
 field(c,"accepted_at",v.accepted_at);field(c,"action_instance_ordinal",v.action_instance_ordinal);
 field(c,"original_order",v.original_order);field(c,"timing_definition_ref",v.timing_definition_ref);
 field(c,"timing_definition_version",v.timing_definition_version);
 field(c,"control_binding_source",v.control_binding_source);field(c,"mission_scope_source",v.mission_scope_source);
 field(c,"adoption",v.adoption);return c.status();
}
}
