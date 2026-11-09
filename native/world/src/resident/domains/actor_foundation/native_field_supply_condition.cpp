#include "native_field_supply_condition.hpp"
#include "military_supply_condition.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
#include "resident/kernel/native_domain_read_versions.hpp"
#include "resident/kernel/native_military_training_period.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code).error};
}
class FieldSupplyWrite final : public MilitaryActorConditionWrite {
  WorldBundle &world_;
  CompleteExactWorldBindingOwner &owner_;

public:
  FieldSupplyWrite(WorldBundle &w, CompleteExactWorldBindingOwner &o)
      : world_(w), owner_(o) {}
  Status
  after_military_condition_sources(CommonWrite &common,
                                   const MilitaryState &military,
                                   const std::vector<StableId> &ids) override {
    try {
      auto &w = world_;
      auto *writes = w.active_exact_write_context();
      require(&common == static_cast<CommonWrite *>(&w.common) &&
                  &military == &w.military && writes &&
                  w.has_active_read_phase() && w.read_stamps &&
                  w.actor_foundation && w.growth_permanent && w.field_battle &&
                  w.character_injury_sources && ids.size() == 1,
              "FieldSupplyOriginalCandidateRequired");
      ReadStampWriteBoundary callback_boundary(*w.read_stamps);
      const auto capture = w.common.actor_condition_capture_identity();
      require(bool(capture), "FieldSupplyActualCaptureRequired");
      // 只在回调时读完整 owner；factory 不读尚在构造中的图。
      const auto &reads = owner_.reads();
      const auto *core = reads.original_core;
      require(core && &core->characters.early.world == &w &&
                  &core->characters.early.complete == &owner_,
              "FieldSupplyOriginalOwnerRequired");
      auto body = take(reads.character_conditions.sources());
      require(&body.common == &w.common &&
                  &body.state == &*w.actor_foundation &&
                  body.military == &w.military && !body.military_exact &&
                  body.military_supply_periods &&
                  body.injury_field == &*w.field_battle &&
                  body.injury_sources == &*w.character_injury_sources,
              "FieldSupplyOriginalBodyMismatch");
      require(body.military_supply_periods == core->characters.early.periods &&
                  body.injury_field_clock ==
                      core->characters.early.field_application_clock &&
                  body.injury_field_clock,
              "FieldSupplyOriginalClockPeriodMismatch");
      const SourceRef ref{"military", "military_supply", ids.front()};
      auto source = take(read_field_military_supply_source(
          w.common, w.military, *body.military_supply_periods, ref, 1,
          body.actual_now));
      require(source.original_supply &&
                  take(compare_world_moments(source.original_period.opening_at,
                                             body.actual_now)) == 0,
              "FieldSupplyActualOpeningRequired");
      const auto &facts = w.common.state().facts.rows();
      require(!facts.empty() && facts.back().first == ref.id &&
                  facts.back().second.kind == ref.kind &&
                  facts.back().second.domain == ref.domain,
              "FieldSupplyActualNewSourceRequired");
      const auto &drafts = w.common.actor_condition_execution_drafts();
      const auto original_draft_count = drafts.size();
      // 已签发 Bound 必须构成完整已消费前缀。剩下的真实 draft 才是本次
      // callback 的未消费尾；不按 mouth ordinal 或相同 fatigue 数值搜索旧写。
      const auto count_at = [&](Integer sequence) {
        std::size_t count = 0;
        const auto add = [&](const auto &bound) {
          require(bound.capture_identity == capture &&
                      bound.capture_sequence > 0 &&
                      static_cast<std::uintmax_t>(bound.capture_sequence) <=
                          drafts.size(),
                  "FieldSupplyForeignCaptureBound");
          if (bound.capture_sequence == sequence)
            ++count;
        };
        for (const auto &b : writes->condition_writes)
          add(b);
        for (const auto &b : writes->availability_writes)
          add(b);
        for (const auto &b : writes->condition_creations)
          add(b);
        return count;
      };
      require(drafts.size() <=
                  static_cast<std::size_t>(std::numeric_limits<Integer>::max()),
              "FieldSupplyCaptureSequenceOverflow");
      std::size_t first = 0;
      while (first < drafts.size() &&
             count_at(static_cast<Integer>(first + 1)) == 1)
        ++first;
      for (std::size_t n = first; n < drafts.size(); ++n)
        require(count_at(static_cast<Integer>(n + 1)) == 0,
                "FieldSupplyNonPrefixCaptureConsumption");
      const auto &supply = *source.original_supply;
      std::size_t cursor = first;
      if (supply.shortage > 0)
        for (const auto &actor : supply.mouth_ids) {
          if (!w.common.actor(actor))
            continue;
          require(
              cursor < drafts.size() &&
                  drafts[cursor].sequence == static_cast<Integer>(cursor + 1) &&
                  drafts[cursor].actor_id == actor && drafts[cursor].before &&
                  drafts[cursor].kind ==
                      ActorConditionExecutionDraft::Kind::mutation,
              "FieldSupplyActualTailActorMismatch");
          ++cursor;
        }
      require(cursor == drafts.size(), "FieldSupplyActualTailSizeMismatch");
      auto *capability = owner_.growth_capability_writer();
      require(capability && capability->bound_to(w.common, *w.growth_permanent),
              "FieldSupplyActualCapabilityRequired");
      const auto &early = core->characters.early;
      NativeDomainReadVersionSource versions(w, early.formations, early.returns,
                                             early.periods);
      auto current =
          w.make_current_read_stamp_reader(versions, {4096, 4096, 1024 * 1024});
      if (supply.shortage > 0) {
        auto applied = take(bind_field_military_supply_shortages(
            w.common, *w.actor_foundation, reads.character_conditions, ref,
            static_cast<Integer>(first + 1), *current, *w.read_stamps, versions,
            *capability));
        require(drafts.size() == original_draft_count &&
                    applied.size() == original_draft_count - first,
                "FieldSupplyActualBoundRequired");
        for (auto &bound : applied)
          writes->condition_writes.push_back(std::move(bound));
      }
      return common_success();
    } catch (Failure &e) {
      return {false, {}, std::move(e.error)};
    } catch (const std::bad_alloc &) {
      return common_failure("FieldSupplyResourceExhausted");
    } catch (const std::length_error &) {
      return common_failure("FieldSupplyResourceExhausted");
    }
  }
};
} // namespace
Outcome<std::unique_ptr<MilitaryActorConditionWrite>>
make_native_field_supply_condition_writer(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner) {
  try {
    return {true, std::make_unique<FieldSupplyWrite>(world, owner), {}};
  } catch (const std::bad_alloc &) {
    return {false, {}, common_failure("FieldSupplyResourceExhausted").error};
  }
}
} // namespace hanmo::resident
