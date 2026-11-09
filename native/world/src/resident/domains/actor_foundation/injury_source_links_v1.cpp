#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "condition_application_fact.hpp"
#include "condition_capture.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
bool same_original_key(const CharacterInjuryApplicationV1& a,
                       const CharacterInjuryApplicationV1& b) {
  if (a.actor_id != b.actor_id || a.original_source.index() != b.original_source.index())
    return false;
  if (const auto* battle = std::get_if<CharacterBattleReceiptSourceV1>(&a.original_source)) {
    const auto& other = std::get<CharacterBattleReceiptSourceV1>(b.original_source);
    // schema is not a new business key: changing it cannot reapply a result.
    return same_source_ref(battle->original_receipt, other.original_receipt);
  }
  if (const auto* rest = std::get_if<OriginalRestProvidedCause>(&a.original_source)) {
    const auto& other = std::get<OriginalRestProvidedCause>(b.original_source);
    // Authored rest completes once per action. Payload edits, including a forged
    // fulfillment ref, must not turn the same original action into a new key.
    // Long-rest periods are not admitted by this injury-link reader (see below).
    return rest->action_id == other.action_id;
  }
  const auto& treatment = std::get<CharacterTreatmentActionSourceV1>(a.original_source);
  const auto& other = std::get<CharacterTreatmentActionSourceV1>(b.original_source);
  return treatment.action_id == other.action_id &&
         treatment.completion_ordinal == other.completion_ordinal;
}
} // namespace
Status validate_character_injury_source_links_v1(
    const CommonStore& common, const AuthoredActorFoundationState& state,
    const ActorFoundationCatalog& catalog, const CharacterInjurySourcesStateV1& sources) {
  auto selected = condition_application_facts_selected(
      catalog.identity, ConditionApplicationHistoryOwner::injury_sources);
  if (!selected.ok) return {false, {}, selected.error};
  if (!selected.value)
    return common_failure("CharacterInjurySourceProfileRequired", {}, ErrorKind::unsupported_capability);
  auto headers = validate_condition_application_fact_headers(common, state, catalog);
  if (!headers.ok) return headers;
  const auto& applications = sources.applications;
  const auto& receipts = state.character_condition_extensions.applied_receipts;
  if (applications.order().size() != applications.rows().size())
    return common_failure("CharacterInjuryApplicationOrderInvalid");
  std::size_t next = 0;
  // Original receipt order has already been matched to the actual Common heads.
  // Filter that order; never reconstruct it from M, result ID or lexical order.
  for (const auto& item : receipts.rows()) {
    const auto& receipt = item.second;
    const auto* cause = std::get_if<CharacterInjurySourceRefV1>(&receipt.cause);
    const auto* key = std::get_if<CharacterInjuryConditionBusinessKeyV1>(&receipt.business_key);
    if (!cause && !key) {
      if (applications.find(item.first))
        return common_failure("CharacterInjuryApplicationUnexpected", item.first);
      continue;
    }
    if (!cause || !key || cause->result_id != item.first ||
        key->source_ref.result_id != item.first || key->actor_id != receipt.actor_id)
      return common_failure("CharacterInjuryReceiptLinkMismatch", item.first);
    if (next >= applications.rows().size() || applications.order()[next] != item.first ||
        applications.rows()[next].first != item.first)
      return common_failure("CharacterInjuryApplicationOrderInvalid", item.first);
    const auto& application = applications.rows()[next].second;
    if (applications.find(item.first) != &application || application.actor_id != receipt.actor_id)
      return common_failure("CharacterInjuryApplicationActorMismatch", item.first);
    if (const auto* rest = std::get_if<OriginalRestProvidedCause>(&application.original_source)) {
      // The live long-rest owner only writes HP/fatigue and puts the actual
      // period ordinal in RestConditionBusinessKey, absent from source/1.
      // Do not turn its action-wide fulfillment ref into a fabricated injury
      // period key. Its original route remains untouched, pending source-owner
      // contract reconciliation; this guard is not a new long-rest rule.
      if (rest->rest_kind != "rest")
        return common_failure("CharacterInjuryRestSourceKindNotAdmitted", item.first,
                              ErrorKind::unsupported_capability);
    }
    for (std::size_t i = 0; i < next; ++i)
      if (same_original_key(applications.rows()[i].second, application))
        return common_failure("CharacterInjuryOriginalSourceAlreadyApplied", item.first);
    bool injury_seen = false;
    for (const auto& change : receipt.scalar_changes) {
      if (change.field != ConditionScalarChange::Field::injury) continue;
      if (injury_seen || change.before != application.progress.before.injury ||
          change.after != application.progress.after.injury)
        return common_failure("CharacterInjuryScalarLinkMismatch", item.first);
      injury_seen = true;
    }
    // A real unwounded participant may write only HP/fatigue. This is not a
    // license to change injury without the original same-candidate scalar write.
    if (!injury_seen && application.progress.before.injury != application.progress.after.injury)
      return common_failure("CharacterInjuryScalarWriteMissing", item.first);
    ++next;
  }
  if (next != applications.rows().size())
    return common_failure("CharacterInjuryApplicationUnreferenced");
  return common_success();
}
Status validate_character_injury_bound_source_link_v1(
    const CommonStore& common, const AuthoredActorFoundationState& state,
    const ActorFoundationCatalog& catalog, const CharacterInjurySourcesStateV1& sources,
    const BoundActorConditionWrite& bound) {
  auto actual = read_actual_actor_condition_write(common, state, bound);
  if (!actual.ok) return {false, {}, actual.error};
  if (!actual.value ||
      !std::holds_alternative<CharacterInjurySourceRefV1>(actual.value->cause))
    return common_failure("CharacterInjuryActualBoundWriteRequired", bound.receipt_ref.result_id);
  // A prior/source-copy pointer cannot certify the newly bound candidate write.
  if (sources.applications.rows().empty() ||
      sources.applications.rows().back().first != actual.value->result_id ||
      state.character_condition_extensions.applied_receipts.rows().empty() ||
      &state.character_condition_extensions.applied_receipts.rows().back().second != actual.value)
    return common_failure("CharacterInjuryBoundApplicationNotLast", bound.receipt_ref.result_id);
  return validate_character_injury_source_links_v1(common, state, catalog, sources);
}
} // namespace hanmo::resident
