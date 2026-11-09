// ITEM-USE-SOURCE-2. Only S9 changes ordinary instance condition.
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <optional>
#include <variant>
#include <vector>
#include <string_view>
namespace hanmo::resident {
struct BattleWear {SourceRef battle_or_session_ref,result_ref;Integer usage_ordinal=0;};
struct GatherToolWear {SourceRef gather_receipt_ref;Integer tool_usage_ordinal=0;};
struct InfusionFailureWear {SourceRef infusion_result_ref;Integer item_slot_ordinal=0;};
struct FacilityAttackToolWear {StableId facility_attack_result_id,episode_id;Integer period_ordinal=0,tool_usage_ordinal=0;};
using ItemUseSource=std::variant<BattleWear,GatherToolWear,InfusionFailureWear,FacilityAttackToolWear>;
enum class ItemConditionSourceKind {use_damage,repair_completed};
enum class ItemConditionDisposition {applied,no_change};
struct ItemConditionHead {StableId instance_id;Integer instance_version=0;std::optional<StableId> last_condition_result_ref;};
// Same original adopted-head fields, borrowed before fresh output construction.
struct ItemConditionHeadBuildView {std::string_view instance_id;Integer instance_version=0;std::optional<std::string_view> last_condition_result_ref;};
struct ItemConditionResultView {
 StableId result_id,instance_id;ItemConditionSourceKind source_kind=ItemConditionSourceKind::use_damage;
 std::optional<ItemUseSource> use_source;SourceRef source_fact_ref,source_activity_ref;
 std::optional<Integer> usage_ordinal;Integer effective_tick=0;
 Integer instance_version_before=0,instance_version_after=0,durability_before=0,durability_after=0;
 Integer max_durability_before=0,max_durability_after=0;std::vector<SourceRef> resource_effect_refs;
 ItemConditionDisposition disposition=ItemConditionDisposition::no_change;std::optional<StableId> reason;
};
bool same_item_use_source(const ItemUseSource&,const ItemUseSource&);
Status validate_item_use_source(const ItemUseSource&);
Status validate_item_condition_result_shape(const ItemConditionResultView&);
}
