#include "s3_event_time.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
// Borrowed decomposition of the one original textual key format. It is not a
// source, record, decoded row or separately persisted identity.
struct S3EventKeyParts {
 std::size_t tag;std::optional<Integer> table;std::string_view first;
 std::optional<std::string_view> second;
};
namespace {
S3EventKeyParts key_parts(const S3EventRecordRef&ref){return std::visit([&](const auto&r)->S3EventKeyParts{
 using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecord>||std::is_same_v<T,S3ContactInquiryRecord>||std::is_same_v<T,S3VisitAppointmentMeetingRecord>)return {ref.index(),static_cast<Integer>(r.record_table),r.record_id,std::nullopt};
 else if constexpr(std::is_same_v<T,S3InteractionStepRecord>)return {ref.index(),std::nullopt,r.original_session_id,r.original_step_execution_key};
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecord>)return {ref.index(),std::nullopt,r.order_id,r.installment_result_id};
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecord>)return {ref.index(),std::nullopt,r.order_id,r.return_result_id};
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecord>)return {ref.index(),std::nullopt,r.order_id,r.residual_delivery_result_id};
 else return {ref.index(),std::nullopt,r.order_id,r.result_delivery_result_id};
 },ref);}
template<class Emit>void emit_key(const S3EventKeyParts&p,S3EventRole role,Emit emit){
 auto number=[&](auto value){using N=decltype(value);char buffer[std::numeric_limits<N>::digits10+3];const auto encoded=std::to_chars(buffer,buffer+sizeof(buffer),value);if(encoded.ec!=std::errc{})throw std::bad_alloc();emit(std::string_view(buffer,static_cast<std::size_t>(encoded.ptr-buffer)));};
 auto part=[&](std::string_view value){number(value.size());emit(":");emit(value);};
 number(p.tag);emit(":");if(p.table){number(*p.table);emit(":");}
 part(p.first);if(p.second)part(*p.second);emit(":");number(static_cast<Integer>(role));
}
std::size_t key_size(const S3EventKeyParts&p,S3EventRole role){std::size_t size=0;emit_key(p,role,[&](std::string_view value){if(value.size()>SIZE_MAX-size)throw std::bad_alloc();size+=value.size();});return size;}
CheckpointMemoryCharge reserve_key(const S3EventKeyParts&p,S3EventRole role){return CheckpointMemoryCharge(0);}
template<class Record,class Table>bool same_single_record(const S3EventRecordRef&ref,Table table,std::string_view id)noexcept{
 const auto*record=std::get_if<Record>(&ref);return record&&record->record_table==table&&record->record_id==id;
}

SourceRefBuildView borrow_ref(const SourceRef&r){return {r.domain,r.kind,r.id};}
SourceRef own_ref(const SourceRefBuildView&r){return {StableId(r.domain),StableId(r.kind),StableId(r.id)};}
S3EventRecordBuildView borrow_record(const S3EventRecordRef&ref){return std::visit([](const auto&r)->S3EventRecordBuildView{using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecord>)return S3PersonalAppointmentRecordBuildView{r.record_table,r.record_id};
 else if constexpr(std::is_same_v<T,S3ContactInquiryRecord>)return S3ContactInquiryRecordBuildView{r.record_table,r.record_id};
 else if constexpr(std::is_same_v<T,S3VisitAppointmentMeetingRecord>)return S3VisitAppointmentMeetingRecordBuildView{r.record_table,r.record_id};
 else if constexpr(std::is_same_v<T,S3InteractionStepRecord>)return S3InteractionStepRecordBuildView{r.original_session_id,r.original_step_execution_key};
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecord>)return S3ProfessionalInstallmentRecordBuildView{r.order_id,r.installment_result_id};
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecord>)return S3ProfessionalCustodyReturnRecordBuildView{r.order_id,r.return_result_id};
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecord>)return S3ResidualCopyDeliveryRecordBuildView{r.order_id,r.residual_delivery_result_id};
 else return S3ProfessionalResultDeliveryRecordBuildView{r.order_id,r.result_delivery_result_id};
 },ref);}
