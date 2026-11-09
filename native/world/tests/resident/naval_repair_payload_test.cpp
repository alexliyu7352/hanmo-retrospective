// Common真实Action头与本域原工单载荷的窄冷边界；不称完整World冷读已通过。
#include "resident/domains/military/naval_repair_codec.hpp"
#include "resident/storage/common_store.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool ok,const char* name){++checks;if(!ok)throw std::runtime_error(name);}
// 组件只提供明确工单/行动，执行和钱货由另有的正式World风险银行检验。
NavalRepairState state(){NavalRepairFact f;f.id="action:5";f.request_key="work";f.tick=4;auto& t=f.terms;t.actor_id="actor";t.fleet_id="fleet:3";t.army_id="army:2";t.vessel_id="ship";t.unit_id="unit";t.captain_actor_id="actor";t.formation_source_id="action:1";t.command_source_id="action:1";t.rule_id="patch";t.inventory_id="hold";t.owner={Owner::Kind::organization,"org",{}};t.anchor=SettlementLocation{"town","dock"};t.duration=2;t.integrity_before=20;t.integrity_after=40;t.crew_ids={"crew"};t.participants={"actor","crew"};t.materials={{"timber",3}};NavalRepairState out;out.starts.push_back(f);return out;}
}
int main(){try{auto repairs=state();CommonState raw;raw.identity.tick=4;raw.actions.set("action:5",ActionCoreView{"action:5","naval_repair","actor",{"actor","crew"},4,2});CommonStore common(raw,std::make_shared<const CommonCatalog>());
 auto payload=naval_repair_codec::action_payloads(common,repairs);check(payload.ok&&payload.value.find("action:5"),"active action has real payload");auto encoded=naval_repair_codec::encode(repairs);check(compare_boundary(*payload.value.find("action:5"),encoded.find("starts")->as<BoundaryValue::Array>()->front()).exact(),"payload is exact original start including frozen terms");
 for(int mutation=0;mutation<4;++mutation){auto bad=common;auto* action=bad.state().actions.find("action:5");if(mutation==0)action->actor_id="foreign";if(mutation==1)action->start_tick=3;if(mutation==2)action->participants={"actor"};if(mutation==3)action->kind="wait";check(!naval_repair_codec::action_payloads(bad,repairs).ok,"wrong Common actor/tick/participants/kind refuses");}
 auto missing=repairs;missing.starts.clear();check(!naval_repair_codec::action_payloads(common,missing).ok,"Common repair cannot lose its original payload");auto empty=common;empty.state().actions.erase("action:5");check(!naval_repair_codec::action_payloads(empty,repairs).ok,"active work cannot lose its Common action");
 auto closed=repairs;closed.results.push_back({"naval_repair:6","action:5","actor","cancel","NavalRepairCancelled","",4,false,{}});check(!naval_repair_codec::action_payloads(common,closed).ok,"closed work cannot keep active action");auto ended=naval_repair_codec::action_payloads(empty,closed);check(ended.ok&&ended.value.as<BoundaryValue::Dict>()->empty(),"closed work exports no active params");
 auto extra=common;extra.state().actions.set("action:7",ActionCoreView{"action:7","naval_repair","actor",{"actor"},4,2});check(!naval_repair_codec::action_payloads(extra,repairs).ok,"orphan repair action refuses");extra.state().actions.find("action:7")->kind="wait";check(naval_repair_codec::action_payloads(extra,repairs).ok,"unrelated wait remains its original domain responsibility");std::cout<<"NAVAL_REPAIR_PAYLOAD="<<checks<<" FAILURES=0\n";return 0;}catch(const std::exception& e){std::cerr<<"NAVAL_REPAIR_PAYLOAD_FAIL "<<e.what()<<'\n';return 1;}}
