// 10-28唯一伤损表：临时值取原真实等级；旧创仍归原receipt/effect来源所有。
#pragma once
#include "model.hpp"
namespace hanmo::resident {
struct BoundaryValue;
bool injury_grade_effects_v3_selected(const ActorFoundationCatalog &) noexcept;
Status validate_injury_grade_effects_map_v3(const InjuryCapabilityMap &,
                                            const CatalogIdentity &);
Outcome<InjuryCapabilityMap>
decode_authored_injury_grade_effects_v3(const BoundaryValue &,
                                        const CatalogIdentity &);
// 只由原冷作者安装使用；不是World任意setter，不接受作者另填两套选择。
Status install_original_injury_grade_bindings_v3(ActorFoundationCatalog &);
struct CharacterConditionSources;
// Caller has authenticated the complete original prefix and current
// before/after through the original scalar rule. Prior crossings are re-read
// from original receipts, never from effect count or caller-supplied total. No
// selected-result/composer callback occurs.
Outcome<bool>
read_original_injury_scar_due_v3(const CharacterConditionSources &,
                                 const StableId &actor, Integer revision_before,
                                 Integer injury_before, Integer injury_after);
Outcome<ActorAttributes> original_current_injury_losses_v3(Integer grade);
} // namespace hanmo::resident
