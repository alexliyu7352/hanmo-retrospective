// 本叶只核新账与事实来源并导出快照；不计算应付、不收款、不更改旧Need。
#include "occupation_fiscal_debt_source.hpp"
#include "../economic_phase_clock.hpp"
#include "occupation_fiscal_responsibility.hpp"
#include "../local_economy_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
namespace {
using Observed=Outcome<std::vector<FiscalDebtObservation>>;
Observed fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
bool header(const CommonStore& c,const StableId& id,const StableId& kind){const auto* h=c.state().facts.find(id);auto n=organization_instance_sequence(id,kind);return h&&h->id==id&&h->domain=="local_economy"&&h->kind==kind&&n&&*n<c.state().next_instance_id;}
bool before(const CommonStore& c,const StableId& a,const StableId& b){bool seen=false;for(const auto& row:c.state().facts.rows()){if(row.first==b)return seen;if(row.first==a)seen=true;}return false;}
}
Observed observe_occupation_fiscal_debt_fact(const CommonStore& common,const LocalEconomyState& economy,const OccupationFiscalSettlementState& state,const FiscalSettlementCatalog& catalog,const StableId& id,const CurrentMomentRead* actual_clock){
 auto actual_tick=read_economic_calendar_tick(common,actual_clock);if(!actual_tick.ok)return {false,{},actual_tick.error};const Integer observed_tick=actual_tick.value;
 if(catalog.version!=1||state.version!=1)return fail("UnsupportedOccupationFiscalDebtVersion",id);
 if(state.initialized_tick<0||state.initialized_tick>observed_tick||state.last_step_tick!=observed_tick||economy.last_step_tick!=observed_tick)return fail("OccupationFiscalDebtClockMismatch",id);
 const auto* pause=state.suspended_periods.find(id);const auto* period=state.periods.find(id);if(pause&&period)return fail("InvalidOccupationFiscalDebtPeriod",id);if(pause)period=&pause->period;
 if(period){const auto* district=catalog.districts.find(period->district_id);if(!header(common,id,pause?occupation_fiscal_suspension_kind:occupation_fiscal_period_kind)||period->id!=id||!district||period->policy.site_id!=district->site_id||period->due_tick<state.initialized_tick||period->due_tick>observed_tick||period->period_start_tick!=period->due_tick-district->period_ticks||(pause&&!period->source_assessment_ids.empty()))return fail("InvalidOccupationFiscalDebtPeriod",id);return {true,{}, {}};}
 const auto* payment=state.payments.find(id);const auto* row=payment?state.assessments.find(payment->assessment_id):state.assessments.find(id);
 if(!row||!header(common,id,payment?occupation_fiscal_payment_kind:occupation_fiscal_assessment_kind)||(payment&&payment->id!=id)||(!payment&&row->assessment.id!=id))return fail("UnsupportedOccupationFiscalDebtSource",id);
 const auto& assessment=row->assessment;period=state.periods.find(assessment.period_id);if(!period)return fail("InvalidOccupationFiscalDebtPeriod",id);
 EconomicPublicAccountSelection selected{{Owner::Kind::organization,period->policy.organization_id,{}},{period->policy.public_wallet_id},{period->policy.public_inventory_id}};
 DynamicStepInput phase{common.tick(),observed_tick,{},{},{}};const auto* actual_phase=actual_clock&&observed_tick!=common.tick()?&phase:nullptr;auto original=read_occupation_fiscal_responsibility_source(common,economy,catalog,state,selected,{EconomicResponsibilityKind::fiscal_assessment,assessment.id,{}},actual_phase);if(!original.ok)return {false,{},original.error};
 const auto* district=catalog.districts.find(period->district_id);const auto* source=district?district->sources.find(assessment.source_id):nullptr;if(!source||!original.value.original_principal||original.value.original_beneficiaries.size()!=1)return fail("InvalidOccupationFiscalDebtSource",id);
 const Integer tick=payment?payment->tick:period->due_tick;if(tick<period->due_tick||tick>observed_tick)return fail("InvalidOccupationFiscalDebtTick",id);
 Integer cash=0,grain=0;
 if(payment){bool found=false;Integer previous=period->due_tick;for(const auto& entry:state.payments.rows()){const auto& p=entry.second;if(p.assessment_id==assessment.id){if(p.id!=entry.first||!header(common,p.id,occupation_fiscal_payment_kind)||!before(common,assessment.id,p.id)||p.tick<previous||p.tick>tick||p.cash_paid<0||p.grain_paid<0||(!p.cash_paid&&!p.grain_paid)||!local_economy_detail::add(cash,p.cash_paid,cash)||!local_economy_detail::add(grain,p.grain_paid,grain))return fail("InvalidOccupationFiscalDebtPayment",p.id);previous=p.tick;}if(entry.first==id){found=true;break;}}if(!found)return fail("OccupationFiscalDebtPaymentMissing",id);}
 std::vector<FiscalDebtObservation> out;
 for(const auto unit:{FiscalDebtUnit::cash,FiscalDebtUnit::grain}){const bool money=unit==FiscalDebtUnit::cash;const auto& debt=money?assessment.cash:assessment.grain;const Integer paid=money?cash:grain;const StableId suffix=money?".cash":".grain";
  if(debt.id!=assessment.id+suffix||debt.obligation_id!=source->id+suffix||debt.due_tick!=period->due_tick||debt.original_due<0||paid>debt.original_due||debt.paid_amount<paid||debt.paid_amount>debt.original_due)return fail("InvalidOccupationFiscalDebtAmount",id);
  FiscalDebtObservation v;v.source_fact_id=id;v.assessment_id=assessment.id;v.period_id=period->id;v.source_id=source->id;v.obligation_id=debt.obligation_id;v.arrear_id=debt.id;v.jurisdiction_id=period->policy.jurisdiction_id;v.site_id=period->policy.site_id;v.policy_fact_id=period->policy.source_fact_id;v.item_id=money?StableId{}:assessment.grain_item_id;v.taxpayer=*original.value.original_principal;v.beneficiary=original.value.original_beneficiaries.front();v.unit=unit;v.fact_tick=tick;v.due_tick=period->due_tick;v.due_amount=debt.original_due;v.paid_amount=paid;v.outstanding_amount=debt.original_due-paid;out.push_back(std::move(v));
 }
 return {true,std::move(out),{}};
}
}
