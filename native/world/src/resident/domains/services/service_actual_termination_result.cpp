#include "service_actual_termination_result.hpp"
#include "resident/storage/common_store.hpp"
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
bool source(const SourceRef&v){return !v.domain.empty()&&!v.kind.empty()&&!v.id.empty();}
bool same_source(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same_charge(const ServiceEarnedChargeRef&a,const ServiceEarnedChargeRef&b){if(a.index()!=b.index())return false;if(const auto*x=std::get_if<SourceRef>(&a))return same_source(*x,std::get<SourceRef>(b));const auto&x=std::get<StudyLessonCharge>(a);const auto&y=std::get<StudyLessonCharge>(b);return x.action_id==y.action_id&&x.lesson_ordinal==y.lesson_ordinal;}
}
Status validate_study_service_termination_id(const StableId&encoded,const StableId&action){
 constexpr std::string_view prefix="study_service_terminal:",suffix=":actual_service_termination";
 if(action.empty())return common_failure("StudyTerminationActionRequired");
 char digits[3*sizeof(std::size_t)+1];auto n=std::to_chars(digits,digits+sizeof digits,action.size());if(n.ec!=std::errc{})return common_failure("StudyTerminationIdOverflow",{},ErrorKind::internal);
 const std::string_view length(digits,static_cast<std::size_t>(n.ptr-digits));std::string_view value(encoded);
 if(value.size()<prefix.size()||value.substr(0,prefix.size())!=prefix)return common_failure("NonCanonicalStudyTerminationId",action);value.remove_prefix(prefix.size());
 if(value.size()<length.size()+1||value.substr(0,length.size())!=length||value[length.size()]!=':')return common_failure("NonCanonicalStudyTerminationId",action);value.remove_prefix(length.size()+1);
 if(value.size()!=action.size()+suffix.size()||value.substr(0,action.size())!=action||value.substr(action.size())!=suffix)return common_failure("NonCanonicalStudyTerminationId",action);
 return common_success();
}
Outcome<Integer> service_actual_termination_result_contract_schema(Integer schema){
 if(schema<1||schema>service_actual_termination_result_schema)return {false,0,common_failure("UnsupportedServiceResultSchema",{},ErrorKind::unsupported_capability).error};
 return {true,schema==4?3:schema,{}};
}
Status validate_service_actual_termination_result_shape(const ServiceActualTerminationResult&v,Integer schema){
 auto nested=service_actual_termination_result_contract_schema(schema);if(!nested.ok)return {false,{},nested.error};
 auto valid=validate_service_scope_terminal_identity(v.scope,v.original_terminal_source,nested.value);if(!valid.ok)return valid;if(v.termination_id.empty())return common_failure("ServiceTerminationIdRequired");
 const auto*study=std::get_if<GrowthStudyService>(&v.scope);
 if(study){if(nested.value!=3)return common_failure("StudyServiceResultSchemaRequired",{},ErrorKind::unsupported_capability);valid=validate_study_service_termination_id(v.termination_id,study->action_id);if(!valid.ok)return valid;const auto*terms=std::get_if<StudyFactTermsRef>(&v.accepted_fee_terms_ref);if(!terms||terms->action_id!=study->action_id)return common_failure("StudyServiceFeeTermsMismatch");}
 else {const auto*terms=std::get_if<SourceRef>(&v.accepted_fee_terms_ref);if(!terms||!source(*terms))return common_failure("OriginalServiceFeeTermsRequired");if(const auto*work=std::get_if<S3WorkCommission>(&v.scope)){if(!same_source(work->expense_terms_ref,*terms))return common_failure("WorkServiceFeeTermsMismatch");const auto*terminal=std::get_if<WorkCommissionTerminal>(&v.original_terminal_source);if(!terminal||v.termination_id!=terminal->task_terminal_fact_id)return common_failure("NonCanonicalWorkServiceTerminationId");}}
 if(const auto*repair=std::get_if<RepairService>(&v.scope)){const auto*terminal=std::get_if<RepairTerminal>(&v.original_terminal_source);if(!terminal||v.termination_id!=terminal->repair_result_id)return common_failure("NonCanonicalRepairServiceTerminationId");}
 if(const auto*reason=std::get_if<ServiceVerifiedDeath>(&v.reason)){if(reason->death_ref.domain!="family_life"||reason->death_ref.kind!="family_death"||reason->death_ref.id.empty()||reason->actor_id.empty()||reason->role.empty())return common_failure("InvalidServiceVerifiedDeath");}
 else if(!same_original_service_terminal(std::get<ServiceOwnerConfirmedUnavailability>(v.reason).terminal_ref,v.original_terminal_source))return common_failure("ServiceUnavailabilityTerminalMismatch");
 if(v.effective_game_time.kind==ActorFoundationAt::Kind::legacy_tick){if(v.effective_game_time.legacy_tick<0||study)return common_failure("InvalidServiceCalendarTime");}
 else if(v.effective_game_time.kind==ActorFoundationAt::Kind::exact_moment){if(v.effective_game_time.legacy_tick!=0)return common_failure("InactiveServiceTimePayload");auto at=encode_world_moment(v.effective_game_time.exact_moment);if(!at.ok)return {false,{},at.error};}
 else return common_failure("InvalidServiceTimePrecision");
 for(std::size_t n=0;n<v.earned_charge_refs.size();++n){const auto&charge=v.earned_charge_refs[n];if(study){const auto*c=std::get_if<StudyLessonCharge>(&charge);if(!c||c->action_id!=study->action_id||c->accepted_terms_ref.action_id!=study->action_id||c->lesson_ordinal<1)return common_failure("StudyServiceChargeMismatch");}else {const auto*c=std::get_if<SourceRef>(&charge);if(!c||!source(*c))return common_failure("OriginalServiceChargeRequired");}for(std::size_t p=0;p<n;++p)if(same_charge(v.earned_charge_refs[p],charge))return common_failure("DuplicateServiceCharge");}
 for(const auto*refs:{&v.existing_payable_refs,&v.custody_or_continuity_refs})for(std::size_t n=0;n<refs->size();++n){if(!source((*refs)[n]))return common_failure("InvalidServiceResponsibilitySource");for(std::size_t p=0;p<n;++p)if(same_source((*refs)[p],(*refs)[n]))return common_failure("DuplicateServiceResponsibilitySource");}
 return common_success();
}
}
