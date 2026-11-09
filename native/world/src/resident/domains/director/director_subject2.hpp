#pragma once
#include "director_model.hpp"
#include "director_subject2_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/affairs/state_need.hpp"
#include <string_view>
namespace hanmo::resident {
struct HouseholdReservePurchaseProposal;
class ProjectionWriter;
struct CohortKnownSubjectView {
  SubjectRef subject;
  Integer source_revision = 0;
  StableId household_id;
  SourceRef location_source_ref;
  Integer labor_capacity = 0, labor_allocated = 0;
  SourceRef accepted_policy_ref, controller_authority_ref;
  std::vector<StableId> wallet_refs, inventory_refs;
  std::vector<SourceRef> purchase_responsibilities, transport_responsibilities,
      known_market_refs, known_report_refs;
};
struct OrganizationKnownSubjectView {
  SubjectRef subject;
  Integer organization_revision = 0;
  std::vector<SourceRef> approved_intent_refs, executable_obligations,
      continuity_refs, known_report_refs, known_dossier_refs;
  std::vector<StableId> approved_wallet_refs, approved_inventory_refs,
      eligible_office_refs;
};
using DirectorKnownSubject2 =
    std::variant<KnownSubjectView, CohortKnownSubjectView,
                 OrganizationKnownSubjectView>;
enum class DirectorDecisionRoute2 {
  actor,
  cohort_purchase,
  cohort_household_routine,
  institutional_continuation,
  await_officer
};
struct SubjectAdmissionRead2 {
  DirectorRegistration2 registration;
  DirectorDecisionRoute2 route = DirectorDecisionRoute2::actor;
  bool source_active = false, has_frozen_responsibility = false;
  StableId template_id;
};
struct SubjectDecisionProposal2 {
  SubjectRef subject;
  Integer registration_epoch = 0, decision_seq = 0;
  StableId policy_id, plan_id, template_id, goal_ref;
  DirectorDecisionRoute2 route = DirectorDecisionRoute2::actor;
  std::optional<DecisionProposal> actor_proposal;
  // Ephemeral original S5 quote; persisted plans retain only real owner source
  // references.
  std::shared_ptr<const HouseholdReservePurchaseProposal> cohort_purchase_quote;
  std::optional<SubjectExecutionBinding> binding;
  Integer next_step_index = 0;
  PlanPhase phase = PlanPhase::waiting;
  std::vector<StableId> commitment_ids, known_report_ids;
  StableId wait_reason;
  SourceRef policy_source_ref;
  Integer owner_source_revision = 0;
  std::vector<ReadVersion> expected_versions;
};
struct SubjectDecisionApplied2 {
  // 只保运行时返回深值；入原持久表时仍按该表的实际复制边界独立计费。
  CheckpointCopyLease payload_memory;
  DecisionOutcome outcome;
  std::optional<SourceRef> applied_effect_ref;
  std::optional<SubjectExecutionBinding> actual_binding;
  std::optional<DirectorAcceptedCommand> accepted_command;
  SubjectDecisionApplied2() = default;
  SubjectDecisionApplied2(const SubjectDecisionApplied2 &) = delete;
  SubjectDecisionApplied2 &operator=(const SubjectDecisionApplied2 &) = delete;
  // 移动尚未完成时，两端共享原实际 payload 的租约，不复制深值后冒用同一份账。
  SubjectDecisionApplied2(SubjectDecisionApplied2 &&other)
      : payload_memory(other.payload_memory), outcome(std::move(other.outcome)),
        applied_effect_ref(std::move(other.applied_effect_ref)),
        actual_binding(std::move(other.actual_binding)),
        accepted_command(std::move(other.accepted_command)) {}
  SubjectDecisionApplied2 &operator=(SubjectDecisionApplied2 &&other) {
    if (this != &other) {
      auto incoming = other.payload_memory;
      // 任一字段移动抛错，目标仍保两份原租约，直到剩余深值正常析构。
      payload_memory = CheckpointCopyLease::hold_both(payload_memory, incoming);
      outcome = std::move(other.outcome);
      applied_effect_ref = std::move(other.applied_effect_ref);
      actual_binding = std::move(other.actual_binding);
      accepted_command = std::move(other.accepted_command);
      payload_memory = std::move(incoming);
    }
    return *this;
  }
};
// These are five finite owner routes. No arbitrary CommandData is deserialized
// for a non-Actor.
class DirectorSubjectOwnerRead {
public:
  virtual ~DirectorSubjectOwnerRead() = default;
  virtual Outcome<std::vector<SubjectAdmissionRead2>>
  selected_admissions() const = 0;
  virtual Outcome<SubjectAdmissionRead2>
  current_admission(const SubjectRef &) const = 0;
  virtual Outcome<DirectorKnownSubject2>
  known_subject(const SubjectRef &) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose_actor(const DirectorRegistration2 &, const KnownSubjectView &,
                Integer candidates) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose_cohort_purchase(const DirectorRegistration2 &,
                          const CohortKnownSubjectView &,
                          Integer candidates) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose_cohort_household_routine(const DirectorRegistration2 &,
                                   const CohortKnownSubjectView &,
                                   Integer candidates) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose_institutional_continuation(const DirectorRegistration2 &,
                                     const OrganizationKnownSubjectView &,
                                     Integer candidates) const = 0;
  virtual Outcome<std::optional<SubjectDecisionProposal2>>
  propose_await_officer(const DirectorRegistration2 &,
                        const OrganizationKnownSubjectView &,
                        Integer candidates) const = 0;
  virtual Status validate_plan_binding(const SubjectPlanBinding &) const = 0;
  // Each selected template checks its actual finite step and original business
  // source.
  virtual Status validate_plan(const FinitePlan &) const = 0;
  // 只从本次真实LC01写结果返回原A地址；没有实际owner不能拿任意字符串推进。
  virtual Outcome<const StableId *>
  read_family_applied_goal5(const SubjectDecisionProposal2 &proposal,
                            const SubjectDecisionApplied2 &) const {
    return {false, nullptr,
            common_failure("DirectorFamilyAppliedGoalReaderRequired",
                           proposal.plan_id, ErrorKind::unsupported_capability)
                .error};
  }
  virtual Status validate_applied_effect(const SubjectRef &,
                                         DirectorDecisionRoute2,
                                         const SourceRef &) const = 0;
  virtual Status validate_continuation(const SubjectContinuation &) const = 0;
  virtual Status validate_wake(const SubjectWake2 &) const = 0;
  // Enumerates only actually applied original sources of admitted subjects.
  // Used to rebuild the hot wake index, not to create messages, memory,
  // authority or effects.
  virtual Status visit_actual_wakes(
      const std::function<Status(const SubjectWake2 &, const WorldMoment &)> &)
      const = 0;
};
class DirectorSubjectExecutor {
public:
  virtual ~DirectorSubjectExecutor() = default;
  // The caller runs these through the complete WorldBundle candidate/savepoint,
  // never Common-only rollback.
  virtual Outcome<SubjectDecisionApplied2>
  apply_actor(const SubjectDecisionProposal2 &) = 0;
  virtual Outcome<SubjectDecisionApplied2>
  apply_cohort_purchase(const SubjectDecisionProposal2 &) = 0;
  virtual Outcome<SubjectDecisionApplied2>
  apply_cohort_household_routine(const SubjectDecisionProposal2 &) = 0;
  virtual Outcome<SubjectDecisionApplied2>
  apply_institutional_continuation(const SubjectDecisionProposal2 &) = 0;
  virtual Outcome<SubjectDecisionApplied2>
  apply_await_officer(const SubjectDecisionProposal2 &) = 0;
  // 物理6本人或已选物理5家庭B：原Common结果已经真正封存，同候选可见
  // 此次实际adoption、plan和schedule；此尾完成来源绑定与真实S0收知。
  // 最外层才全世界核验并发布，失败撤整个候选，不能只撤采用这一行。
  virtual Status
  finalize_actor_adoption_in_candidate(const SubjectDecisionProposal2 &,
                                       const DirectorAdoptionResult2 &,
                                       const WorldMoment &) {
    return common_failure("DirectorActorAdoptionFinalizerRequired");
  }
};
StableId director_subject_key2(const SubjectRef &);
StableId director_adoption_key2(const SubjectRef &, Integer registration_epoch,
                                Integer decision_seq);
const FinitePlan *find_director_plan2(const DirectorState &,
                                      const StableId &plan_id);
Status reconcile_director_subjects2(const CommonStore &, DirectorState &,
                                    const DirectorCatalog &,
                                    const DirectorSubjectOwnerRead &,
                                    const WorldMoment &actual);
Status rebuild_director_wake_cache2(DirectorState &,
                                    const DirectorSubjectOwnerRead &,
                                    const WorldMoment &);
Status wake_director_subject2(DirectorState &, const SubjectWake2 &,
                              const WorldMoment &actual,
                              const DirectorSubjectOwnerRead &);
Outcome<std::vector<SubjectRef>> director_due_subjects2(const DirectorState &,
                                                        const DirectorCatalog &,
                                                        Integer tick);
Outcome<std::optional<SubjectDecisionProposal2>>
propose_director_subject2(const DirectorState &, const DirectorCatalog &,
                          const SubjectRef &, Integer tick,
                          const DirectorSubjectOwnerRead &);
// Borrow the original last adopted/rejected/waiting transition for this exact
// key. A matching retry never invokes a business writer or consumes another
// boundary slot.
Outcome<const DirectorAdoptionResult2 *>
read_director_adoption_result2(const DirectorState &,
                               const SubjectDecisionProposal2 &,
                               const DirectorSubjectOwnerRead &);
Status adopt_director_subject2(const CommonStore &, DirectorState &,
                               const DirectorCatalog &,
                               const SubjectDecisionProposal2 &,
                               const DirectorSubjectOwnerRead &,
                               DirectorSubjectExecutor &,
                               const WorldMoment &actual);
Status continue_director_subject2(DirectorState &, const SubjectContinuation &,
                                  const DirectorSubjectOwnerRead &,
                                  const WorldMoment &actual);
Status validate_director_subjects2(const CommonStore &, const DirectorState &,
                                   const DirectorCatalog &,
                                   const DirectorSubjectOwnerRead &,
                                   const WorldMoment &actual,
                                   const SocialState *actual_social = nullptr);
std::vector<SourceRef> director_subject_source_pins2(const DirectorState &);
Outcome<DirectorState> decode_director_state6(const BoundaryValue &,
                                              bool regional_life,
                                              bool named_membership);
Outcome<BoundaryValue> encode_director_state6(const DirectorState &);
Status visit_director_checkpoint6(CodecVisitor &, DirectorState &,
                                  bool regional_life, bool named_membership);
Status emit_director_state6_projection(ProjectionWriter &, std::string_view,
                                       const DirectorState &);
Outcome<DirectorState> decode_director_state2(const BoundaryValue &,
                                              bool regional_life,
                                              bool named_membership);
Outcome<BoundaryValue> encode_director_state2(const DirectorState &);
Status visit_director_checkpoint2(CodecVisitor &, DirectorState &,
                                  bool regional_life, bool named_membership);
class ProjectionWriter;
Status emit_director_state2_projection(ProjectionWriter &, std::string_view,
                                       const DirectorState &);
Outcome<DirectorSubjects2State>
decode_director_subjects2(const BoundaryValue &);
Outcome<BoundaryValue>
encode_director_subjects2(const DirectorSubjects2State &);
Status visit_director_subjects2_checkpoint(CodecVisitor &,
                                           DirectorSubjects2State &);
class ProjectionWriter;
Status emit_director_subjects2_projection(ProjectionWriter &, std::string_view,
                                          const DirectorSubjects2State &);
} // namespace hanmo::resident
