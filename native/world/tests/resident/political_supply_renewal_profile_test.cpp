// 必要profile门直接调用原Director目录递归；旧调用/旧检查点不会因新模板自动获权。
#include "resident/domains/politics/supply/politics_supply_renewal_validation.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include <iostream>
using namespace hanmo::resident;
int main(){int checks=0,failed=0;auto check=[&](bool yes,const char* label){++checks;if(!yes){++failed;std::cerr<<label<<'\n';}};
 auto parsed=parse_boundary_json(R"({"registry_version":1,"review_interval_ticks":2,"retry_delay_ticks":2,"decision_budget":4,"candidates_per_subject":4,"policies":{"north":{"organization_id":"new","templates":["political_supply_renewal"],"request_arrear_after_ticks":1,"decline_work_after_ticks":2,"leave_after_ticks":4,"supply_renewal_intent_id":"renew"}},"admissions":{"north":{"kind":"organization_members","policy_id":"north","organization_id":"new"}},"political_supply_renewals":{"version":1,"intents":{"renew":{"id":"renew","takeover_rule_id":"rule","author_policy_id":"food"}}}})");
 check(parsed.ok,"fixture parsed");if(!parsed.ok)return 1;auto value=parsed.value;
 const auto decode=[&](const BoundaryValue& v,bool pair){return decode_director_catalog(v,false,false,true,false,1,false,DirectorMilitaryAppointmentProfile{},DirectorSupplyRenewalProfile{pair});};
 auto current=decode(value,true);check(current.ok,"explicit pair survives actual registry recursion");if(!current.ok){std::cerr<<current.error.original.code<<'\n';return 1;}
 check(current.value.dynamic_subjects&&current.value.supply_renewals&&current.value.policies.find("north")->supply_renewal_intent_id==std::optional<StableId>{"renew"},"explicit intent is preserved in actual policy");
 check(!decode(value,false).ok,"pair missing rejected");
 check(!decode_director_catalog(value,false,false,true,false,1,false,DirectorMilitaryAppointmentProfile{}).ok,"old eight-argument entry remains strict disabled");
 auto bad=value;bad.as<BoundaryValue::Dict>()->push_back({"political_supply_renewals",*value.find("political_supply_renewals")});check(!decode(bad,true).ok,"direct duplicate profile root rejected");
 bad=value;bad.find("policies")->find("north")->as<BoundaryValue::Dict>()->push_back({"organization_id",BoundaryValue{"foreign"}});check(!decode(bad,true).ok,"duplicate nested policy authority scope rejected");
 bad=value;bad.find("political_supply_renewals")->find("version")->data=Integer{2};check(!decode(bad,true).ok,"unknown renewal version rejected");
 bad=value;bad.find("policies")->find("north")->find("supply_renewal_intent_id")->data=StableId{"missing"};check(!decode(bad,true).ok,"unknown explicit intent rejected");
 bad=value;bad.find("policies")->find("north")->find("templates")->data=BoundaryValue::Array{BoundaryValue{"await_adjustment"}};check(!decode(bad,true).ok,"intent without selected template rejected");
 auto old=parse_boundary_json(R"({"review_interval_ticks":2,"retry_delay_ticks":2,"decision_budget":4,"candidates_per_subject":4,"subjects":{}})");auto legacy=decode_director_catalog(old.value,false,false,false);check(legacy.ok&&!legacy.value.supply_renewals,"legacy original four-argument empty catalog remains unchanged");
 std::vector<std::pair<StableId,StableId>> modules={{"economic_supply_policies","1"},{"political_supply_adoptions","1"},{political_supply_renewal_module,"1"}};
 check(validate_political_supply_renewal_modules(true,modules).ok,"only explicit complete pair plus renewal marker accepted");
 check(!validate_political_supply_renewal_modules(false,modules).ok,"stray renewal marker rejected");auto missing=modules;missing.pop_back();check(!validate_political_supply_renewal_modules(true,missing).ok,"enabled catalog requires marker");
 missing=modules;missing.erase(missing.begin());check(!validate_political_supply_renewal_modules(true,missing).ok,"half supply pair cannot authorize renewal");
 auto duplicate=modules;duplicate.push_back(modules.back());check(!validate_political_supply_renewal_modules(true,duplicate).ok,"duplicate marker rejected");
 duplicate=modules;duplicate.back().second="2";check(!validate_political_supply_renewal_modules(true,duplicate).ok,"unknown marker version rejected");check(validate_political_supply_renewal_modules(false,{}).ok,"old empty markers preserved");
 DirectorState state;check(validate_political_supply_renewal_director(legacy.value,state,nullptr,nullptr).ok,"old domain cold validation unchanged");
 FinitePlan plan;plan.template_id="political_supply_renewal";plan.subject={SubjectKind::actor,"npc"};plan.goal_ref="fake";state.plans.insert("npc",plan);check(!validate_political_supply_renewal_director(legacy.value,state,nullptr,nullptr).ok,"old profile rejects forged new plan");
 check(!validate_political_supply_renewal_director(current.value,state,nullptr,nullptr).ok,"explicit profile still requires actual political and economic sources");
 std::cout<<checks<<" checks "<<failed<<" failures\n";return failed?1:0;
}
