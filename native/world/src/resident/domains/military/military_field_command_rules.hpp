// 野外应用以专项签受、原伤亡与本軍现场三者为前置；原候选一次发布，失败由调用方整候选回滚。
#pragma once
#include "military_field_command_source.hpp"
namespace hanmo::resident {
Outcome<MilitaryFieldCommandQuote> preview_military_field_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryFieldCommandAssign&);
Outcome<MilitaryEffect> apply_military_field_command(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryFieldCommandAssign&);
Status validate_military_field_commands(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Status validate_military_field_actor_occupation(const CommonStore&,const MilitaryState&,const NavalState*,const StableId& actor_id);
Status apply_military_field_fleet_binding(const CommonRead&,const MilitaryState&,NavalState&,const MilitaryFieldCommandFact&);
// 冻结目标比较给当前应用与政治历史共用，不能只比较同人而略过中间来源变化。
bool same_military_field_target(const MilitaryFieldCommandTarget&,const MilitaryFieldCommandTarget&);
bool military_field_command_effect_shape(const MilitaryEffect&);
Status validate_military_field_command_cached_effect(const MilitaryState&,const MilitaryEffect&,const StableId& actor_id);
}
