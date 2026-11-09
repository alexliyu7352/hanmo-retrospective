// 作者节点来源只证明当前会话确已到达该节点；求教、同意等业务语义由具名领域定义。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "interaction_model.hpp"
#include <string_view>
namespace hanmo::resident {
// 这是本次报价的完整来源，不可把execution_epoch写成跨档有效的永久许可。
struct AuthoredNodeSourceProof {
  WorldIdentity source_identity;
  StableId actor_id, provider_actor_id, session_id, dialogue_id, node_id,
      node_kind;
  InteractionContext context;
};
// 同步借用实际原会话/作者节点及输入身份；不保存本 view 自身，不跨根变更保留。
struct AuthoredNodeSourceBorrowed {
  const WorldIdentity *source_identity = nullptr;
  const StableId *actor_id = nullptr, *provider_actor_id = nullptr,
                 *session_id = nullptr, *dialogue_id = nullptr,
                 *node_id = nullptr, *node_kind = nullptr;
  const InteractionContext *context = nullptr;
  const DialogueNode *actual_node = nullptr;
};
Outcome<AuthoredNodeSourceBorrowed> read_authored_node_source_borrowed(
    const CommonStore &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &, const StableId &);

// 只规划上述已验实际字段的第一次复制，不是来源验证或第二会话。

// 调用方必须先按上面唯一计划持有目标租约；本口只fresh复制，不自行授予来源有效性。
AuthoredNodeSourceProof
copy_authored_node_source_preleased(const AuthoredNodeSourceBorrowed &);
class OwnedAuthoredNodeSourceProof {
  CheckpointMemoryCharge memory_;
  AuthoredNodeSourceProof proof_;
  friend Outcome<OwnedAuthoredNodeSourceProof>
  preview_authored_node_source_owned(const CommonStore &,
                                     const InteractionState &,
                                     const InteractionCatalog &,
                                     const InteractionAccess &,
                                     const StableId &, const StableId &);

public:
  OwnedAuthoredNodeSourceProof() = default;
  OwnedAuthoredNodeSourceProof(const OwnedAuthoredNodeSourceProof &) = delete;
  OwnedAuthoredNodeSourceProof &
  operator=(const OwnedAuthoredNodeSourceProof &) = delete;
  OwnedAuthoredNodeSourceProof(OwnedAuthoredNodeSourceProof &&) noexcept =
      default;
  OwnedAuthoredNodeSourceProof &
  operator=(OwnedAuthoredNodeSourceProof &&) = delete;
  const AuthoredNodeSourceProof &proof() const noexcept { return proof_; }
};
Outcome<OwnedAuthoredNodeSourceProof> preview_authored_node_source_owned(
    const CommonStore &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &, const StableId &);
// 仅查本人真实say/choice/待确认action；不读对方私人会话，不把经过文本当作本人意愿。
Outcome<AuthoredNodeSourceProof> preview_authored_node_source(
    const CommonStore &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &actor_id,
    const StableId &provider_actor_id);
// 具名领域写入请求或TrainingFact前重验当前完整来源、节点及当面条件；只读，不新建请求账。
Status revalidate_authored_node_source(const CommonStore &,
                                       const InteractionState &,
                                       const InteractionCatalog &,
                                       const InteractionAccess &,
                                       const AuthoredNodeSourceProof &);
} // namespace hanmo::resident
