// One finite router over the original actors and original S5/S6
// responsibilities. All references borrow one candidate and one fresh actual
// read scope.
#pragma once
#include "director_actor_proposal2.hpp"
#include "director_authority_wake2.hpp"
#include "director_cohort_sources2.hpp"
#include "director_native_wakes2.hpp"
namespace hanmo::resident {
// Implemented by the actual central command commit. The original business
// source must belong to this actor and an actually sealed Common result/history
// pair. Validation runs before adoption exists; it cannot use that future row
// as proof.
class DirectorActorCommandEffectRead {
public:
  virtual ~DirectorActorCommandEffectRead() = default;
  virtual bool bound_to(const CommonStore &) const noexcept = 0;
  virtual Status validate_actor_command_effect(const StableId &actor,
                                               const SourceRef &) const = 0;
  // LC01采用前核本次真实typed命令与sealed结果；首发A/B还须所存
  // actor/epoch/decision_seq/policy全等。重复B须本次already_posted结果和
  // 原四根不变证明，保第一次B路由，不能把同actor任意旧结果当新采用。
  virtual Status
  validate_family_command_application5(const SubjectDecisionProposal2 &proposal,
                                       const SourceRef &) const {
    return common_failure("DirectorFamilyCommandApplicationReaderRequired",
                          proposal.plan_id, ErrorKind::unsupported_capability);
  }
};
// S6 retains original institutional approvals, archives and continuity. This
// finite reader is mandatory when institutional admissions are selected.
class DirectorInstitutionalSourceRead2 {
public:
  virtual ~DirectorInstitutionalSourceRead2() = default;
  virtual bool bound_to(const CommonStore &,
                        const DirectorState &) const noexcept = 0;
  virtual Status visit_admissions(
      const std::function<Status(const SubjectAdmissionRead2 &)> &) const = 0;
  virtual Outcome<SubjectAdmissionRead2>
  admission(const SubjectRef &) const = 0;
  virtual Outcome<OrganizationKnownSubjectView>
  known(const SubjectRef &) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose(const DirectorRegistration2 &, const OrganizationKnownSubjectView &,
          DirectorDecisionRoute2, Integer) const = 0;
  virtual Status validate_binding(const SubjectPlanBinding &) const = 0;
  virtual Status validate_plan(const FinitePlan &) const = 0;
  virtual Status validate_effect(const SubjectRef &, DirectorDecisionRoute2,
                                 const SourceRef &) const = 0;
  virtual Status validate_continuation(const SubjectContinuation &) const = 0;
  virtual Status visit_wakes(const DirectorOriginalWakeVisitor2 &) const = 0;
  virtual Status validate_wake(const SubjectWake2 &) const = 0;
};
struct NativeDirectorSubjectContext2 {
  const DirectorActorProposalContext2 &actors;
  const ActivityRead &activity;
  const InteractionState &interaction;
  const SocialState &social;
  const ExactResultTimeRead &results;
  const ExactResultReadStamp &stamp;
  const CurrentMomentRead &clock;
  const DirectorActorCommandEffectRead &actor_effects;
  const DirectorAuthorityWakeRead2 &authorities;
  const CharacterConditionSources &conditions;
  // 同一已准入身份选择 Task Work /5 或 /6 时，必须借入其真实唤醒来源。
  const DirectorWorkWakeRead2 *work = nullptr;
  const NativeDirectorCohortSources2 *cohorts = nullptr;
  const CohortPurchaseDirectorRoute2 *cohort_purchase = nullptr;
  const DirectorInstitutionalSourceRead2 *institutions = nullptr;
  DirectorConversationResponse conversation_response;
};
class NativeDirectorSubjectOwnerRead final : public DirectorSubjectOwnerRead {
  const NativeDirectorSubjectContext2 &x_;
  Status binding() const;

public:
  explicit NativeDirectorSubjectOwnerRead(
      const NativeDirectorSubjectContext2 &x)
      : x_(x) {}
  Status validate_bindings() const { return binding(); }
  bool bound_to(const CommonStore &, const DirectorState &) const noexcept;
  Outcome<std::vector<SubjectAdmissionRead2>>
  selected_admissions() const override;
  Outcome<SubjectAdmissionRead2>
  current_admission(const SubjectRef &) const override;
  Outcome<DirectorKnownSubject2>
  known_subject(const SubjectRef &) const override;
  Outcome<std::optional<SubjectDecisionProposal2>>
  propose_actor(const DirectorRegistration2 &, const KnownSubjectView &,
                Integer) const override;
  Outcome<std::optional<SubjectDecisionProposal2>>
  propose_cohort_purchase(const DirectorRegistration2 &,
                          const CohortKnownSubjectView &,
                          Integer) const override;
  Outcome<std::optional<SubjectDecisionProposal2>>
  propose_cohort_household_routine(const DirectorRegistration2 &,
                                   const CohortKnownSubjectView &,
                                   Integer) const override;
  Outcome<std::optional<SubjectDecisionProposal2>>
  propose_institutional_continuation(const DirectorRegistration2 &,
                                     const OrganizationKnownSubjectView &,
                                     Integer) const override;
  Outcome<std::optional<SubjectDecisionProposal2>>
  propose_await_officer(const DirectorRegistration2 &,
                        const OrganizationKnownSubjectView &,
                        Integer) const override;
  Status validate_plan_binding(const SubjectPlanBinding &) const override;
  Status validate_plan(const FinitePlan &) const override;
  Outcome<const StableId *>
  read_family_applied_goal5(const SubjectDecisionProposal2 &,
                            const SubjectDecisionApplied2 &) const override;
  Status validate_applied_effect(const SubjectRef &, DirectorDecisionRoute2,
                                 const SourceRef &) const override;
  Status validate_continuation(const SubjectContinuation &) const override;
  Status validate_wake(const SubjectWake2 &) const override;
  Status
  visit_actual_wakes(const DirectorOriginalWakeVisitor2 &) const override;
};
} // namespace hanmo::resident
