#pragma once
#include "resident/model/commitment_read.hpp"
#include "appointments/personal_appointment.hpp"
#include "visit_appointments/visit_appointment_meeting.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_boundary.hpp"
namespace hanmo::resident {
struct S3CommitmentReadAccess {
 const CommonStore& common;const TaskState& tasks;const TaskCatalog& task_catalog;
 const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& domain_versions;ReadMode mode=ReadMode::committed_current;
 const InteractionState& interaction;const InteractionCatalog& interaction_catalog;const InteractionAccess& interaction_access;
 StableId requesting_actor;const CommonCatalog& common_catalog;
 const PersonalAppointmentCatalog* appointment_catalog=nullptr;const PersonalAppointmentState* appointments=nullptr;
 const VisitAppointmentState* invitations=nullptr;const VisitAppointmentMeetingState* meetings=nullptr;
 const PrivateShopOrderRead* shop_orders=nullptr;
 const AffairsState* affairs=nullptr;const KnownSubjectView* known_subject=nullptr;const TaskExecutionServices* execution=nullptr;
 const TaskMessageNoticeServices* notices=nullptr;const TaskMessageReturnServices* returns=nullptr;
 // Actual state-claim release preview, supplied by that original owner; no caller-authored available boolean.
 std::function<Outcome<CommitmentOperationGate>(const StateNeedTaskContract&)> state_claim_release;
};
class NativeS3CommitmentRead final:public CommitmentRuleRead {const S3CommitmentReadAccess&access_;public:explicit NativeS3CommitmentRead(const S3CommitmentReadAccess&a):access_(a){}Outcome<CommitmentLifecycleSummary> summary(const CommitmentRef&,const ReadStamp&)const override;};
}
