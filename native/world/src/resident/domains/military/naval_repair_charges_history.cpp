#include "resident/domains/military/military_model.hpp"
// 维修收费不继承占领者经营许可；原工单从确切事实前界恢复作者服务和冻结预约。
#include "naval_repair_charges_history.hpp"
#include "naval_repair_charges_codec.hpp"
#include "occupation_rules.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 占领链是新到旧，必须选实际前界前的最大Common序号，不能按向量末项或今日所有者倒填。
Outcome<MilitaryOwnership> owner_before(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& mc,const MilitaryAccess& access,const StableId& garrison,const StableId& boundary,Integer tick){
 auto current=military_garrison_owner(c,m,mc,access,garrison,c.tick());if(!current.ok)return current;const auto* camp=find(mc.garrisons,garrison);const auto& order=c.state().facts.order();auto edge=std::find(order.begin(),order.end(),boundary);if(!camp||edge==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("NavalRepairChargeHistoryBoundary",boundary).error};MilitaryOwnership out;out.organization_id=camp->organization_id;out.stronghold_id=current.value.stronghold_id;std::optional<std::size_t> best;
 for(const auto& id:current.value.chain){const auto* fact=find(m.occupation_facts,id);const auto* head=c.state().facts.find(id);const auto at=std::find(order.begin(),order.end(),id);if(!fact||!head||head->kind!="military_occupation"||head->domain!="military"||at==order.end())return {false,{},common_failure("NavalRepairChargeOccupationRequired",id).error};if(at>=edge)continue;if(fact->tick>tick)return {false,{},common_failure("NavalRepairChargeHistoryBoundary",id).error};const auto position=std::size_t(at-order.begin());if(!best||position>*best){best=position;out.organization_id=fact->to_organization_id;out.occupation_id=id;}}
 return {true,std::move(out),{}};
}
}
// 每个收费泊地工单恰有一份同序合同；浮泊自修没有公营收款方，不能夹带扣款。
Status validate_naval_repair_charge_history(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& mc,const MilitaryAccess& access,const NavalState& n,const NavalCatalog& catalog){
 if(!catalog.repair_charge_policy)return n.repair_charges.charges.empty()&&n.repair_charges.field_order.empty()?common_success():common_failure("NavalRepairChargeProfileRequired");auto checked=validate_naval_repair_charge_policy(*catalog.repair_charge_policy,catalog,mc,access.common_catalog);if(!checked.ok)return checked;checked=naval_repair_charges_codec::validate(n.repair_charges);if(!checked.ok)return checked;std::size_t next=0;
 for(const auto& start:n.repairs.starts){const NavalRepairResult* result=nullptr;for(const auto& r:n.repairs.results)if(r.action_id==start.id)result=&r;const auto* anchor=std::get_if<SettlementLocation>(&start.terms.anchor);if(!anchor){if(result&&result->reason=="NavalRepairServiceChanged")return common_failure("NavalRepairChargeSourceRequired",result->id);continue;}
  if(next>=n.repair_charges.charges.size()||n.repair_charges.charges[next].action_id!=start.id)return common_failure("NavalRepairChargeSourceRequired",start.id);const auto& charge=n.repair_charges.charges[next++];const auto& t=charge.terms;const auto* service=find(catalog.repair_charge_policy->services,t.service_id);const auto* army=find(m.armies,start.terms.army_id);const auto* head=c.state().facts.find(start.id);if(!service||!army||!head||head->kind!="naval_repair_start"||head->domain!="military_naval"||t.service_version!=service->version||t.rule_id!=service->rule_id||t.rule_id!=start.terms.rule_id||t.berth_place_id!=service->berth_place_id||t.berth_place_id!=anchor->place_id||t.garrison_id!=service->garrison_id||t.army_id!=army->id||t.payer_wallet_id!=army->wallet_id||t.payee_wallet_id!=service->wallet_id||t.amount!=service->fee||!owner_equal(t.payer_owner,start.terms.owner)||t.payee_owner.kind!=Owner::Kind::organization||t.payee_owner.id!=service->organization_id||t.payee_owner.extra)return common_failure("NavalRepairChargeSourceMismatch",start.id);
  auto original=owner_before(c,m,mc,access,t.garrison_id,start.id,start.tick);if(!original.ok)return {false,{},original.error};if(original.value.organization_id!=service->organization_id||original.value.occupation_id!=t.occupation_source_id)return common_failure("NavalRepairChargeAuthorityHistoryMismatch",start.id);
  if(result){const auto* terminal=c.state().facts.find(result->id);if(!terminal||terminal->kind!="naval_repair_result"||terminal->domain!="military_naval")return common_failure("NavalRepairChargeSourceRequired",result->id);auto at_end=owner_before(c,m,mc,access,t.garrison_id,result->id,result->tick);if(!at_end.ok)return {false,{},at_end.error};const bool original_service=at_end.value.organization_id==t.payee_owner.id&&at_end.value.occupation_id==t.occupation_source_id;if(result->success&&!original_service)return common_failure("NavalRepairChargeAuthorityHistoryMismatch",result->id);if(result->reason=="NavalRepairServiceChanged"&&(original_service||at_end.value.occupation_id.empty()||result->interruption_source_id!=at_end.value.occupation_id))return common_failure("NavalRepairChargeInterruptionMismatch",result->id);}
 }
 return next==n.repair_charges.charges.size()?common_success():common_failure("NavalRepairUnexpectedChargeSource");
}
}
