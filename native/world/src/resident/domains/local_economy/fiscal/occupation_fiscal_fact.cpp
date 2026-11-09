// 动态事实只投影发生时原账；以后偿还不倒改旧收据，也不把暂停期说成已完成评税。
#include "occupation_fiscal_settlement.hpp"
#include "../local_economy_internal.hpp"
namespace hanmo::resident {
std::optional<DynamicFactView> occupation_fiscal_dynamic_fact(const OccupationFiscalSettlementState& state,const FiscalSettlementCatalog& catalog,const StableId& id){
 DynamicFactView out;out.id=id;out.rule_version="occupation_fiscal_settlement_v1";
 const auto* paused=state.suspended_periods.find(id);const auto* period=state.periods.find(id);if(paused)period=&paused->period;
 if(period){out.kind=paused?occupation_fiscal_suspension_kind:occupation_fiscal_period_kind;out.tick=period->due_tick;out.subject_id=period->policy.jurisdiction_id;out.participants={{{SubjectKind::organization,period->policy.organization_id},"principal"}};out.cause_fact_ids={period->legacy_period_id,period->policy.source_fact_id};out.outcome=paused?"taxpayer_assets_captured":"tax_assessed";if(paused)for(const auto& source:paused->sources)for(const auto* container:{&source.wallet,&source.inventory})for(const auto& fact:container->occupation_fact_ids)if(std::find(out.cause_fact_ids.begin(),out.cause_fact_ids.end(),fact)==out.cause_fact_ids.end())out.cause_fact_ids.push_back(fact);return out;}
 const auto* payment=state.payments.find(id);const auto* record=payment?state.assessments.find(payment->assessment_id):state.assessments.find(id);if(!record)return {};const auto& assessment=record->assessment;period=state.periods.find(assessment.period_id);const auto* district=period?catalog.districts.find(period->district_id):nullptr;const auto* source=district?district->sources.find(assessment.source_id):nullptr;if(!source)return {};
 out.kind=payment?occupation_fiscal_payment_kind:occupation_fiscal_assessment_kind;out.tick=payment?payment->tick:period->due_tick;out.subject_id=source->taxpayer.id;out.participants={{source->taxpayer,"taxpayer"},{{SubjectKind::organization,period->policy.organization_id},"principal"}};
 if(payment){Integer cash=0,grain=0;for(const auto& row:state.payments.rows()){if(row.second.assessment_id==assessment.id&&(row.second.cash_paid<0||row.second.grain_paid<0||!local_economy_detail::add(cash,row.second.cash_paid,cash)||!local_economy_detail::add(grain,row.second.grain_paid,grain)))return {};if(row.first==id)break;}out.outcome=cash==assessment.cash.original_due&&grain==assessment.grain.original_due?"tax_paid":"tax_partly_paid";out.cause_fact_ids={assessment.id};}
 else{out.outcome="tax_assessed";out.cause_fact_ids={period->id};out.cause_fact_ids.insert(out.cause_fact_ids.end(),assessment.production_receipt_ids.begin(),assessment.production_receipt_ids.end());}return out;
}
}