S3EventRecordRef own_record(const S3EventRecordBuildView&ref){return std::visit([](const auto&r)->S3EventRecordRef{using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecordBuildView>)return S3PersonalAppointmentRecord{r.record_table,StableId(r.record_id)};
 else if constexpr(std::is_same_v<T,S3ContactInquiryRecordBuildView>)return S3ContactInquiryRecord{r.record_table,StableId(r.record_id)};
 else if constexpr(std::is_same_v<T,S3VisitAppointmentMeetingRecordBuildView>)return S3VisitAppointmentMeetingRecord{r.record_table,StableId(r.record_id)};
 else if constexpr(std::is_same_v<T,S3InteractionStepRecordBuildView>)return S3InteractionStepRecord{StableId(r.original_session_id),StableId(r.original_step_execution_key)};
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecordBuildView>)return S3ProfessionalInstallmentRecord{StableId(r.order_id),StableId(r.installment_result_id)};
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecordBuildView>)return S3ProfessionalCustodyReturnRecord{StableId(r.order_id),StableId(r.return_result_id)};
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecordBuildView>)return S3ResidualCopyDeliveryRecord{StableId(r.order_id),StableId(r.residual_delivery_result_id)};
 else return S3ProfessionalResultDeliveryRecord{StableId(r.order_id),StableId(r.result_delivery_result_id)};
 },ref);}
S3EventActualSourceBuildView borrow_actual(const S3EventActualSource&source){return std::visit([](const auto&r)->S3EventActualSourceBuildView{using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3ActionStartedSource>)return S3ActionStartedSourceBuildView{r.action_id,borrow_ref(r.original_start_result_ref)};
 else if constexpr(std::is_same_v<T,S3ActionFinishedSource>)return S3ActionFinishedSourceBuildView{r.action_id,borrow_ref(r.original_completion_result_ref)};
 else if constexpr(std::is_same_v<T,S3CommittedZeroStepSource>)return S3CommittedZeroStepSourceBuildView{borrow_ref(r.original_commit_result_ref),borrow_ref(r.actual_contact_source_ref)};
 else if constexpr(std::is_same_v<T,S3ActualTerminationSource>)return S3ActualTerminationSourceBuildView{borrow_ref(r.original_cause_ref),r.original_action_terminal_ref?std::optional<SourceRefBuildView>{borrow_ref(*r.original_action_terminal_ref)}:std::nullopt};
 else return S3ReceivedKnowledgeSourceBuildView{borrow_ref(r.original_knowledge_receipt_ref)};
 },source);}
S3EventActualSource own_actual(const S3EventActualSourceBuildView&source){return std::visit([](const auto&r)->S3EventActualSource{using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3ActionStartedSourceBuildView>)return S3ActionStartedSource{StableId(r.action_id),own_ref(r.original_start_result_ref)};
 else if constexpr(std::is_same_v<T,S3ActionFinishedSourceBuildView>)return S3ActionFinishedSource{StableId(r.action_id),own_ref(r.original_completion_result_ref)};
 else if constexpr(std::is_same_v<T,S3CommittedZeroStepSourceBuildView>)return S3CommittedZeroStepSource{own_ref(r.original_commit_result_ref),own_ref(r.actual_contact_source_ref)};
 else if constexpr(std::is_same_v<T,S3ActualTerminationSourceBuildView>)return S3ActualTerminationSource{own_ref(r.original_cause_ref),r.original_action_terminal_ref?std::optional<SourceRef>{own_ref(*r.original_action_terminal_ref)}:std::nullopt};
 else return S3ReceivedKnowledgeSource{own_ref(r.original_knowledge_receipt_ref)};
 },source);}
S3EventPrecisionBuildView borrow_precision(const S3EventPrecision&precision){
 if(const auto*exact=std::get_if<S3ExactEventMoment>(&precision))return S3ExactEventMomentBuildView{exact->occurred_at,borrow_actual(exact->actual_source)};
 return S3LegacyEventTick{};
}
S3EventPrecision own_precision(const S3EventPrecisionBuildView&precision){
 if(const auto*exact=std::get_if<S3ExactEventMomentBuildView>(&precision))return S3ExactEventMoment{exact->occurred_at,own_actual(exact->actual_source)};
 return S3LegacyEventTick{};
}
CheckpointMemoryCharge reserve_event(const S3EventTimeBuildView&view){return CheckpointMemoryCharge(0);}

