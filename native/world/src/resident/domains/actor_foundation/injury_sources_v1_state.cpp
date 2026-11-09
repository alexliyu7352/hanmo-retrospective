#include "injury_sources_v1_state.hpp"
#include "condition_application_fact.hpp"
#include "injury_source_links_v1.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status validate_character_injury_sources_v1_layout(
    const CatalogIdentity &identity,
    const CharacterInjurySourcesStateV1 *state) {
  auto selected = condition_application_facts_selected(
      identity, ConditionApplicationHistoryOwner::injury_sources);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (selected.value != bool(state))
    return common_failure("CharacterInjurySourcesStateSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Outcome<CharacterInjurySourcesStateV1> make_initial_character_injury_sources_v1(
    const CommonStore &common, const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &catalog) {
  CharacterInjurySourcesStateV1 result;
  auto selected =
      validate_character_injury_sources_v1_layout(catalog.identity, &result);
  if (!selected.ok)
    return {false, {}, selected.error};
  // 唯一fresh入口。已有任意身体应用说明这不是可补空源的初始化切面。
  if (!foundation.character_condition_extensions.applied_receipts.empty())
    return {false,
            {},
            common_failure("CharacterInjuryFreshInitialReceiptBoundaryRequired")
                .error};
  auto linked = validate_character_injury_source_links_v1(common, foundation,
                                                          catalog, result);
  if (!linked.ok)
    return {false, {}, linked.error};
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
