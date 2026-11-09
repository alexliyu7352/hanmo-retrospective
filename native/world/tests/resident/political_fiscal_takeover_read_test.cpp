// 税政只读必要门：旧当期来源保留，失控后新期为空，指定较旧/未来政策均拒。
#include "resident/domains/politics/politics_fiscal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_phase.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool ok,const char* what){++checks;if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
struct Boundary final:FiscalPolicyOccupationRead{const CommonStore& c;Integer loss=5;explicit Boundary(const CommonStore& c):c(c){}Outcome<std::optional<FiscalPolicyOccupationBoundary>> fiscal_policy_occupation(const FiscalPolicySnapshot&)const override{return {true,FiscalPolicyOccupationBoundary{c.state().identity,"military:3",loss},{}};}};
}
int main(){
    CommonState initial;initial.identity.tick=6;initial.next_instance_id=4;
    initial.facts.insert("politics_fiscal:1",{"politics_fiscal:1","political_fiscal_policy_changed","politics"});initial.facts.insert("politics_fiscal:2",{"politics_fiscal:2","political_fiscal_policy_changed","politics"});
    auto cc=std::make_shared<CommonCatalog>();PlaceDefinition place;place.id="town";cc->places.insert(place.id,place);CommonStore common(initial,cc);
    PoliticsState politics;Jurisdiction j;j.id="district";j.seat_place_id="town";j.political_organization_id="old";j.controlling_organization_id="new";j.public_wallet_id="treasury";j.public_inventory_ids={"granary"};politics.jurisdictions.insert(j.id,j);
    PoliticalFiscalState state;for(Integer n=1;n<=2;++n){FiscalPolicyRecord r;r.id="politics_fiscal:"+std::to_string(n);r.snapshot={"district","town","old","treasury","granary",r.id,n,n,n+1,FiscalTaxMode::normal};state.policies.insert(r.id,r);}
    PoliticalFiscalCatalog catalog;catalog.enabled=true;Boundary boundary(common);FiscalPolicyAccess read(common,*cc,politics,state,catalog,&boundary);
    auto old=read.fiscal_policy_for_due_tick("town",2);check(old.ok&&old.value&&old.value->source_fact_id=="politics_fiscal:1","old due picks exact old source");
    auto latest=read.fiscal_policy_for_assessment_source("town",4,"politics_fiscal:2");check(latest.ok&&latest.value,"frozen pre-loss policy readable");
    check(!read.fiscal_policy_for_assessment_source("town",4,"politics_fiscal:1").ok,"arbitrary earlier policy rejected");check(!read.fiscal_policy_for_assessment_source("town",2,"politics_fiscal:2").ok,"future source rejected");
    auto same=read.fiscal_policy_for_due_tick("town",5);check(same.ok&&!same.value,"new occupation-tick assessment has no policy");
    check(read.fiscal_policy_for_assessment_source("town",5,"politics_fiscal:2").ok,"same-tick saved source readable for S5 sequence proof");check(!read.fiscal_policy_for_assessment_source("town",6,"politics_fiscal:2").ok,"later historical policy rejected");
    check(!read.fiscal_policy_for_due_tick("town",7).ok,"cold future due rejected");
    DynamicStepInput phase{6,7,{},{},{}};FiscalPolicyAccess phased(common,*cc,politics,state,catalog,phase,&boundary);auto next=phased.fiscal_policy_for_due_tick("town",7);check(next.ok&&!next.value,"explicit same-step due returns no policy");
    check(political_takeover_evaluation_tick(common,nullptr).value==6,"political cold tick unchanged");check(political_takeover_evaluation_tick(common,&phase).value==7,"political phase to accepted");phase.to_tick=8;check(!political_takeover_evaluation_tick(common,&phase).ok,"two tick leap rejected");phase={5,6,{},{},{}};check(!political_takeover_evaluation_tick(common,&phase).ok,"stale from rejected");
    FiscalPolicyAccess cold(common,*cc,politics,state,catalog);check(!cold.fiscal_policy_for_due_tick("town",4).ok,"no typed occupation proof remains denied");
    std::cout<<"political fiscal takeover read: "<<checks<<" checks, 0 failures\n";
}
