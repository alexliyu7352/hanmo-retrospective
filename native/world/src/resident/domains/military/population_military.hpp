// 人口军籍适配器入口；只消费真实军事收据，在中央同候选完成身份和供养交接。
#pragma once
#include "population_military_model.hpp"
namespace hanmo::resident {
// 冷业务边界只接受两类明确命令，未知参数由本域严格解码。
inline bool population_military_command_kind(const StableId& kind){return kind=="population_enroll"||kind=="population_release";}
Outcome<PopulationMilitaryCommand> decode_population_military_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_population_military_quote(const PopulationMilitaryQuote&);
Outcome<PopulationMilitaryQuote> preview_population_military(const PopulationMilitaryReadContext&,const StableId& actor,const PopulationMilitaryCommand&);
Status start_population_military(PopulationMilitaryWriteContext&,const ActionCoreView&,const PopulationMilitaryCommand&,const PopulationMilitaryQuote&);
Outcome<PopulationMilitaryResult> finish_population_military(PopulationMilitaryWriteContext&,const ActionCoreView&);
Status cancel_population_military(PopulationMilitaryWriteContext&,const StableId& action_id);
Outcome<PopulationMilitaryEffects> reconcile_population_military(PopulationMilitaryWriteContext&,Integer finish_tick);
// /3 consumes the original precise casualty at its true applied M in the same
// candidate; the S5 population writer still owns every count/labor mutation.
Outcome<PopulationMilitaryEffects> consume_exact_population_military_casualty_in_candidate(
    PopulationMilitaryWriteContext&,const SourceRef& actual_casualty);
struct PopulationMilitaryCasualtyRead {
    const std::vector<MilitaryRosterReceipt>* units=nullptr;
    std::optional<WorldMoment> occurred_at;Integer source_schema=1;
};
Outcome<PopulationMilitaryCasualtyRead> read_population_military_casualty(
    const PopulationMilitaryReadContext&,const PopulationMilitaryFact&);
// Source schema is selected by the actual original row branch, independently
// of the containing physical population_military section version.
Outcome<Integer> read_population_military_event_source_schema(const PopulationMilitaryReadContext&,const SourceRef&);
Outcome<WorldMoment> read_exact_population_military_event_moment(const PopulationMilitaryReadContext&,const SourceRef&,Integer source_schema);
Status validate_population_military_fact_source_tail(const PopulationMilitaryReadContext&,const PopulationMilitaryFact&);
Outcome<Integer> read_population_military_boundary_tick(const PopulationMilitaryReadContext&);
Status validate_population_military(const PopulationMilitaryReadContext&);
// 只证实际未选，不造空军籍 provider；同 World 原根与反向历史全部须无残留。
Status validate_unselected_population_military_sources(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const MilitaryState &, const MilitaryCatalog &, const PopulationMilitaryState &,
    const PopulationMilitaryCatalog *, const CatalogIdentity &);
Outcome<PopulationContinuity> population_military_continuity(const PopulationMilitaryReadContext&);
Outcome<std::vector<StableId>> population_military_roster(const PopulationMilitaryReadContext&,const StableId& source_id,const StableId& unit_id);
bool population_military_historical_member(const PopulationMilitaryReadContext&,const StableId& member_id,Integer supply_tick);
// 训练按开课时的真实在籍区间核人，结课后退籍不撤销既有训练史。
bool population_military_training_member(const PopulationMilitaryReadContext&,const StableId& source_id,const StableId& unit_id,const StableId& member_id,Integer start_tick);
// 人口损失必须追到已结算战斗中的原始伤亡回执，孤立行不能成为新权威。
Status validate_population_military_casualty_source(const PopulationMilitaryReadContext&,const StableId& military_receipt_id);
Outcome<BoundaryValue> population_military_page(const PopulationMilitaryReadContext&,const StableId& observer);
Outcome<Integer> selected_population_military_version(const PopulationMilitaryCatalog&,bool actual_integration);
Outcome<PopulationMilitaryCatalog> decode_population_military_catalog(const BoundaryValue&);
Outcome<PopulationMilitaryState> decode_population_military_state(const BoundaryValue&);
BoundaryValue encode_population_military_state(const PopulationMilitaryState&);
Status visit_population_military_checkpoint(CodecVisitor&,PopulationMilitaryState&);
// Explicit physical selection; /3 retains the original integration branch only
// when the actual original Catalog selected it. No new table or inferred mode.
Outcome<PopulationMilitaryState> decode_population_military_state(const BoundaryValue&,Integer selected_version,bool integration);
BoundaryValue encode_population_military_state(const PopulationMilitaryState&,Integer selected_version,bool integration);
Status visit_population_military_checkpoint(CodecVisitor&,PopulationMilitaryState&,Integer selected_version,bool integration);
// 人口owner在同一候选中核来源并应用；军事owner不能直接覆盖cohort人数。
Outcome<LocalEconomyEffects> apply_population_military_fact(PopulationMilitaryWriteContext&,const StableId& fact_id);
Status validate_population_continuity(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const PopulationContinuity&);
} // 预览/页面不分配ID、扣粮或改变既有承诺。
