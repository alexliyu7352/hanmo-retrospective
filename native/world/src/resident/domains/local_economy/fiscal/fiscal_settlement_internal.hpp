// 本域新旧财政复用同一纳税来源、真实产粮和Common转账实现，不复制第二套经济账。
#pragma once
#include "fiscal_settlement.hpp"
namespace hanmo::resident::fiscal_settlement_detail {
Outcome<Owner> taxpayer_owner(const CommonStore&,const LocalEconomyState&,const OrganizationRead&,const FiscalTaxSource&,const StableId& site,const EconomicTakeoverReadContext*);
Outcome<Integer> produced_basis(const CommonStore&,const LocalEconomyState&,const FiscalTaxSource&,Integer from,Integer through,std::vector<StableId>& receipts);
Integer following_tax_base(const FiscalDistrictProfile&,Integer current,FiscalTaxMode);
// 仅新段固定事实名的支付入口；原财政仍只生成fiscal_payment。
Status collect_occupation_payment(CommonStore&,OrderedTable<FiscalPaymentReceipt>&,FiscalSourceAssessment&,const FiscalPolicySnapshot&,Integer tick,LocalEconomyEffects&);
}
