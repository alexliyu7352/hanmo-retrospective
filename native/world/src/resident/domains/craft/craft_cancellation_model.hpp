// C02原取消来源；已耗量只反读实际工序，本表不保存可推进的材料余额。
#pragma once
#include "item_service.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
// 只由原接受的取消/产物权属政策派生。属性规则仍由同锁ItemService目录解释。
struct CraftRecoveryTerms {
 StableId definition_id,destination_inventory_id;Owner owner;
 StableId property_policy_id;Integer property_policy_version=0;
 std::optional<Integer> quality_score;std::vector<StableId> fit_tag_ids;
};
struct CraftRecoveryOutput {
 CraftRecoveryTerms terms;Integer consumed_quantity=0,recoverable_quantity=0;
};
struct CraftReleasedMaterial {
 Integer selection_ordinal=0;CargoSelection original_remaining;
};
struct CraftCancellationSource {
 StableId id,job_id,actor_id;Integer gate_ordinal=0;
 std::optional<StableId> last_closed_gate_ref;WorldMoment occurred_at;
 std::vector<CraftReleasedMaterial> released_materials;
 std::vector<CraftRecoveryOutput> recoveries;
 std::vector<MomentOwnerSource> actual_disposition_sources;
};
struct CraftCancellationResult {
 StableId id,job_id,cancellation_ref;WorldMoment occurred_at;
 // 连续ordinal严格沿取消源recoveries顺序；每项只一次真正Materialize。
 std::vector<ItemMaterialization> recovered_outputs;
 std::vector<MomentOwnerSource> actual_settlement_sources;
};
SourceRef craft_cancellation_source(const StableId&);
SourceRef craft_cancelled_source(const StableId&);
}
