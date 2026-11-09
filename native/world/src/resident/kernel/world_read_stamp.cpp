#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "world_read_stamp.hpp"
#include "world_bundle.hpp"
#include "native_actual_read_sources.hpp"
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
PhaseCurrentReadScope::~PhaseCurrentReadScope(){world_.leave_phase_read_context(generation_);}
Outcome<std::unique_ptr<PhaseCurrentReadScope>> WorldBundle::enter_phase_read_context(const StableId& phase,const StableId& provider,const MomentSpan& span,const WorldMoment& actual)const{
    if(active_read_phase_||phase.empty()||provider.empty())return {false,{},common_failure("InvalidReadPhaseContext").error};
    auto order=compare_world_moments(span.from,span.to);if(!order.ok)return {false,{},order.error};if(order.value>0)return {false,{},common_failure("InvalidReadPhaseSpan").error};
    auto from=compare_world_moments(span.from,actual);if(!from.ok)return {false,{},from.error};auto until=compare_world_moments(actual,span.to);if(!until.ok)return {false,{},until.error};if(from.value>0||until.value>0)return {false,{},common_failure("ActualReadMomentOutsideSpan").error};
    if(read_phase_generation_==std::numeric_limits<std::uint64_t>::max())return {false,{},common_failure("ReadPhaseGenerationExhausted",{},ErrorKind::internal).error};
    auto valid=read_stamps->invalidate_after_write();if(!valid.ok)return {false,{},valid.error};
    const auto generation=read_phase_generation_+1;

    auto context_memory=CheckpointMemoryCharge(0);CheckpointAllocator<ActiveReadPhase> control;
    auto context=std::shared_ptr<const ActiveReadPhase>(new ActiveReadPhase{std::move(context_memory),checkpoint_fresh_string_copy(phase),checkpoint_fresh_string_copy(provider),span,actual,generation},std::default_delete<const ActiveReadPhase>{},std::move(control));
    std::unique_ptr<PhaseCurrentReadScope> scope(new PhaseCurrentReadScope(*this,generation));
    read_phase_generation_=generation;active_read_phase_=std::move(context);return {true,std::move(scope),{}};
}
Outcome<WorldMoment> WorldBundle::actual_phase_moment()const{
    if(active_read_phase_)return {true,active_read_phase_->actual_moment,{}};
    if(!world_timeline)return {false,{},common_failure("ExactWorldTimelineRequired",{},ErrorKind::unsupported_capability).error};
    return read_world_moment(common.tick(),*world_timeline);
}
void WorldBundle::leave_phase_read_context(std::uint64_t generation)const noexcept {
    if(!active_read_phase_||active_read_phase_->generation!=generation){read_stamps->retire();return;}
    active_read_phase_.reset();read_stamps->note_mutation_boundary();
}
WorldCurrentReadStampRead::WorldCurrentReadStampRead(const WorldBundle& w,const DomainReadVersionSource& v,ReadStampLimits limits):world_(w),versions_(v),limits_(limits){}
std::unique_ptr<CurrentReadStampRead> WorldBundle::make_current_read_stamp_reader(const DomainReadVersionSource& versions,ReadStampLimits limits)const{
    return std::unique_ptr<CurrentReadStampRead>(new WorldCurrentReadStampRead(*this,versions,limits));
}
Outcome<WorldMoment> WorldCurrentMomentRead::read_current_moment()const{return world_.actual_phase_moment();}
Outcome<WorldMoment> WorldCurrentReadStampRead::read_current_moment()const{return world_.actual_phase_moment();}
Outcome<std::shared_ptr<const ReadStamp>> WorldCurrentReadStampRead::capture_binding(const WorldBundle& world,const DomainReadVersionSource& versions,const std::vector<ResourceKey>& keys,const std::vector<StableId>& sources){
    const auto phase=world.active_read_phase_;
    if(!phase||phase->phase.empty())return {false,{},common_failure("PhaseCurrentReadOutsidePhase").error};
    std::size_t text=phase->phase.size();
    auto count=[&](const StableId& id){if(id.size()>std::numeric_limits<std::size_t>::max()-text)return false;text+=id.size();return true;};
    for(const auto& key:keys)if(!count(key.id))return {false,{},common_failure("ReadStampCapacityExceeded").error};
    for(const auto& id:sources)if(!count(id))return {false,{},common_failure("ReadStampCapacityExceeded").error};
    WorldCurrentReadStampRead reader(world,versions,{std::max(std::size_t{1},keys.size()),std::max(std::size_t{1},sources.size()),text});
    return reader.capture(keys, {}, sources);
}
Outcome<std::shared_ptr<const ReadStamp>> WorldCurrentReadStampRead::capture(const std::vector<ResourceKey>& keys,const std::vector<DomainReadKey>& domains,const std::vector<StableId>& sources)const{
    const auto phase=world_.active_read_phase_;if(!phase)return {false,{},common_failure("PhaseCurrentReadOutsidePhase").error};
    if(keys.size()>limits_.max_versions||domains.size()>limits_.max_versions-keys.size()||sources.size()>limits_.max_sources)return {false,{},common_failure("ReadStampCapacityExceeded").error};
    // 有限输入直接前缀查重；数组实际allocator收费，字符串在首次copy之前取得租约。

    for(std::size_t n=0;n<keys.size();++n){const auto& key=keys[n];for(std::size_t j=0;j<n;++j)if(keys[j].kind==key.kind&&keys[j].id==key.id)return {false,{},common_failure("DuplicateReadStampResource",key.id).error};}
    for(std::size_t n=0;n<domains.size();++n){const auto& key=domains[n];for(std::size_t j=0;j<n;++j)if(domains[j].domain==key.domain&&domains[j].kind==key.kind&&domains[j].id==key.id)return {false,{},common_failure("DuplicateReadStampDomain",key.id).error};}
    for(std::size_t n=0;n<sources.size();++n){const auto& id=sources[n];for(std::size_t j=0;j<n;++j)if(sources[j]==id)return {false,{},common_failure("DuplicateReadStampActualSource",id).error};auto source=validate_native_actual_read_source(world_,id,phase->actual_moment);if(!source.ok)return {false,{},source.error};}
    auto held_text=CheckpointMemoryCharge(0);CheckpointVector<ReadVersion> actual_resources;CheckpointVector<DomainReadVersion> actual_domains;actual_resources.reserve(keys.size());actual_domains.reserve(domains.size());
    for(const auto& key:keys)actual_resources.push_back(world_.common.read_version(key));
    for(const auto& key:domains){auto revision=versions_.current_version(key.domain,key.kind,key.id);if(!revision.ok)return {false,{},revision.error};actual_domains.push_back({checkpoint_fresh_string_copy(key.domain),checkpoint_fresh_string_copy(key.kind),checkpoint_fresh_string_copy(key.id),revision.value});}
    // 查詢窗可伸向未來，但當前版本票只覆蓋已到達的真實切點；不得把預測上界當作來源時刻。
    const MomentSpan observed{phase->span.from, phase->actual_moment};
    return world_.read_stamps->issue(
        world_.common.state().identity, ReadMode::phase_current, phase->phase,
        observed, actual_resources, actual_domains, sources, limits_);
}
}
