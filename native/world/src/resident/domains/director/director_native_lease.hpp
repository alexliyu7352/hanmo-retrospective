// Concrete original Director adapters, retained with the whole native world
// dependency owner. No second policy, source registry, scheduler, or executor.
#pragma once
#include "director_exact_runtime.hpp"
#include "director_native_executor2.hpp"
#include "director_native_subjects2.hpp"
#include "resident/domains/tasks/work_execution/work_native_lease.hpp"
namespace hanmo::resident {
class CompleteExactWorldBindingOwner;
struct NativeDirectorExactDependencies {
  const WorldBundle &world;
  DirectorActorProposalContext2 actors;
  const ActivityRead &activity;
  const ExactResultTimeRead &results;
  const ExactResultReadStamp &result_stamp;
  const CurrentMomentRead &clock;
  const DirectorActorCommandEffectRead &actor_effects;
  const DirectorAuthorityWakeRead2 &authorities;
  const CharacterConditionSources &conditions;
  // 仅真实已选 task_work_execution /5 或 /6 借入 Work。
  // 不从当前任务表或本人所选命令推断是否启用。
  const NativeTaskWorkSources *work = nullptr;
  const HouseholdPurchaseContext *household = nullptr;
  const HouseholdPurchaseKnownMarketRead *household_knowledge = nullptr;
  const DirectorInstitutionalSourceRead2 *institutions = nullptr;
  DirectorConversationResponse conversation_response;
  bool propagation_selected = false;
  const PropagationReadContext *propagation = nullptr;
  // 实际保活的 S5/S7 家户图可独立于 Task Work 被借入。
  const NativeHouseholdServiceLease *household_owner = nullptr;
};
class NativeDirectorExactSources final {
  NativeDirectorExactDependencies dependencies_;
  std::optional<DirectorWorkWakeRead2> work_;
  std::optional<DirectorCohortSourceContext2> cohort_context_;
  std::optional<NativeDirectorCohortSources2> cohorts_;
  std::optional<CohortPurchaseDirectorRoute2> cohort_purchase_;
  NativeDirectorSubjectContext2 subject_context_;
  NativeDirectorSubjectOwnerRead subjects_;

public:
  explicit NativeDirectorExactSources(NativeDirectorExactDependencies);
  NativeDirectorExactSources(const NativeDirectorExactSources &) = delete;
  NativeDirectorExactSources &
  operator=(const NativeDirectorExactSources &) = delete;
  NativeDirectorExactSources(NativeDirectorExactSources &&) = delete;
  NativeDirectorExactSources &operator=(NativeDirectorExactSources &&) = delete;
  Status validate_bindings() const;
  Status validate_sources() const;
  DirectorExactReadBinding read_binding() const;
  const NativeDirectorSubjectOwnerRead &subjects() const { return subjects_; }
  const NativeDirectorExactDependencies &dependencies() const {
    return dependencies_;
  }
};
class NativeDirectorExactReadLease final : public DirectorExactReadLease,
                                           public CheckpointAllocatedObject {
  // Must be first: original contexts outlive every adapter borrowing them.
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  const CompleteExactWorldBindingOwner *actual_owner_ = nullptr;
  NativeDirectorExactSources sources_;

public:
  NativeDirectorExactReadLease(std::unique_ptr<CompleteExactWorldBindingOwner>,
                               NativeDirectorExactDependencies);
  // The enclosing native factory retains this same owner until the inner lease
  // is destroyed. This overload borrows it; it never takes or reacquires it.
  NativeDirectorExactReadLease(const CompleteExactWorldBindingOwner &,
                               NativeDirectorExactDependencies);
  ~NativeDirectorExactReadLease();
  NativeDirectorExactReadLease(const NativeDirectorExactReadLease &) = delete;
  NativeDirectorExactReadLease &
  operator=(const NativeDirectorExactReadLease &) = delete;
  NativeDirectorExactReadLease(NativeDirectorExactReadLease &&) = delete;
  NativeDirectorExactReadLease &
  operator=(NativeDirectorExactReadLease &&) = delete;
  Status validate_bindings() const;
  Status validate_sources() const;
  DirectorExactReadBinding reads() const override {
    return sources_.read_binding();
  }
  const NativeDirectorExactSources &sources() const { return sources_; }
};
struct NativeDirectorExactWriteDependencies {
  NativeDirectorExactDependencies read;
  NativeDirectorExecutorContext2 execution;
  PropagationChannelWrite *propagation_writer = nullptr;
};
class NativeDirectorExactWriteLease final : public DirectorExactWriteLease,
                                            public CheckpointAllocatedObject {
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  CompleteExactWorldBindingOwner *actual_owner_ = nullptr;
  NativeDirectorExactWriteDependencies dependencies_;
  NativeDirectorExactSources sources_;
  NativeDirectorSubjectExecutor executor_;

public:
  NativeDirectorExactWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner>,
                                NativeDirectorExactWriteDependencies);
  NativeDirectorExactWriteLease(CompleteExactWorldBindingOwner &,
                                NativeDirectorExactWriteDependencies);
  ~NativeDirectorExactWriteLease();
  NativeDirectorExactWriteLease(const NativeDirectorExactWriteLease &) = delete;
  NativeDirectorExactWriteLease &
  operator=(const NativeDirectorExactWriteLease &) = delete;
  NativeDirectorExactWriteLease(NativeDirectorExactWriteLease &&) = delete;
  NativeDirectorExactWriteLease &
  operator=(NativeDirectorExactWriteLease &&) = delete;
  Status validate_bindings() const;
  DirectorExactWriteBinding writes() override;
  const NativeDirectorExactSources &sources() const { return sources_; }
};
// 直接借入同一完整世界 owner；身体及实际选中的 Work/家户依赖均由它保活。
// 不保存指向工厂局部时刻 at 的引用；构造绑定检查与原来源完整校验仍分开。
Outcome<std::shared_ptr<const DirectorExactReadLease>>
make_native_director_read_lease_in_owner(const WorldBundle &,
                                         const CompleteExactWorldBindingOwner &,
                                         NativeDirectorExactDependencies,
                                         const WorldMoment &);
Outcome<std::shared_ptr<DirectorExactWriteLease>>
make_native_director_write_lease_in_owner(WorldBundle &,
                                          CompleteExactWorldBindingOwner &,
                                          NativeDirectorExactWriteDependencies,
                                          const WorldMoment &);
} // namespace hanmo::resident
