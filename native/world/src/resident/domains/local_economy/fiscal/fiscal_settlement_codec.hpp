// 财政独立模块复用原严格字段访问器；所有snapshot、账期与双币种收据都逐字段保存。
#pragma once
#include "fiscal_settlement.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<FiscalTaxMode> {inline static constexpr std::pair<FiscalTaxMode,const char*> names[]={{FiscalTaxMode::lenient,"lenient"},{FiscalTaxMode::normal,"normal"},{FiscalTaxMode::surcharge,"surcharge"}};static constexpr const auto& rows(){return names;}};
// 政治来源字段完全复用S6只读合同，不保存一句作者字符串代替实际政策。
template<class A> void record(A& a,FiscalPolicySnapshot& v){a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("organization_id",v.organization_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("source_fact_id",v.source_fact_id);a("policy_revision",v.policy_revision);a("issued_tick",v.issued_tick);a("effective_from_tick",v.effective_from_tick);a("mode",v.mode);}
// 作者税源只有原容器、原生产者和明确单位，不另存可花余额。
template<class A> void record(A& a,FiscalTaxSource& v){a("id",v.id);a("taxpayer",v.taxpayer);a("wallet_id",v.wallet_id);a("inventory_id",v.inventory_id);a("grain_item_id",v.grain_item_id);a("production_unit_ids",v.production_unit_ids);a("base_cash_due",v.base_cash_due);a("grain_share_bp",v.grain_share_bp);a("civilian_grain_reserve",v.civilian_grain_reserve);}
template<class A> void record(A& a,FiscalDistrictProfile& v){a("id",v.id);a("site_id",v.site_id);a("period_ticks",v.period_ticks);a("first_due_tick",v.first_due_tick);a("initial_tax_base_bp",v.initial_tax_base_bp);a("normal_recovery_bp",v.normal_recovery_bp);a("sources",v.sources);}
template<class A> void record(A& a,FiscalSettlementCatalog& v){a("version",v.version);a("districts",v.districts);}
template<class A> void record(A& a,FiscalDistrictProgress& v){a("id",v.id);a("registration_fact_id",v.registration_fact_id);a("next_due_tick",v.next_due_tick);a("tax_base_bp",v.tax_base_bp);}
template<class A> void record(A& a,FiscalSourceRemainders& v){a("cash",v.cash);a("grain",v.grain);}
template<class A> void record(A& a,FiscalPeriodAssessment& v){a("id",v.id);a("district_id",v.district_id);a("registration_fact_id",v.registration_fact_id);a("period_start_tick",v.period_start_tick);a("due_tick",v.due_tick);a("tax_base_bp",v.tax_base_bp);a("policy",v.policy);a("source_assessment_ids",v.source_assessment_ids);}
template<class A> void record(A& a,FiscalSourceAssessment& v){a("id",v.id);a("period_id",v.period_id);a("source_id",v.source_id);a("wallet_id",v.wallet_id);a("inventory_id",v.inventory_id);a("grain_item_id",v.grain_item_id);a("owner",v.owner);a("produced_grain",v.produced_grain);a("civilian_grain_reserve",v.civilian_grain_reserve);a("cash_remainder_before",v.cash_remainder_before);a("grain_remainder_before",v.grain_remainder_before);a("cash_remainder_after",v.cash_remainder_after);a("grain_remainder_after",v.grain_remainder_after);a("production_receipt_ids",v.production_receipt_ids);a("cash",v.cash);a("grain",v.grain);}
template<class A> void record(A& a,FiscalPaymentReceipt& v){a("id",v.id);a("assessment_id",v.assessment_id);a("tick",v.tick);a("cash_paid",v.cash_paid);a("grain_paid",v.grain_paid);}
template<class A> void record(A& a,FiscalSettlementState& v){a("version",v.version);a("initialized_tick",v.initialized_tick);a("last_step_tick",v.last_step_tick);a("districts",v.districts);a("remainders",v.remainders);a("periods",v.periods);a("assessments",v.assessments);a("payments",v.payments);}
}
