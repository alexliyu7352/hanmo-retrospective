// 单job预算和首失败拥有；第一诊断随租约保留，固定描述可在最外层受控物化。
#include "checkpoint_job_budget.hpp"
#include "checkpoint_not_implemented.hpp"
#include "checkpoint_capacity.hpp"
#include "checkpoint_retention.hpp"
#include <picosha2.h>
#include <algorithm>
#include <cassert>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <stdexcept>
namespace hanmo::resident {
namespace {
using Code=CheckpointJobFailureCode;
bool add(std::uint64_t a,std::uint64_t b,std::uint64_t& out){if(b>UINT64_MAX-a)return false;out=a+b;return true;}
class Fingerprint {
    picosha2::hash256_one_by_one hash_;
public:
    void raw(const void* p,std::size_t n){auto b=static_cast<const std::uint8_t*>(p);while(n){auto k=std::min<std::size_t>(64,n);hash_.process(b,b+k);b+=k;n-=k;}}
    void number(std::uint64_t n){std::uint8_t b[8];for(unsigned i=0;i<8;++i)b[i]=std::uint8_t(n>>(i*8));raw(b,8);}
    void text(const StableId& s){number(s.size());raw(s.data(),s.size());}
    void map(const std::map<StableId,std::uint64_t>& m){number(m.size());for(const auto& x:m){text(x.first);number(x.second);}}
    CheckpointDigest finish(){CheckpointDigest d{};hash_.finish();hash_.get_hash_bytes(d.begin(),d.end());return d;}
};
CheckpointDigest fingerprint(const CapacityProfile& p){Fingerprint f;f.text(p.profile_id);f.number(p.revision);f.number(p.max_checkpoint_bytes);f.number(p.max_header_bytes);f.map(p.max_section_bytes_by_registered_name);f.number(p.max_nesting_depth);f.number(p.max_bigint_encoded_bytes);f.number(p.stream_chunk_bytes);f.number(p.max_staging_disk_bytes);return f.finish();}
CheckpointDigest fingerprint(const SlotCapacityProfile& p){Fingerprint f;f.text(p.profile_id);f.number(p.revision);f.number(p.max_slot_bytes);f.number(p.max_slot_header_bytes);f.number(p.max_continuation_bytes);f.number(p.max_admission_capsule_bytes);f.number(p.max_slot_staging_disk_bytes);return f.finish();}// Job creation borrows the original finite shape rules before any diagnostic allocation.
enum class Resource:std::uint8_t{none,slot,encode,scratch,candidate};
struct Checked { Code code=Code::none;std::uint64_t required=0;Resource resource=Resource::none; };
}
struct CheckpointJobBudgetState {
 mutable std::mutex mutex;
 StableId capacity_id,slot_id;
 std::uint64_t capacity_revision=0,slot_revision=0;
 CheckpointDigest capacity_fingerprint{},slot_fingerprint{};
 std::uint64_t diagnostic_limit=0;
 bool cancelled=false;

