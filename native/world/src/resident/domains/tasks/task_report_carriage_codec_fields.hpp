#pragma once
#include "task_report_carriage_types.hpp"
#include "task_dynamic_codec.hpp"
#include "resident/domains/interaction/report_disclosure_codec_fields.hpp"
namespace hanmo::resident::task_report_carriage_codec {
template<class A>Status terms(A&a,TaskCarrierReportTerms&v){a.field("report_id",v.report_id);a.field("sender_actor_id",v.sender_actor_id);a.field("carrier_actor_id",v.carrier_actor_id);a.field("recipient_actor_id",v.recipient_actor_id);a.object("actual_carrier_disclosure",[&](auto&n){return report_disclosure_codec::moment_source(n,v.actual_carrier_disclosure);});return a.field("earliest_delivery_tick",v.earliest_delivery_tick);}
template<class A>Status dispatch(A&a,TaskReportCarriageDispatch&v){a.field("id",v.id);a.field("action_id",v.action_id);a.object("original_execution_source",[&](auto&n){return report_disclosure_codec::moment_source(n,v.original_execution_source);});a.object("sender_knowledge_source",[&](auto&n){return report_disclosure_codec::source(n,v.sender_knowledge_source);});a.object("permission",[&](auto&n){return report_disclosure_codec::permission(n,v.permission);});return a.object("sent_at",[&](auto&n){return dynamic_codec::exact_moment(n,v.sent_at);});}
template<class A>Status carriage(A&a,TaskReportCarriage&v){a.field("agreement_id",v.agreement_id);a.object("terms",[&](auto&n){return terms(n,v.terms);});return a.optional("dispatch",v.dispatch,[](auto&n,auto&d){return dispatch(n,d);});}
template<class A>Status handover(A&a,TaskReportHandoverSource&v){a.field("id",v.id);a.field("carriage_id",v.carriage_id);a.object("actual_arrival",[&](auto&n){return report_disclosure_codec::moment_source(n,v.actual_arrival);});a.object("contact_source",[&](auto&n){return report_disclosure_codec::moment_source(n,v.contact_source);});a.field("dialogue_node_id",v.dialogue_node_id);a.field("dialogue_phase",v.dialogue_phase);return a.object("handed_over_at",[&](auto&n){return dynamic_codec::exact_moment(n,v.handed_over_at);});}
}
