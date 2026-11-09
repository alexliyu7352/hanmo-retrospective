#include "resident/domains/military/military_model.hpp"
// 接舷俘获的完整域候选；资源recipe和真人后果必须在外层同一Common事务中一起提交。
#pragma once
#include "naval_capture.hpp"
namespace hanmo::resident {
struct NavalCaptureSettlement {
 MilitaryState military;NavalState naval;NavalCaptureFact fact;ResourceRecipe resources;
 std::vector<MilitaryActorChange> actor_changes;std::vector<StableId> interrupted_action_ids;bool replayed=false;
};
Outcome<NavalCaptureSettlement> settle_naval_capture_candidate(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalCaptureCommand&,const StableId& fact_id);
}
