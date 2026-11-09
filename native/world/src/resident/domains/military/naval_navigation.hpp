// 航损只消费原Travel实际成功步，写入须在原World候选、接敌定锚后且建场初始船体冻结前。
#pragma once
#include "naval_navigation_model.hpp"
#include "naval_battle_state.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 严格显式Naval5前置；本头独立交合同，不使Naval1..4自动启用或增加空序列。
Status validate_naval_navigation_policy(const NavalNavigationPolicy&,const NavalCatalog&);
// 只追加船体远航门；编队、人手、载重、水域、军库等仍调用原真实可航证明。
Outcome<NavalVoyageFact> preview_naval_voyage(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const NavalNavigationPolicy&,const StableId& actor_id,const TravelQuote&);
// 必须紧接原begin_travel/改道成功后调用，绑定刚生成的真实journey与Action；不另扣费或移动。
Outcome<StableId> record_naval_voyage(CommonWrite&,const TravelState&,NavalNavigationState&,const NavalVoyageFact& prepared,const StableId& journey_id);
// 纯整数算式同时供热写和冷史使用；未知船体/溢出拒绝，失能不产生虚构死亡或沉船。
Outcome<NavalNavigationVesselStep> naval_navigation_vessel_step(const NavalNavigationPolicy&,const StableId& vessel_id,Integer integrity,Integer completed_before,const StableId& previous_step_id);
}
