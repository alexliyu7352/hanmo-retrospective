#include "native_growth_market_sources.hpp"
#include "complete_growth_command_binding.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "native_domain_read_versions.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/growth/growth_fixed_start.hpp"
#include "resident/domains/growth/growth_fixed_command.hpp"
#include "resident/domains/growth/growth_capability_chain.hpp"
#include "resident/domains/growth/study_adjustment_time2.hpp"
#include "resident/domains/growth/study_service_termination.hpp"
namespace hanmo::resident {
Status bind_complete_growth_command_start(const WorldBundle* before,WorldBundle& world,
    const StableId& actor,Integer sequence,const CommitResult& result){
    if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))return common_success();
    const auto* start=result.details?std::get_if<ActionStarted>(&result.details->data):nullptr;
    if(!start)return common_success();
    const auto* action=world.common.action(start->action_id);
    if(!action)return common_failure("CompleteActualStartedActionRequired",start->action_id);
    if(!world.catalog->complete_exact_time_runtime)return common_failure("CompleteExactRuntimeRequired");
    const ActionTimeRegistration* selected=nullptr;
    for(const auto& registration:world.catalog->complete_exact_time_runtime->action_times)
        if(registration.action_kind==action->kind){if(selected)return common_failure("DuplicateActualActionTimeRegistration",action->kind);selected=&registration;}
    if(!selected)return common_failure("CompleteActualActionTimeRegistrationRequired",action->kind);
    if(selected->owner_domain!="growth")return common_success();
    if(!before||before==&world||before->catalog.get()!=world.catalog.get()||!world.action_elapsed)
        return common_failure("CompleteGrowthActualBeforeRequired",action->id);
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},now.error};
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if(!world.has_active_read_phase()){
        auto entered=world.enter_phase_read_context("growth_command_acceptance","growth",{now.value,now.value},now.value);
        if(!entered.ok)return {false,{},entered.error};phase=std::move(entered.value);
    }
    auto lease=world.complete_exact_time_bindings();if(!lease.ok)return {false,{},lease.error};
    if(!lease.value)return common_failure("CompleteGrowthOwningLeaseRequired");
    const auto& binding=lease.value->reads();auto body=binding.character_conditions.sources();
    if(!body.ok)return {false,{},body.error};
    if(&body.value.common!=&world.common||body.value.growth!=&world.growth||body.value.elapsed!=&*world.action_elapsed)
        return common_failure("CompleteGrowthCandidateBindingMismatch");
    NativeDomainReadVersionSource versions(world,binding.military_formations,binding.military_returns,binding.military_supply_periods);
    auto reads=world.make_current_read_stamp_reader(versions,{4096,4096,1024*1024});
    auto original=world.services.growth_access();
    auto market=read_native_growth_market_exact_sources(world);if(!market.ok)return {false,{},std::move(market.error)};
    GrowthFixedCaptureServices services{*reads,*world.read_stamps,versions,original,lease.value->growth_start_participation_writer(),market.value?&*market.value:nullptr};
    auto bound=bind_growth_fixed_started_command_in_candidate(before->growth,world.common,
        world.growth,body.value,*world.action_elapsed,services,actor,sequence,result);
    return bound.ok?common_success():Status{false,{},bound.error};
}
Status bind_complete_growth_release_result(const WorldBundle* before,WorldBundle& world,
    const StableId& actor,Integer sequence,const CommitResult& result){
    if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))return common_success();
    bool new_study_release=false;
    for(const auto& row:world.growth.facts.rows())if(const auto* study=std::get_if<StudyFact>(&row.second);study&&study->study_service_terminal){
        const auto* prior=before?before->growth.facts.find(row.first):nullptr;
        const auto* old=prior?std::get_if<StudyFact>(prior):nullptr;
        if(!old||!old->study_service_terminal)new_study_release=true;
    }
    if(new_study_release){
        if(!before||before==&world||before->catalog.get()!=world.catalog.get())
            return common_failure("StudyReleaseActualBeforeRequired",result.result_id);
        auto from=before->actual_phase_moment();if(!from.ok)return {false,{},from.error};
        auto at=world.actual_phase_moment();if(!at.ok)return {false,{},at.error};
        auto released=bind_study_service_release_result_in_candidate(before->growth,world.common,
            world.growth,actor,sequence,result,from.value,at.value);
        if(!released.ok)return released;
    }
    return common_success();
}
Status bind_complete_growth_command_result(const WorldBundle* before,WorldBundle& world,
    const StableId& actor,Integer sequence,const CommitResult& result){
    if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))return common_success();
    auto release=bind_complete_growth_release_result(before,world,actor,sequence,result);if(!release.ok)return release;
    const bool adjustment=result.kind=="study_challenge_choose"||result.kind=="study_challenge_exit"||
        result.kind=="study_etiquette_choose"||result.kind=="study_etiquette_exit";
    const bool control=result.kind=="study_pause"||result.kind=="study_resume"||
        result.kind=="study_cancel"||result.kind=="rest_cancel";
    if(adjustment||control){
        if(!before||before==&world||before->catalog.get()!=world.catalog.get()||
           !world.catalog->growth||!world.action_elapsed)
            return common_failure("CompleteGrowthActualBeforeRequired",result.result_id);
        auto from=before->actual_phase_moment();if(!from.ok)return {false,{},from.error};
        auto at=world.actual_phase_moment();if(!at.ok)return {false,{},at.error};
        if(adjustment){
            auto adjusted=bind_study_adjustment_result_in_candidate(before->growth,world.common,
                world.growth,*world.catalog->growth,actor,sequence,result,from.value,at.value);
            if(!adjusted.ok)return adjusted;
        }
        std::unique_ptr<PhaseCurrentReadScope> phase;
        if(!world.has_active_read_phase()){
            auto entered=world.enter_phase_read_context("growth_command_acceptance","growth",{at.value,at.value},at.value);
            if(!entered.ok)return {false,{},entered.error};phase=std::move(entered.value);
        }
        auto lease=world.complete_exact_time_bindings();if(!lease.ok)return {false,{},lease.error};
        if(!lease.value)return common_failure("CompleteGrowthOwningLeaseRequired");
        auto body=lease.value->reads().character_conditions.sources();if(!body.ok)return {false,{},body.error};
        if(&body.value.common!=&world.common||body.value.growth!=&world.growth||
           body.value.elapsed!=&*world.action_elapsed||body.value.growth_catalog!=world.catalog->growth.get())
            return common_failure("CompleteGrowthCandidateBindingMismatch");
        if(adjustment){
            if(world.catalog->growth->complete_capability_application_chain){
                auto* capability=lease.value->growth_capability_writer();
                if(!capability||!world.growth_permanent||!capability->bound_to(world.common,*world.growth_permanent))
                    return common_failure("GrowthCapabilityActualAdjustmentOwnerRequired",result.result_id);
                // 真实提交绑定之后先采用净技能源，再冻结 FSL 新输入，保持原应用顺序。
                auto applied=record_study_adjustment_capability_for_commit_in_candidate(body.value,world.growth,result,*capability);
                if(!applied.ok)return applied;
            }
            auto adopted=adopt_growth_adjustment_inputs_in_candidate(before->growth,world.common,
                world.growth,body.value,*world.action_elapsed,actor,sequence,result);
            if(!adopted.ok)return adopted;
        }else{
            NativeDomainReadVersionSource versions(world,lease.value->reads().military_formations,lease.value->reads().military_returns,lease.value->reads().military_supply_periods);
            auto reads=world.make_current_read_stamp_reader(versions,{4096,4096,1024*1024});
            auto original=world.services.growth_access();
            auto market=read_native_growth_market_exact_sources(world);if(!market.ok)return {false,{},std::move(market.error)};
    GrowthFixedCaptureServices services{*reads,*world.read_stamps,versions,original,lease.value->growth_start_participation_writer(),market.value?&*market.value:nullptr};
            auto bound=bind_growth_fixed_control_command_in_candidate(before->growth,world.common,
                world.growth,body.value,*world.action_elapsed,services,actor,sequence,result);
            if(!bound.ok)return {false,{},bound.error};
        }
    }
    return bind_complete_growth_command_start(before,world,actor,sequence,result);
}
}
