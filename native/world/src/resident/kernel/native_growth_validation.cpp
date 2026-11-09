#include "native_growth_market_sources.hpp"
#include "native_growth_validation.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "native_domain_read_versions.hpp"
#include "world_read_stamp.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/growth/growth_complete_validation.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/growth/growth_terminal_publication.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> result){if(!result.ok)throw Failure{std::move(result.error)};return std::move(result.value);}
template<class F>Status guard(F call){try{call();return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
bool complete(const WorldBundle& world){return world.catalog&&world.catalog->exact_time&&exact_complete_selected(*world.catalog->exact_time);}
void same_candidate(const WorldBundle& world,const CharacterConditionSources& body){
    if(!world.actor_foundation||!world.action_elapsed||!world.world_timeline||
       &body.common!=&world.common||&body.state!=&*world.actor_foundation||
       &body.catalog!=world.catalog->actor_foundation.get()||body.growth!=&world.growth||
       body.growth_catalog!=world.catalog->growth.get()||body.shared!=world.catalog->common.get()||
       body.elapsed!=&*world.action_elapsed||body.scale!=&world.world_timeline->scale_ref||
       take(compare_world_moments(body.actual_now,take(world.actual_phase_moment())))!=0)
        fail("CompleteGrowthCandidateBindingMismatch");
}
std::unique_ptr<PhaseCurrentReadScope> own_phase(const WorldBundle& world,const char* name){
    if(world.has_active_read_phase())return {};
    const auto now=take(world.actual_phase_moment());
    return take(world.enter_phase_read_context(name,"growth",{now,now},now));
}
const StudyVerifiedParticipantDeath& cause(const StudyFact& fact,const StableId& id){
    if(!fact.study_service_terminal)fail("StudyActualTerminationSourceRequired",id);
    const auto* value=std::get_if<StudyVerifiedParticipantDeath>(&fact.study_service_terminal->stop_cause);
    if(!value)fail("StudyVerifiedExactParticipantDeathRequired",id);
    return *value;
}
// The stamp owns the actual actor versions and real Death headers. Reserve the
// exact fresh input copies before constructing ordinary STL input vectors.
struct DeathStampInputs {
    CheckpointMemoryCharge memory;
    std::vector<ResourceKey> actors;
    std::vector<StableId> deaths;
};
DeathStampInputs death_inputs(const GrowthState& growth,const StudyPendingReleaseProofs* only){
    std::size_t actor_count=0,death_count=0;
    auto selected=[&](const StableId& id){if(!only)return true;for(const auto& proof:*only)if(proof.action_id()&&*proof.action_id()==id)return true;return false;};
    for(const auto& row:growth.facts.rows())if(selected(row.first))if(const auto* fact=std::get_if<StudyFact>(&row.second);fact&&fact->study_service_terminal){
        const auto& value=cause(*fact,row.first);bool first_actor=true,first_death=true;
        for(const auto& prior:growth.facts.rows()){
            if(prior.first==row.first)break;
            if(!selected(prior.first))continue;
            const auto* old=std::get_if<StudyFact>(&prior.second);
            if(!old||!old->study_service_terminal)continue;
            const auto& previous=cause(*old,prior.first);
            if(previous.actor_id==value.actor_id)first_actor=false;
            if(previous.death_ref.id==value.death_ref.id)first_death=false;
        }
        if(first_actor){if(++actor_count>4096)fail("ReadStampCapacityExceeded");}
        if(first_death){if(++death_count>4096)fail("ReadStampCapacityExceeded");}
    }

    DeathStampInputs out{CheckpointMemoryCharge(0),{}, {}};out.actors.reserve(actor_count);out.deaths.reserve(death_count);
    for(const auto& row:growth.facts.rows())if(selected(row.first))if(const auto* fact=std::get_if<StudyFact>(&row.second);fact&&fact->study_service_terminal){
        const auto& value=cause(*fact,row.first);
        if(std::none_of(out.actors.begin(),out.actors.end(),[&](const ResourceKey& key){return key.id==value.actor_id;}))
            out.actors.push_back({ResourceKey::Kind::actor,checkpoint_fresh_string_copy(value.actor_id)});
        if(std::find(out.deaths.begin(),out.deaths.end(),value.death_ref.id)==out.deaths.end())
            out.deaths.push_back(checkpoint_fresh_string_copy(value.death_ref.id));
    }
    return out;
}
}
Status validate_selected_growth_state(const WorldBundle& world){return guard([&]{
    if(!world.catalog||!world.catalog->growth||!world.catalog->common)fail("GrowthCatalogRequired");
    auto original=world.services.growth_access();
    if(!complete(world)){
      // 旧选型没有终态发表段；不能遗弃真表或把共同孤头当作旧结果。
      if (!world.growth.terminal_publications.empty())
        fail("UnexpectedGrowthTerminalPublications");
      for (const auto &row : world.common.state().facts.rows())
        if (row.second.kind == "growth_result_terminal")
          fail("UnexpectedGrowthTerminalPublications", row.first);
      take(validate_growth(world.common, world.growth, *world.catalog->growth,
                           *world.catalog->common, original));
      return;
    }
    if(!world.catalog->growth->complete_study_sources)fail("CompleteGrowthSourceValidatorRequired");
    auto phase=own_phase(world,"growth_published_source_closure");
    auto owner=take(world.complete_exact_time_bindings());if(!owner)fail("CompleteGrowthOwningLeaseRequired");
    auto body=take(owner->reads().character_conditions.sources());same_candidate(world,body);
    NativeDomainReadVersionSource versions(world,owner->reads().military_formations,owner->reads().military_returns,owner->reads().military_supply_periods);
    auto reads=world.make_current_read_stamp_reader(versions,{4096,4096,1024*1024});
    auto inputs=death_inputs(world.growth,nullptr);
    std::optional<FamilyVerifiedDeathRead> deaths;std::shared_ptr<const ReadStamp> stamp;
    if(!inputs.deaths.empty()){
        if(!body.deaths||!body.life_sources)fail("StudyActualTerminationReaderRequired");
        deaths.emplace(world.common,*body.deaths,*body.life_sources,*world.read_stamps,versions);
        stamp=take(reads->capture(inputs.actors,{},inputs.deaths));
    }
    auto market=take(read_native_growth_market_exact_sources(world));
    GrowthCompleteValidationSources sources{body,deaths?&*deaths:nullptr,stamp.get(),nullptr,market?&*market:nullptr};
    take(validate_growth_with_complete_sources(world.common,world.growth,*world.catalog->growth,
        *world.catalog->common,original,sources));
    // 稳定世界沿同一当刻 owner 反验整表，不在 getter/构造器借空来源过门。
    if (!body.action_registrations)
      fail("CompleteGrowthRegistrationsRequired");
    GrowthActionTimeRead terminal_reader(
        world.common, world.growth, *world.catalog->growth,
        *world.catalog->common, *body.elapsed, *body.scale,
        world.catalog->identity, body.actual_now, {}, body.interaction,
        body.interaction_catalog, market ? &*market : nullptr);
    take(validate_growth_terminal_publications(world.common, world.growth,
                                               terminal_reader,
                                               *body.action_registrations));
});}
Status validate_pending_growth_releases(const WorldBundle& world,const StudyPendingReleaseProofs& proofs){return guard([&]{
    if(proofs.empty())return;
    if(!complete(world))fail("StudyPendingReleaseCompleteProfileRequired");
    auto phase=own_phase(world,"growth_private_release_closure");
    auto owner=take(world.complete_exact_time_bindings());if(!owner)fail("CompleteGrowthOwningLeaseRequired");
    auto body=take(owner->reads().character_conditions.sources());same_candidate(world,body);
    if(!body.deaths||!body.life_sources)fail("StudyActualTerminationReaderRequired");
    NativeDomainReadVersionSource versions(world,owner->reads().military_formations,owner->reads().military_returns,owner->reads().military_supply_periods);
    FamilyVerifiedDeathRead deaths(world.common,*body.deaths,*body.life_sources,*world.read_stamps,versions);
    auto reads=world.make_current_read_stamp_reader(versions,{4096,4096,1024*1024});
    auto inputs=death_inputs(world.growth,&proofs);auto stamp=take(reads->capture(inputs.actors,{},inputs.deaths));
    auto original=world.services.growth_access();
    for(std::size_t n=0;n<proofs.size();++n){
        const auto* id=proofs[n].action_id();if(!id)fail("StudyPendingReleaseProofRequired");
        for(std::size_t previous=0;previous<n;++previous)
            if(proofs[previous].action_id()&&*proofs[previous].action_id()==*id)fail("DuplicateStudyPendingReleaseProof",*id);
        const auto* raw=world.growth.facts.find(*id);
        const auto* study=raw?std::get_if<StudyFact>(raw):nullptr;
        if(!study||!study->study_service_terminal||
           study->study_service_terminal->reservation_release_results.size()!=1)
            fail("StudyActualTerminationSourceRequired",*id);
        const auto& release=study->study_service_terminal->reservation_release_results.front();
        if(release.original_release_commit_ref.result_id.empty())
            take(read_study_service_termination_in_capture(body,world.growth,original,{*id},*stamp,deaths,proofs[n]));
        else{
            // The real result/history has already been appended in this same
            // submission. The pending witness must not replace that identity.
            take(read_study_service_termination(world.common,world.growth,*world.catalog->growth,
                body.catalog,original,{*id},body.actual_now,*stamp,deaths));
        }
    }
});}
}