constexpr std::string_view duration_suffix="/duration_ticks";
std::size_t duration_rule_size(std::string_view policy){
 if(policy.size()>SIZE_MAX-duration_suffix.size())throw std::bad_alloc();
 return policy.size()+duration_suffix.size();
}
CheckpointMemoryCharge reserve_duration_rule(std::string_view policy){
 return CheckpointMemoryCharge(0);
}
bool present(const SourceRef&r){return !r.domain.empty()&&!r.kind.empty()&&!r.id.empty();}
bool valid_record(const S3EventRecordRef&ref){return std::visit([](const auto&r){using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecord>)return !r.record_id.empty()&&static_cast<Integer>(r.record_table)>=0&&static_cast<Integer>(r.record_table)<=3;
 else if constexpr(std::is_same_v<T,S3ContactInquiryRecord>)return !r.record_id.empty()&&static_cast<Integer>(r.record_table)>=0&&static_cast<Integer>(r.record_table)<=1;
 else if constexpr(std::is_same_v<T,S3VisitAppointmentMeetingRecord>)return !r.record_id.empty()&&static_cast<Integer>(r.record_table)>=0&&static_cast<Integer>(r.record_table)<=2;
 else if constexpr(std::is_same_v<T,S3InteractionStepRecord>)return !r.original_session_id.empty()&&!r.original_step_execution_key.empty();
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecord>)return !r.order_id.empty()&&!r.installment_result_id.empty();
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecord>)return !r.order_id.empty()&&!r.return_result_id.empty();
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecord>)return !r.order_id.empty()&&!r.residual_delivery_result_id.empty();
 else return !r.order_id.empty()&&!r.result_delivery_result_id.empty();},ref);}
bool role_matches(const S3EventTime&e){return std::visit([&](const auto&r){using T=std::decay_t<decltype(r)>;const auto role=e.event_role;
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecord>){constexpr S3EventRole roles[]={S3EventRole::accepted,S3EventRole::cancelled,S3EventRole::started,S3EventRole::finished};return role==roles[static_cast<std::size_t>(r.record_table)];}
 else if constexpr(std::is_same_v<T,S3ContactInquiryRecord>)return role==(r.record_table==S3ContactInquiryTable::started?S3EventRole::started:S3EventRole::finished);
 else if constexpr(std::is_same_v<T,S3VisitAppointmentMeetingRecord>){constexpr S3EventRole roles[]={S3EventRole::started,S3EventRole::cancelled,S3EventRole::finished};return role==roles[static_cast<std::size_t>(r.record_table)];}
 else if constexpr(std::is_same_v<T,S3InteractionStepRecord>)return role==S3EventRole::started||role==S3EventRole::completed||role==S3EventRole::zero_step_committed;
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecord>)return role==S3EventRole::goods_accepted;
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecord>)return role==S3EventRole::custody_returned;
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecord>)return role==S3EventRole::residual_delivered;
 else return role==S3EventRole::result_received;},e.original_record_ref);}
}
S3EventTimeBuildView borrow_s3_event_time(const S3EventTime&row){return {borrow_record(row.original_record_ref),row.event_role,borrow_precision(row.precision)};}
S3EventTimeInput::S3EventTimeInput(const S3EventTimeBuildView&view)
 :copied_fields_(reserve_event(view)),row_{own_record(view.original_record_ref),view.event_role,own_precision(view.precision)}{}
S3ServiceDurationRuleInput::S3ServiceDurationRuleInput(std::string_view policy)
 :copied_fields_(reserve_duration_rule(policy)),value_(duration_rule_size(policy),'\0'){
 auto end=std::copy(policy.begin(),policy.end(),value_.begin());
 std::copy(duration_suffix.begin(),duration_suffix.end(),end);
}
bool matches_s3_service_duration_rule(std::string_view rule,std::string_view policy)noexcept{
 if(rule.size()<policy.size()||rule.substr(0,policy.size())!=policy)return false;
 rule.remove_prefix(policy.size());return rule==duration_suffix;
}
bool same_s3_event_record(const S3EventRecordRef&a,const S3EventRecordRef&b){if(a.index()!=b.index())return false;return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);
 if constexpr(std::is_same_v<T,S3PersonalAppointmentRecord>||std::is_same_v<T,S3ContactInquiryRecord>||std::is_same_v<T,S3VisitAppointmentMeetingRecord>)return x.record_table==y.record_table&&x.record_id==y.record_id;
 else if constexpr(std::is_same_v<T,S3InteractionStepRecord>)return x.original_session_id==y.original_session_id&&x.original_step_execution_key==y.original_step_execution_key;
 else if constexpr(std::is_same_v<T,S3ProfessionalInstallmentRecord>)return x.order_id==y.order_id&&x.installment_result_id==y.installment_result_id;
 else if constexpr(std::is_same_v<T,S3ProfessionalCustodyReturnRecord>)return x.order_id==y.order_id&&x.return_result_id==y.return_result_id;
 else if constexpr(std::is_same_v<T,S3ResidualCopyDeliveryRecord>)return x.order_id==y.order_id&&x.residual_delivery_result_id==y.residual_delivery_result_id;
 else return x.order_id==y.order_id&&x.result_delivery_result_id==y.result_delivery_result_id;},a);}
