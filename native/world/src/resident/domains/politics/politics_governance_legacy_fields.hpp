// 原/1字段表唯一保留；显式/2只在该四根之后追加真实治安来源。
#pragma once
#include "politics_governance.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 费用与时长全由明确作者规则给定，不在任务接单时补猜测值。
template<class A> void record(A& a,AppeasementRule& v){a("id",v.id);a("version",v.version);a("name_key",v.name_key);a("grain_item_id",v.grain_item_id);a("duration_ticks",v.duration_ticks);a("cash_amount",v.cash_amount);a("grain_amount",v.grain_amount);a("base_support_gain",v.base_support_gain);a("support_cap",v.support_cap);}
// 完成时的技能快照在待结算期间保持不变，原XP和级别可相互核验。
template<class A> void record(A& a,GovernanceOutcomePlan& v){a("rule_id",v.rule_id);a("rule_version",v.rule_version);a("executor_actor_id",v.executor_actor_id);a("work_completed_tick",v.work_completed_tick);a("rites_xp",v.rites_xp);a("rites_level",v.rites_level);a("support_delta",v.support_delta);a("support_cap",v.support_cap);}
// 时间窗与受领分配策略跟随同一作者策略，任务不从玩家数值补造条款。
template<class A> void record(A& a,GovernanceCommissionPolicy& v){a("id",v.id);a("rule_id",v.rule_id);a("relief_policy_id",v.relief_policy_id);a("offer_valid_ticks",v.offer_valid_ticks);a("completion_deadline_ticks",v.completion_deadline_ticks);}
// 批准冻结原公权和公库，不保存另一份可花余额或Task状态。
template<class A> void record(A& a,GovernanceAuthorityRecord& v){a("id",v.id);a("policy_id",v.policy_id);a("issuer_actor_id",v.issuer_actor_id);a("office_id",v.office_id);a("grant_id",v.grant_id);a("organization_id",v.organization_id);a("executor_actor_id",v.executor_actor_id);a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("relief_policy_id",v.relief_policy_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("continuity_policy_id",v.continuity_policy_id);a("authority_epoch",v.authority_epoch);a("jurisdiction_revision",v.jurisdiction_revision);a("issued_tick",v.issued_tick);a("offer_expires_tick",v.offer_expires_tick);a("completion_deadline_ticks",v.completion_deadline_ticks);a("rule",v.rule);}
// 工作事实与冻结结果是唯一绑定，Task域仅保存这个记录ID。
template<class A> void record(A& a,RecordedGovernancePlan& v){a("id",v.id);a("authority_fact_id",v.authority_fact_id);a("work_fact_id",v.work_fact_id);a("task_id",v.task_id);a("contract_id",v.contract_id);a("action_id",v.action_id);a("value",v.value);}
// 民心只沿有来源的后果更新，恢复不根据当前税政重算。
template<class A> void record(A& a,JurisdictionCivilState& v){a("id",v.id);a("registration_fact_id",v.registration_fact_id);a("last_fact_id",v.last_fact_id);a("support",v.support);a("revision",v.revision);}
// 前后值与上条来源同时保存，重试不能把同一工作或税期再施加一次。
template<class A> void record(A& a,CivilConsequence& v){a("id",v.id);a("jurisdiction_id",v.jurisdiction_id);a("source_domain",v.source_domain);a("source_fact_id",v.source_fact_id);a("plan_id",v.plan_id);a("previous_fact_id",v.previous_fact_id);a("tick",v.tick);a("before",v.before);a("after",v.after);a("requested_delta",v.requested_delta);a("revision",v.revision);}
template<class A> void record(A& a,PoliticsGovernanceState& v){a("authorities",v.authorities);a("plans",v.plans);a("civil",v.civil);a("consequences",v.consequences);}
template<class A> void record(A& a,PoliticsGovernanceCatalog& v){a("enabled",v.enabled);a("appeasement_rules",v.appeasement_rules);a("commission_policies",v.commission_policies);a("initial_support",v.initial_support);}
}
