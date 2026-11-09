#include "native_actor_social_occupancy.hpp"
#include "original_catalog_identity.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
namespace hanmo::resident {
namespace {
Outcome<Integer> selection(const BoundaryValue& scenario){try{
    using namespace boundary;
    const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return {true,0,{}};
    const BoundaryValue* selected=nullptr;
    for(const auto& row:object(*dynamic,"scenario/dynamic_world"))if(row.first=="actor_social_occupancy"){
        if(selected)fail("DuplicateActorSocialOccupancySelection","scenario/dynamic_world");selected=&row.second;
    }
    if(!selected)return {true,0,{}};
    fields(*selected,{"version"},{},"actor_social_occupancy");
    const auto version=get_int(*selected,"version","actor_social_occupancy");
    if(object(*selected,"actor_social_occupancy").size()!=1||(version!=1&&version!=2&&version!=3))
        fail("UnsupportedActorSocialOccupancyVersion","actor_social_occupancy/version");
    auto exact=decode_exact_time_profile(scenario);if(!exact.ok)return {false,0,std::move(exact.error)};
    if(!exact.value||!exact_complete_selected(*exact.value))
        unsupported("ActorSocialOccupancyCompleteProfileRequired","actor_social_occupancy");
    auto growth=read_complete_growth_phase_version(*exact.value);if(!growth.ok)return {false,0,std::move(growth.error)};
    if(growth.value<2)unsupported("ActorSocialOccupancyGrowthCommandCutProfileRequired","actor_social_occupancy");
    return {true,version,{}};
}catch(const boundary::DecodeFailure& e){return {false,0,e.error};}}
Status append(CatalogIdentity& identity,Integer selected){
    for(const auto& row:identity.module_versions)if(row.first=="actor_social_occupancy")
        return common_failure("DuplicateActorSocialOccupancyMarker");
    if(selected)identity.module_versions.emplace_back("actor_social_occupancy",std::to_string(selected));
    return common_success();
}

}
Outcome<Integer> selected_native_actor_social_occupancy_version(const CatalogIdentity& identity){
    Integer result=0;
    for(const auto& row:identity.module_versions)if(row.first=="actor_social_occupancy"){
        if(result||(row.second!="1"&&row.second!="2"&&row.second!="3"))return {false,{},common_failure("ActorSocialOccupancyModuleMismatch",{},ErrorKind::internal).error};
        result=row.second=="1"?1:row.second=="2"?2:3;
    }
    return {true,result,{}};
}
Status append_native_actor_social_occupancy_module(const BoundaryValue& scenario,CatalogIdentity& identity){
    auto selected=selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
    return append(identity,selected.value);
}
Status prepare_native_actor_social_occupancy_selection(const BoundaryValue& scenario,PreparedCatalog& catalog){
    if(catalog.actor_social_occupancy_selected||catalog.actor_social_occupancy_initial)
        return common_failure("ActorSocialOccupancyCatalogAlreadyPrepared");
    auto selected=selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
    auto added=append(catalog.identity,selected.value);if(!added.ok)return added;
    catalog.actor_social_occupancy_selected=selected.value!=0;return common_success();
}
NativeActorSocialOccupancyWorld::NativeActorSocialOccupancyWorld(WorldBundle& w)
    :world_(w),clock_(w),initial_({w.common,*w.catalog->common,*w.actor_foundation,
        *w.catalog->actor_foundation,w.catalog->family.get(),*w.catalog->actor_social_occupancy_initial,clock_}),
     read_(w.common,w.interaction,initial_,clock_,w.catalog->interaction.get()),write_(w.common,w.interaction,initial_,clock_){}
bool NativeActorSocialOccupancyWorld::bound_to(const CommonStore& c,const InteractionState& i)const noexcept{return write_.bound_to(c,i);}
Outcome<ActorSocialOccupancy> NativeActorSocialOccupancyWorld::capture(const CommonStore& c,const StableId& actor)const{return write_.capture(c,actor);}
Status NativeActorSocialOccupancyWorld::record(const StableId& actor,const ActorSocialOccupancy& before,ActorSocialOccupancyCause cause,const StableId& context,const StableId& initiator,const std::optional<SourceRef>& source){
    auto* writes=world_.active_exact_write_context();
    if(!writes||!world_.common.actor_condition_capture_identity())return common_failure("SocialOccupancyActualWriteCaptureRequired",actor);
    auto recorded=write_.record(actor,before,cause,context,initiator,source);if(!recorded.ok)return recorded;
    return consume_complete_growth_quiet_inputs_in_candidate(world_,*writes);
}
// 同一原 Party 成功采用先记真实条款，再消费当刻安静条件的变化。
Status NativeActorSocialOccupancyWorld::record_party_member(const StableId& actor,const ActorSocialOccupancy& before,ActorSocialOccupancyCause cause,const StableId& party,const StableId& initiator,const PartyMembershipAcceptance& acceptance){
    auto* writes=world_.active_exact_write_context();
    if(!writes||!world_.common.actor_condition_capture_identity())return common_failure("SocialOccupancyActualWriteCaptureRequired",actor,ErrorKind::internal);
    auto recorded=write_.record_party_member(actor,before,cause,party,initiator,acceptance);if(!recorded.ok)return recorded;
    return consume_complete_growth_quiet_inputs_in_candidate(world_,*writes);
}
Status validate_native_actor_social_occupancy(const WorldBundle& w){
    if(!w.catalog)return common_failure("ActorSocialOccupancyCatalogRequired");
    auto layout=validate_actor_social_occupancy_layout(w.common,w.interaction,
        w.catalog->actor_social_occupancy_selected,w.catalog->identity.module_versions,
        w.catalog->actor_social_occupancy_initial.get());if(!layout.ok)return layout;
    if(!w.catalog->actor_social_occupancy_selected)return common_success();
    if(!same_original_catalog_identity(w.catalog->identity,w.catalog->actor_social_occupancy_initial->identity))
        return common_failure("SocialOccupancyInitialCatalogMismatch");
    const auto* owner=w.services.actor_social_occupancy_world();
    if(!owner)return common_failure("ActorSocialOccupancyOriginalOwningSourcesRequired");
    return owner->read().validate_all();
}
}
