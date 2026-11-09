// 原Travel半日后安装真实水上相交；仍用同一行动、费用和Common事务，不另开舟行时钟。
#pragma once
#include "naval_battle_creation.hpp"
#include "naval_gunnery_source.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct NavalHistoryAccess;
// 仅在完整World候选运行，任何晚ID/截停/建场失败必须撤回所有领域和原共同动作。
Outcome<StableId> install_naval_contact(CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,TravelState&,NavalState&,const NavalCatalog&,const NavalContactHit&,const DynamicStepInput* repair_step=nullptr,const std::vector<TerrainExecution>* navigation_steps=nullptr,const NavalHistoryAccess* navigation_geometry=nullptr,const NavalGunnerySkillRead* gunnery_skills=nullptr);
// 共享阶段只借实际战场占用；查询不消耗随机数，不代演员推进或改路。
std::vector<StableId> naval_battle_held_actors(const NavalState&,const MilitaryState&);
std::vector<StableId> naval_battle_occupied_fleets(const NavalState&);
std::vector<NavalContactHistory> naval_contact_history(const NavalState&);
}
