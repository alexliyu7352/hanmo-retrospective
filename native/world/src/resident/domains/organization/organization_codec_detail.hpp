// 组织冷字段合同与地方经济共用严格编解码工具，不保留任意 JSON 载荷。
#pragma once
#include "organization_model.hpp"
#include "named_membership_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<OrganizationStatus> { static std::vector<std::pair<OrganizationStatus,StableId>> rows() { return {{OrganizationStatus::active,"active"},{OrganizationStatus::vacant,"vacant"},{OrganizationStatus::limited,"limited"},{OrganizationStatus::dissolved,"dissolved"}}; } };
template<> struct EnumNames<OfficeStatus> { static std::vector<std::pair<OfficeStatus,StableId>> rows() { return {{OfficeStatus::active,"active"},{OfficeStatus::vacant,"vacant"},{OfficeStatus::invalidated,"invalidated"}}; } };
template<> struct EnumNames<MemberResponse> { static std::vector<std::pair<MemberResponse,StableId>> rows() { return {{MemberResponse::serving,"serving"},{MemberResponse::payment_requested,"payment_requested"},{MemberResponse::refusing_new_duties,"refusing_new_duties"},{MemberResponse::awaiting_release,"awaiting_release"},{MemberResponse::departed,"departed"}}; } };
template<> struct EnumNames<ParticipationKind> { static std::vector<std::pair<ParticipationKind,StableId>> rows() { return {{ParticipationKind::administrative_reference,"administrative_reference"},{ParticipationKind::personal_action,"personal_action"},{ParticipationKind::cohort_allocation,"cohort_allocation"}}; } };
template<> struct EnumNames<OrgActivityStatus> { static std::vector<std::pair<OrgActivityStatus,StableId>> rows() { return {{OrgActivityStatus::approved,"approved"},{OrgActivityStatus::active,"active"},{OrgActivityStatus::waiting,"waiting"},{OrgActivityStatus::completed,"completed"},{OrgActivityStatus::cancelled,"cancelled"},{OrgActivityStatus::failed,"failed"}}; } };
template<> struct EnumNames<OrgActivityKind> { static std::vector<std::pair<OrgActivityKind,StableId>> rows() { return {{OrgActivityKind::maintain_duty,"maintain_duty"},{OrgActivityKind::population_training,"population_training"},{OrgActivityKind::contracted_work,"contracted_work"}}; } };
template<> struct EnumNames<ExecutionMode> { static std::vector<std::pair<ExecutionMode,StableId>> rows() { return {{ExecutionMode::background,"background"},{ExecutionMode::local,"local"}}; } };
// 遍历 OrganizationRecord 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrganizationRecord& v) { a("id",v.id);a("kind",v.kind);a("status",v.status);a("administrative_parent",v.administrative_parent);a("branch_parent",v.branch_parent);// legacy2/3仍严格plain非空string；同一模型槽，不降级absent为""。
    bool reading=false;if constexpr(std::is_same_v<A,Decoder>)reading=true;else if constexpr(std::is_same_v<A,Checkpointer>)reading=a.codec.reading();
    if(!reading&&(!v.headquarters_site||v.headquarters_site->empty())){if constexpr(std::is_same_v<A,Checkpointer>){a.codec.reject("LegacyOrganizationHeadquartersRequired",v.id);return;}else boundary::fail("LegacyOrganizationHeadquartersRequired",v.id);}
    if constexpr(std::is_same_v<A,Projector>){a("headquarters_site",*v.headquarters_site);}
    else{StableId legacy_seat=reading?StableId{}:*v.headquarters_site;a("headquarters_site",legacy_seat);
    if(legacy_seat.empty()){if constexpr(std::is_same_v<A,Checkpointer>){a.codec.reject("LegacyOrganizationHeadquartersRequired",v.id);return;}else boundary::fail("LegacyOrganizationHeadquartersRequired",v.id);}if(reading)v.headquarters_site=std::move(legacy_seat);}a("policy_profile_id",v.policy_profile_id);a("office_ids",v.office_ids);a("succession_policy_id",v.succession_policy_id); }
