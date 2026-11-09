// 舟战损失仍写原 MilitaryCasualtyFact；人口只消费同一人员分类来源，不能再造海军人口账。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct NavalOriginalSourceContext;
Outcome<MilitaryCasualtyFact> make_naval_casualty_receipt(const MilitaryState& after,const StableId& receipt_id,const StableId& naval_source,Integer tick,const std::vector<StableId>& unit_ids,const std::vector<MilitaryActorChange>&);
// 原轮次具名损失和人口 token 损失合并成一次真人变化；纯读不根据当前 alive 倒推旧结果。
Outcome<std::vector<MilitaryActorChange>> naval_round_actor_changes(const CommonRead&,const NavalTacticalFrame& before,const NavalTacticalFrame& after);
// 实际水上死亡保留原接敌锚点；遗体/私人物品不随后来换主的军团或船隐式移动。
struct NavalRemainsLocationSource {StableId actor_id,fleet_id,army_id,round_id,casualty_id;WildernessLocation anchor;};
Outcome<std::optional<NavalRemainsLocationSource>> naval_remains_location(const CommonRead&,const MilitaryState&,const NavalBattleState&,const StableId& actor_id);
Outcome<std::optional<NavalRemainsLocationSource>> query_naval_remains_location(const CommonStore&,const MilitaryState&,const NavalState&,const StableId& actor_id);
// 完整 Naval 冷史须另行验证战术结果；本门精确绑定已验证来源与原人口可消费收据。
Status validate_naval_casualty_receipt(const CommonStore&,const MilitaryState&,const NavalState&,const StableId& receipt_id,Integer limit_tick);
Status validate_naval_casualty_receipt(const CommonStore&,const MilitaryState&,const NavalState&,const StableId& receipt_id,Integer limit_tick,const NavalOriginalSourceContext&);
}
