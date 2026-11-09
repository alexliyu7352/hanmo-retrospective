#include "native_growth_market_sources.hpp"
#include "complete_growth_command_route.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/growth/growth_fixed_command.hpp"
#include "resident/domains/growth/study_adjustment_time2.hpp"
namespace hanmo::resident {
namespace {
bool complete(const WorldBundle& world){
    return world.catalog->exact_time&&exact_complete_selected(*world.catalog->exact_time);
}
bool adjustment(const StableId& kind){
    return kind=="study_challenge_choose"||kind=="study_challenge_exit"||
        kind=="study_etiquette_choose"||kind=="study_etiquette_exit";
}
bool control(const StableId& kind){
    return kind=="study_pause"||kind=="study_resume"||
        kind=="study_cancel"||kind=="rest_cancel";
}
bool needs_permit(const WorldBundle& world,const GrowthCommand& command){
    return complete(world)&&(control(command.kind)||
        (adjustment(command.kind)&&world.common.action(command.action_id)));
}
// Destruction order keeps the actual phase alive until all of its borrowed
// readers and the domain permit have been consumed.
struct ActualGrowthCommandRead {
    std::unique_ptr<PhaseCurrentReadScope> phase;
    std::unique_ptr<CompleteExactWorldBindingOwner> owner;
};
Outcome<ActualGrowthCommandRead> read_actual_growth_command(const WorldBundle& world){
    if(!world.catalog->growth||!world.action_elapsed)
        return {false,{},common_failure("CompleteGrowthCommandSourcesRequired").error};
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},now.error};
    ActualGrowthCommandRead read;
    if(!world.has_active_read_phase()){
        auto phase=world.enter_phase_read_context("growth_command","growth",{now.value,now.value},now.value);
        if(!phase.ok)return {false,{},phase.error};read.phase=std::move(phase.value);
    }
    auto owner=world.complete_exact_time_bindings();if(!owner.ok)return {false,{},owner.error};
    if(!owner.value)return {false,{},common_failure("CompleteGrowthOwningLeaseRequired").error};
    read.owner=std::move(owner.value);
    return {true,std::move(read),{}};
}
Status same_candidate(const WorldBundle& world,const CharacterConditionSources& body){
    if(&body.common!=&world.common||body.growth!=&world.growth||
       body.elapsed!=&*world.action_elapsed||body.growth_catalog!=world.catalog->growth.get())
        return common_failure("CompleteGrowthCandidateBindingMismatch");
    return common_success();
}
}
Outcome<GrowthQuote> preview_selected_growth_command(const WorldBundle& world,
    const StableId& actor,const GrowthCommand& command){
    auto original=world.services.growth_access();
    if(!needs_permit(world,command))
        return preview_growth(world.common,world.growth,*world.catalog->growth,
            *world.catalog->common,world.social,original,actor,command);
    auto read=read_actual_growth_command(world);if(!read.ok)return {false,{},read.error};
    auto body=read.value.owner->reads().character_conditions.sources();if(!body.ok)return {false,{},body.error};
    auto same=same_candidate(world,body.value);if(!same.ok)return {false,{},same.error};
    auto market=read_native_growth_market_exact_sources(world);if(!market.ok)return {false,{},std::move(market.error)};
    auto permit=prepare_growth_fixed_command(body.value,world.growth,actor,command,market.value?&*market.value:nullptr);
    if(!permit.ok)return {false,{},permit.error};
    return preview_growth_at_fixed_command(world.common,world.growth,*world.catalog->growth,
        *world.catalog->common,world.social,original,actor,command,permit.value);
}
Outcome<GrowthEffect> apply_selected_growth_instant(WorldBundle& world,
    const StableId& actor,const GrowthQuote& quote){
    auto original=world.services.growth_access();
    if(needs_permit(world,quote.command)){
        auto read=read_actual_growth_command(world);if(!read.ok)return {false,{},read.error};
        auto body=read.value.owner->reads().character_conditions.sources();if(!body.ok)return {false,{},body.error};
        auto same=same_candidate(world,body.value);if(!same.ok)return {false,{},same.error};
        auto market=read_native_growth_market_exact_sources(world);if(!market.ok)return {false,{},std::move(market.error)};
    auto permit=prepare_growth_fixed_command(body.value,world.growth,actor,quote.command,market.value?&*market.value:nullptr);
        if(!permit.ok)return {false,{},permit.error};
        return apply_growth_instant_at_fixed_command(world.common,world.growth,*world.catalog->growth,
            *world.catalog->common,world.social,original,actor,quote,permit.value);
    }
    if(complete(world)&&adjustment(quote.command.kind)){
        // The original study has already terminated; its late-answer writer
        // still records the real moment, without inventing a live Action.
        auto at=world.actual_phase_moment();if(!at.ok)return {false,{},at.error};
        return apply_growth_instant_at_moment(world.common,world.growth,*world.catalog->growth,
            *world.catalog->common,world.social,original,actor,quote,StudyAdjustmentWriteContext{at.value});
    }
    return apply_growth_instant(world.common,world.growth,*world.catalog->growth,
        *world.catalog->common,world.social,original,actor,quote);
}
}
