// 新尾Common投影的孤头、漏行、坏字段和禁用反例；不构造真实会面或World。
#include "resident/domains/family/voluntary_path_payloads.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
int main(){int n=0;try{auto check=[&](bool ok,const char* why){++n;if(!ok)throw std::runtime_error(why);};
 auto catalog=std::make_shared<CommonCatalog>();CommonState seed;seed.next_instance_id=9;CommonStore c(seed,catalog);VoluntaryPathState state;
 check(voluntary_path_fact_payloads(c,nullptr).ok,"old empty no new tail");
 VoluntaryPathCancellation cancel;cancel.plan_id="family:1";cancel.actor_id="parent";cancel.reason="predecessor_cancel";cancel.tick=3;cancel.revision=4;
 state.cancellations.insert("voluntary_path:5",cancel);check(!voluntary_path_fact_payloads(c,&state).ok,"payload needs exact Common header");
 c.state().facts.insert("voluntary_path:5",{"voluntary_path:5",voluntary_path_cancelled_kind,voluntary_path_module});auto projection=voluntary_path_fact_payloads(c,&state);check(projection.ok,"typed cancellation projection");
 check(!voluntary_path_fact_payloads(c,nullptr).ok,"disabled profile refuses actual header");
 auto full=boundary::obj({{"facts",projection.value}});check(validate_voluntary_path_json_payloads(c,&state,full).ok,"matching full JSON");
 auto bad=full;*bad.find("facts")->find("voluntary_path:5")->find("actor_id")=BoundaryValue{"other"};check(!validate_voluntary_path_json_payloads(c,&state,bad).ok,"changed actor rejected");
 bad=boundary::obj({{"facts",boundary::obj({})}});check(!validate_voluntary_path_json_payloads(c,&state,bad).ok,"missing JSON payload rejected");
 c.state().facts.find("voluntary_path:5")->field_order={"domain","revision","tick","reason","actor_id","plan_id","kind"};projection=voluntary_path_fact_payloads(c,&state);check(projection.ok&&boundary::order(*projection.value.find("voluntary_path:5")).front()=="domain","saved header field order preserved");
 check(!validate_voluntary_path_json_payloads(c,&state,full).ok,"different field order rejected");
 c.state().facts.insert("voluntary_path:6",{"voluntary_path:6",voluntary_path_cancelled_kind,voluntary_path_module});check(!voluntary_path_fact_payloads(c,&state).ok,"orphan extra header rejected");
 c.state().facts.erase("voluntary_path:6");c.state().facts.find("voluntary_path:5")->domain="family";check(!voluntary_path_fact_payloads(c,&state).ok,"wrong domain rejected");
 VoluntaryPathQuote q;q.plan_id="family:1";auto quote=encode_voluntary_path_quote(q);check(boundary::get_int(quote,"duration","")==0&&boundary::get_string(quote,"succession_plan_id","")=="family:1","quote remains zero-time exact plan");
 std::cout<<"VOLUNTARY_PATH_PAYLOAD "<<n<<" checks 0 failures\n";return 0;
 }catch(const std::exception& e){std::cerr<<n<<" checks: "<<e.what()<<'\n';return 1;}}
