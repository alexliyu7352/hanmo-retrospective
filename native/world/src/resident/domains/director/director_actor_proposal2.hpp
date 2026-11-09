#pragma once
#include "director_subject2.hpp"
namespace hanmo::resident {
struct PublicSupplyInspectionReadContext;
class ExactResultTimeRead;
class ExactResultReadStamp;
// Same original finite Actor candidates. All selected business providers are
// borrowed from one current World lease; no alternate policy or command
// registry.
struct DirectorActorProposalContext2 {
  const CommonStore &common;
  const DirectorState &state;
  const DirectorCatalog &catalog;
  const OrganizationRead &organization;
  const AffairsState &affairs;
  const TaskState &tasks;
  const TaskCatalog &task_catalog;
  const CommonCatalog &common_catalog;
  const GrowthCatalog &growth;
  const TravelCatalog &travel;
  const OrganizationState &organizations;
  const OrganizationCatalog &organization_catalog;
  const GrowthAccess *growth_access = nullptr;
  const GrowthLifeRead *life = nullptr;
  const DirectorMembershipContext *membership = nullptr;
  const DirectorPoliticalContext *politics = nullptr;
  const DirectorDiplomaticContext *diplomacy = nullptr;
  const EconomicSupplyPolicyRead *issued_supply = nullptr;
  const TaskResponsibilityDirectorContext *task_responsibility = nullptr;
  const VisitReceptionRead *visit_reception = nullptr;
  const PersonalAppointmentAttendanceRead *appointment_attendance = nullptr;
  // 只续同世界已存在LC01计划；空值不使旧模板获得新家庭机会。
  const FamilyDirectorPostProposalContext5 *family_post = nullptr;
  // 只借原同世界公告读取；未选该来源时不扩旧 actor 私生活策略。
  const PublicSupplyInspectionReadContext *public_supply_inspection = nullptr;
  const ExactResultTimeRead *public_supply_results = nullptr;
  const ExactResultReadStamp *public_supply_stamp = nullptr;
};
Outcome<std::optional<SubjectAdmissionRead2>>
read_native_director_actor_admission2(
    const CommonStore &, const DirectorState &, const DirectorCatalog &,
    const OrganizationRead &, const TaskState &, const ActivityRead &,
    const StableId &,
    const DirectorConversationResponse &original_conversation_response = {});
Outcome<std::optional<SubjectDecisionProposal2>>
propose_native_director_actor2(const DirectorActorProposalContext2 &,
                               const DirectorRegistration2 &,
                               const KnownSubjectView &, Integer candidates);
// The adapter's finite metadata must remain exactly the original transition.
Status validate_director_actor_proposal2(const SubjectDecisionProposal2 &);
} // namespace hanmo::resident
