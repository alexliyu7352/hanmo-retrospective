// N§4.3固定因果图。原领域提供真实预测/采用，S0只选最早Moment并协调同候选先后。
#pragma once
#include "hanmo/resident/checkpoint_inline_function.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
#include "resident/domains/growth/study_pending_release.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
#include "world_timeline.hpp"
#include <memory>
namespace hanmo::resident {
class InteractionSelectedResponsePending;
class InteractionSessionDeathPending;
class InteractionActionConfirmationPending;
struct WorldBundle;
struct AdvanceDetails;
struct MilitaryCalendarCandidate;
struct CraftIntervalCandidate;
struct ExactPeriodContext;
// 一次候选的真实写入汇总；不是持久结果表，全部身体写仍由Common唯一capture逐draft反验。
struct ExactPhaseWriteContext {
  std::vector<BoundActorConditionWrite> condition_writes;
  // Same live capture prefix only. No stored Death cursor or restore repair.
  std::size_t interaction_condition_cursor = 0;
  bool interaction_condition_consumer_active = false;
  std::vector<BoundActorAvailabilityWrite> availability_writes;
  // 原 create_actor tap 的实际初值采用见证；不把创建伪装成身体变更 receipt。
  std::vector<BoundActorConditionCreation> condition_creations;
  // Writer-issued witnesses live only through this same candidate capture.
  // They are never a published result, checkpoint field, or clone input.
  StudyPendingReleaseProofs pending_study_releases;
  std::vector<BoundSpatialLocationWrite> spatial_location_writes;
  AdvanceDetails &advance;
  // 原围城四腿共用本次未发布候选的热输出；不进入世界状态或目录，不跨期缓存。
  std::unique_ptr<MilitaryCalendarCandidate> military_calendar;
  // 原 Craft 正片在真实 from 准备、真实 to
  // 消费；仅本栈保存，不能进入克隆或存档。
  std::unique_ptr<CraftIntervalCandidate> craft_interval;
  // Borrowed only from the enclosing zero-command capture. An advance or
  // restore has no private selection and cannot synthesize a future seal.
  InteractionSelectedResponsePending *selected_interaction_response = nullptr;
  // Same actual outer command capture, including advance commands. Opening
  // has no Common command seal and cannot bind a retained selected death tail.
  InteractionSessionDeathPending *pending_interaction_deaths = nullptr;
  // The one outer command capture owns this opaque original S3 confirmation.
  // It is not a saved Action or a guessed future Common result.
  InteractionActionConfirmationPending *pending_interaction_confirmation =
      nullptr;
  // Borrowed only while the original advance stack owns its already selected
  // from/to span. A zero-length due read scope is not itself this proof.
  const MomentSpan *actual_advance_span() const noexcept {
    return actual_advance_span_;
  }

private:
  friend class WorldExactAdvanceSpanScope;
  const MomentSpan *actual_advance_span_ = nullptr;

public:
  explicit ExactPhaseWriteContext(AdvanceDetails &);
  ~ExactPhaseWriteContext();
  ExactPhaseWriteContext(const ExactPhaseWriteContext &) = delete;
  ExactPhaseWriteContext &operator=(const ExactPhaseWriteContext &) = delete;
};
// 同一候选只建立一个原军事日历batch；重入必须仍是同一个真实期间。
Outcome<MilitaryCalendarCandidate *>
acquire_military_calendar_candidate(ExactPhaseWriteContext &,
                                    const ExactPeriodContext &,
                                    ExactArithmeticBudget budget = {});
// 固定stage不是自由插件字符串；零时叶、宏观边界和正片分别有实际原语。
enum class ExactStage {
  interval_motion_work,
  same_moment_hits,
  immediate_consequences,
  resource_recovery,
  estate_production,
  action_completions,
  accident_due_loss,
  contact_install,
  patrol_arrival,
  military_duty_period,
  work_inspection,
  probe_window_close,
  probe_next_open,
  condition_due,
  life_age,
  birth_due,
  guardianship_monitor,
  arcane_expiry,
  due_message_receipt,
  political_due_close,
  knowledge_reaction_close,
  state_satisfaction,
  growth_recovery,
  state_need_deadline_close,
  reply_receipts,
  custody_response_deadline_close,
  exile_review,
  occupancy_evaluation,
  achievement,
  social_exchange,
  siege_results,
  campaign_adoption,
  economic_supply_affairs,
  military_siege_period,
  military_siege_battles_finished
};
enum class ExactProviderId {
  motion,
  military,
  growth,
  local_economy,
  estate,
  commerce,
  interaction,
  social,
  affairs,
  tasks,
  family,
  politics,
  medicine,
  crafting,
  study,
  arcana,
  facility_claims
};
const char *exact_provider_key(ExactProviderId) noexcept;
const char *exact_stage_key(ExactStage) noexcept;
enum class ExactScheduleKind { exact_due, boundary_only };
// 预测没有成果身份，不持久；basis是原有限实际来源，版本只使未来预测失效。
struct ExactEventPrediction {
  WorldMoment at;
  ExactStage stage = ExactStage::action_completions;
  SourceRef actual_source_basis;
  Integer source_schema = 0, dependency_revision = 0;
};
// 仅已固定原 SafeExit 派生转换；kind 区分有限状态腿，不是进度 ordinal
// 或持久事件。
enum class ExactDerivedDeadlineKind {
  safe_exit_termination_window,
  safe_exit_window_expiry
};
struct ExactDerivedDeadlinePrediction {
  ExactEventPrediction original;
  ExactDerivedDeadlineKind kind =
      ExactDerivedDeadlineKind::safe_exit_termination_window;
};
struct ExactDueKey {
  ExactProviderId provider = ExactProviderId::motion;
  ExactStage stage = ExactStage::action_completions;
  SourceRef actual_source_basis;
  Integer source_schema = 0;
};
// 当前原owner应用结果：明确已消费原源/完成实际有限转换，不用新ordinal冒世界时间。
struct ExactEventApplied {
  SourceRef applied_result;
  Integer source_schema = 0;
};
struct ExactPeriodContext {
  StableId period_id;
  WorldMoment opening_at;
  Integer through_tick = 0;
};
// 期间源仍持原owner段，S0只有本栈索引，不能把玩家成功请求缓存当opening root。
struct ExactPeriodOwnerSources {
  SourceRef opening_source;
  Integer opening_schema = 0;
  std::optional<SourceRef> closing_source;
  std::optional<Integer> closing_schema;
};
// 实际业务闭期源与原日历末屏障是不同承诺；后者无成果时不得伪造SourceRef。
enum class ExactPeriodClosureKind { owner_result, stage_barrier };
struct ExactPhaseRegistration {
  ExactProviderId provider = ExactProviderId::motion;
  StableId profile_id;
  Integer version = 0;
  std::vector<ExactStage> stages;
  // 原日历step可合法无业务成果；只在实际期尾按同一N拓扑执行，不伪造due/source。
  std::vector<ExactStage> calendar_stages;
  ExactScheduleKind schedule_kind = ExactScheduleKind::exact_due;
  bool interval_writer = false, period_source_owner = false;
  ExactPeriodClosureKind period_closure_kind =
      ExactPeriodClosureKind::owner_result;
  // 唯一effect writer键由compiled注册给出，选中旧同效writer时compose必须拒绝。
  std::vector<StableId> effect_writer_keys;
  // 同一阶段内真正producer→consumer边；compiled
  // owner明确给出，作者不能自行消边。
  std::vector<ExactProviderId> interval_after, opening_after, closing_after;
  // 真实专业producer→consumer补充边；只能compiled提供，合并固定N边后统一拒环。
  std::vector<std::pair<ExactStage, ExactStage>> causal_edges;
  CheckpointInlineFunction<Status(const WorldBundle &)>
      validate_author_selection;
  // 原owner全部保留来源/当期历史反向覆盖；冷读与稳定片都执行，不能只查当前单个ref。
  CheckpointInlineFunction<Status(const WorldBundle &, const WorldMoment &)>
      validate_world_sources;
  CheckpointInlineFunction<Outcome<std::vector<ExactEventPrediction>>(
      const WorldBundle &, const WorldMoment &, const WorldMoment &)>
      predict;
  CheckpointInlineFunction<Outcome<ExactEventApplied>(
      WorldBundle &, const ExactEventPrediction &, ExactPhaseWriteContext &)>
      apply_due;
  // 只在已选择合法正片后采用真实from准入；所有from回调先于任何to区间writer。
  // 未选择正片、零时事件和仅接受命令均不能调用；失败由同一未发布世界候选整体撤销。
  CheckpointInlineFunction<Status(WorldBundle &, const MomentSpan &,
                                  ExactPhaseWriteContext &)>
      adopt_interval_from;
  CheckpointInlineFunction<Status(WorldBundle &, const MomentSpan &,
                                  ExactPhaseWriteContext &)>
      provide_interval;
  CheckpointInlineFunction<Status(WorldBundle &, ExactStage,
                                  const ExactPeriodContext &,
                                  ExactPhaseWriteContext &)>
      apply_calendar_stage;
  CheckpointInlineFunction<Outcome<ExactPeriodOwnerSources>(
      WorldBundle &, const ExactPeriodContext &, ExactPhaseWriteContext &)>
      open_period;
  CheckpointInlineFunction<Outcome<ExactPeriodOwnerSources>(
      WorldBundle &, const ExactPeriodContext &, ExactPhaseWriteContext &)>
      close_period;
  CheckpointInlineFunction<Outcome<ExactPeriodOwnerSources>(
      const WorldBundle &, const ExactPeriodContext &)>
      read_period_sources;
  CheckpointInlineFunction<Status(const WorldBundle &,
                                  const ExactEventApplied &)>
      validate_applied_source;
  // 原源绝对期限只更新其派生字段时不产生业务成果；只能由编译注册明确选择以下有限
  // kind。
  std::vector<ExactDerivedDeadlineKind> derived_deadline_kinds;
  CheckpointInlineFunction<Outcome<std::vector<ExactDerivedDeadlinePrediction>>(
      const WorldBundle &, const WorldMoment &, const WorldMoment &)>
      predict_derived_deadlines;
  CheckpointInlineFunction<Status(WorldBundle &,
                                  const ExactDerivedDeadlinePrediction &,
                                  ExactPhaseWriteContext &)>
      apply_derived_deadline;
  // 采用后另取本 World 的 fresh
  // lease，反核原源及真实后态；不能以空结果代表成功。
  CheckpointInlineFunction<Status(const WorldBundle &,
                                  const ExactDerivedDeadlinePrediction &)>
      validate_derived_deadline;
};
// Catalog只选编译注册identity；作者不能输入function、任意stage或自行声明已完成provider。
struct ExactPhaseSelection {
  ExactProviderId provider = ExactProviderId::motion;
  StableId profile_id;
  Integer version = 0;
};
struct ExactPhaseGraph {
  std::vector<ExactPhaseRegistration> providers;
  std::vector<std::pair<ExactStage, ExactStage>> causal_edges;
  std::vector<ExactStage> ordered_stages;
  std::vector<std::size_t> interval_order, opening_order, closing_order;
};
struct ExactPhaseTechnicalBudget {
  std::size_t max_same_moment_transitions = 100000;
};
Outcome<ExactPhaseGraph> compose_exact_phase_graph(
    const std::vector<ExactPhaseRegistration> &compiled,
    const std::vector<ExactPhaseSelection> &selected,
    const std::vector<StableId> &selected_legacy_writer_keys);
Status validate_exact_phase_selection(const ExactPhaseGraph &,
                                      const WorldBundle &);
Status validate_exact_phase_sources(const ExactPhaseGraph &,
                                    const WorldBundle &, const WorldMoment &);
Status validate_no_exact_due(const ExactPhaseGraph &, const WorldBundle &,
                             const WorldMoment &);
// permit_to只是上界，返回最早实际event或宏观边界；不把许可预算乘进业务required
// H*N。
Outcome<WorldMoment> select_exact_slice_end(const ExactPhaseGraph &,
                                            const WorldBundle &,
                                            const WorldMoment &current,
                                            const WorldMoment &permit_to,
                                            ExactArithmeticBudget budget = {});
// 每个前驱后重新取phase-current
// prediction；所有同Moment真实叶结清才可发布稳定时刻。
// 返回值仅含真实业务成果；已反验的派生 deadline 不制造 ExactEventApplied。
Outcome<std::vector<ExactEventApplied>>
consume_exact_due_events(const ExactPhaseGraph &, WorldBundle &,
                         const WorldMoment &, bool at_calendar_boundary,
                         ExactPhaseWriteContext &,
                         ExactPhaseTechnicalBudget technical = {},
                         ExactArithmeticBudget arithmetic = {},
                         const ExactPeriodContext *closing_period = nullptr);
Status provide_exact_intervals(const ExactPhaseGraph &, WorldBundle &,
                               const MomentSpan &, ExactPhaseWriteContext &);
Outcome<std::vector<ExactPeriodOwnerSources>>
open_exact_registered_period(const ExactPhaseGraph &, WorldBundle &,
                             const ExactPeriodContext &,
                             ExactPhaseWriteContext &);
Outcome<std::vector<ExactPeriodOwnerSources>>
close_exact_registered_period(const ExactPhaseGraph &, WorldBundle &,
                              const ExactPeriodContext &,
                              ExactPhaseWriteContext &);
Status validate_exact_registered_period_sources(const ExactPhaseGraph &,
                                                const WorldBundle &,
                                                const ExactPeriodContext &,
                                                PeriodBoundaryState);
} // namespace hanmo::resident
