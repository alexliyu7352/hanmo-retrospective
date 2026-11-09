// 从正式初态与既有军籍事件重建指定接敌前名册；这是只读推导，不保存第二份兵员账。
#pragma once
#include "military_model.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;class ExactTrainingPeriodRead;
struct CommonActualFactPrefix;struct SourceRefBuildView;
// Borrowed enclosing endpoint, not the occurrence M of a nested ordinal cut.
// The original owner keeps the prefix and at alive for this synchronous read.
struct MilitaryActualSourceWindow {
 const CommonActualFactPrefix& outer_prefix;const WorldMoment& at;
};
Status validate_military_actual_source_window(const CommonStore&,const CommonActualFactPrefix& nested_prefix,const WorldMoment& actual_now,const MilitaryActualSourceWindow*,ExactArithmeticBudget={});
Status validate_military_source_in_actual_window(const CommonStore&,const SourceRef&,const WorldMoment& occurred_at,const WorldMoment& actual_now,const MilitaryActualSourceWindow*,ExactArithmeticBudget={});
Status validate_military_source_in_actual_window(const CommonStore&,const SourceRefBuildView&,const WorldMoment& occurred_at,const WorldMoment& actual_now,const MilitaryActualSourceWindow*,ExactArithmeticBudget={});
struct PopulationMilitaryState;struct PopulationMilitaryCatalog;struct DynamicStepInput;struct MilitaryExactState;
struct MilitaryHistorySource {SourceRef source_ref;Integer source_schema=1,original_serial=0;};
struct MilitaryRosterHistoryTrace {std::vector<SourceRef> authored_unit_sources;std::vector<MilitaryHistorySource> original_changes;Integer composition_revision=0;};
struct MilitaryRosterHistoryRead {std::vector<MilitaryUnitRecord> units;MilitaryRosterHistoryTrace trace;};
struct MilitaryArmyMembersHistoryRead {std::vector<StableId> named_actor_ids;MilitaryRosterHistoryTrace trace;};
// Synchronous read-only observation of the same original roster fold. A source
// relevant to the requested unit set calls before/after for each requested unit;
// unchanged units remain explicit observations, not invented change records.
// Borrowed units are valid only for the callback. Any failure discards the read.
class MilitaryRosterHistoryObserver {
public:virtual ~MilitaryRosterHistoryObserver()=default;
 virtual Status before_original_event(const MilitaryHistorySource&,const WorldMoment&,const MilitaryUnitRecord&)=0;
 virtual Status after_original_event(const MilitaryHistorySource&,const WorldMoment&,const MilitaryUnitRecord&)=0;
};
Status visit_military_roster_original_events_exact(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,
 const std::vector<StableId>& actual_unit_ids,const WorldMoment& actual_now,
 MilitaryRosterHistoryObserver&,const PopulationMilitaryState* population=nullptr,
 const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget={},
 const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,
 const ExactTrainingPeriodRead* periods=nullptr);

