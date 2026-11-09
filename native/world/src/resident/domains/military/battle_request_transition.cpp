// 先检查幂等历史，再一次性准备裁定与计划候选；所有错误保持调用者原对象不变。
#include "battle_request_transition.hpp"
#include "battle_request_codec.hpp"
#include "resident/model/common_model.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 同幂等键只能复用原始意图，字典显示顺序不属于业务内容。
bool same_request(const MilitaryBattleRequest& a,const MilitaryBattleRequest& b){return std::tie(a.battle_id,a.request_key,a.request_kind,a.unit_id,a.target_unit_id)==std::tie(b.battle_id,b.request_key,b.request_kind,b.unit_id,b.target_unit_id);}
// 失败沿原typed错误出口，不把非法输入持久化为统帅拒绝。
Outcome<MilitaryRequestTransition> fail(const StableId& code){return {false,{},common_failure(code).error};}
}
// 历史回执优先于本轮关闭/额度判断；重试已完成意图不能再次更改新轮AI计划。
Outcome<MilitaryRequestTransition> prepare_military_request_transition(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog,const MilitaryBattleRequest& request,const StableId& actor,const MilitaryRequestAuthority& authority,const MilitaryRequestPolicy& policy,const std::vector<MilitaryRequestReceipt>& history,const StableId& receipt_id,Integer tick,const MilitaryDamageSources& sources){
 if(!request.fields_valid||actor.empty()||request.battle_id!=battle.id||request.request_key.empty())return fail("InvalidMilitaryRequest");
 std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId>> keys;
 const MilitaryRequestReceipt* original=nullptr;Integer used=0;
 for(const auto& row:history){
  if(!military_request_codec::validate(row).ok)return fail("InvalidMilitaryRequestHistory");
  if(row.id.empty()||row.actor_id.empty()||row.request.battle_id.empty()||row.request.request_key.empty()||row.round<1||row.tick<0||!ids.insert(row.id).second||!keys.emplace(row.actor_id,row.request.battle_id,row.request.request_key).second)return fail("InvalidMilitaryRequestHistory");
  if(row.actor_id==actor&&row.request.battle_id==battle.id){if(row.request.request_key==request.request_key)original=&row;if(row.round==battle.round)++used;}
 }
 if(original){if(!same_request(original->request,request))return fail("MilitaryRequestKeyConflict");MilitaryRequestTransition next;next.battle=battle;next.receipt=*original;next.replay=true;return {true,std::move(next),{}};}
 if(actor!=battle.controller_actor_id)return fail("MilitaryCommanderUnauthorized");
 if(policy.per_actor_round_limit<1)return fail("InvalidMilitaryRequestPolicy");
 if(used>=policy.per_actor_round_limit)return fail("MilitaryRequestRoundLimit");
 if(receipt_id.empty()||ids.count(receipt_id)||tick<0||tick!=battle.last_tick)return fail("InvalidMilitaryRequestReceipt");
 if(authority.commander_actor_id.empty()||authority.commander_actor_id==actor||authority.army_id.empty()||authority.office_id.empty()||authority.formation_source.empty()||authority.appointment_source.empty())return fail("MilitaryRequestInvalidAuthority");
 bool own_unit=false;for(const auto& unit:battle.units)if(unit.unit_id==request.unit_id)own_unit=unit.army_id==authority.army_id;
 if(!own_unit)return fail("MilitaryRequestInvalidAuthority");
 auto decision=consider_military_request(battle,catalog,request,authority.ai_unit_ids,sources);if(!decision.ok)return {false,{},decision.error};
 MilitaryRequestTransition next;next.battle=battle;auto& receipt=next.receipt;receipt.id=receipt_id;receipt.actor_id=actor;receipt.commander_actor_id=authority.commander_actor_id;receipt.army_id=authority.army_id;receipt.office_id=authority.office_id;receipt.formation_source=authority.formation_source;receipt.appointment_source=authority.appointment_source;receipt.tick=tick;receipt.round=battle.round;receipt.request=request;receipt.decision=std::move(decision.value);
 if(receipt.decision.accepted){bool replaced=false;for(auto& plan:next.battle.ai_plans)if(plan.first==receipt.decision.response_unit_id){if(replaced)return fail("InvalidMilitaryRequestHistory");receipt.previous_order=plan.second;plan.second=receipt.decision.response_order;replaced=true;}if(!replaced)return fail("MilitaryRequestResponseUnavailable");auto valid=validate_military_battle(next.battle,catalog,true);if(!valid.ok)return {false,{},valid.error};}
 auto receipt_valid=military_request_codec::validate(receipt);if(!receipt_valid.ok)return {false,{},receipt_valid.error};
 return {true,std::move(next),{}};
}
}
