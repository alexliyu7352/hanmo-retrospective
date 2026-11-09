// 只核真实产品分流函数；旧Need严格观察、完整World事务另沿同代风险门。
#include "resident/kernel/occupation_fiscal_integration.hpp"
#include <iostream>
using namespace hanmo::resident;
// 有效新账四类不冒旧债，未知源保持原序；坏新源不得静默漏过。
int main(){
 int checks=0;auto check=[&](bool ok){++checks;if(!ok){std::cerr<<"failed routing check "<<checks<<'\n';std::exit(1);}};
 OrderedTable<FactRecord> facts;OccupationFiscalSettlementState state;
 facts.insert("old",{"old","fiscal_period_assessed","local_economy"});facts.insert("unknown",{"unknown","unknown_fiscal_kind","local_economy"});
 facts.insert("p",{"p",occupation_fiscal_period_kind,"local_economy"});OccupationFiscalPeriodAssessment p;p.id="p";state.periods.insert("p",p);
 facts.insert("a",{"a",occupation_fiscal_assessment_kind,"local_economy"});OccupationFiscalSourceAssessment a;a.assessment.id="a";state.assessments.insert("a",a);
 facts.insert("paid",{"paid",occupation_fiscal_payment_kind,"local_economy"});FiscalPaymentReceipt paid;paid.id="paid";state.payments.insert("paid",paid);
 facts.insert("pause",{"pause",occupation_fiscal_suspension_kind,"local_economy"});OccupationFiscalPeriodSuspension pause;pause.period.id="pause";state.suspended_periods.insert("pause",pause);
 const std::vector<StableId> input={"old","p","a","paid","pause","unknown","missing"};auto out=legacy_fiscal_need_sources(facts,state,input);check(out.ok&&out.value==std::vector<StableId>({"old","unknown","missing"}));check(input.size()==7);
 for(const auto& id:std::vector<StableId>{"p","a","paid","pause"}){check(legacy_fiscal_need_sources(facts,state,{id}).ok);check(!legacy_fiscal_need_sources(facts,OccupationFiscalSettlementState{},{id}).ok);auto bad=facts;bad.find(id)->domain="wrong";check(!legacy_fiscal_need_sources(bad,state,{id}).ok);bad=facts;bad.find(id)->id="wrong";check(!legacy_fiscal_need_sources(bad,state,{id}).ok);}
 state.periods.find("p")->id="other";check(!legacy_fiscal_need_sources(facts,state,{"p"}).ok);state.periods.find("p")->id="p";paid.id="p";state.payments.insert("p",paid);check(!legacy_fiscal_need_sources(facts,state,{"p"}).ok);
 std::cout<<"fiscal need routing checks="<<checks<<" failures=0\n";
}
