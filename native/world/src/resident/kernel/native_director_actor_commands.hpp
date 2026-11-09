// 原 Actor 命令与 Director6 的真实接受桥；只借同一个未发布世界候选。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/director/director_native_executor2.hpp"
#include "resident/domains/director/director_native_subjects2.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
// 同一个拥有 lease 同时把此对象交给 owner reader 与 executor；预采用核验只认
// 本次真实写入与已封口 Common 结果。冷读改读原 Started，不反借未来 adoption。
class NativeDirectorActorCommandBindings final
    : public DirectorActorCommandEffectRead,
      public DirectorActorCommandWrite2 {
  struct Pending;
  const WorldBundle &world_;
  WorldBundle *writable_ = nullptr;
  const CompleteExactWorldReadBindings *actual_ = nullptr;
  CheckpointUniquePtr<Pending> pending_;
  Status selection() const;
  Status message_selection() const;
  Status pending_identity(const StableId &, const SourceRef &) const;
  Status validate_started_effect(const StableId &, const SourceRef &) const;

public:
  explicit NativeDirectorActorCommandBindings(
      const WorldBundle &,
      const CompleteExactWorldReadBindings *actual = nullptr);
  explicit NativeDirectorActorCommandBindings(
      WorldBundle &, const CompleteExactWorldReadBindings *actual = nullptr);
  ~NativeDirectorActorCommandBindings();
  NativeDirectorActorCommandBindings(
      const NativeDirectorActorCommandBindings &) = delete;
  NativeDirectorActorCommandBindings &
  operator=(const NativeDirectorActorCommandBindings &) = delete;
  NativeDirectorActorCommandBindings(NativeDirectorActorCommandBindings &&) =
      delete;
  NativeDirectorActorCommandBindings &
  operator=(NativeDirectorActorCommandBindings &&) = delete;
  bool bound_to(const CommonStore &) const noexcept override;
  bool bound_to(const CommonStore &,
                const DirectorState &) const noexcept override;
  Status validate_actor_command_effect(const StableId &,
                                       const SourceRef &) const override;
  // 原 Growth 开工、receipt2 托留/领读、Reply4/Invitation2 及真实公告
  // Inspect。actual 借同一 owning lease， 在 NPC seal/adoption
  // 尾不重新构造外层或捕获尚未绑定的 P10。
  Outcome<SubjectDecisionApplied2>
  apply_actor_command_in_candidate(const SubjectDecisionProposal2 &) override;
  Status finalize_actor_adoption_in_candidate(const SubjectDecisionProposal2 &,
                                              const DirectorAdoptionResult2 &,
                                              const WorldMoment &) override;
};
} // namespace hanmo::resident
