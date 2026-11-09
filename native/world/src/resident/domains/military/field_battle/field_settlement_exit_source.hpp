#pragma once
#include "field_departure_source.hpp"
#include "field_exit_source.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct PoliticalRelationsReadContext;
class PoliticalTakeoverTerritoryAccess;
struct FieldBattleCatalog;
class CheckpointMemoryCharge;
struct FieldSettlementExitSourceRead {
  const FieldBattleExitRecord *application = nullptr;
  FieldDepartureSourceRead departure;
};
Outcome<FieldSettlementExitSourceRead> read_field_settlement_exit_source(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess &, const FieldBattleCatalog &,
    const SourceRef &, ExactArithmeticBudget = {});
Outcome<SourceRef> record_field_settlement_exit_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess &, const FieldBattleCatalog &,
    const StableId &battle, const StableId &formation, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
// Field-only 的空 takeover 指针仅委托原静态控制reader；不跳过权限或历史核验。
Outcome<FieldSettlementExitSourceRead> read_field_settlement_exit_source(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess *, const FieldBattleCatalog &,
    const SourceRef &, ExactArithmeticBudget = {});
Outcome<SourceRef> record_field_settlement_exit_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess *, const FieldBattleCatalog &,
    const StableId &battle, const StableId &formation, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
// Finite dispatch on the actual retained location variant, never a fallback.
Outcome<const FieldBattleExitRecord *> read_field_actual_exit_application(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const TravelCatalog &, const FieldBattleCatalog &, const SourceRef &,
    const FieldSegmentApplicationRead &, ExactArithmeticBudget,
    const FieldExactContactReadContext *, const PoliticalRelationsReadContext *,
    const PoliticalTakeoverTerritoryAccess *);
// Actual original departure chooses its source branch; the caller supplies no
// destination, location setter, or settlement/road mode. S1 adoption follows
// this record in the same outer candidate.
Outcome<SourceRef> record_field_exit_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess &, const FieldBattleCatalog &,
    const StableId &battle, const StableId &formation, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
// 无 takeover 分支依旧必须提供可反读的原静态聚落控制。
Outcome<SourceRef> record_field_exit_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const PoliticalTakeoverTerritoryAccess *, const FieldBattleCatalog &,
    const StableId &battle, const StableId &formation, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
