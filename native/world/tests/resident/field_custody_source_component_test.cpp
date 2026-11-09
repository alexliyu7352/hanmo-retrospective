#include "field_custody_source_component_test.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/character_condition_replay.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include <stdexcept>
#include <limits>
namespace hanmo::resident {
namespace {
// 精确错误码只核实际 typed 合同；原 source 失败不得容错继续记为命中。
void require(bool accepted, const std::string &message) {
  if (!accepted) throw std::runtime_error("Field custody source component: " + message);
}
void rejected(const Outcome<const CapacityTransitionV2 *> &value, const char *code,
              FieldCustodySourceComponentCoverage &coverage) {
  require(!value.ok && value.error.original.code == code,
          std::string("expected ") + code + ", actual " + value.error.original.code);
  ++coverage.negative_cases;
}
} // namespace
void check_field_custody_source_components(
    const CharacterConditionSources &body, FieldCustodySourceComponentCoverage &coverage) {
  for (const auto &[id, row] : body.state.character_condition_extensions.capacity_transitions.rows()) {
    const auto *cause = std::get_if<MilitaryAvailabilityCauseV2>(&row.cause_ref);
    if (!cause || cause->source_schema != 1 || coverage.checked_transitions.contains(id)) continue;
    const SourceRef source{"actor_foundation", "capacity_transition", id};
    const auto actual = read_original_field_availability_transition_source(body, source, 2);
    require(actual.ok && actual.value == &row,
            "real Field transition read: " + actual.error.original.code);
    ++coverage.positive_cases;
    rejected(read_original_field_availability_transition_source(body, source, 97),
             "MilitaryAvailabilityTypedSourceRequired", coverage);
    auto foreign = source;
    foreign.domain = "foreign_actor_foundation";
    rejected(read_original_field_availability_transition_source(body, foreign, 2),
             "MilitaryAvailabilityTypedSourceRequired", coverage);
    foreign = source;
    foreign.kind = "foreign_capacity_transition";
    rejected(read_original_field_availability_transition_source(body, foreign, 2),
             "MilitaryAvailabilityTypedSourceRequired", coverage);
    foreign = source;
    foreign.id += ":absent";
    rejected(read_original_field_availability_transition_source(body, foreign, 2),
             "MilitaryAvailabilityTransitionMissing", coverage);
    // 拷贝的只有借用 context；原根、行、时钟与已应用事实均不修改。
    auto missing_clock = body;
    missing_clock.injury_field_clock = nullptr;
    rejected(read_original_field_availability_transition_source(missing_clock, source, 2),
             "FieldAvailabilityOriginalSourceRequired", coverage);
    auto missing_military = body;
    missing_military.military = nullptr;
    rejected(read_original_field_availability_transition_source(missing_military, source, 2),
             "FieldAvailabilityOriginalSourceRequired", coverage);
    auto wrong_moment = body;
    wrong_moment.actual_now.half_day_tick = body.actual_now.half_day_tick == 0 ? 1 : 0;
    rejected(read_original_field_availability_transition_source(wrong_moment, source, 2),
             "FieldAvailabilityOriginalMomentMismatch", coverage);
    coverage.checked_transitions.insert(id);
  }
}
// 仅用已存军旅原头。坏 head/未来和真实后继导致的 stale 分开计数。
void check_field_military_head_source_components(
    const CharacterConditionSources &body, const GroupMovementExecutionState &groups,
    FieldCustodySourceComponentCoverage &coverage) {
  for (const auto &[id, opening] : groups.openings.rows()) {
    if (!opening.military_order || coverage.checked_military_openings.contains(id)) continue;
    const auto &accepted = *opening.military_order;
    const auto check_head = [&](const ActorConditionHead &head) {
      const auto actual = read_character_condition_verified_prefix(body, head, accepted.accepted_at);
      require(actual.ok && actual.value.head.actor_id == head.actor_id &&
              actual.value.head.condition_revision == head.condition_revision,
              "real military frozen head: " + actual.error.original.code);
      ++coverage.military_head_positive_cases;
      const auto reject_head = [&](const Outcome<CharacterConditionAtHead> &read, const char *code) {
        require(!read.ok && read.error.original.code == code,
                std::string("historical head expected ") + code + ", actual " + read.error.original.code);
        ++coverage.military_head_negative_cases;
      };
      auto broken = head;
      broken.latest_receipt_ref = ConditionReceiptRefV2{"component:absent:condition-receipt"};
      reject_head(read_character_condition_verified_prefix(body, broken, accepted.accepted_at),
                  "CharacterHistoricalConditionHeadMismatch");
      broken = head;
      broken.condition_revision = -1;
      reject_head(read_character_condition_verified_prefix(body, broken, accepted.accepted_at),
                  "CharacterHistoricalConditionBoundaryInvalid");
      if (body.actual_now.half_day_tick < std::numeric_limits<Integer>::max()) {
        auto future = body.actual_now;
        ++future.half_day_tick;
        reject_head(read_character_condition_verified_prefix(body, head, future),
                    "CharacterHistoricalConditionBoundaryInvalid");
      }
      // 不制造后继 receipt；只有 bank 真的存在更早于今日的后继才验 stale。
      for (const auto &[key, receipt] : body.state.character_condition_extensions.applied_receipts.rows()) {
        if (receipt.actor_id != head.actor_id || receipt.condition_revision_after <= head.condition_revision)
          continue;
        const auto order = compare_world_moments(receipt.effective_at.exact_moment, body.actual_now);
        require(order.ok, "real later receipt moment");
        if (order.value >= 0) continue;
        reject_head(read_character_condition_verified_prefix(body, head, body.actual_now),
                    "CharacterHistoricalConditionHeadStale");
        ++coverage.military_head_stale_cases;
        break;
      }
    };
    // finite-return 的 issuing 头是继承原命令来源，不冒本次新权限/新时刻。
    if (accepted.kind == MilitaryMarchOrderKind::campaign_depart)
      check_head(accepted.issuing_condition_head);
    check_head(accepted.commander_condition_head);
    coverage.checked_military_openings.insert(id);
  }
}
std::string field_custody_source_component_report(const FieldCustodySourceComponentCoverage &coverage) {
  return "FIELD_CUSTODY_SOURCE " +
      std::string(coverage.positive_cases ? "EXERCISED" : "NOT_EXERCISED") +
      " positive=" + std::to_string(coverage.positive_cases) +
      " negative=" + std::to_string(coverage.negative_cases) +
      "\nFIELD_MILITARY_HEAD_SOURCE " +
      std::string(coverage.military_head_positive_cases ? "EXERCISED" : "NOT_EXERCISED") +
      " positive=" + std::to_string(coverage.military_head_positive_cases) +
      " negative=" + std::to_string(coverage.military_head_negative_cases) +
      " stale=" + std::to_string(coverage.military_head_stale_cases);
}
} // namespace hanmo::resident
