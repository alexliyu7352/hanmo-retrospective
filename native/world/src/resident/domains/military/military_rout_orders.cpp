// 自动退路与存下来的AI指令必须逐字段相等；无新随机、无玩家草案依赖。
#include "military_rout_orders.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
namespace {
Outcome<MilitaryRoutRetreatQuote> source(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& id){const auto* rule=military_rout_rule_in_use(b,c);if(!rule)return {false,{},common_failure("MilitaryRoutProfileRequired",id).error};auto progress=read_military_rout_progress(b,c);if(!progress.ok)return {false,{},progress.error};bool found=false;for(const auto& p:progress.value)if(p.unit_id==id)found=true;if(!found)return {false,{},common_failure("MilitaryRoutProgressMissing",id).error};return quote_military_rout_retreat(b,c,*rule,id);}
MilitaryOrder order(const MilitaryRoutRetreatQuote& q){return {q.at_exit?"rout_withdraw":q.can_progress?"rout_move":"rout_wait",q.destination,q.heading,{},{},{}};}
}
Outcome<MilitaryOrder> make_military_rout_order(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& id){auto read=source(b,c,id);if(!read.ok)return {false,{},read.error};return {true,order(read.value),{}};}
Outcome<MilitaryOrderQuote> quote_military_rout_order(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& id,const MilitaryOrder& supplied){auto read=source(b,c,id);if(!read.ok)return {false,{},read.error};auto expected=order(read.value);if(supplied.action!=expected.action||supplied.destination!=expected.destination||supplied.heading!=expected.heading||!supplied.target_id.empty()||!supplied.tactic_id.empty())return {false,{},common_failure("MilitaryRoutFrozenOrderMismatch",id).error};MilitaryOrderQuote quoted;quoted.order=std::move(expected);quoted.path=std::move(read.value.path);quoted.cost=read.value.cost;return {true,std::move(quoted),{}};}
}
