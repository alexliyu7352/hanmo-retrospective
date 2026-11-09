#include "resident/kernel/catalog_section_observation.hpp"
// provider原声明是短期只读来源；不解释文字、不自动接受邀约，也不修改任何会话。
#include "dialogue_provider_source.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<AuthoredProviderSourceBorrowed> read_authored_provider_source_borrowed(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &provider) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::authored_read, 54, "InteractionCatalog", "read_authored_provider_source_borrowed", &catalog);

  auto meeting = read_authored_node_source_borrowed(common, state, catalog,
                                                    access, actor, provider);
  if (!meeting.ok)
    return {false, {}, meeting.error};
  if (*meeting.value.node_kind != "say")
    return {false, {}, common_failure("ProviderDeclarationRequired").error};
  const DialogueNode *node = meeting.value.actual_node;
  if (!node)
    return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
  const StableId *speaker =
      node->speaker == "$provider"
          ? &provider
          : (node->speaker == "$player" ? &actor : &node->speaker);
  if (*speaker != provider)
    return {false, {}, common_failure("ProviderDeclarationRequired").error};
  return {true, {meeting.value, speaker}, {}};
}
Outcome<OwnedAuthoredProviderSourceProof>
preview_authored_provider_source_owned(const CommonStore &common,
                                       const InteractionState &state,
                                       const InteractionCatalog &catalog,
                                       const InteractionAccess &access,
                                       const StableId &actor,
                                       const StableId &provider) {
  auto actual = read_authored_provider_source_borrowed(common, state, catalog,
                                                       access, actor, provider);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &v = actual.value.meeting;
  OwnedAuthoredProviderSourceProof out;


  (out.memory_).grow(0);
  out.proof_ = {copy_authored_node_source_preleased(v),
                checkpoint_fresh_string_copy(*actual.value.speaker_actor_id)};
  return {true, std::move(out), {}};
}

AuthoredProviderSourceProof copy_authored_provider_source_preleased(
    const AuthoredProviderSourceProof &source) {
  const auto &p = source.meeting;
  return {
      copy_authored_node_source_preleased(AuthoredNodeSourceBorrowed{
          &p.source_identity, &p.actor_id, &p.provider_actor_id, &p.session_id,
          &p.dialogue_id, &p.node_id, &p.node_kind, &p.context, nullptr}),
      checkpoint_fresh_string_copy(source.speaker_actor_id)};
}
// 旧裸值兼容入口不冒同 job 拥有闭合。
Outcome<AuthoredProviderSourceProof> preview_authored_provider_source(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &provider) {
  auto actual = preview_authored_provider_source_owned(common, state, catalog,
                                                       access, actor, provider);
  if (!actual.ok)
    return {false, {}, actual.error};
  return {true, actual.value.proof(), {}};
}
Status revalidate_authored_provider_source(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const AuthoredProviderSourceProof &proof) {
  auto checked = revalidate_authored_node_source(common, state, catalog, access,
                                                 proof.meeting);
  if (!checked.ok)
    return checked;
  auto fresh = read_authored_provider_source_borrowed(
      common, state, catalog, access, proof.meeting.actor_id,
      proof.meeting.provider_actor_id);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  if (proof.speaker_actor_id != *fresh.value.speaker_actor_id)
    return common_failure("StaleProviderDeclarationSource");
  return common_success();
}
} // namespace hanmo::resident