// Cold entry boundary is an actual retained source and its own exact M; now is
// only the trusted observation upper bound, never substituted for that boundary.
Outcome<MilitaryRosterHistoryRead> read_military_rosters_before_exact(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const std::vector<StableId>& unit_ids,const SourceRef& actual_boundary,const WorldMoment& boundary_at,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
// Ordinal-only cut for an original source with no exact occurrence M. The
// same original roster reducer retains source, partition and predecessor gates.
Outcome<MilitaryRosterHistoryRead> read_military_rosters_at_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const std::vector<StableId>& unit_ids,const CommonActualFactPrefix&,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const MilitaryActualSourceWindow* window=nullptr);
struct FieldBattleWorldState;class FieldSegmentApplicationRead;
struct MilitaryFieldHistoryRead {const FieldBattleWorldState& field;const FieldSegmentApplicationRead& clock;};
struct MilitarySelectedHistoryRead {const CatalogIdentity& identity;const MilitaryExactState* exact;const MilitaryFieldHistoryRead* field=nullptr;};
Status validate_selected_military_field_history(const CommonStore&,const MilitaryState&,const CatalogIdentity&,const MilitaryFieldHistoryRead*);
Outcome<std::optional<WorldMoment>> read_selected_military_field_casualty_moment(const CommonStore&,const MilitaryState&,const MilitaryCasualtyFact&,const MilitaryFieldHistoryRead*);
Status validate_selected_military_roster_root(const CommonStore&,const CatalogIdentity&,const MilitaryExactState*);
// Explicit selected dynamic owner; a Field exact-prefix read does not select
// the old dynamic battle module or create an empty replacement root.
Outcome<MilitaryRosterHistoryRead> read_military_rosters_at_selected_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CatalogIdentity&,const MilitaryExactState*,const std::vector<StableId>& unit_ids,const CommonActualFactPrefix&,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const MilitaryActualSourceWindow* window=nullptr,const MilitaryFieldHistoryRead* field=nullptr);
Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_at_selected_actual_prefix(const CommonStore&,const MilitaryState&,const CatalogIdentity&,const MilitaryExactState*,const StableId& army_id,const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& actual_now,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const MilitaryFieldHistoryRead* field=nullptr);
Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_at_selected_actual_prefix(const CommonStore&,const MilitaryState&,const CatalogIdentity&,const MilitaryExactState*,const StableId& army_id,const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& actual_now,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const MilitaryFieldHistoryRead* field=nullptr);
// Same original member reducer, with tracked shell and borrowed retained IDs.
// Pointers remain valid only while these original World rows remain unchanged.
// Same reducer at an actual Common prefix and its independently proved M.
// Schema 2 formation completion is required: a legacy action-start head does
// not prove completion membership. Empty/current prefixes are valid cuts.
Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_at_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const StableId& army_id,const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& actual_now,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_before_exact(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const StableId& army_id,const SourceRef& actual_boundary,const WorldMoment& boundary_at,const WorldMoment& actual_now,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_before_exact(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const StableId& army_id,const SourceRef& actual_boundary,const WorldMoment& boundary_at,const WorldMoment& actual_now,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_exact(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const StableId& army_id,const WorldMoment& actual_now,const std::optional<SourceRef>& before_source={},ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
// New selected exact history folds the same original roster writer events, including /2 casualties.
// The boundary, when present, is an actual Common source, never an invented sequence.
Outcome<MilitaryRosterHistoryRead> read_military_rosters_exact(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,
 const std::vector<StableId>& unit_ids,const WorldMoment& actual_now,
 const std::optional<SourceRef>& before_source={},const PopulationMilitaryState* population=nullptr,
 const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
// before_id必须是同一Common中已登记的实际事件；同tick按唯一分配序处理。
// 人口来源部队必须提供原人口军籍账，缺失时严格拒绝，不能从现值或快照补兵。
// 冻结CommonState入口便于同源只读核验；不绕过调用者的完整World准入。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(
 const CommonState&,const MilitaryState&,const MilitaryCatalog&,
 const std::vector<StableId>& unit_ids,Integer tick,const StableId& before_id,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
 const std::vector<StableId>& unit_ids,Integer tick,const StableId& before_id,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 原单半日内部人口交接可读到已实际写入的to；稳定旧签名保持当前tick上限。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const std::vector<StableId>& unit_ids,
 Integer tick,const StableId& before_id,const DynamicStepInput&,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 具名当前投影不接受外来tick或假边界；用于核原军籍分类没有被旁路改写。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_current(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const std::vector<StableId>& unit_ids,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 与人物占用读共用形成→整编→伤俘成员重放，必须是真实Common事实前边界。
Outcome<std::vector<StableId>> military_army_actor_members_before(const CommonStore&,const MilitaryState&,const StableId& army_id,Integer tick,const StableId& before_fact_id);
// 只借原围城确认头核旧成员；同候选后发伤亡仍按真实Common序排除。
Outcome<std::vector<StableId>> military_army_actor_members_before(const CommonStore&,const MilitaryState&,const StableId& army_id,Integer tick,const StableId& before_fact_id,const DynamicStepInput&);
// 调用者先验证完整Military/Naval历史；这里仅读真实dead变化，不从alive/名册现值猜来源。
struct MilitaryActorDeath {StableId actor_id,casualty_fact_id,battle_id;Integer tick=0;};
Outcome<std::optional<MilitaryActorDeath>> query_military_actor_death(const CommonStore&,const MilitaryState&,const StableId& actor_id);
// 仅原未提交单步候选可观察to；调用者仍须完整验证真实Military/Naval历史。
Outcome<std::optional<MilitaryActorDeath>> query_military_actor_death(const CommonStore&,const MilitaryState&,const StableId& actor_id,const DynamicStepInput&);

}
