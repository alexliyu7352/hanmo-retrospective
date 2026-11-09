// 同一World的弟子来源与原控制命令桥；无第二控制状态，完整师承先验后才验新许可。
#pragma once
#include "disciple_control_receipts.hpp"
#include "family_legacy_commands.hpp"
namespace hanmo::resident {
class WorldBundle;
Status validate_family_disciple_legacy_world(const WorldBundle&);
Status bind_family_disciple_legacy_result_world(WorldBundle&,const FamilyLegacyState* before_legacy,const DiscipleControlState* before_bindings,const StableId& actor,Integer request,const CommitResult&);
Outcome<FamilyLegacyQuote> preview_family_legacy_world(const WorldBundle&,const StableId& actor,const FamilyLegacyCommand&);
Outcome<FamilyEffect> apply_family_legacy_world(WorldBundle&,const StableId& actor,const FamilyLegacyCommand&,const FamilyLegacyQuote&);
Outcome<BoundaryValue> query_family_legacy_world(const WorldBundle&,Integer page);
}
