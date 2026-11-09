#include "native_growth_permanent_world.hpp"
#include "native_growth_permanent_catalog.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
namespace hanmo::resident {
Status validate_native_growth_permanent_layout(const WorldBundle& world){
    if(!world.catalog)return common_failure("GrowthPermanentCatalogRequired");
    auto catalog=validate_native_growth_permanent_catalog(*world.catalog);if(!catalog.ok)return catalog;
    const bool selected=world.catalog->growth->complete_capability_application_chain;
    if(selected!=bool(world.growth_permanent))return common_failure("GrowthPermanentSelectedStatePairRequired");
    if(selected&&!world.card_acquisitions_seed_complete)return common_failure("GrowthPermanentOriginalCardStateRequired");
    return common_success();
}
NativeWorldGrowthPermanentBindings::NativeWorldGrowthPermanentBindings(WorldBundle& world,
    const CharacterConditionWorldRead& conditions,const WorkPracticeContributionRead& work)
    :world_(world),conditions_(conditions),work_(work){}
bool NativeWorldGrowthPermanentBindings::bound_to(const CommonStore& common,const GrowthPermanentState& state)const noexcept{
    return &common==&world_.common&&world_.growth_permanent&&&state==&*world_.growth_permanent&&work_.bound_to(common);
}
bool NativeWorldGrowthPermanentBindings::bound_to(const CommonStore& common,const TaskState& tasks)const noexcept{
    return &common==&world_.common&&&tasks==&world_.tasks&&work_.bound_to(common);
}
Status NativeWorldGrowthPermanentBindings::require_live_write()const{
    if(!world_.active_exact_write_context()||!world_.common.actor_condition_capture_identity())
        return common_failure("GrowthCapabilityActualCandidateCaptureRequired",{},ErrorKind::internal);
    return common_success();
}
template<class F>Status NativeWorldGrowthPermanentBindings::with_current_sources(F&& call)const{
    if(!world_.catalog||!world_.catalog->exact_time||!exact_complete_selected(*world_.catalog->exact_time)||
       !world_.catalog->growth||!world_.catalog->growth->complete_capability_application_chain||
       !world_.catalog->growth_permanent||!world_.growth_permanent||!world_.actor_foundation||
       !world_.action_elapsed)
        return common_failure("GrowthCapabilityOriginalOwningSourcesRequired",{},ErrorKind::unsupported_capability);
    if(!work_.bound_to(world_.common))
        return common_failure("GrowthCapabilitySameCandidateSourceMismatch",{},ErrorKind::internal);
    auto now=world_.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
    auto body=conditions_.sources();if(!body.ok)return {false,{},std::move(body.error)};
    auto at=compare_world_moments(body.value.actual_now,now.value);if(!at.ok)return {false,{},std::move(at.error)};
    if(at.value||&body.value.common!=&world_.common||&body.value.state!=&*world_.actor_foundation||
       body.value.growth!=&world_.growth||body.value.growth_catalog!=world_.catalog->growth.get()||
       body.value.permanent_growth!=&*world_.growth_permanent||
       body.value.permanent_growth_catalog!=world_.catalog->growth_permanent.get()||
       body.value.work_practice!=&work_||body.value.elapsed!=&*world_.action_elapsed)
        return common_failure("GrowthCapabilitySameCandidateSourceMismatch",{},ErrorKind::internal);
    GrowthPermanentSources sources{body.value,*world_.catalog->growth_permanent,*world_.growth_permanent,work_};
    // These borrows are used synchronously and cannot escape this stack. The
    // next call re-reads actual_phase_moment after any original writer.
    NativeGrowthCapabilityRevisionWrite writer(world_.common,*world_.actor_foundation,
        world_.growth,*world_.growth_permanent,sources);
    return call(body.value,sources,writer);
}
Status NativeWorldGrowthPermanentBindings::record_actual_growth_change(const StableId& actor,
    const LifeGrowthContributionRef& source,const WorldMoment& at){
    auto live=require_live_write();if(!live.ok)return live;
    return with_current_sources([&](const CharacterConditionSources&,const GrowthPermanentSources&,GrowthCapabilityRevisionWrite& writer){
        return writer.record_actual_growth_change(actor,source,at);
    });
}
Status NativeWorldGrowthPermanentBindings::record_actual_condition_change(const BoundActorConditionWrite& actual){
    auto live=require_live_write();if(!live.ok)return live;
    return with_current_sources([&](const CharacterConditionSources&,const GrowthPermanentSources&,GrowthCapabilityRevisionWrite& writer){
        return writer.record_actual_condition_change(actual);
    });
}
Status NativeWorldGrowthPermanentBindings::consume_completed_result(const WorkResultSource& result,const WorldMoment& at){
    auto live=require_live_write();if(!live.ok)return live;
    return with_current_sources([&](const CharacterConditionSources& body,const GrowthPermanentSources& sources,GrowthCapabilityRevisionWrite&){
        auto same=compare_world_moments(body.actual_now,at);if(!same.ok)return Status{false,{},std::move(same.error)};
        if(same.value)return common_failure("WorkGrowthActualCompletionMomentMismatch",{},ErrorKind::internal);
        NativeWorkCompletedGrowthConsumer consumer(world_.common,world_.growth,*world_.growth_permanent,
            sources,world_.tasks,*world_.action_elapsed,*this);
        return consumer.consume_completed_result(result,at);
    });
}
Status NativeWorldGrowthPermanentBindings::initialize_actor_in_candidate(const StableId& actor){
    return with_current_sources([&](const CharacterConditionSources& body,const GrowthPermanentSources&,GrowthCapabilityRevisionWrite&){
        return initialize_growth_capability_revision_for_actor_in_candidate(world_.common,*world_.growth_permanent,body,actor);
    });
}
Status NativeWorldGrowthPermanentBindings::validate()const{
    auto complete=conditions_.validate();if(!complete.ok)return complete;
    return with_current_sources([&](const CharacterConditionSources&,const GrowthPermanentSources& sources,GrowthCapabilityRevisionWrite&){
        auto results=validate_growth_permanent_results(sources);if(!results.ok)return results;
        return validate_growth_capability_chain(sources,world_.growth);
    });
}
}
