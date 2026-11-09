// 原politics /1字段表唯一持有；/2借同前缀并在原fact末尾追加精确时刻。
#pragma once
#include "politics_model.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<AppointmentStatus>{static std::vector<std::pair<AppointmentStatus,StableId>> rows(){return {{AppointmentStatus::offered,"offered"},{AppointmentStatus::accepted,"accepted"},{AppointmentStatus::declined,"declined"},{AppointmentStatus::withdrawn,"withdrawn"},{AppointmentStatus::expired,"expired"},{AppointmentStatus::arrived,"arrived"},{AppointmentStatus::invalidated,"invalidated"}};}};
// 辖区存身份与公产引用，持有人继续从Organization唯一Office读取。
template<class A> void record(A& a,Jurisdiction& v){a("id",v.id);a("name_key",v.name_key);a("office_name_key",v.office_name_key);a("seat_place_id",v.seat_place_id);a("political_organization_id",v.political_organization_id);a("controlling_organization_id",v.controlling_organization_id);a("office_id",v.office_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_ids",v.public_inventory_ids);a("revision",v.revision);}
// 资格与时窗由具体作者政策提供，解码不补任命数值。
template<class A> void record(A& a,AppointmentPolicy& v){a("id",v.id);a("name_key",v.name_key);a("response_ticks",v.response_ticks);a("arrival_ticks",v.arrival_ticks);a("minimum_administration",v.minimum_administration);a("minimum_merit",v.minimum_merit);}
// 未决与终态任命保存同一来源链，冷恢复不会再次授出权限。
template<class A> void record(A& a,PoliticalAppointment& v){a("id",v.id);a("jurisdiction_id",v.jurisdiction_id);a("policy_id",v.policy_id);a("actor_id",v.actor_id);a("issuer_actor_id",v.issuer_actor_id);a("issuer_office_id",v.issuer_office_id);a("organization_id",v.organization_id);a("issuer_epoch",v.issuer_epoch);a("target_office_epoch",v.target_office_epoch);a("jurisdiction_revision",v.jurisdiction_revision);a("created_tick",v.created_tick);a("expires_tick",v.expires_tick);a("revision",v.revision);a("status",v.status);a("offered_fact_id",v.offered_fact_id);a("last_fact_id",v.last_fact_id);}
// 事实链与epoch为权限交接的永久证据，不用当前持有人覆盖历史。
template<class A> void record(A& a,PoliticsFact& v){a("id",v.id);a("kind",v.kind);a("jurisdiction_id",v.jurisdiction_id);a("appointment_id",v.appointment_id);a("actor_id",v.actor_id);a("issuer_actor_id",v.issuer_actor_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("decision_actor_id",v.decision_actor_id);a("witness_actor_ids",v.witness_actor_ids);a("tick",v.tick);a("office_epoch",v.office_epoch);a("status",v.status);a("cause_fact_ids",v.cause_fact_ids);}
// 所有目录字段均必填；不吞掉尚不支持的外交或占领字段。
template<class A> void record(A& a,PoliticsCatalog& v){a("enabled",v.enabled);a("jurisdictions",v.jurisdictions);a("appointment_policies",v.appointment_policies);a("command_name_keys",v.command_name_keys);}
// 持久状态只遍历本域权威表。
template<class A> void record(A& a,PoliticsState& v){a("jurisdictions",v.jurisdictions);a("appointments",v.appointments);a("facts",v.facts);}
}