bool same_s3_event_record(const S3EventRecordRef&ref,S3PersonalAppointmentTable table,std::string_view id)noexcept{return same_single_record<S3PersonalAppointmentRecord>(ref,table,id);}
bool same_s3_event_record(const S3EventRecordRef&ref,S3ContactInquiryTable table,std::string_view id)noexcept{return same_single_record<S3ContactInquiryRecord>(ref,table,id);}
bool same_s3_event_record(const S3EventRecordRef&ref,S3VisitAppointmentMeetingTable table,std::string_view id)noexcept{return same_single_record<S3VisitAppointmentMeetingRecord>(ref,table,id);}
S3EventTimeKey::S3EventTimeKey(S3EventKeyParts parts,S3EventRole role)
 :copied_fields_(reserve_key(parts,role)),value_(key_size(parts,role),'\0'){
 auto at=value_.begin();emit_key(parts,role,[&](std::string_view bytes){at=std::copy(bytes.begin(),bytes.end(),at);});
}
S3EventTimeKey::S3EventTimeKey(const S3EventRecordRef&ref,S3EventRole role):S3EventTimeKey(key_parts(ref),role){}
S3EventTimeKey::S3EventTimeKey(S3PersonalAppointmentTable table,std::string_view id,S3EventRole role)
 :S3EventTimeKey(S3EventKeyParts{0,static_cast<Integer>(table),id,std::nullopt},role){}
S3EventTimeKey::S3EventTimeKey(S3ContactInquiryTable table,std::string_view id,S3EventRole role)
 :S3EventTimeKey(S3EventKeyParts{1,static_cast<Integer>(table),id,std::nullopt},role){}
S3EventTimeKey::S3EventTimeKey(S3VisitAppointmentMeetingTable table,std::string_view id,S3EventRole role)
 :S3EventTimeKey(S3EventKeyParts{2,static_cast<Integer>(table),id,std::nullopt},role){}
Status validate_s3_event_time_shape(const S3EventTime&e){
 if(!valid_record(e.original_record_ref)||!role_matches(e))return common_failure("S3EventRecordRoleMismatch",{},ErrorKind::internal);
 const auto*x=std::get_if<S3ExactEventMoment>(&e.precision);if(!x)return common_success();auto moment=compare_world_moments(x->occurred_at,x->occurred_at);if(!moment.ok)return {false,{},moment.error};
 const auto kind=x->actual_source.index();bool allowed=false;switch(e.event_role){case S3EventRole::accepted:case S3EventRole::cancelled:case S3EventRole::zero_step_committed:allowed=kind==2;break;case S3EventRole::started:allowed=kind==0;break;case S3EventRole::finished:case S3EventRole::completed:allowed=kind==1||kind==3;break;case S3EventRole::goods_accepted:case S3EventRole::custody_returned:case S3EventRole::residual_delivered:allowed=kind==1||kind==2;break;case S3EventRole::result_received:allowed=kind==4;break;}
 if(!allowed)return common_failure("S3EventActualSourceKindMismatch",{},ErrorKind::internal);
 const bool source=std::visit([](const auto&s){using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,S3ActionStartedSource>)return !s.action_id.empty()&&present(s.original_start_result_ref);else if constexpr(std::is_same_v<T,S3ActionFinishedSource>)return !s.action_id.empty()&&present(s.original_completion_result_ref);else if constexpr(std::is_same_v<T,S3CommittedZeroStepSource>)return present(s.original_commit_result_ref)&&s.original_commit_result_ref.domain=="common"&&s.original_commit_result_ref.kind=="command_result"&&present(s.actual_contact_source_ref);else if constexpr(std::is_same_v<T,S3ActualTerminationSource>)return present(s.original_cause_ref)&&(!s.original_action_terminal_ref||present(*s.original_action_terminal_ref));else return present(s.original_knowledge_receipt_ref);},x->actual_source);
 return source?common_success():common_failure("S3EventActualSourceMissing",{},ErrorKind::internal);
}
Outcome<const S3ExactEventMoment*> read_s3_exact_event(const S3EventTime&e){auto valid=validate_s3_event_time_shape(e);if(!valid.ok)return {false,nullptr,valid.error};const auto*exact=std::get_if<S3ExactEventMoment>(&e.precision);if(!exact)return {false,nullptr,common_failure("S3OriginalEventIsLegacyTick",{},ErrorKind::unsupported_capability).error};return {true,exact,{}};}
}
