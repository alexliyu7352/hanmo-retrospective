// 共用消息系统仍负责实际送达；这些字段没有授予接收人同意或条约生效。
#include "diplomacy_projection.hpp"
#include "diplomacy_delegation.hpp"
namespace hanmo::resident {
std::optional<DynamicFactView> diplomacy_dynamic_fact(const DiplomacyState& state,const StableId& id){
    DynamicFactView out;out.id=id;out.rule_version=state.version==2?"diplomacy_v2":"diplomacy_v1";out.publicly_visible=false;
    if(const auto* fact=state.facts.find(id)){out.kind=fact->kind;out.tick=fact->tick;out.subject_id=fact->offer_id;out.outcome=fact->kind;out.cause_fact_ids=fact->cause_fact_ids;for(const auto& actor:fact->witness_actor_ids)out.participants.push_back({{SubjectKind::actor,actor},actor==fact->actor_id?"decision_maker":"witness"});if(const auto* reply=state.replies.find(id))out.outcome=reply->decision==DiplomaticReplyKind::accept?"accepted_pending_ratification":"refused";return out;}
    if(const auto* mission=state.missions.find(id)){out.kind=diplomatic_mission_authority_fact_kind;out.tick=mission->issued_tick;out.subject_id=mission->proposal_id;out.outcome="mission_authorized";out.cause_fact_ids={mission->proposal_fact_id};out.participants.push_back({{SubjectKind::actor,mission->issuer_actor_id},"issuer"});return out;}
    if(state.v2){
        if(const auto* known=state.v2->mission_knowledge.find(id)){out.kind=diplomatic_mission_known_fact_kind;out.tick=known->tick;out.subject_id=known->mission_authority_fact_id;out.outcome="mission_communicated";out.cause_fact_ids={known->mission_authority_fact_id};out.participants={{{SubjectKind::actor,known->issuer_actor_id},"issuer"},{{SubjectKind::actor,known->envoy_actor_id},"recipient"}};return out;}
        if(const auto* record=state.v2->reports.find(id)){const auto& report=record->source;out.kind=diplomatic_envoy_report_fact_kind;out.tick=report.completed_tick;out.subject_id=report.task_id;out.outcome="report_received";out.cause_fact_ids={report.mission_authority_fact_id,report.presentation_fact_id,record->delivery_fact_id};out.participants={{{SubjectKind::actor,report.envoy_actor_id},"envoy"},{{SubjectKind::actor,report.receiver.actor_id},"receiver"}};return out;}
    }
    return {};
}
}
