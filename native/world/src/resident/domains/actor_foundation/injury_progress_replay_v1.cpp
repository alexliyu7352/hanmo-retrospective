#include "injury_progress_replay_v1.hpp"
#include "character_condition_replay.hpp"
#include "created_origin.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_service_coverage_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/storage/common_store.hpp"
#include "rest_condition.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
[[noreturn]] void fail(const char *code, const StableId &id) {
  throw Failure{common_failure(code, id).error};
}
bool same_progress(const GrowthRecoveryProgress &a,
                   const GrowthRecoveryProgress &b) {
  return a.injury == b.injury && a.points == b.points;
}
} // namespace
Outcome<GrowthRecoveryProgress> read_character_injury_progress_at_head_v1(
    const CharacterInjuryReplaySourcesV1 &source,
    const ActorConditionHead &expected, const WorldMoment &boundary) {
  try {
    const auto &body = source.conditions;
    take(validate_character_injury_source_links_v1(
        body.common, body.state, body.catalog, source.injury));
    // The explicit service catalog may be selected while source1 remains the
    // sole point owner. Legacy recovery records are rejected by whole coverage;
    // absence is ordinary Rest, not a fabricated treatment provider.
    take(validate_injury_recovery_selection_v1(body));
    const auto checked =
        take(read_character_condition_chain_cut(body, expected, boundary));
    const auto initial = take(read_character_initial_values_v2(
        body.common, body.state, body.catalog, body.family_catalog, body.shared,
        expected.actor_id, body.actual_now, body.born_sources));
    GrowthRecoveryProgress progress{initial.condition->injury, 0};
    Integer revision = 0;
    for (const auto &row :
         body.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = row.second;
      if (receipt.actor_id != expected.actor_id)
        continue;
      if (receipt.condition_revision_after > expected.condition_revision)
        break;
      if (receipt.condition_revision_before != revision)
        fail("CharacterInjuryProgressPrefixMismatch", row.first);
      const auto *ref = std::get_if<CharacterInjurySourceRefV1>(&receipt.cause);
      if (ref) {
        const auto *application =
            source.injury.applications.find(ref->result_id);
        if (!application ||
            !same_progress(application->progress.before, progress))
          fail("CharacterInjuryProgressBeforeMismatch", row.first);
        if (const auto *battle = std::get_if<CharacterBattleReceiptSourceV1>(
                &application->original_source)) {
          if (battle->original_receipt.domain == "personal") {
            if (!source.personal || !body.personal_catalog)
              fail("CharacterInjuryPersonalOwningSourcesRequired", row.first);
            const auto lifecycle =
                take(selected_native_personal_battle_lifecycle(
                    body.catalog.identity));
            take(validate_personal_injury_original_result_v1(
                body, source.injury, receipt, *source.personal,
                *body.personal_catalog, lifecycle));
          } else if (battle->original_receipt.domain == "military") {
            if (!source.field || !body.military)
              fail("CharacterInjuryFieldOwningSourcesRequired", row.first);
            take(validate_field_injury_original_result_v1(
                body, source.injury, receipt, *body.military, *source.field));
          } else
            fail("CharacterInjuryUnknownBattleOwner", row.first);
        } else if (std::holds_alternative<OriginalRestProvidedCause>(
                       application->original_source)) {
          take(validate_rest_injury_original_result_v1(body, source.injury,
                                                       receipt));
        } else {
          take(validate_treatment_injury_original_result_v1(body, source.injury,
                                                            receipt));
        }
        progress = application->progress.after;
      } else {
        // Original non-injury condition rows remain owned by their original
        // cause validator. They cannot silently change the sole injury/point
        // history.
        for (const auto &scalar : receipt.scalar_changes)
          if (scalar.field == ConditionScalarChange::Field::injury &&
              (scalar.before != progress.injury ||
               scalar.after != progress.injury))
            fail("CharacterInjuryUnreferencedChange", row.first);
      }
      revision = receipt.condition_revision_after;
    }
    if (revision != expected.condition_revision ||
        progress.injury != checked.body.injury)
      fail("CharacterInjuryProgressHeadMismatch", expected.actor_id);
    return {true, progress, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
