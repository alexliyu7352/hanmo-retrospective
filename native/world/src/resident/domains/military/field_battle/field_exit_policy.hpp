#pragma once
#include "field_generation.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct PoliticalRelationsReadContext;
class PoliticalTakeoverTerritoryAccess;
struct FieldBattleCatalog;
class CommonStore;
struct MilitaryState;
struct MilitaryCatalog;
// 无占领历史的原据点归属；返回不可变目录来源，运行字段仅作反覆盖断言。
Outcome<StableId> read_field_initial_settlement_controller(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const StableId &settlement);
struct FieldQualifiedExitRead {
  std::vector<std::size_t> cells;
  std::optional<StableId> settlement_id;
  std::optional<std::size_t> settlement_cell;
};
// Actual current prefix, before the Field contact is appended. Does not write
// a contact, relationship, runtime jurisdiction, or world location.
Outcome<FieldQualifiedExitRead> read_field_exits_before_contact_install(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess &, const FieldBattleCatalog &,
    const SourceRef &actual_movement_contact, Integer side,
    ExactArithmeticBudget = {});
// Retained exact initial Field contact is the sole historical cut.
Outcome<FieldQualifiedExitRead> read_field_exits_at_original_contact(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess &, const FieldBattleCatalog &,
    const SourceRef &actual_field_contact, Integer side,
    ExactArithmeticBudget = {});
// 未选takeover仍借同World军事owner按原占领链/cut读归属；缺reader或controller绝不中立。
Outcome<FieldQualifiedExitRead> read_field_exits_before_contact_install(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess *, const FieldBattleCatalog &,
    const SourceRef &, Integer side, ExactArithmeticBudget = {});
Outcome<FieldQualifiedExitRead> read_field_exits_at_original_contact(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess *, const FieldBattleCatalog &,
    const SourceRef &, Integer side, ExactArithmeticBudget = {});
} // namespace hanmo::resident
