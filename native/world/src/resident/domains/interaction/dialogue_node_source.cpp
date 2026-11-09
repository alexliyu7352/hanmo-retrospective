#include "resident/kernel/catalog_section_observation.hpp"
// 有限作者节点证明复用原当面与继续点约束；不授予命令执行权、NPC意愿或关系后果。
#include "dialogue_node_source.hpp"
#include "face_to_face_contact.hpp"
#include "interaction_actions.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_store.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {

InteractionContext copy_context_preleased(const InteractionContext &source) {
  InteractionContext out;
  out.place_id = checkpoint_fresh_string_copy(source.place_id);
  out.provider_actor_id =
      checkpoint_fresh_string_copy(source.provider_actor_id);
  out.role_id = checkpoint_fresh_string_copy(source.role_id);
  if (source.kind)
    out.kind = checkpoint_fresh_string_copy(*source.kind);
  if (source.encounter_id)
    out.encounter_id = checkpoint_fresh_string_copy(*source.encounter_id);
  if (source.input_error)
    out.input_error = checkpoint_fresh_string_copy(*source.input_error);
  out.field_order.reserve(source.field_order.size());
  for (const auto &field : source.field_order)
    out.field_order.push_back(checkpoint_fresh_string_copy(field));
  return out;
}
// 当前完整来源任一维变化均须重新报价，旧会话ID不能跨世界继续使用。
bool same_source(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 原上下文只比业务身份字段，字典排列不构成另一份接触权限。
bool same_context(const InteractionContext &a, const InteractionContext &b) {
  return a.place_id == b.place_id &&
         a.provider_actor_id == b.provider_actor_id && a.role_id == b.role_id &&
         a.kind == b.kind && a.encounter_id == b.encounter_id &&
         a.input_error == b.input_error;
}
} // namespace
Outcome<AuthoredNodeSourceBorrowed> read_authored_node_source_borrowed(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &provider) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::authored_read, 53, "InteractionCatalog", "read_authored_node_source_borrowed", &catalog);

  if (actor.empty() || provider.empty() || actor == provider)
    return {false, {}, common_failure("AuthoredNodeSourceRequired").error};
  const InteractionSession *current = nullptr;
  for (const auto &session : state.interactions)
    if (session.actor_id == actor) {
      if (current)
        return {false, {}, common_failure("AmbiguousAuthoredNodeSource").error};
      current = &session;
    }
  if (!current || current->context.provider_actor_id != provider)
    return {false, {}, common_failure("AuthoredNodeSourceRequired").error};
  if (current->phase != "say" && current->phase != "choice" &&
      current->phase != "action")
    return {false, {}, common_failure("AuthoredNodeSourceRequired").error};
  if (current->id.empty() || !current->pending_action_id.empty())
    return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
  if (!face_to_face_contact_context_valid(current->context))
    return {false, {}, common_failure("UnsupportedAuthoredNodeContext").error};
  const DialogueDefinition *dialogue = nullptr;
  for (const auto &row : catalog.dialogues)
    if (row.id == current->dialogue_id) {
      if (dialogue)
        return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
      dialogue = &row;
    }
  if (!dialogue || dialogue->context_role != current->context.role_id)
    return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
  if (current->context.kind) {
    bool bound = false;
    for (const auto &topic : catalog.regional_actor_topics)
      if (topic.actor_id == provider && topic.dialogue_id == dialogue->id)
        bound = true;
    if (!bound && access.born_personal_session) {
      auto actual = access.born_personal_session(*current);
      if (!actual.ok)
        return {false, {}, actual.error};
      bound = actual.value;
    }
    if (!bound && access.known_personal_session) {
      auto actual = access.known_personal_session(*current);
      if (!actual.ok)
        return {false, {}, actual.error};
      bound = actual.value;
    }
    if (!bound)
      return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
  }
  const DialogueNode *node = nullptr;
  for (const auto &row : dialogue->nodes)
    if (row.id == current->node_id) {
      if (node)
        return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
      node = &row;
    }
  if (!node || node->kind != current->phase)
    return {false, {}, common_failure("InvalidAuthoredNodeSource").error};
  // action仅证明原待确认节点；已启动waiting/trade不能再次被当作新的请求入口。
  if (node->kind == "action" &&
      (!node->command || !current->pending_command ||
       !same_dialogue_command(*node->command, *current->pending_command)))
    return {false, {}, common_failure("DialogueCommandMismatch").error};
  // 原 validator 成功只返回同一 ctx；先租真实 ctx
  // 全副本，覆盖其返回到本栈析构。

  (void)0;
  auto validation_memory = CheckpointMemoryCharge(0);
  auto checked = validate_interaction(common, state, catalog, access, actor,
                                      current->context);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true,
          {&common.state().identity, &actor, &provider, &current->id,
           &current->dialogue_id, &current->node_id, &node->kind,
           &current->context, node},
          {}};
}

AuthoredNodeSourceProof
copy_authored_node_source_preleased(const AuthoredNodeSourceBorrowed &v) {
  return {*v.source_identity,
          checkpoint_fresh_string_copy(*v.actor_id),
          checkpoint_fresh_string_copy(*v.provider_actor_id),
          checkpoint_fresh_string_copy(*v.session_id),
          checkpoint_fresh_string_copy(*v.dialogue_id),
          checkpoint_fresh_string_copy(*v.node_id),
          checkpoint_fresh_string_copy(*v.node_kind),
          copy_context_preleased(*v.context)};
}
Outcome<OwnedAuthoredNodeSourceProof> preview_authored_node_source_owned(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &provider) {
  auto actual = read_authored_node_source_borrowed(common, state, catalog,
                                                   access, actor, provider);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &v = actual.value;
  OwnedAuthoredNodeSourceProof out;


  (out.memory_).grow(0);
  out.proof_ = copy_authored_node_source_preleased(v);
  return {true, std::move(out), {}};
}
// 原裸 ABI 只作兼容；正式 owning consumer 必持上方结果到自己的计账复制完毕。
Outcome<AuthoredNodeSourceProof> preview_authored_node_source(
    const CommonStore &common, const InteractionState &state,
    const InteractionCatalog &catalog, const InteractionAccess &access,
    const StableId &actor, const StableId &provider) {
  auto actual = preview_authored_node_source_owned(common, state, catalog,
                                                   access, actor, provider);
  if (!actual.ok)
    return {false, {}, actual.error};
  return {true, actual.value.proof(), {}};
}
Status revalidate_authored_node_source(const CommonStore &common,
                                       const InteractionState &state,
                                       const InteractionCatalog &catalog,
                                       const InteractionAccess &access,
                                       const AuthoredNodeSourceProof &proof) {
  if (!same_source(proof.source_identity, common.state().identity))
    return common_failure("StaleAuthoredNodeSource");
  auto fresh = read_authored_node_source_borrowed(
      common, state, catalog, access, proof.actor_id, proof.provider_actor_id);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  const auto &actual = fresh.value;
  if (*actual.session_id != proof.session_id ||
      *actual.dialogue_id != proof.dialogue_id ||
      *actual.node_id != proof.node_id ||
      *actual.node_kind != proof.node_kind ||
      !same_context(*actual.context, proof.context))
    return common_failure("StaleAuthoredNodeSource");
  return common_success();
}
} // namespace hanmo::resident