// 遍历 OfficeState 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OfficeState& v) { a("id",v.id);a("organization_id",v.organization_id);a("holder_actor_id",v.holder_actor_id);a("deputy_actor_id",v.deputy_actor_id);a("epoch",v.epoch);a("status",v.status);a("grant_ids",v.grant_ids); }
// 遍历 Grant 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,Grant& v) { a("id",v.id);a("office_id",v.office_id);a("allowed_command_kinds",v.allowed_command_kinds);a("scopes",v.scopes);a("wallet_ids",v.wallet_ids);a("inventory_ids",v.inventory_ids);a("amount_limit",v.amount_limit);a("valid_from",v.valid_from);a("valid_until",v.valid_until);a("may_delegate",v.may_delegate);a("continuity_policy_id",v.continuity_policy_id); }
// 遍历 InstitutionalIssuer 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,InstitutionalIssuer& v) { a("id",v.id);a("organization_id",v.organization_id);a("continuity_policy_id",v.continuity_policy_id);a("obligation_ids",v.obligation_ids);a("source_container_ids",v.source_container_ids);a("amount_limit",v.amount_limit); }
// 遍历 MemberPolicy 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,MemberPolicy& v) { a("id",v.id);a("request_after_ticks",v.request_after_ticks);a("refuse_after_ticks",v.refuse_after_ticks);a("leave_after_ticks",v.leave_after_ticks);a("minimum_arrears",v.minimum_arrears);a("may_leave",v.may_leave); }
// 遍历 OrgDuty 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrgDuty& v) { a("id",v.id);a("organization_id",v.organization_id);a("actor_id",v.actor_id);a("role_id",v.role_id);a("office_id",v.office_id);a("policy_id",v.policy_id);a("obligation_ids",v.obligation_ids);a("commitment_ids",v.commitment_ids);a("active",v.active);a("activity_terms_id",v.activity_terms_id); }
// 遍历 OrgMemberState 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrgMemberState& v) { a("id",v.id);a("organization_id",v.organization_id);a("actor_id",v.actor_id);a("policy_id",v.policy_id);a("response",v.response);a("response_since_tick",v.response_since_tick);a("next_review_tick",v.next_review_tick);a("last_fact_id",v.last_fact_id);a("duty_ids",v.duty_ids);if(a.named_membership)a("episode_source",v.episode_source); }
// 遍历 Participation 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,Participation& v) { a("subject",v.subject);a("role",v.role);a("binding_kind",v.binding_kind);a("binding_id",v.binding_id); }
// 遍历 ExecutionBinding 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ExecutionBinding& v) { a("activity_id",v.activity_id);a("phase",v.phase);a("mode",v.mode);a("execution_epoch",v.execution_epoch);a("local_session_id",v.local_session_id);a("last_receipt_id",v.last_receipt_id); }
// 遍历 OrgActivity 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrgActivity& v) { a("id",v.id);a("kind",v.kind);a("principal",v.principal);a("responsible_office_id",v.responsible_office_id);a("executors",v.executors);a("commitment_ids",v.commitment_ids);a("started_tick",v.started_tick);a("phase_started_tick",v.phase_started_tick);a("due_tick",v.due_tick);a("outcome_sequence",v.outcome_sequence);a("phase",v.phase);a("terms_id",v.terms_id);a("status",v.status);a("result_fact_ids",v.result_fact_ids);a("participation",v.participation);a("execution",v.execution);a("result_code",v.result_code); }
// 遍历 OrgActivityTerms 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrgActivityTerms& v) { a("id",v.id);a("kind",v.kind);a("duration_ticks",v.duration_ticks);a("training_capacity",v.training_capacity);a("training_site_id",v.training_site_id);a("continuity_policy_id",v.continuity_policy_id);a("consenting_cohort_ids",v.consenting_cohort_ids); }
// 遍历 OrganizationFact 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrganizationFact& v) { a("id",v.id);a("kind",v.kind);a("organization_id",v.organization_id);a("actor_id",v.actor_id);a("office_id",v.office_id);a("activity_id",v.activity_id);a("tick",v.tick);a("amount",v.amount);a("cause_fact_ids",v.cause_fact_ids);a("response",v.response);a("code",v.code); }
// 作者目录成员只含静态seed字段；运行episode_source仅由OrganizationState的严格布局持有。
template<class A> void record(A& a,OrganizationCatalog& v) {
    a("enabled",v.enabled);a("organizations",v.organizations);a("offices",v.offices);a("grants",v.grants);a("institutional_issuers",v.institutional_issuers);a("member_policies",v.member_policies);a("duties",v.duties);
    const bool named_profile=a.named_membership;a.named_membership=false;a("members",v.members);a.named_membership=named_profile;
    a("activity_terms",v.activity_terms);a("initial_activities",v.initial_activities);if(a.named_membership){a("named_membership_enabled",v.named_membership_enabled);a("membership_terms",v.membership_terms);a("named_membership_policies",v.named_membership_policies);}
}
// 遍历 OrganizationState 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,OrganizationState& v) { a("organizations",v.organizations);a("offices",v.offices);a("duties",v.duties);a("members",v.members);a("activities",v.activities);a("facts",v.facts);if(a.named_membership){a("membership_offers",v.membership_offers);a("membership_decisions",v.membership_decisions);} }
}
