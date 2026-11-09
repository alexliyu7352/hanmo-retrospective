#include "profile_acquisition_world.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "growth_cards.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "resident/domains/tasks/envoy_task_rules.hpp"
#include <algorithm>
namespace hanmo::resident {
struct ProfileAcquisitionWorldRead::Impl:CurrentMomentRead {
    const WorldBundle& world;std::optional<DiplomaticMissionAccess> missions;std::optional<DiplomaticReportAccess> reports;std::unique_ptr<EnvoyTaskRead> envoy;
    std::optional<CardAcquisitionSources> source;ProfileControlEligibilitySources control;
    explicit Impl(const WorldBundle& w):world(w),control{w.common,w.family,*w.catalog->family,*w.catalog->common,w.family_legacy_enabled()?&w.family_legacy:nullptr,w.actor_foundation?&*w.actor_foundation:nullptr,w.catalog->actor_foundation.get(),this}{}
    Outcome<WorldMoment> read_current_moment()const override{return world.actual_phase_moment();}
};
ProfileAcquisitionWorldRead::ProfileAcquisitionWorldRead(CheckpointMemoryCharge memory,std::unique_ptr<Impl> value):object_memory_(std::move(memory)),impl_(std::move(value)){}
ProfileAcquisitionWorldRead::~ProfileAcquisitionWorldRead()=default;
const CardAcquisitionSources& ProfileAcquisitionWorldRead::sources()const{return *impl_->source;}
const CardAcquisitionState& ProfileAcquisitionWorldRead::acquisitions()const{return impl_->world.card_acquisitions;}
const ProfileControlEligibilitySources& ProfileAcquisitionWorldRead::control_sources()const{return impl_->control;}
namespace {
Status selected_acquisition_profile(const WorldBundle& world){
    if(!world.catalog||!world.catalog->common||!world.catalog->growth||!world.catalog->tasks||!world.catalog->family)return common_failure("ProfileAcquisitionWorldSourcesRequired",{},ErrorKind::unsupported_capability);
    const auto& modules=world.catalog->identity.module_versions;
    const auto selected=std::find_if(modules.begin(),modules.end(),[](const auto& row){return row.first=="growth_permanent_contributions";});
    const bool chain=world.catalog->growth->complete_capability_application_chain;
    if(selected==modules.end()||std::count_if(modules.begin(),modules.end(),[](const auto& row){return row.first=="growth_permanent_contributions";})!=1||selected->second!=(chain?"3":"1"))return common_failure("ProfileAcquisitionCompleteSourceProfileRequired",{},ErrorKind::unsupported_capability);
    if(chain&&(!world.growth_permanent||!world.catalog->growth_permanent))return common_failure("GrowthPermanentOriginalSourcesRequired");
    return common_success();
}
}
Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>> ProfileAcquisitionWorldRead::create(const WorldBundle& world){
    auto selected=selected_acquisition_profile(world);if(!selected.ok)return {false,{},selected.error};
    if(!world.card_acquisitions_seed_complete)return {false,{},common_failure("ProfileAcquisitionCompleteSeedRequired",{},ErrorKind::unsupported_capability).error};
    return bind_original_sources(world);
}
Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>> ProfileAcquisitionWorldRead::bind_original_sources(const WorldBundle& world){
    auto selected=selected_acquisition_profile(world);if(!selected.ok)return {false,{},selected.error};
    bool complete_origin=false;for(const auto& module:world.catalog->identity.module_versions)if(module.first=="actor_start_context")complete_origin=module.second=="3";
    auto actual_now=world.actual_phase_moment();if(complete_origin&&!actual_now.ok)return {false,{},actual_now.error};if(complete_origin&&(!world.actor_foundation||!world.catalog->actor_foundation))return {false,{},common_failure("ProfileCreatedOriginSourcesRequired").error};
    auto mode=complete_origin?read_character_creation_mode_v2(world.common,*world.actor_foundation,*world.catalog->actor_foundation,world.family,*world.catalog->family,*world.catalog->common,actual_now.value):family_character_creation_mode_body(world.common,world.family,*world.catalog->family,*world.catalog->common);if(!mode.ok)return {false,{},mode.error};const auto& cards=world.catalog->growth->actor_start.cards;const bool needs_envoy=std::any_of(cards.definitions.rows().begin(),cards.definitions.rows().end(),[](const auto& row){return row.second.condition=="completed_envoy_round_trip_v1";});
    CheckpointMemoryCharge memory(0);auto context=std::make_unique<Impl>(world);
    if(needs_envoy){if(cards.version!=2||!world.envoy_tasks_enabled()||world.envoy_tasks.version!=2||!world.diplomacy.v2||!world.catalog->diplomacy||!world.catalog->organization)return {false,{},common_failure("ActorCardEnvoySourceRequired").error};context->missions.emplace(world.common,world.organization,*world.catalog->organization,*world.catalog->diplomacy,world.diplomacy);context->reports.emplace(DiplomaticHistoryRead{world.common,world.organization,*world.catalog->organization,*world.catalog->diplomacy,world.diplomacy},*world.diplomacy.v2);context->envoy=std::make_unique<EnvoyTaskRead>(world.common,world.tasks,world.envoy_tasks,*context->missions,*context->missions,*context->reports);}
    context->source.emplace(CardAcquisitionSources{world.common.state(),*world.catalog->common,world.growth,*world.catalog->growth,world.tasks,*world.catalog->tasks,cards,world.military,world.catalog->growth->insights,mode.value,needs_envoy?&world.envoy_tasks:nullptr,context->envoy.get(),context.get()});return {true,std::unique_ptr<ProfileAcquisitionWorldRead>(new ProfileAcquisitionWorldRead(std::move(memory),std::move(context))),{}};
}
Status initialize_profile_acquisitions_from_original_seed_in_candidate(WorldBundle& world){
    // This is a write-side new-world path, not a reader option or restore repair.
    // Its caller owns the whole private world; every ID, mapping and delivery is
    // discarded with that candidate if any later initialization fails.
    const auto& common=world.common.state();
    if(common.identity.tick!=0||common.identity.revision!=0||!common.history.empty()||!common.results.empty()||!common.actions.empty())return common_failure("ProfileAcquisitionFreshWorldRequired");
    if(std::all_of(common.identity.world_id.begin(),common.identity.world_id.end(),[](auto byte){return byte==0;}))return common_failure("ProfileAcquisitionActualWorldIdentityRequired");
    if(common.request_sequences.size()!=common.actors.size())return common_failure("ProfileAcquisitionInitialRequestSequenceRequired");
    for(const auto& actor:common.actors.rows()){const auto* sequence=common.request_sequences.find(actor.first);if(!sequence||*sequence)return common_failure("ProfileAcquisitionInitialRequestSequenceRequired",actor.first);}
    auto current=world.actual_phase_moment();if(!current.ok)return {false,{},current.error};
    auto zero=compare_world_moments(current.value,WorldMoment{});if(!zero.ok)return {false,{},zero.error};if(zero.value)return common_failure("ProfileAcquisitionFreshWorldRequired");
    auto bound=ProfileAcquisitionWorldRead::bind_original_sources(world);if(!bound.ok)return {false,{},bound.error};
    const auto& sources=bound.value->sources();const auto& control=bound.value->control_sources();
    if(world.card_acquisitions_seed_complete){auto original=validate_card_acquisitions(sources,world.card_acquisitions);if(!original.ok)return original;return validate_profile_delivery_outbox(common,world.profile_deliveries,sources,world.card_acquisitions,control);}
    if(!world.card_acquisitions.card_acquisitions.empty()||!world.card_acquisitions.legacy_card_source_mappings.empty()||!world.profile_deliveries.entries.empty())return common_failure("ProfileAcquisitionFreshStateRequired");
    if(world.catalog->growth->cards.enabled&&!world.growth.cards)return common_failure("OriginalGrowthCardSeedRequired");
    if(world.growth.cards){auto original=validate_growth_cards(common,world.growth,*world.growth.cards,world.catalog->growth->cards);if(!original.ok)return original;}
    // Map every real original acquisition, including NPC ownership, while only
    // the actual initial controlled actor can produce profile deliveries.
    for(const auto& actor:common.actors.rows()){auto mapped=bind_verified_legacy_card_acquisitions(world.common,world.card_acquisitions,sources,actor.first);if(!mapped.ok)return {false,{},mapped.error};}
    auto valid=validate_card_acquisitions(sources,world.card_acquisitions);if(!valid.ok)return valid;
    for(const auto& row:world.card_acquisitions.legacy_card_source_mappings.rows())if(row.second.actor_id==common.metadata.player_actor_id){auto appended=append_profile_acquisition_deliveries(world.common,world.profile_deliveries,sources,world.card_acquisitions,control,LegacyCardSourceMappingRefV1{row.first});if(!appended.ok)return appended;}
    valid=validate_profile_delivery_outbox(common,world.profile_deliveries,sources,world.card_acquisitions,control);if(!valid.ok)return valid;
    world.card_acquisitions_seed_complete=true;return common_success();
}
Status validate_profile_acquisitions_world(const WorldBundle& world){
    auto read=ProfileAcquisitionWorldRead::create(world);if(!read.ok)return {false,{},read.error};
    return validate_card_acquisitions(read.value->sources(),read.value->acquisitions());
}
Status record_growth_training_card_acquisitions_in_candidate(WorldBundle& world,const StableId& id){
    if(!world.catalog||!world.catalog->growth||!world.catalog->growth->complete_study_sources||!world.catalog->growth->cards.enabled)return common_failure("CanonicalTrainingCardProfileRequired",id);
    auto read=ProfileAcquisitionWorldRead::create(world);if(!read.ok)return {false,{},read.error};const auto& sources=read.value->sources();
    auto at=read_growth_training_card_acquired_at(sources,id);if(!at.ok)return {false,{},at.error};
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto actual=compare_world_moments(at.value.exact_moment,now.value);if(!actual.ok)return {false,{},actual.error};if(at.value.kind!=ActorFoundationAt::Kind::exact_moment||actual.value)return common_failure("CardAcquisitionActualApplyMomentRequired",id);
    const auto* raw=world.growth.facts.find(id);const auto* training=raw?std::get_if<TrainingFact>(raw):nullptr;if(!training)return common_failure("CardActualCompletedTrainingRequired",id);
    for(const auto& definition:world.catalog->growth->cards.definitions.rows())if(definition.second.personal_skill_id==training->training.skill_id){
        auto source=record_verified_card_acquisition(world.common,world.card_acquisitions,sources,definition.first,GrowthTrainingCardCompletion{id,training->training.student_id,definition.first,training->training.skill_id},at.value);if(!source.ok)return {false,{},source.error};
        if(training->training.student_id==world.common.state().metadata.player_actor_id){auto delivered=world.record_profile_acquisition(source.value);if(!delivered.ok)return delivered;}
    }
    return validate_card_acquisitions(sources,world.card_acquisitions);
}
Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>> WorldBundle::profile_acquisition_read()const{return ProfileAcquisitionWorldRead::create(*this);}
}
