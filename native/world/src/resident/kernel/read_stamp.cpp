#include "resident/checkpoint/checkpoint_retention.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "read_stamp.hpp"
#include "world_copy_plan.hpp"
#include "world_retention.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <limits>
#include <new>
#include <utility>
namespace hanmo::resident {
struct ReadStampState {std::uint64_t slice=0,savepoint=0;bool retired=false;};
namespace {
RuleError error(const char* code,ErrorKind kind=ErrorKind::rule){RuleError e;e.original={code,"read_stamp"};e.kind=kind;return e;}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool add_text(std::size_t& used,const StableId& value,std::size_t limit){if(value.size()>limit-used)return false;used+=value.size();return true;}
}
ReadStampAuthority::ReadStampAuthority():state_(checkpoint_make_shared<ReadStampState>()){}
// weak 资格仍按原 state 判断；真实 allocator owner 不复活已退役的业务权限。
void ReadStamp::retain_checkpoint_memory(CheckpointRetentionCensus& c)const{
    c.owner(candidate_allocation_);c.vector(resources_);c.vector(domains_);c.vector(sources_);
    retain_canonical_fraction(c,span_.from.sub_half_day);retain_canonical_fraction(c,span_.to.sub_half_day);
    c.resource(text_memory_.resource());
}
void ReadStampAuthority::retain_checkpoint_memory(CheckpointRetentionCensus& c)const{c.object(checkpoint_object_allocation(this));c.owner(checkpoint_shared_allocation_owner(state_));}
ReadStampAuthority::~ReadStampAuthority(){retire();}
void ReadStampAuthority::retire() noexcept {if(state_)state_->retired=true;}
void ReadStampAuthority::note_mutation_boundary() noexcept {
    if(!state_||state_->retired)return;
    if(state_->slice==std::numeric_limits<std::uint64_t>::max()){state_->retired=true;return;}
    ++state_->slice;
}
Status ReadStampAuthority::usable() const {
    if(!state_||state_->retired)return {false,{},error("RetiredReadStampAuthority",ErrorKind::internal)};
    return {true,{},{}};
}
Status ReadStampAuthority::invalidate_after_write() {
    if(!state_||state_->retired)return {false,{},error("RetiredReadStampAuthority",ErrorKind::internal)};
    if(state_->slice==std::numeric_limits<std::uint64_t>::max()){state_->retired=true;return {false,{},error("ReadStampGenerationExhausted",ErrorKind::internal)};}
    ++state_->slice;return {true,{},{}};
}
Status ReadStampAuthority::invalidate_after_savepoint() {
    if(!state_||state_->retired)return {false,{},error("RetiredReadStampAuthority",ErrorKind::internal)};
    if(state_->savepoint==std::numeric_limits<std::uint64_t>::max()){state_->retired=true;return {false,{},error("ReadStampGenerationExhausted",ErrorKind::internal)};}
    ++state_->savepoint;return invalidate_after_write();
}
Outcome<std::shared_ptr<const ReadStamp>> ReadStampAuthority::issue(const WorldIdentity& identity,ReadMode mode,const StableId& phase,
    const MomentSpan& span,ReadStampRange<ReadVersion> resources,ReadStampRange<DomainReadVersion> domains,ReadStampRange<StableId> sources,
    const ReadStampLimits& limits,ExactArithmeticBudget arithmetic) const {
    if(!state_||state_->retired)return {false,{},error("RetiredReadStampAuthority",ErrorKind::internal)};
    if(!limits.max_versions||!limits.max_sources||!limits.max_text_bytes||phase.empty())return {false,{},error("InvalidReadStampLimits",ErrorKind::internal)};
    if(resources.size()>limits.max_versions||domains.size()>limits.max_versions-resources.size()||sources.size()>limits.max_sources)
        return {false,{},error("ReadStampCapacityExceeded",ErrorKind::internal)};
    auto order=compare_world_moments(span.from,span.to,arithmetic);
    if(!order.ok)return {false,{},std::move(order.error)};
    if(order.value>0)return {false,{},error("InvalidReadStampSpan")};
    std::size_t text=0;
    if(!add_text(text,phase,limits.max_text_bytes))return {false,{},error("ReadStampCapacityExceeded",ErrorKind::internal)};
    for(const auto& r:resources)if(r.resource.id.empty()||!add_text(text,r.resource.id,limits.max_text_bytes))return {false,{},error("InvalidReadStampResource")};
    for(const auto& d:domains)if(d.domain.empty()||d.kind.empty()||d.id.empty()||!add_text(text,d.domain,limits.max_text_bytes)||!add_text(text,d.kind,limits.max_text_bytes)||!add_text(text,d.id,limits.max_text_bytes))return {false,{},error("InvalidReadStampDomain")};
    for(const auto& s:sources)if(s.empty()||!add_text(text,s,limits.max_text_bytes))return {false,{},error("InvalidReadStampSource")};
    // 数组/BigNat/对象/控制块直接使用真实allocator；普通字符串仅走锁定STL的fresh-copy窄口。


    auto held_text=CheckpointMemoryCharge(0);CheckpointAllocator<ReadStamp> allocation;auto* raw=allocation.allocate(1);
    try{::new(static_cast<void*>(raw))ReadStamp;}catch(...){allocation.deallocate(raw,1);throw;}
    std::shared_ptr<ReadStamp> stamp(raw,CheckpointSharedDelete<ReadStamp>{allocation},allocation);
    stamp->text_memory_=std::move(held_text);stamp->candidate_=state_;stamp->candidate_allocation_=checkpoint_shared_allocation_owner(state_);stamp->slice_=state_->slice;stamp->savepoint_=state_->savepoint;
    stamp->identity_=identity;stamp->mode_=mode;stamp->phase_=checkpoint_fresh_string_copy(phase);stamp->span_=span;
    if(resources.size())stamp->resources_=CheckpointVector<ReadVersion>(resources.begin(),resources.end());if(domains.size())stamp->domains_=CheckpointVector<DomainReadVersion>(domains.begin(),domains.end());if(sources.size())stamp->sources_=CheckpointVector<StableId>(sources.begin(),sources.end());
    return {true,std::move(stamp),{}};
}
Status ReadStampAuthority::validate(const ReadStamp& stamp,const WorldIdentity& identity,ReadMode mode,const CommonRead& common,const DomainReadVersionSource& domains) const {
    const auto candidate=stamp.candidate_.lock();
    if(!candidate||!state_||candidate.get()!=state_.get()||candidate->retired||stamp.slice_!=state_->slice||stamp.savepoint_!=state_->savepoint)
        return {false,{},error("ExpiredReadStamp")};
    if(!same_identity(stamp.identity_,identity)||stamp.mode_!=mode)return {false,{},error("ReadStampContextMismatch")};
    for(const auto& r:stamp.resources_)if(common.read_version(r.resource).version!=r.version)return {false,{},error("ReadStampResourceChanged")};
    for(const auto& d:stamp.domains_){auto actual=domains.current_version(d.domain,d.kind,d.id);if(!actual.ok)return {false,{},std::move(actual.error)};if(actual.value!=d.revision)return {false,{},error("ReadStampDomainChanged")};}
    return {true,{},{}};
}
}
