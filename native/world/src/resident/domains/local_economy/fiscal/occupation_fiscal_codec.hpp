// 独立新财政段逐字段严格保存；原财政1记录/枚举编码仍原样复用。
#pragma once
#include "occupation_fiscal_settlement.hpp"
#include "fiscal_settlement_codec.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<FiscalIncomeAccountKind>{inline static constexpr std::pair<FiscalIncomeAccountKind,const char*> names[]={{FiscalIncomeAccountKind::cash,"cash"},{FiscalIncomeAccountKind::grain,"grain"}};static constexpr const auto& rows(){return names;}};
template<class A> void record(A& a,FiscalIncomeRemainder& v){a("coarse",v.coarse);a("fine",v.fine);}
template<class A> void record(A& a,FiscalIncomeBucketKey& v){a("district_id",v.district_id);a("source_id",v.source_id);a("organization_id",v.organization_id);a("public_account_id",v.public_account_id);a("kind",v.kind);}
template<class A> void record(A& a,FiscalIncomeBucket& v){a("key",v.key);a("remainder",v.remainder);}
template<class A> void record(A& a,PoliticalOccupationRuleSource& v){a("rule_id",v.rule_id);a("rule_version",v.rule_version);a("integration_ticks",v.integration_ticks);a("income_basis_points",v.income_basis_points);}
template<class A> void record(A& a,OccupationFiscalPeriodAssessment& v){a("id",v.id);a("legacy_period_id",v.legacy_period_id);a("district_id",v.district_id);a("period_start_tick",v.period_start_tick);a("due_tick",v.due_tick);a("tax_base_bp",v.tax_base_bp);a("policy",v.policy);a("registration_fact_id",v.registration_fact_id);a("occupation_fact_id",v.occupation_fact_id);a("occupation_rule",v.occupation_rule);a("occupation_tick",v.occupation_tick);a("integration_until_tick",v.integration_until_tick);a("income_basis_points",v.income_basis_points);a("source_assessment_ids",v.source_assessment_ids);}
template<class A> void record(A& a,OccupationFiscalSourceAssessment& v){a("assessment",v.assessment);a("cash_fine_before",v.cash_fine_before);a("grain_fine_before",v.grain_fine_before);a("cash_fine_after",v.cash_fine_after);a("grain_fine_after",v.grain_fine_after);}
template<class A> void record(A& a,OccupationFiscalLegacyCarryClaim& v){a("key",v.key);a("legacy_period_id",v.legacy_period_id);a("legacy_assessment_id",v.legacy_assessment_id);a("legacy_policy_fact_id",v.legacy_policy_fact_id);a("claim_period_id",v.claim_period_id);a("remainder",v.remainder);}
template<class A> void record(A& a,OccupationFiscalTaxpayerContainer& v){a("id",v.id);a("owner",v.owner);a("occupation_fact_ids",v.occupation_fact_ids);}
template<class A> void record(A& a,OccupationFiscalTaxSourcePause& v){a("source_id",v.source_id);a("original_taxpayer",v.original_taxpayer);a("wallet",v.wallet);a("inventory",v.inventory);}
template<class A> void record(A& a,OccupationFiscalPeriodSuspension& v){a("period",v.period);a("sources",v.sources);}
template<class A> void record(A& a,OccupationFiscalSettlementState& v){a("version",v.version);a("initialized_tick",v.initialized_tick);a("last_step_tick",v.last_step_tick);a("buckets",v.buckets);a("legacy_carry_claims",v.legacy_carry_claims);a("periods",v.periods);a("suspended_periods",v.suspended_periods);a("assessments",v.assessments);a("payments",v.payments);}
}
