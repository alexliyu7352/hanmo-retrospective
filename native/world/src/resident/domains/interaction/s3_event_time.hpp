// S3 physical register v03 §13.1. Private immutable original-leaf events;
// no event manager, new checkpoint section, live progress or second clock.
#pragma once
#include "resident/kernel/world_timeline.hpp"
#include "resident/storage/common_codec.hpp"
#include <string_view>
namespace hanmo::resident {
// The three original appointment/inquiry owners use this exact catalog rule
// spelling. This owns only their synchronous text input, not duration policy.
class S3ServiceDurationRuleInput {
 CheckpointMemoryCharge copied_fields_;
 StableId value_;
public:
 explicit S3ServiceDurationRuleInput(std::string_view policy_id);
 S3ServiceDurationRuleInput(const S3ServiceDurationRuleInput&)=delete;
 S3ServiceDurationRuleInput(S3ServiceDurationRuleInput&&)=delete;
 S3ServiceDurationRuleInput& operator=(const S3ServiceDurationRuleInput&)=delete;
 S3ServiceDurationRuleInput& operator=(S3ServiceDurationRuleInput&&)=delete;
 const StableId& value()const noexcept{return value_;}
};
bool matches_s3_service_duration_rule(std::string_view rule,std::string_view policy_id)noexcept;
enum class S3PersonalAppointmentTable {accepted,cancelled,started,finished};
enum class S3ContactInquiryTable {started,finished};
enum class S3VisitAppointmentMeetingTable {started,cancelled,finished};
struct S3PersonalAppointmentRecord {S3PersonalAppointmentTable record_table=S3PersonalAppointmentTable::accepted;StableId record_id;};
struct S3ContactInquiryRecord {S3ContactInquiryTable record_table=S3ContactInquiryTable::started;StableId record_id;};
struct S3VisitAppointmentMeetingRecord {S3VisitAppointmentMeetingTable record_table=S3VisitAppointmentMeetingTable::started;StableId record_id;};
struct S3InteractionStepRecord {StableId original_session_id,original_step_execution_key;};
struct S3ProfessionalInstallmentRecord {StableId order_id,installment_result_id;};
struct S3ProfessionalCustodyReturnRecord {StableId order_id,return_result_id;};
struct S3ResidualCopyDeliveryRecord {StableId order_id,residual_delivery_result_id;};
struct S3ProfessionalResultDeliveryRecord {StableId order_id,result_delivery_result_id;};
using S3EventRecordRef=std::variant<S3PersonalAppointmentRecord,S3ContactInquiryRecord,S3VisitAppointmentMeetingRecord,S3InteractionStepRecord,S3ProfessionalInstallmentRecord,S3ProfessionalCustodyReturnRecord,S3ResidualCopyDeliveryRecord,S3ProfessionalResultDeliveryRecord>;
enum class S3EventRole {accepted,cancelled,started,finished,completed,zero_step_committed,goods_accepted,custody_returned,residual_delivered,result_received};
struct S3LegacyEventTick {}; // Reads the designated original field; stores no tick.
struct S3ActionStartedSource {StableId action_id;SourceRef original_start_result_ref;};
struct S3ActionFinishedSource {StableId action_id;SourceRef original_completion_result_ref;};
struct S3CommittedZeroStepSource {SourceRef original_commit_result_ref,actual_contact_source_ref;};
struct S3ActualTerminationSource {SourceRef original_cause_ref;std::optional<SourceRef> original_action_terminal_ref;};
struct S3ReceivedKnowledgeSource {SourceRef original_knowledge_receipt_ref;};
using S3EventActualSource=std::variant<S3ActionStartedSource,S3ActionFinishedSource,S3CommittedZeroStepSource,S3ActualTerminationSource,S3ReceivedKnowledgeSource>;
struct S3ExactEventMoment {WorldMoment occurred_at;S3EventActualSource actual_source;};
using S3EventPrecision=std::variant<S3LegacyEventTick,S3ExactEventMoment>;
struct S3EventTime {S3EventRecordRef original_record_ref;S3EventRole event_role=S3EventRole::accepted;S3EventPrecision precision;};
// Borrowed inputs for the same original record/source alternatives. They are
// synchronous construction views, never saved rows or independent evidence.
struct S3PersonalAppointmentRecordBuildView {S3PersonalAppointmentTable record_table;std::string_view record_id;};
struct S3ContactInquiryRecordBuildView {S3ContactInquiryTable record_table;std::string_view record_id;};
struct S3VisitAppointmentMeetingRecordBuildView {S3VisitAppointmentMeetingTable record_table;std::string_view record_id;};
struct S3InteractionStepRecordBuildView {std::string_view original_session_id,original_step_execution_key;};
struct S3ProfessionalInstallmentRecordBuildView {std::string_view order_id,installment_result_id;};
struct S3ProfessionalCustodyReturnRecordBuildView {std::string_view order_id,return_result_id;};
struct S3ResidualCopyDeliveryRecordBuildView {std::string_view order_id,residual_delivery_result_id;};
struct S3ProfessionalResultDeliveryRecordBuildView {std::string_view order_id,result_delivery_result_id;};
using S3EventRecordBuildView=std::variant<S3PersonalAppointmentRecordBuildView,S3ContactInquiryRecordBuildView,S3VisitAppointmentMeetingRecordBuildView,S3InteractionStepRecordBuildView,S3ProfessionalInstallmentRecordBuildView,S3ProfessionalCustodyReturnRecordBuildView,S3ResidualCopyDeliveryRecordBuildView,S3ProfessionalResultDeliveryRecordBuildView>;
struct S3ActionStartedSourceBuildView {std::string_view action_id;SourceRefBuildView original_start_result_ref;};
struct S3ActionFinishedSourceBuildView {std::string_view action_id;SourceRefBuildView original_completion_result_ref;};
struct S3CommittedZeroStepSourceBuildView {SourceRefBuildView original_commit_result_ref,actual_contact_source_ref;};
struct S3ActualTerminationSourceBuildView {SourceRefBuildView original_cause_ref;std::optional<SourceRefBuildView> original_action_terminal_ref;};
struct S3ReceivedKnowledgeSourceBuildView {SourceRefBuildView original_knowledge_receipt_ref;};
using S3EventActualSourceBuildView=std::variant<S3ActionStartedSourceBuildView,S3ActionFinishedSourceBuildView,S3CommittedZeroStepSourceBuildView,S3ActualTerminationSourceBuildView,S3ReceivedKnowledgeSourceBuildView>;
struct S3ExactEventMomentBuildView {const WorldMoment& occurred_at;S3EventActualSourceBuildView actual_source;};
using S3EventPrecisionBuildView=std::variant<S3LegacyEventTick,S3ExactEventMomentBuildView>;
struct S3EventTimeBuildView {S3EventRecordBuildView original_record_ref;S3EventRole event_role;S3EventPrecisionBuildView precision;};


S3EventTimeBuildView borrow_s3_event_time(const S3EventTime&);
class S3EventTimeInput {
 CheckpointMemoryCharge copied_fields_;
 S3EventTime row_;
public:
 explicit S3EventTimeInput(const S3EventTimeBuildView&);
 S3EventTimeInput(const S3EventTimeInput&)=delete;
 S3EventTimeInput(S3EventTimeInput&&)=delete;
 S3EventTimeInput& operator=(const S3EventTimeInput&)=delete;
 S3EventTimeInput& operator=(S3EventTimeInput&&)=delete;
 const S3EventTime& row()const noexcept{return row_;}
};
bool same_s3_event_record(const S3EventRecordRef&,const S3EventRecordRef&);
bool same_s3_event_record(const S3EventRecordRef&,S3PersonalAppointmentTable,std::string_view id)noexcept;
bool same_s3_event_record(const S3EventRecordRef&,S3ContactInquiryTable,std::string_view id)noexcept;
bool same_s3_event_record(const S3EventRecordRef&,S3VisitAppointmentMeetingTable,std::string_view id)noexcept;
// A pure composite key, never Common.next_id or a separate execution identity.
// The original output owns its text through lookup/adoption. The three borrowed
// table/id forms do not first copy an owning record merely to look it up.
struct S3EventKeyParts;
class S3EventTimeKey {
 CheckpointMemoryCharge copied_fields_;
 StableId value_;
 S3EventTimeKey(S3EventKeyParts,S3EventRole);
public:
 S3EventTimeKey(const S3EventRecordRef&,S3EventRole);
 S3EventTimeKey(S3PersonalAppointmentTable,std::string_view id,S3EventRole);
 S3EventTimeKey(S3ContactInquiryTable,std::string_view id,S3EventRole);
 S3EventTimeKey(S3VisitAppointmentMeetingTable,std::string_view id,S3EventRole);
 S3EventTimeKey(const S3EventTimeKey&)=delete;
 S3EventTimeKey(S3EventTimeKey&&)=delete;
 S3EventTimeKey& operator=(const S3EventTimeKey&)=delete;
 S3EventTimeKey& operator=(S3EventTimeKey&&)=delete;
 const StableId& value()const noexcept{return value_;}
};
Status validate_s3_event_time_shape(const S3EventTime&);
// An explicit exact read never coerces LegacyTick into (tick,0).
Outcome<const S3ExactEventMoment*> read_s3_exact_event(const S3EventTime&);
}
