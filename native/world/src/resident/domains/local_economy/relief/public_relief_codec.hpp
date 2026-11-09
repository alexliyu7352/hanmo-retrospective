// 独立public_relief/1只编码责任与收据，不复制钱包余额或任务生命周期。
#pragma once
#include "public_relief_rules.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 严格逐字段保存HouseholdReliefRecipient，未知字段由共同访问器拒绝。
template<class A> void record(A& a,HouseholdReliefRecipient& v){a("household_group_id",v.household_group_id);a("cohort_id",v.cohort_id);a("cash",v.cash);a("grain",v.grain);}
// 严格逐字段保存PublicReliefTerms，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefTerms& v){a("id",v.id);a("task_id",v.task_id);a("contract_id",v.contract_id);a("acceptance_fact_id",v.acceptance_fact_id);a("authority_source_fact_id",v.authority_source_fact_id);a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("organization_id",v.organization_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("item_id",v.item_id);a("reservation_id",v.reservation_id);a("total_cash",v.total_cash);a("total_grain",v.total_grain);a("recipients",v.recipients);}
// 严格逐字段保存HouseholdReliefTransfer，未知字段由共同访问器拒绝。
template<class A> void record(A& a,HouseholdReliefTransfer& v){a("household_group_id",v.household_group_id);a("cohort_id",v.cohort_id);a("wallet_id",v.wallet_id);a("inventory_id",v.inventory_id);a("cash",v.cash);a("grain",v.grain);}
// 严格逐字段保存PublicReliefReceipt，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefReceipt& v){a("id",v.id);a("terms_id",v.terms_id);a("task_id",v.task_id);a("contract_id",v.contract_id);a("action_id",v.action_id);a("acceptance_fact_id",v.acceptance_fact_id);a("authority_source_fact_id",v.authority_source_fact_id);a("work_fact_id",v.work_fact_id);a("tick",v.tick);a("cash_transferred",v.cash_transferred);a("grain_transferred",v.grain_transferred);a("recipients",v.recipients);}
// 严格逐字段保存PublicReliefPolicy，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefPolicy& v){a("id",v.id);a("site_id",v.site_id);a("organization_id",v.organization_id);a("item_id",v.item_id);a("recipients",v.recipients);}
// 严格逐字段保存PublicReliefCatalog，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefCatalog& v){a("version",v.version);a("policies",v.policies);}
// 严格逐字段保存PublicReliefCommitment，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefCommitment& v){a("terms",v.terms);a("policy_id",v.policy_id);a("reserved_tick",v.reserved_tick);a("transfers",v.transfers);}
// 严格逐字段保存PublicReliefRelease，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefRelease& v){a("id",v.id);a("terms_id",v.terms_id);a("tick",v.tick);}
// 严格逐字段保存PublicReliefState，未知字段由共同访问器拒绝。
template<class A> void record(A& a,PublicReliefState& v){a("version",v.version);a("commitments",v.commitments);a("receipts",v.receipts);a("releases",v.releases);}
}