 bool failed=false,materialized=false,materialize_attempted=false;
 Code first_code=Code::none;std::uint64_t first_required=0;Resource first_resource=Resource::none;
 RuleError first_error; // 只移动写一次；共享只读，不让裸错误脱离诊断lease。
 // 原 token 只核同 job 的存活、取消和用途；不再持内存用量或子额度账。
 Checked check(std::uint8_t use)const noexcept{
  if(failed)return {Code::already_failed};
  if(cancelled)return {Code::cancelled};
  if(use&~7u)return {Code::invalid_use};
  return {};
 }

};
class CheckpointJobFailureAccess {
public:
 static CheckpointJobFailure state(std::shared_ptr<CheckpointJobBudgetState> s)noexcept{CheckpointJobFailure result;result.state_=std::move(s);return result;}
};
namespace {
// 调用者持mutex；只首错写state，后续新操作给无分配固定already_failed。
CheckpointJobFailure record(const std::shared_ptr<CheckpointJobBudgetState>& s,Checked failure)noexcept{
 if(failure.code==Code::already_failed)return CheckpointJobFailure::fixed(Code::already_failed);
 if(!s->failed){s->failed=true;s->first_code=failure.code;s->first_required=failure.required;s->first_resource=failure.resource;}
 return CheckpointJobFailureAccess::state(s);
}
CheckpointJobStatus status(const std::shared_ptr<CheckpointJobBudgetState>& s,Checked result){if(result.code==Code::none)return {true,{}, {}};return {false,{},record(s,result)};}
// 固定失败码直接映射字面量；缺实现保持明确失败，不分配或伪造成功诊断。
const char *code_name(Code c) noexcept {
  switch (c) {
  case Code::invalid_job:
    return "InvalidCheckpointJob";
  case Code::invalid_profile:
    return "InvalidCapacityProfile";
  case Code::control_budget:
    return "CheckpointJobControlBudgetExceeded";
  case Code::allocation_failed:
    return "CheckpointAllocationFailed";
  case Code::cancelled:
    return "CheckpointJobCancelled";
  case Code::profile_mismatch:
    return "CheckpointJobProfileMismatch";
  case Code::invalid_use:
    return "InvalidCheckpointJobUse";
  case Code::size_overflow:
    return "CheckpointJobSizeOverflow";
  case Code::reservation_mismatch:
    return "CheckpointJobReservationMismatch";
  case Code::write_range:
    return "CheckpointJobWriteRange";
  case Code::capacity_exceeded:
    return "CheckpointCapacityExceeded";
  case Code::already_failed:
    return "CheckpointJobAlreadyFailed";
  case Code::diagnostic_capacity:
    return "CheckpointDiagnosticCapacityExceeded";
  case Code::json_context_required:
    return "CheckpointJsonDecodeContextRequired";
  case Code::default_shape_required:
    return "CheckpointDefaultConstructionShapeRequired";
  case Code::default_shape_toolchain:
    return "CheckpointDefaultConstructionToolchainUnsupported";
  case checkpoint_not_implemented_code:
    return "NOT_IMPLEMENTED";
  default:
    return "CheckpointJobFailure";
  }
}
std::uint64_t error_bytes(const RuleError& e)noexcept{
 std::uint64_t count=0;const auto add_string=[&](const StableId& v){std::uint64_t next;if(!add(count,v.capacity()+1,next))count=UINT64_MAX;else count=next;};
 for(const auto* s:{&e.original.code,&e.original.path,&e.phase,&e.domain,&e.command,&e.source_id})add_string(*s);
 for(const auto* s:{&e.army_id,&e.reason,&e.limit,&e.feasibility,&e.skill_id,&e.actor_id,&e.battle_id})if(*s)add_string(**s);return count;
}
}
const char* checkpoint_job_failure_code_name(CheckpointJobFailureCode code)noexcept{return code_name(code);}
CheckpointJobFailure CheckpointJobFailure::fixed(Code code,std::uint64_t required)noexcept{CheckpointJobFailure f;f.fixed_=code;f.required_=required;return f;}
CheckpointJobFailureCode CheckpointJobFailure::code()const noexcept{if(!state_)return fixed_;std::lock_guard<std::mutex> lock(state_->mutex);return state_->first_code;}
std::uint64_t CheckpointJobFailure::required()const noexcept{if(!state_)return required_;std::lock_guard<std::mutex> lock(state_->mutex);return state_->first_required;}
const RuleError* CheckpointJobFailure::rule_error()const noexcept{if(!state_)return nullptr;std::lock_guard<std::mutex> lock(state_->mutex);return state_->materialized?&state_->first_error:nullptr;}
bool CheckpointJobFailure::try_materialize_rule_error()noexcept{
 if(!state_)return false;std::lock_guard<std::mutex> lock(state_->mutex);if(state_->materialized)return true;if(state_->materialize_attempted)return false;state_->materialize_attempted=true;
 try{RuleError error;error.original={code_name(state_->first_code),"checkpoint/job"};error.kind=ErrorKind::internal;
  if(state_->first_code==Code::capacity_exceeded){const bool slot=state_->first_resource==Resource::slot;const char* resource=slot?"slot_job_bytes":state_->first_resource==Resource::encode?"encode_job_bytes":state_->first_resource==Resource::scratch?"decode_scratch_bytes":"decoded_candidate_bytes";error.original.path="checkpoint/capacity/";error.original.path+=resource;error.source_id=slot?state_->slot_id:state_->capacity_id;error.limit=std::to_string(slot?state_->slot_revision:state_->capacity_revision);error.reason=std::to_string(state_->first_required);}
  if(state_->first_code==Code::diagnostic_capacity){error.source_id=state_->capacity_id;error.limit=std::to_string(state_->capacity_revision);error.reason=std::to_string(state_->first_required);}
  if(error_bytes(error)>state_->diagnostic_limit)return false;state_->first_error=std::move(error);state_->materialized=true;return true;
 }catch(const std::bad_alloc&){return false;}catch(const std::length_error&){return false;}
}
CheckpointJobFailureLease::CheckpointJobFailureLease(CheckpointJobFailure&& value)noexcept:value_(std::move(value)){}
CheckpointJobFailureLease::CheckpointJobFailureLease(std::shared_ptr<const CheckpointJobFailure> value)noexcept:alias_(std::move(value)){}
CheckpointJobFailureLease CheckpointJobFailureLease::retain(const CheckpointJobFailure& value)noexcept{
 CheckpointJobFailureLease result;result.value_.state_=value.state_;result.value_.fixed_=value.fixed_;result.value_.required_=value.required_;return result;
}
bool CheckpointJobFailureLease::try_materialize_rule_error()const noexcept{const auto* error=get();if(!error)return false;auto held=retain(*error);return held.value_.try_materialize_rule_error();}
CheckpointJobFailureLease::CheckpointJobFailureLease(const CheckpointJobFailureLease& other)noexcept:alias_(other.alias_){value_.state_=other.value_.state_;value_.fixed_=other.value_.fixed_;value_.required_=other.value_.required_;}
CheckpointJobFailureLease& CheckpointJobFailureLease::operator=(const CheckpointJobFailureLease& other)noexcept{if(this!=&other){CheckpointJobFailureLease copy(other);*this=std::move(copy);}return *this;}
CheckpointJobFailureLease& CheckpointJobFailureLease::operator=(CheckpointJobFailure&& value)noexcept{alias_.reset();value_=std::move(value);return *this;}
CheckpointJobFailureLease& CheckpointJobFailureLease::operator=(std::shared_ptr<const CheckpointJobFailure> value)noexcept{value_=CheckpointJobFailure{};alias_=std::move(value);return *this;}
void CheckpointJobFailureLease::reset()noexcept{alias_.reset();value_=CheckpointJobFailure{};}
CheckpointJobFailureLease CheckpointJobReservation::failure_lease()const noexcept{
 if(!state_)return {};std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->failed)return {};return CheckpointJobFailureLease(CheckpointJobFailureAccess::state(state_));
}
CheckpointJobFailure CheckpointJobReservation::capture_fixed_failure(Code code,std::uint64_t required)const noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::reservation_mismatch);std::lock_guard<std::mutex> lock(state_->mutex);return record(state_,{code,required});
}
CheckpointJobFailure CheckpointJobReservation::capture_boundary_failure(std::string_view code,std::initializer_list<std::string_view> path,ErrorKind kind)const noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::reservation_mismatch);CheckpointJobBudget original;original.state_=state_;return original.capture_boundary_failure(code,path,kind);
}
CheckpointJobFailure CheckpointJobReservation::capture_capacity_failure(std::initializer_list<std::string_view> resource,std::uint64_t required,std::string_view profile,std::uint64_t revision)const noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::reservation_mismatch);CheckpointJobBudget original;original.state_=state_;return original.capture_capacity_failure(resource,required,profile,revision);
}
CheckpointJobResult<CheckpointJobReservation> CheckpointJobReservation::reserve_related(CheckpointJobUse use,std::uint64_t bytes)const{
 if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::reservation_mismatch)};CheckpointJobBudget original;original.state_=state_;return original.reserve(use,bytes);
}
CheckpointJobFailure CheckpointJobReservation::capture_failure(RuleError&& error)const noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::reservation_mismatch);CheckpointJobBudget original;original.state_=state_;return original.capture_failure(std::move(error));
}
// 控制块由真实 shared_ptr 持有；仅保原 token 的同 job 用途检查。
void CheckpointJobReservation::retain_control(CheckpointRetentionCensus& c)const{
 if(state_)c.original_token(*this);
}
CheckpointJobReservation::CheckpointJobReservation(std::shared_ptr<CheckpointJobBudgetState> s,std::uint64_t n,std::uint8_t u)noexcept:state_(std::move(s)),bytes_(n),use_(u){}
CheckpointJobReservation::~CheckpointJobReservation()noexcept{reset();}
void CheckpointJobReservation::reset()noexcept{state_.reset();bytes_=0;use_=0;}
CheckpointJobReservation::CheckpointJobReservation(CheckpointJobReservation&& o)noexcept:state_(std::move(o.state_)),bytes_(o.bytes_),use_(o.use_){o.bytes_=0;o.use_=0;}
CheckpointJobReservation& CheckpointJobReservation::operator=(CheckpointJobReservation&& o)noexcept{if(this!=&o){reset();state_=std::move(o.state_);bytes_=o.bytes_;use_=o.use_;o.bytes_=0;o.use_=0;}return *this;}
bool CheckpointJobReservation::belongs_to(const CheckpointJobBudget& j)const noexcept{return state_&&state_==j.state_;}
bool CheckpointJobReservation::memory_accounting_enabled()const noexcept{return false;}
CheckpointJobStatus CheckpointJobReservation::grow(std::uint64_t n){if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::reservation_mismatch)};std::lock_guard<std::mutex> lock(state_->mutex);auto checked=state_->failed?Checked{Code::already_failed}:state_->cancelled?Checked{Code::cancelled}:n>UINT64_MAX-bytes_?Checked{Code::size_overflow}:state_->check(use_);if(checked.code==Code::none)bytes_+=n;return status(state_,checked);}
bool CheckpointJobReservation::release_destroyed(std::uint64_t n)noexcept{if(!state_||n>bytes_)return false;std::lock_guard<std::mutex> lock(state_->mutex);bytes_-=n;return true;}
CheckpointJobStatus CheckpointJobReservation::add_constraints(CheckpointJobUse u)const{if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::reservation_mismatch)};std::lock_guard<std::mutex> lock(state_->mutex);const auto added=std::uint8_t(std::uint8_t(u)&~use_);auto result=state_->check(added);if(result.code==Code::none)use_|=added;return status(state_,result);}
CheckpointJobStatus CheckpointJobReservation::merge(CheckpointJobReservation&& other){if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::reservation_mismatch)};if(state_!=other.state_||use_!=other.use_||this==&other||other.bytes_>UINT64_MAX-bytes_){std::lock_guard<std::mutex> lock(state_->mutex);return status(state_,{state_->failed?Code::already_failed:other.bytes_>UINT64_MAX-bytes_?Code::size_overflow:Code::reservation_mismatch});}bytes_+=other.bytes_;other.bytes_=0;other.use_=0;other.state_.reset();return {true,{}, {}};}
// 历史验证入口仅为源码兼容别名；旧严格内存账的实现已删除，不能由任何模式重开。
CheckpointJobResult<CheckpointJobBudget> CheckpointJobBudget::create_metered_for_verification(const CapacityProfile& p,const SlotCapacityProfile& s){return create(p,s);}
bool CheckpointJobBudget::memory_accounting_enabled()const noexcept{return false;}
CheckpointJobResult<CheckpointJobBudget> CheckpointJobBudget::create(const CapacityProfile& p,const SlotCapacityProfile& s){
 if(!valid_capacity_profile_shape(p)||!valid_slot_capacity_profile_shape(s))return {false,{},CheckpointJobFailure::fixed(Code::invalid_profile)};
 const auto id=std::max(p.profile_id.size(),s.profile_id.size());
 if(id>(UINT64_MAX-4096)/8)return {false,{},CheckpointJobFailure::fixed(Code::size_overflow)};
 try{auto state=std::make_shared<CheckpointJobBudgetState>();state->capacity_id=StableId(p.profile_id.begin(),p.profile_id.end());state->slot_id=StableId(s.profile_id.begin(),s.profile_id.end());state->capacity_revision=p.revision;state->slot_revision=s.revision;state->diagnostic_limit=4096+id*8;state->capacity_fingerprint=fingerprint(p);state->slot_fingerprint=fingerprint(s);CheckpointJobBudget result;result.state_=std::move(state);return {true,std::move(result),{}};}catch(const std::bad_alloc&){return {false,{},CheckpointJobFailure::fixed(Code::allocation_failed)};}
}
CheckpointJobStatus CheckpointJobBudget::check_profile(const CapacityProfile& p)const{if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::invalid_job)};std::lock_guard<std::mutex> lock(state_->mutex);if(state_->failed)return {false,{},CheckpointJobFailure::fixed(Code::already_failed)};if(state_->cancelled)return status(state_,{Code::cancelled});try{return fingerprint(p)==state_->capacity_fingerprint?CheckpointJobStatus{true,{}, {}}:status(state_,{Code::profile_mismatch});}catch(const std::bad_alloc&){return status(state_,{Code::allocation_failed});}}
CheckpointJobStatus CheckpointJobBudget::check_profile(const SlotCapacityProfile& p)const{if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::invalid_job)};std::lock_guard<std::mutex> lock(state_->mutex);if(state_->failed)return {false,{},CheckpointJobFailure::fixed(Code::already_failed)};if(state_->cancelled)return status(state_,{Code::cancelled});try{return fingerprint(p)==state_->slot_fingerprint?CheckpointJobStatus{true,{}, {}}:status(state_,{Code::profile_mismatch});}catch(const std::bad_alloc&){return status(state_,{Code::allocation_failed});}}
CheckpointJobResult<CheckpointJobReservation> CheckpointJobBudget::reserve(CheckpointJobUse use,std::uint64_t n){if(!state_)return {false,{},CheckpointJobFailure::fixed(Code::invalid_job)};std::lock_guard<std::mutex> lock(state_->mutex);auto result=state_->check(std::uint8_t(use));if(result.code!=Code::none)return {false,{},record(state_,result)};return {true,CheckpointJobReservation(state_,n,std::uint8_t(use)),{}};}
CheckpointJobUsage CheckpointJobBudget::usage()const noexcept{if(!state_)return {};std::lock_guard<std::mutex> lock(state_->mutex);CheckpointJobUsage result;result.cancelled=state_->cancelled;return result;}
void CheckpointJobBudget::cancel()noexcept{if(state_){std::lock_guard<std::mutex> lock(state_->mutex);state_->cancelled=true;}}
CheckpointJobFailure CheckpointJobBudget::capture_fixed_failure(Code code,std::uint64_t required)noexcept{if(!state_)return CheckpointJobFailure::fixed(code,required);std::lock_guard<std::mutex> lock(state_->mutex);return record(state_,{code,required});}
CheckpointJobFailure CheckpointJobBudget::capture_failure(RuleError&& error)noexcept{if(!state_)return CheckpointJobFailure::fixed(Code::invalid_job);std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->failed){const auto bytes=error_bytes(error);state_->failed=true;if(bytes>state_->diagnostic_limit){state_->first_code=Code::diagnostic_capacity;state_->first_required=bytes;}else{state_->first_code=Code::legacy_error;state_->first_error=std::move(error);state_->materialized=true;}}return CheckpointJobFailureAccess::state(state_);}
CheckpointJobFailure CheckpointJobBudget::capture_boundary_failure(std::string_view code,std::initializer_list<std::string_view> path,ErrorKind kind)noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::invalid_job);
 std::lock_guard<std::mutex> lock(state_->mutex);
 if(state_->failed)return CheckpointJobFailureAccess::state(state_);
 if(state_->cancelled)return record(state_,{Code::cancelled});
 std::uint64_t length=0,required=0;
 for(auto part:path)if(!add(length,part.size(),length)){length=UINT64_MAX;break;}
 // This finite path only sets the original code/path and ErrorKind. Other
 // original strings remain empty. Account the same six capacities as error_bytes.
 bool fits=add(std::max<std::uint64_t>(15,code.size()),1,required);
 std::uint64_t path_bytes=0;
 fits=fits&&add(std::max<std::uint64_t>(15,length),1,path_bytes)&&add(required,path_bytes,required)&&add(required,64,required);
 state_->failed=true;state_->materialize_attempted=true;
 if(!fits||length>SIZE_MAX||required>state_->diagnostic_limit){state_->first_code=Code::diagnostic_capacity;state_->first_required=fits?required:UINT64_MAX;return CheckpointJobFailureAccess::state(state_);}
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE != 14 || !defined(_GLIBCXX_USE_CXX11_ABI) || !_GLIBCXX_USE_CXX11_ABI
 state_->first_code=Code::default_shape_toolchain;return CheckpointJobFailureAccess::state(state_);
