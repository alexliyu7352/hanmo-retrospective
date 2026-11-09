#include "study_service_scope_read.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
// 原接受 ref 比较只读其唯一字段，不由显示版本或包装 ID 重建执行。
bool same_execution(const StudyAcceptedExecutionRef&a,const StudyAcceptedExecutionRef&b){
 if(a.index()!=b.index())return false;
 return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);
  if(x.action_id!=y.action_id)return false;
  if constexpr(std::is_same_v<T,TeachStudyRequestStart>)return x.request_id==y.request_id&&x.session_id==y.session_id;
  else return true;
 },a);
}
}
Outcome<const StudyServiceTerminalV2*> read_original_study_service_termination(
 const StudyServiceScopeReadContext&x,const GrowthStudyService&scope,const StudyTerminal&terminal,const ReadStamp&stamp){
 // 不构造临时共享联合，避免借读时复制原 action/request/session 字符串。
 if(scope.action_id.empty()||scope.accepted_terms_ref.action_id!=scope.action_id||terminal.action_id!=scope.action_id)
  return {false,nullptr,common_failure("StudyServiceScopeTerminalMismatch",scope.action_id).error};
 const bool accepted_shape=std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;
  if(v.action_id!=scope.action_id)return false;
  if constexpr(std::is_same_v<T,TeachStudyRequestStart>)return !v.request_id.empty()&&!v.session_id.empty();
  else return true;
 },scope.accepted_execution_ref);
 if(!accepted_shape)return {false,nullptr,common_failure("StudyServiceAcceptedExecutionMismatch",scope.action_id).error};
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,nullptr,now.error};
 auto accepted=read_study_accepted_execution(x.common,x.growth,x.catalog,x.foundation,scope.action_id,now.value,x.access);
 if(!accepted.ok)return {false,nullptr,accepted.error};
 if(!accepted.value||accepted.value->terms_ref.action_id!=scope.action_id||!same_execution(accepted.value->accepted_execution_ref,scope.accepted_execution_ref))
  return {false,nullptr,common_failure("StudyServiceAcceptedScopeMismatch",scope.action_id).error};
 auto source=read_study_service_termination(x.common,x.growth,x.catalog,x.foundation,x.access,terminal,now.value,stamp,x.deaths);
 if(!source.ok)return source;
 if(!source.value||source.value->action_id!=scope.action_id||source.value->terminal_ref.action_id!=terminal.action_id||source.value->terms_ref.action_id!=scope.accepted_terms_ref.action_id||!same_execution(source.value->accepted_execution_ref,scope.accepted_execution_ref))
  return {false,nullptr,common_failure("StudyServiceOriginalTerminalMismatch",scope.action_id).error};
 return source;
}

Outcome<OwnedServiceActualTerminationResult> read_study_service_actual_termination(
 const StudyServiceScopeReadContext&x,const CharacterConditionSources&body,
 const GrowthStudyService&scope,const StudyTerminal&terminal,const ReadStamp&stamp){
 if(&body.common!=&x.common||body.growth!=&x.growth||body.growth_catalog!=&x.catalog||&body.catalog!=&x.foundation)
  return {false,{},common_failure("StudyServiceResultWorldMismatch",scope.action_id,ErrorKind::internal).error};
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto same=compare_world_moments(now.value,body.actual_now);if(!same.ok)return {false,{},same.error};if(same.value!=0)return {false,{},common_failure("StudyServiceResultClockMismatch",scope.action_id,ErrorKind::internal).error};
 auto original=read_original_study_service_termination(x,scope,terminal,stamp);if(!original.ok)return {false,{},original.error};const auto&t=*original.value;
 // 原普通收费 profile 的责任表非空即须其原 owner 后继，不能吞掉再输出空债。
 if(!t.responsibility_refs.empty())return {false,{},common_failure("StudyOriginalResponsibilityReadRequired",scope.action_id,ErrorKind::unsupported_capability).error};
 for(const auto&ref:t.fee_source_refs){auto charge=read_study_lesson_charge(body,x.growth,ref);if(!charge.ok)return {false,{},charge.error};if(!charge.value.original_study||!charge.value.original_lesson||!charge.value.actual_lesson_source||charge.value.amount<=0)return {false,{},common_failure("StudyOriginalPaidLessonRequired",scope.action_id).error};}
 OwnedServiceActualTerminationResult output;
 std::visit([&](const auto&e){using E=std::decay_t<decltype(e)>;if constexpr(std::is_same_v<E,TeachStudyRequestStart>){}},scope.accepted_execution_ref);

 if(t.fee_source_refs.size()>SIZE_MAX/sizeof(ServiceEarnedChargeRef))throw std::bad_alloc();

 constexpr std::string_view prefix="study_service_terminal:",suffix=":actual_service_termination";char digits[3*sizeof(std::size_t)+1];auto length=std::to_chars(digits,digits+sizeof digits,scope.action_id.size());if(length.ec!=std::errc{})return {false,{},common_failure("StudyTerminationIdOverflow",{},ErrorKind::internal).error};const auto count=static_cast<std::size_t>(length.ptr-digits);
 if(scope.action_id.size()>SIZE_MAX-prefix.size()-suffix.size()-count-2)throw std::bad_alloc();const auto bytes=prefix.size()+count+1+scope.action_id.size()+suffix.size();(output.memory).grow(0);
 StableId encoded(bytes,'\0');std::size_t pos=0;auto put=[&](std::string_view v){std::copy(v.begin(),v.end(),encoded.begin()+pos);pos+=v.size();};put(prefix);put(std::string_view(digits,count));put(":");put(scope.action_id);put(suffix);
 auto&result=output.value;result.termination_id=std::move(encoded);result.scope=scope;result.original_terminal_source=terminal;result.accepted_fee_terms_ref=t.terms_ref;result.reason=ServiceOwnerConfirmedUnavailability{terminal};result.effective_game_time.kind=ActorFoundationAt::Kind::exact_moment;result.effective_game_time.exact_moment=t.terminated_at;
 result.earned_charge_refs.reserve(t.fee_source_refs.size());for(const auto&ref:t.fee_source_refs)result.earned_charge_refs.emplace_back(ref);
 // 原 continuation_check 已由同 terminal 验证；挑战/迟答与预约释放仍在原 Growth 槽，
 // 它们不是 AdvancePayment/Payable/客户保管，不制造三类假 SourceRef。
 auto valid=validate_service_actual_termination_result_shape(result,service_actual_termination_result_schema);if(!valid.ok)return {false,{},valid.error};return {true,std::move(output),{}};
}
}
