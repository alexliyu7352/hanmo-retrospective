// 舟师部署仅产生原军令体系下的编舰与Travel意图；不写船只、部队或世界坐标。
#pragma once
#include "naval_battle_state.hpp"
#include "military_schedule.hpp"
namespace hanmo::resident {
// 作者绑定实际防守军令和有限水上待命点；不构造预成舰队或新的军事权限。
Outcome<std::vector<NavalFleetPlan>> decode_naval_fleet_plans(const BoundaryValue&);
Status validate_naval_fleet_plan_sources(const NavalCatalog&,const MilitaryCatalog&,const CommonCatalog&,const TravelCatalog&);
struct NavalFleetProposal { bool handled=false;std::optional<ResidentMilitaryProposal> proposal; };
Outcome<NavalFleetProposal> naval_fleet_plan_proposal(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const TravelCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalCatalog&,const MilitaryPlan&,const CampaignRecord&);
}
