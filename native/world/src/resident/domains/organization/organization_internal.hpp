#include "organization_service_effect_capture.hpp"
// 组织私有查询只借实际任职、行动和应付款；不保存第二份忙碌或工资总账。
#pragma once
#include "organization_model.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
namespace hanmo::resident::organization_detail {
using local_economy_detail::has;
using local_economy_detail::add;
// 伤病、死亡与不可用身份立即阻止新授权；个人忙碌不撤销纯行政身份。
inline bool eligible_officer(const CommonStore& common,const StableId& id){const auto* actor=common.state().actors.find(id);return actor&&actor->alive&&actor->available&&actor->injury==0;}
inline bool active_activity(const OrgActivity& activity){return activity.status==OrgActivityStatus::approved||activity.status==OrgActivityStatus::active||activity.status==OrgActivityStatus::waiting;}
// 仅实际仍存在的个人行动保留旧服务身份，未完成的组织行政记录不冒充在教课程。
inline bool has_personal_action(const CommonStore& common,const StableId& actor){const auto* person=common.actor(actor);if(!person)return false;if(!person->action_id.empty()&&common.action(person->action_id))return true;for(const auto& row:common.state().actions.rows())if(row.second.actor_id==actor||has(row.second.participants,actor))return true;return false;}
// 终态参与者来自行动owner的不可变条款；存在真实人物不等于参加过该行动。
inline Status terminal_participant(const CommonStore& common,const DynamicActionResult& result,const SubjectRef& bound){std::set<StableId> actors;if(result.participant_actor_ids.empty())return common_failure("InvalidOrganizationActionParticipants",result.action_id);for(const auto& id:result.participant_actor_ids)if(id.empty()||!common.actor(id)||!actors.insert(id).second)return common_failure("InvalidOrganizationActionParticipants",result.action_id);if(bound.kind!=SubjectKind::actor||!actors.count(bound.id))return common_failure("OrganizationActionParticipantMismatch",bound.id);return common_success();}
// 执行者与参与绑定描述同一组主体，不能只改其中一份引用伪造历史职责。
inline bool matching_executors(const OrgActivity& activity){std::set<std::pair<SubjectKind,StableId>> executors,participants;for(const auto& subject:activity.executors)if(subject.id.empty()||!executors.insert({subject.kind,subject.id}).second)return false;for(const auto& participation:activity.participation){if(participation.subject.id.empty())return false;participants.insert({participation.subject.kind,participation.subject.id});}return executors==participants;}
// 真实主行动、参与者和受控会话都算未清承诺，离开不能直接清空它们。
inline bool personally_committed(const CommonStore& common,const OrganizationState& state,const ActivityRead* access,const StableId& actor){
    const auto* person=common.actor(actor);if(!person)return false;if(!person->action_id.empty())return true;
    for(const auto& row:common.state().actions.rows())if(row.second.actor_id==actor||has(row.second.participants,actor))return true;
    if(access&&(access->in_open_battle(actor)||access->in_interaction(actor)))return true;
    for(const auto& row:state.activities.rows())if(active_activity(row.second))for(const auto& p:row.second.participation)if(p.subject.kind==SubjectKind::actor&&p.subject.id==actor&&p.binding_kind!=ParticipationKind::administrative_reference)return true;
    return false;
}
// 服务开关仍由真实职责/旧合同owner判断，新旧成员共用唯一实现。
Status set_member_services(CommonStore&,OrganizationState&,const ActivityRead&,OrgMemberState&,bool enabled,bool departing,OrganizationServiceEffectsBuildView* capture=nullptr);
Status attach_organization_service_effects_in_candidate(OrganizationState&,const OrganizationCatalog&,const StableId& actual_parent,const OrganizationServiceEffectsBuildView&);
struct MemberDebt { Integer amount=0,earliest_due=-1,periods=0;std::vector<StableId> fact_ids; };
// 只汇总本成员职责指定、确实付给本人的义务；不可读取另一人的私账。
inline Outcome<MemberDebt> debt(const CommonStore& common,const OrganizationState& state,const LocalEconomyState& economy,const OrgMemberState& member){
    MemberDebt out;std::set<StableId> obligations;
    for(const auto& id:member.duty_ids){const auto* duty=state.duties.find(id);if(!duty||duty->actor_id!=member.actor_id||duty->organization_id!=member.organization_id)return {false,{},common_failure("InvalidMemberDuty",id).error};obligations.insert(duty->obligation_ids.begin(),duty->obligation_ids.end());}
    const auto* actor=common.actor(member.actor_id);if(!actor)return {false,{},common_failure("UnknownActor",member.actor_id).error};
    for(const auto& row:economy.arrears.rows()){const auto& due=row.second;if(!obligations.count(due.obligation_id)||due.original_due==due.paid_amount)continue;const auto* obligation=economy.obligations.find(due.obligation_id);if(!obligation||obligation->kind==ObligationKind::ration||obligation->principal.id!=member.organization_id||obligation->beneficiary.kind!=SubjectKind::actor||obligation->beneficiary.id!=member.actor_id||obligation->destination!=actor->wallet_id)return {false,{},common_failure("MemberObligationMismatch",due.obligation_id).error};if(due.original_due<due.paid_amount||!add(out.amount,due.original_due-due.paid_amount,out.amount))return {false,{},common_failure("InvalidArrearAmount",due.id).error};out.earliest_due=out.earliest_due<0?due.due_tick:std::min(out.earliest_due,due.due_tick);++out.periods;out.fact_ids.push_back(due.id);}
    return {true,std::move(out),{}};
}
inline StableId response_name(MemberResponse response){switch(response){case MemberResponse::serving:return "serving";case MemberResponse::payment_requested:return "payment_requested";case MemberResponse::refusing_new_duties:return "refusing_new_duties";case MemberResponse::awaiting_release:return "awaiting_release";case MemberResponse::departed:return "departed";}return {};}
} // 私有辅助不向未知观察者返回余额。
