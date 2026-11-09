// Original siege/calendar legs. One transient owner per complete candidate.
#pragma once
#include "military_exact_sources.hpp"
#include "military_lifecycle.hpp"
#include "military_phases.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "siege_world.hpp"
namespace hanmo::resident {
// 外壳实际new/delete也沿同job；原领域内部结果仍由各原producer自己的allocator负责。
struct MilitaryCalendarCandidate : CheckpointAllocatedObject {
  CheckpointMemoryCharge copied_fields;
  MilitaryCalendarCandidate() = default;
  MilitaryCalendarCandidate(const MilitaryCalendarCandidate &) = delete;
  MilitaryCalendarCandidate &
  operator=(const MilitaryCalendarCandidate &) = delete;
  MilitaryCalendarCandidate(MilitaryCalendarCandidate &&) = delete;
  MilitaryCalendarCandidate &operator=(MilitaryCalendarCandidate &&) = delete;
  ExactPeriodContext period;
  bool siege_period_consumed = false, siege_battles_consumed = false,
       siege_results_consumed = false, campaign_consumed = false;
  SiegeWorldStep original_siege_step;
  // Appended only by the actual dynamic battle terminal producer, never by a
  // scan of a mutable 'completed' flag or a replay of the prior battle step.
  CheckpointVector<MomentOwnerSource> actual_completed_battle_sources;
  std::vector<StableId> completed_receipt_ids;
  std::vector<MilitaryJourneyRequest> actual_journey_requests;
  CheckpointVector<MomentOwnerSource> actual_siege_entry_sources,
      actual_consequence_sources;
};
// The exact battle owner translates the ORIGINAL siege request/finished result.
// It must create its real session and S1 writer handoff; no legacy round is
// run.
class MilitaryCalendarBattleWrite {
public:
  virtual ~MilitaryCalendarBattleWrite() = default;
  virtual Status validate_actual_terminal(const MomentOwnerSource &,
                                          const WorldMoment &) const = 0;
  virtual Status
  finish_actual_siege_battle_in_candidate(const MomentOwnerSource &,
                                          const WorldMoment &,
                                          ExactPhaseWriteContext &) = 0;
  virtual Outcome<MomentOwnerSource> create_actual_siege_battle_in_candidate(
      const StableId &siege_id, const SiegeBattleRequest &, const WorldMoment &,
      ExactPhaseWriteContext &) = 0;
  virtual Status validate_actual_siege_entry(const StableId &siege_id,
                                             const SiegeBattleRequest &,
                                             const MomentOwnerSource &,
                                             const WorldMoment &) const = 0;
};
// Consequences and journey requests remain their actual S4/S1 consumers.
class MilitaryCalendarOriginalConsumer {
public:
  virtual ~MilitaryCalendarOriginalConsumer() = default;
  virtual Outcome<std::vector<MomentOwnerSource>>
  apply_actual_military_consequences_in_candidate(const MilitaryEffect &,
                                                  const WorldMoment &,
                                                  ExactPhaseWriteContext &) = 0;
  virtual Status
  begin_actual_military_journey_in_candidate(const MilitaryJourneyRequest &,
                                             const WorldMoment &,
                                             ExactPhaseWriteContext &) = 0;
};
struct MilitaryCalendarWriteBinding {
  CommonStore &common;
  MilitaryState &military;
  const MilitaryCatalog &catalog;
  const MilitaryAccess &access;
  MilitaryCalendarBattleWrite *battles;
  MilitaryCalendarOriginalConsumer *consumers;
  // 仅显式 Field1 无旧围城分支可不借 writer/consumer。原算法仍执行；
  // 任何实际后果或行程必须有真实 consumer，否则整候选失败。
  bool field_without_siege = false;
  MilitaryCalendarWriteBinding(CommonStore &c, MilitaryState &m,
                               const MilitaryCatalog &cat,
                               const MilitaryAccess &a,
                               MilitaryCalendarBattleWrite &b,
                               MilitaryCalendarOriginalConsumer &consumer)
      : common(c), military(m), catalog(cat), access(a), battles(&b),
        consumers(&consumer) {}
  MilitaryCalendarWriteBinding(CommonStore &c, MilitaryState &m,
                               const MilitaryCatalog &cat,
                               const MilitaryAccess &a,
                               MilitaryCalendarBattleWrite *b,
                               MilitaryCalendarOriginalConsumer &consumer,
                               bool field)
      : common(c), military(m), catalog(cat), access(a), battles(b),
        consumers(&consumer), field_without_siege(field) {}
  MilitaryCalendarWriteBinding(CommonStore &c, MilitaryState &m,
                               const MilitaryCatalog &cat,
                               const MilitaryAccess &a,
                               MilitaryCalendarBattleWrite *b,
                               MilitaryCalendarOriginalConsumer *consumer,
                               bool field)
      : common(c), military(m), catalog(cat), access(a), battles(b),
        consumers(consumer), field_without_siege(field) {}
};
Status begin_military_calendar_candidate(MilitaryCalendarCandidate &,
                                         const ExactPeriodContext &,
                                         ExactArithmeticBudget budget = {});
Status register_actual_military_terminal_for_calendar(
    MilitaryCalendarCandidate &, const MilitaryCalendarBattleWrite &,
    const MomentOwnerSource &, const WorldMoment &,
    ExactArithmeticBudget budget = {});
Status apply_military_siege_period_in_candidate(
    MilitaryCalendarWriteBinding &, MilitaryCalendarCandidate &,
    const ExactPeriodContext &, ExactPhaseWriteContext &,
    ExactArithmeticBudget budget = {});
Status apply_military_siege_battles_finished_in_candidate(
    MilitaryCalendarWriteBinding &, MilitaryCalendarCandidate &,
    const ExactPeriodContext &, ExactPhaseWriteContext &,
    ExactArithmeticBudget budget = {});
Status apply_military_siege_results_in_candidate(
    MilitaryCalendarWriteBinding &, MilitaryCalendarCandidate &,
    const ExactPeriodContext &, ExactPhaseWriteContext &,
    ExactArithmeticBudget budget = {});
Status apply_military_campaign_adoption_in_candidate(
    MilitaryCalendarWriteBinding &, MilitaryCalendarCandidate &,
    const ExactPeriodContext &, ExactPhaseWriteContext &,
    ExactArithmeticBudget budget = {});
Status validate_military_calendar_candidate_consumed(
    const MilitaryCalendarCandidate &, const ExactPeriodContext &,
    ExactArithmeticBudget budget = {});
} // namespace hanmo::resident
