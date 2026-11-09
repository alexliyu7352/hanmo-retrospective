// 同一已交命令和原PCG状态必须得出同一船损/兵损/接管根据；事实不反向覆盖活世界。
#include "naval_round_history.hpp"
#include "naval_battle_codec.hpp"
#include "military_round_rng.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
// 只接受Common分配的规范正整数ID，禁止空尾、别名、负数和前导零。
bool fact_id(const StableId& id){const StableId prefix="naval_round:";if(id.rfind(prefix,0)!=0)return false;Integer value=0;const auto* start=id.data()+prefix.size();auto parsed=std::from_chars(start,id.data()+id.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==id.data()+id.size()&&value>0&&std::to_string(value)==id.substr(prefix.size());}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// 每艘变更船仅登记一次真实前后差值；未损船不填伪0伤害事件。
std::vector<NavalHullChange> changes(const NavalAssetState& before,const NavalAssetState& after){std::vector<NavalHullChange> out;for(const auto& v:before.vessels)for(const auto& n:after.vessels)if(v.id==n.id&&v.integrity!=n.integrity)out.push_back({v.id,v.integrity,n.integrity,{}});std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.vessel_id<b.vessel_id;});return out;}
}
// 世界阶段给结束tick和唯一ID；这里只组完整事实，失败不消费时钟、资源或ID。
Outcome<NavalRoundFact> make_naval_round_fact(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& military,const NavalBattleRecord& battle,const StableId& id,Integer tick,const NavalGunneryPreparation* prepared){
 if(battle.frame.round<1||battle.frame.round>policy.max_rounds)return fail<NavalRoundFact>("NavalRoundLimit",battle.frame.id);
 if(!fact_id(id)||battle.last_tick<0||battle.last_tick==std::numeric_limits<Integer>::max()||tick!=battle.last_tick+1)return fail<NavalRoundFact>("InvalidNavalRoundClock",id);
 auto orders=frozen_naval_orders(assets,ships,policy,military,battle,prepared);if(!orders.ok)return {false,{},orders.error};auto result=resolve_naval_combat(assets,ships,policy.combat,military,battle.frame,orders.value,battle.rng_seed,battle.rng_state,military_round_random_draw,prepared);if(!result.ok)return {false,{},result.error};
 NavalRoundFact fact;fact.id=id;fact.battle_id=battle.frame.id;fact.round=battle.frame.round;fact.start_tick=battle.last_tick;fact.tick=tick;fact.before=battle.frame;fact.after=std::move(result.value.frame);fact.orders=std::move(orders.value);fact.rng_seed=battle.rng_seed;fact.rng_before=battle.rng_state;fact.rng_after=std::move(result.value.rng_state);fact.hull_changes=changes(assets,result.value.assets);fact.events=std::move(result.value.events);fact.claims=std::move(result.value.capture_claims);return {true,std::move(fact),{}};
}
// 外层通过事实序列提供可信起点；不同字段序可保留，但业务值/人员/船损都必须精确重放。
Status validate_naval_round_fact(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& military,const NavalBattleRecord& source,const NavalRoundFact& fact,const NavalGunneryPreparation* prepared){
 if(fact.battle_id!=source.frame.id||fact.round!=source.frame.round||fact.start_tick!=source.last_tick||fact.rng_seed!=source.rng_seed||fact.rng_before!=source.rng_state)return common_failure("InvalidNavalRoundSource",fact.id);
 // 重放的种子、起点与已交计划均来自前序权威record，而非被校验事实自称的输入。
 auto expected=make_naval_round_fact(assets,ships,policy,military,source,fact.id,fact.tick,prepared);
 if(!expected.ok)return {false,{},expected.error};expected.value.casualty_source_id=fact.casualty_source_id;
 try{
  if(!compare_boundary(naval_battle_codec::encode_round(expected.value),naval_battle_codec::encode_round(fact)).values_equal)return common_failure("NavalRoundResultMismatch",fact.id);
 }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
 return common_success();
}
}