#else
 static_assert(sizeof(void*)==8&&sizeof(std::string)==32,"pinned original diagnostic copy layout");
 try{
  auto value=make_checkpoint_boundary_error(code,path,kind);
  state_->first_error=std::move(value);state_->first_code=Code::legacy_error;state_->materialized=true;
 }catch(const std::bad_alloc&){state_->first_code=Code::allocation_failed;}
  catch(const std::length_error&){state_->first_code=Code::allocation_failed;}
 return CheckpointJobFailureAccess::state(state_);
#endif
}
CheckpointJobFailure CheckpointJobBudget::capture_capacity_failure(std::initializer_list<std::string_view> resource,std::uint64_t required,std::string_view profile,std::uint64_t revision)noexcept{
 if(!state_)return CheckpointJobFailure::fixed(Code::invalid_job);std::lock_guard<std::mutex> lock(state_->mutex);
 if(state_->failed)return CheckpointJobFailureAccess::state(state_);
 if(state_->cancelled)return record(state_,{Code::cancelled});
 state_->failed=true;state_->materialize_attempted=true;
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE != 14 || !defined(_GLIBCXX_USE_CXX11_ABI) || !_GLIBCXX_USE_CXX11_ABI
 state_->first_code=Code::default_shape_toolchain;
#else
 try{
  const auto bytes=checkpoint_capacity_diagnostic_bytes(resource,required,profile,revision);
  if(bytes>state_->diagnostic_limit){state_->first_code=Code::diagnostic_capacity;state_->first_required=bytes;return CheckpointJobFailureAccess::state(state_);}
  auto value=make_checkpoint_capacity_error(resource,required,profile,revision);
  state_->first_error=std::move(value);state_->first_code=Code::legacy_error;state_->materialized=true;
 }catch(const std::bad_alloc&){state_->first_code=Code::allocation_failed;}
  catch(const std::length_error&){state_->first_code=Code::allocation_failed;}
#endif
 return CheckpointJobFailureAccess::state(state_);
}
void CheckpointJobBytes::retain(CheckpointRetentionCensus& census)const{
 if(bytes_.capacity()>reservation_.bytes())throw std::logic_error("CheckpointJobBytesReservationMismatch");
 if(!census.accounts(reservation_))reservation_.retain_control(census);
 // 逻辑容量与原 token 用途不随计量退役而放宽。
 census.original_token(reservation_);
}
CheckpointJobBytes& CheckpointJobBytes::operator=(CheckpointJobBytes&& o)noexcept{if(this!=&o){Buffer().swap(bytes_);reservation_=std::move(o.reservation_);bytes_=std::move(o.bytes_);}return *this;}
CheckpointJobResult<CheckpointJobBytes> CheckpointJobBytes::allocate(CheckpointJobBudget& job,CheckpointJobUse use,std::uint64_t size){if(size>SIZE_MAX)return {false,{},job.capture_fixed_failure(Code::size_overflow,size)};auto r=job.reserve(use,size);if(!r.ok)return {false,{},std::move(r.error)};try{Buffer bytes(static_cast<std::size_t>(size));return adopt_reserved(std::move(bytes),std::move(r.value));}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(Code::allocation_failed)};}}
CheckpointJobResult<CheckpointJobBytes> CheckpointJobBytes::copy(CheckpointJobBudget& job,CheckpointJobUse use,const Buffer& source){auto result=allocate(job,use,source.size());if(result.ok)std::copy(source.begin(),source.end(),result.value.bytes_.begin());return result;}
CheckpointJobResult<CheckpointJobBytes> CheckpointJobBytes::adopt_reserved(Buffer&& bytes,CheckpointJobReservation&& reservation){if(!reservation.valid())return {false,{},CheckpointJobFailure::fixed(Code::reservation_mismatch)};if(bytes.capacity()>reservation.bytes()){std::lock_guard<std::mutex> lock(reservation.state_->mutex);return {false,{},record(reservation.state_,{reservation.state_->failed?Code::already_failed:Code::reservation_mismatch})};}auto active=reservation.grow(0);if(!active.ok)return {false,{},std::move(active.error)};CheckpointJobBytes result;result.reservation_=std::move(reservation);result.bytes_=std::move(bytes);return {true,std::move(result),{}};}
CheckpointJobStatus CheckpointJobBytes::write(std::size_t offset,const std::uint8_t* bytes,std::size_t size){auto active=reservation_.grow(0);if(!active.ok)return active;if(offset>bytes_.size()||size>bytes_.size()-offset||(!bytes&&size)){std::lock_guard<std::mutex> lock(reservation_.state_->mutex);return status(reservation_.state_,{reservation_.state_->failed?Code::already_failed:Code::write_range});}if(size)std::copy(bytes,bytes+size,bytes_.begin()+offset);return {true,{}, {}};}
}
