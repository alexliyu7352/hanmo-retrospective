#include "resident/domains/military/military_model.hpp"
// 单一海战候选同时结算原兵员与船体；Common真人后果由外层同一World事务调用原军旅适配器。
#pragma once
#include "naval_round_history.hpp"
namespace hanmo::resident {
struct NavalRoundSettlement {
 MilitaryState military;NavalState naval;std::vector<MilitaryActorChange> actor_changes;bool replayed=false;
};
// fact须由本轮可信record重放得到；失败不改输入，重复同事实不再施加伤亡或船损。
Outcome<NavalRoundSettlement> settle_naval_round_candidate(const CommonRead&,const MilitaryState&,const NavalState&,const NavalCatalog&,const MilitaryBattleCatalog&,const NavalRoundFact&,Integer finish_tick);
}
