// 本人原声明仅从真实当面会话的provider说话节点派生；业务决定由消费领域绑定作者政策。
#pragma once
#include "dialogue_node_source.hpp"
namespace hanmo::resident {
// 保留真实发起者，speaker必须为原provider；不是可持久化的同意或代签许可证。
struct AuthoredProviderSourceProof {
  AuthoredNodeSourceProof meeting;
  StableId speaker_actor_id;
};
struct AuthoredProviderSourceBorrowed {
  AuthoredNodeSourceBorrowed meeting;
  const StableId *speaker_actor_id = nullptr;
};
Outcome<AuthoredProviderSourceBorrowed> read_authored_provider_source_borrowed(
    const CommonStore &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &, const StableId &);
class OwnedAuthoredProviderSourceProof {
  CheckpointMemoryCharge memory_;
  AuthoredProviderSourceProof proof_;
  friend Outcome<OwnedAuthoredProviderSourceProof>
  preview_authored_provider_source_owned(const CommonStore &,
                                         const InteractionState &,
                                         const InteractionCatalog &,
                                         const InteractionAccess &,
                                         const StableId &, const StableId &);

public:
  OwnedAuthoredProviderSourceProof() = default;
  OwnedAuthoredProviderSourceProof(const OwnedAuthoredProviderSourceProof &) =
      delete;
  OwnedAuthoredProviderSourceProof &
  operator=(const OwnedAuthoredProviderSourceProof &) = delete;
  OwnedAuthoredProviderSourceProof(
      OwnedAuthoredProviderSourceProof &&) noexcept = default;
  OwnedAuthoredProviderSourceProof &
  operator=(OwnedAuthoredProviderSourceProof &&) = delete;
  const AuthoredProviderSourceProof &proof() const noexcept { return proof_; }
};
Outcome<OwnedAuthoredProviderSourceProof>
preview_authored_provider_source_owned(const CommonStore &,
                                       const InteractionState &,
                                       const InteractionCatalog &,
                                       const InteractionAccess &,
                                       const StableId &, const StableId &);
// 消费者在复制到其实际拥有结果前，按同一完整字段规划，含 context 全
// optional/field_order。

// 先调用唯一planner并持有目标租约，再同步fresh复制；不是另一个业务验证入口。
AuthoredProviderSourceProof
copy_authored_provider_source_preleased(const AuthoredProviderSourceProof &);
// 仅原say节点可证明对方声明；choice、action、旁人或发起者发言一律拒绝。
Outcome<AuthoredProviderSourceProof> preview_authored_provider_source(
    const CommonStore &, const InteractionState &, const InteractionCatalog &,
    const InteractionAccess &, const StableId &actor_id,
    const StableId &provider_actor_id);
// 实际业务提交前重查同一世界、会话与原说话者，不反向制造NPC主动会话。
Status revalidate_authored_provider_source(const CommonStore &,
                                           const InteractionState &,
                                           const InteractionCatalog &,
                                           const InteractionAccess &,
                                           const AuthoredProviderSourceProof &);
} // namespace hanmo::resident
