#include "native_role_dialogue_binding.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
// 只借作者明确点名的主对话；不从同岗位的多个对话中任挑一个。
Outcome<const DialogueDefinition *>
selected_dialogue(const InteractionCatalog &catalog,
                  const ActorDialogueDefinition &actor) {
  if (!actor.dialogue_id)
    return {true, nullptr, {}};
  if (actor.dialogue_id->empty())
    return {
        false, nullptr,
        common_failure("NativeRoleEmptyActorDialogue", actor.actor_id).error};
  const DialogueDefinition *found = nullptr;
  for (const auto &dialogue : catalog.dialogues) {
    if (dialogue.id != *actor.dialogue_id)
      continue;
    if (found)
      return {false, nullptr,
              common_failure("NativeRoleAmbiguousActorDialogue", actor.actor_id)
                  .error};
    found = &dialogue;
  }
  if (!found)
    return {
        false, nullptr,
        common_failure("NativeRoleOriginalActorDialogueMissing", actor.actor_id)
            .error};
  return {true, found, {}};
}
} // namespace
Status bind_native_role_dialogues_from_original_actor_dialogues(
    CommonCatalog &common, const InteractionCatalog &interactions) {
  if (interactions.common.get() != &common)
    return common_failure("NativeRoleDialogueSamePrepareRequired");
  // 先验完整原连接再写私有候选；缺字段并不授权伪造对话或填任意后备值。
  for (std::size_t i = 0; i < interactions.actor_dialogues.size(); ++i) {
    const auto &actor = interactions.actor_dialogues[i];
    if (actor.actor_id.empty())
      return common_failure("NativeRoleActorIdentityRequired");
    for (std::size_t j = 0; j < i; ++j)
      if (interactions.actor_dialogues[j].actor_id == actor.actor_id)
        return common_failure("NativeRoleDuplicateActorDialogue",
                              actor.actor_id);
    auto selected = selected_dialogue(interactions, actor);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (!selected.value || selected.value->context_role.empty())
      continue;
    const auto *role = common.roles.find(selected.value->context_role);
    if (!role || role->id != selected.value->context_role ||
        role->initial_actor != actor.actor_id)
      return common_failure("NativeRoleOriginalDialogueActorMismatch",
                            actor.actor_id);
  }
  // 字符串由同 job 的原解码预算收费；失败交外层整个 prepare 候选撤销。
  for (const auto &actor : interactions.actor_dialogues) {
    auto selected = selected_dialogue(interactions, actor);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (!selected.value || selected.value->context_role.empty())
      continue;
    auto *role = common.roles.find(selected.value->context_role);
    if (!role->dialogue_id)
      role->dialogue_id = checkpoint_decode_boundary_string(selected.value->id);
  }
  return common_success();
}
} // namespace hanmo::resident
