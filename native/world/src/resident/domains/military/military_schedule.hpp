// NPC军政提案只读当前typed候选；中央正式预览/子候选应用并负责普通拒绝与致命错误分类。
#pragma once
#include "military_model.hpp"
#include "naval_commands.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
struct ResidentMilitaryProposal { StableId actor_id; std::variant<MilitaryCommand,TravelCommandPayload,NavalCommand> command; };
Outcome<std::vector<ResidentMilitaryProposal>> military_schedule_proposals(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const TravelCatalog&,const InteractionAccess&,const StableId& player_actor_id,const MilitaryAccess& military_access={},const NavalAssetState* naval=nullptr,const NavalAssetCatalog* naval_catalog=nullptr,bool naval_combat_enabled=false,const NavalCatalog* combat_catalog=nullptr);
}
