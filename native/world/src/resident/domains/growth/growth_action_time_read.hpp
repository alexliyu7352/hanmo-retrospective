// Growth原执行/实际F边界→S0只读适配，不从剩余量猜业务完成。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/kernel/action_time_validation.hpp"
namespace hanmo::resident {
struct InteractionState;
struct InteractionCatalog;
struct CharacterConditionSources;
class ActionTimeActorHistorySink;
struct ActorFoundationCatalog;
struct GrowthMarketHistoryCatalog;
struct GrowthMarketHistoryState;
// 三根只借真实同World拥有值；包自身按值保存，不能借构造时短栈包的地址。
struct GrowthMarketExactSources {
  const ActorFoundationCatalog &actor_foundation;
  const GrowthMarketHistoryCatalog &history_catalog;
  const GrowthMarketHistoryState &history;
};
// borrowed：由完整WorldBundle owning factory在同候选验证栈构造。
// 全部Growth原八kind均F；trade_basket由同owner
// K5交真实LifeActivity后另接有限分支。
class GrowthActionTimeRead final : public ActionTimeOriginalSourceRead {
  const CommonStore &common_;
  const GrowthState &growth_;
  const GrowthCatalog &catalog_;
  const CommonCatalog &shared_;
  const ActionElapsedStore &elapsed_;
  const TimeScaleRef &scale_;
  const CatalogIdentity &identity_;
  WorldMoment current_;
  ExactArithmeticBudget budget_;
  const InteractionState *interaction_;
  const InteractionCatalog *interaction_catalog_;
  std::optional<GrowthMarketExactSources> market_exact_;
  Status validate_fixed_sources(const ActionTimeRegistration &,
                                const ActionTimeOwnerProof &,
                                const ActionElapsedState &,
                                const std::vector<FixedServiceTransition> &,
                                bool whole_store) const;

public:
  GrowthActionTimeRead(const CommonStore &common, const GrowthState &growth,
                       const GrowthCatalog &catalog,
                       const CommonCatalog &shared,
                       const ActionElapsedStore &elapsed,
                       const TimeScaleRef &scale,
                       const CatalogIdentity &identity,
                       const WorldMoment &actual_current,
                       ExactArithmeticBudget budget = {},
                       const InteractionState *interaction = nullptr,
                       const InteractionCatalog *interaction_catalog = nullptr,
                       const GrowthMarketExactSources *market_exact = nullptr)
      : common_(common), growth_(growth), catalog_(catalog), shared_(shared),
        elapsed_(elapsed), scale_(scale), identity_(identity),
        current_(actual_current), budget_(budget), interaction_(interaction),
        interaction_catalog_(interaction_catalog),
        market_exact_(market_exact ? std::optional<GrowthMarketExactSources>(
                                         *market_exact)
                                   : std::nullopt) {}
  // 独立枚举原保留执行与真实边界；只借同根身体来源，不capture/取得新Complete图。
  // 开始/终局复用原采用头；缺中间采用cut的同M由中央sink严格拒绝。
  Status visit_actor_history_with_sources(const CharacterConditionSources &,
                                          const ActionTimeRegistration &,
                                          const StableId &actor,
                                          ActionTimeActorHistorySink &) const;
  Status enumerate_action_time_owners(const ActionTimeRegistration &,
                                      ActionTimeOwnerIdSink &) const override;
  Outcome<ActionTimeOwnerProof>
  read_action_time_owner(const ActionTimeRegistration &,
                         const StableId &) const override;
  Outcome<StableId>
  read_original_business_rule(const ActionTimeRegistration &,
                              const StableId &action_id) const;
  Outcome<WorldMoment>
  read_action_time_source(const ElapsedTimeSource &) const override;
  Status validate_fixed_action_sources(
      const ActionTimeRegistration &, const ActionTimeOwnerProof &,
      const ActionElapsedState &,
      const std::vector<FixedServiceTransition> &) const override;
  // 直接借用同一S0表按action筛选，冷读不复制transition深字段。
  Status
  validate_retained_fixed_action_sources(const ActionTimeRegistration &,
                                         const ActionTimeOwnerProof &,
                                         const ActionElapsedState &) const;
  Status validate_adopted_terminal_source(const ActionTimeRegistration &,
                                          const StableId &action_id) const;
  Status
  validate_shared_action_sources(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const SharedWorkElapsedState &) const override;
  Outcome<RunStop>
  read_action_continuation_stop(const ActionTimeRegistration &,
                                const ActionTimeOwnerProof &,
                                const WorldMoment &) const override;
  Outcome<OriginalActionRemaining>
  read_original_action_remaining(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const WorldMoment &) const override;
  Outcome<StableId>
  read_gate_result_id(const SharedWorkGateResult &) const override;
  Outcome<SharedWorkGateResult>
  read_original_gate_result(const StableId &) const override;
  Status
  validate_original_gate_result(const SharedWorkGateResult &) const override;
};
// P10使用原结果真正的起止；没有terminal边界就不能投影精确结束。
struct GrowthOriginalTimeView {
  StableId action_id, action_kind, actor_id;
  WorldMoment started_at;
  std::optional<WorldMoment> ended_at;
  bool terminal = false;
  SourceRef execution_source;
  std::optional<SourceRef> terminal_source;
};
Outcome<GrowthOriginalTimeView>
read_growth_original_time(const GrowthActionTimeRead &,
                          const ActionTimeRegistration &, const StableId &);
} // namespace hanmo::resident
namespace hanmo::resident {
// 原Growth writer已实际追加FSL边界后才调用；不接受客户端填写的时间/信用/phase。
// Start在原支付/占用采用后，Terminal在原结果和统一release后，仍同未发布candidate。
Outcome<StableId> adopt_growth_fixed_boundary_in_candidate(
    CommonStore &, GrowthState &, const GrowthCatalog &, const CommonCatalog &,
    ActionElapsedStore &, const TimeScaleRef &, const CatalogIdentity &,
    const ActionTimeRegistration &, const StableId &actual_boundary_id,
    const WorldMoment &actual_current, ExactArithmeticBudget budget = {},
    const InteractionState *interaction = nullptr,
    const InteractionCatalog *interaction_catalog = nullptr,
    const GrowthMarketExactSources *market_exact = nullptr);
} // namespace hanmo::resident

namespace hanmo::resident {
// Fixed original eight kinds, selected together with the compiled phase
// version. Durations stay in original Training/Study/Service/Rest terms, not
// this registry.
Outcome<std::vector<ActionTimeRegistration>>
make_growth_action_time_registrations(Integer selected_phase_version);
} // namespace hanmo::resident
