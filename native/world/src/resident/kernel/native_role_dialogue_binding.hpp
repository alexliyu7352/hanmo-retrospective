// 原作者主对话与岗位的有限连接；只用于尚未发布的同一 prepare 候选。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct InteractionCatalog;
// 仅从原 ActorDialogueDefinition 明选的唯一 Dialogue.context_role 补缺失字段。
// 已显式填写的角色字段不覆盖；私人无岗位对话不派生，歧义或错配返回具名错误。
Status bind_native_role_dialogues_from_original_actor_dialogues(
    CommonCatalog &private_prepare_candidate,
    const InteractionCatalog &original_interactions);
} // namespace hanmo::resident
