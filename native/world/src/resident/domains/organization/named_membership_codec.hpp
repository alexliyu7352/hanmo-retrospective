// 新profile的具名成员字段合同；旧布局不遍历这些字段。
#pragma once
#include "named_membership_model.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<MembershipOfferStatus> { static std::vector<std::pair<MembershipOfferStatus,StableId>> rows(){return {{MembershipOfferStatus::pending,"pending"},{MembershipOfferStatus::accepted,"accepted"},{MembershipOfferStatus::declined,"declined"},{MembershipOfferStatus::withdrawn,"withdrawn"},{MembershipOfferStatus::expired,"expired"}};} };
template<> struct EnumNames<MembershipDecisionKind> { static std::vector<std::pair<MembershipDecisionKind,StableId>> rows(){return {{MembershipDecisionKind::accept,"accept"},{MembershipDecisionKind::decline,"decline"},{MembershipDecisionKind::defer,"defer"}};} };
// MembershipTerms逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MembershipTerms& v) { a("id",v.id);a("organization_id",v.organization_id);a("duty_kind",v.duty_kind);a("site_id",v.site_id);a("member_policy_id",v.member_policy_id);a("payer_wallet_id",v.payer_wallet_id);a("wage_rate_profile_id",v.wage_rate_profile_id);a("wage_period_ticks",v.wage_period_ticks);a("offer_duration_ticks",v.offer_duration_ticks);a("continuity_policy_id",v.continuity_policy_id);a("terminal_fraction_policy_id",v.terminal_fraction_policy_id); }
// NamedMembershipPolicy逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,NamedMembershipPolicy& v) { a("id",v.id);a("allowed_duty_kinds",v.allowed_duty_kinds);a("minimum_wage_numerator",v.minimum_wage_numerator);a("minimum_wage_denominator",v.minimum_wage_denominator);a("minimum_wage_period_ticks",v.minimum_wage_period_ticks);a("review_interval_ticks",v.review_interval_ticks);a("decline_known_arrears",v.decline_known_arrears); }
// MembershipOffer逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MembershipOffer& v) { a("id",v.id);a("organization_id",v.organization_id);a("issuer_actor_id",v.issuer_actor_id);a("office_id",v.office_id);a("grant_id",v.grant_id);a("target_actor_id",v.target_actor_id);a("terms_id",v.terms_id);a("authority_epoch",v.authority_epoch);a("created_tick",v.created_tick);a("expires_tick",v.expires_tick);a("revision",v.revision);a("status",v.status);a("source_fact_id",v.source_fact_id);a("decision_fact_id",v.decision_fact_id);a("member_episode_id",v.member_episode_id); }
// MembershipDecision逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MembershipDecision& v) { a("id",v.id);a("offer_id",v.offer_id);a("target_actor_id",v.target_actor_id);a("offer_revision",v.offer_revision);a("kind",v.kind);a("reason",v.reason);a("source_fact_ids",v.source_fact_ids);a("tick",v.tick);a("decision_seq",v.decision_seq);a("next_review_tick",v.next_review_tick); }
// MemberEpisodeSource逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MemberEpisodeSource& v) { a("offer_id",v.offer_id);a("decision_id",v.decision_id);a("terms_id",v.terms_id);a("joined_fact_id",v.joined_fact_id);a("wage_binding_id",v.wage_binding_id);a("joined_tick",v.joined_tick);a("departed_tick",v.departed_tick);a("departure_fact_id",v.departure_fact_id); }
}
