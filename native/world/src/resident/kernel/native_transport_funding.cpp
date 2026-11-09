#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "native_transport_funding.hpp"
#include "world_bundle.hpp"
#include "exact_time_profile.hpp"
namespace hanmo::resident {
namespace {
struct Failure {RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]]void required(const char* code){throw Failure{common_failure(code,{},ErrorKind::unsupported_capability).error};}
Outcome<bool> selected(const BoundaryValue& scenario){try{
    using namespace boundary;const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return {true,false,{}};
    const BoundaryValue* value=nullptr;
    for(const auto& row:object(*dynamic,"scenario/dynamic_world"))if(row.first=="private_transport_funding"){
        if(value)fail("DuplicatePrivateTransportFundingSelection",row.first);value=&row.second;
    }
    if(!value)return {true,false,{}};
    fields(*value,{"version"},{},"private_transport_funding");
    if(object(*value,"private_transport_funding").size()!=1||get_int(*value,"version","private_transport_funding")!=1)
        unsupported("UnsupportedPrivateTransportFundingVersion","private_transport_funding");
    auto exact=decode_exact_time_profile(scenario);if(!exact.ok)return {false,false,std::move(exact.error)};
    if(!exact.value||!exact_complete_selected(*exact.value))unsupported("PrivateTransportFundingCompleteProfileRequired","private_transport_funding");
    return {true,true,{}};
}catch(const boundary::DecodeFailure& error){return {false,false,error.error};}}
Status append(CatalogIdentity& identity,bool value){
    for(const auto& row:identity.module_versions)if(row.first=="private_transport_funding")
        return common_failure("DuplicatePrivateTransportFundingModule",{},ErrorKind::internal);
    if(value)identity.module_versions.emplace_back("private_transport_funding","1");
    return common_success();
}
}
Outcome<Integer> selected_native_private_transport_funding_version(const CatalogIdentity& identity){
    Integer out=0;
    for(const auto& row:identity.module_versions)if(row.first=="private_transport_funding"){
        if(out||row.second!="1")return {false,0,common_failure("PrivateTransportFundingModuleMismatch",{},ErrorKind::internal).error};out=1;
    }
    return {true,out,{}};
}
Status append_native_private_transport_funding_module(const BoundaryValue& scenario,CatalogIdentity& identity){
    auto value=selected(scenario);if(!value.ok)return {false,{},std::move(value.error)};return append(identity,value.value);
}
Status prepare_native_private_transport_funding_selection(const BoundaryValue& scenario,PreparedCatalog& catalog){
    if(catalog.private_transport_funding_selected)return common_failure("PrivateTransportFundingAlreadyPrepared",{},ErrorKind::internal);
    auto value=selected(scenario);if(!value.ok)return {false,{},std::move(value.error)};
    auto added=append(catalog.identity,value.value);if(!added.ok)return added;
    catalog.private_transport_funding_selected=value.value;return common_success();
}
Status validate_native_private_transport_funding_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 159, "PreparedCatalog", "validate_native_private_transport_funding_catalog", &catalog);

    auto version=selected_native_private_transport_funding_version(catalog.identity);if(!version.ok)return {false,{},std::move(version.error)};
    if(bool(version.value)!=catalog.private_transport_funding_selected)return common_failure("PrivateTransportFundingCatalogMismatch",{},ErrorKind::internal);
    if(!version.value)return common_success();
    if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.common||!catalog.tasks||!catalog.family||!catalog.estate||!catalog.actor_foundation)
        return common_failure("PrivateTransportFundingOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    return common_success();
}
Status validate_native_private_transport_funding_layout(const WorldBundle& world){
    if(!world.catalog)return common_failure("PrivateTransportFundingWorldCatalogRequired",{},ErrorKind::internal);
    auto catalog=validate_native_private_transport_funding_catalog(*world.catalog);if(!catalog.ok)return catalog;
    if(world.catalog->private_transport_funding_selected!=bool(world.private_transport_funding))return common_failure("PrivateTransportFundingSelectedStateMismatch",{},ErrorKind::internal);
    if(world.private_transport_funding&&world.private_transport_funding->schema_version!=1)return common_failure("PrivateTransportFundingStateVersionMismatch",{},ErrorKind::internal);
    return common_success();
}
NativeWorldPrivateTransportFundingRead::NativeWorldPrivateTransportFundingRead(const WorldBundle& w)
    :world_(w),clock_(w),estate_history_{w.common,*w.catalog->estate,w.family,*w.actor_foundation,*w.catalog->actor_foundation,clock_,w.estate_employment_enabled()?&w.estate_employment:nullptr},
     estate_titles_(estate_history_,w.estate),
     wallet_context_{w.common,*w.catalog->common,w.family,w.military,*w.catalog->estate,estate_titles_,&*w.actor_foundation,w.catalog->actor_foundation.get(),w.catalog->family.get(),&clock_},
     wallet_titles_(wallet_context_),funding_{w.common,*w.private_transport_funding,w.tasks,wallet_titles_,clock_}{}
Outcome<std::unique_ptr<NativeWorldPrivateTransportFundingRead>> NativeWorldPrivateTransportFundingRead::create(const WorldBundle& world){try{
    take(validate_native_private_transport_funding_layout(world));
    if(!world.private_transport_funding||!world.actor_foundation)required("PrivateTransportFundingActualOwningRootsRequired");
    take(world.actual_phase_moment());
    return {true,std::unique_ptr<NativeWorldPrivateTransportFundingRead>(new NativeWorldPrivateTransportFundingRead(world)),{}};
}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status NativeWorldPrivateTransportFundingRead::validate()const{return validate_self_transport_fundings(funding_);}
NativeWorldPersonalTransportFundingRead::NativeWorldPersonalTransportFundingRead(const WorldBundle& w,std::unique_ptr<NativeWorldPrivateTransportFundingRead> funding)
    :world_(w),funding_(std::move(funding)),movement_(w.common,w.travel,*w.movement_exact,*w.catalog->travel,w.services),
     party_(w.common,*w.movement_exact,w.interaction,*w.catalog->interaction,w.services.actor_social_occupancy_world()->read()),
     context_{funding_->funding(),*w.catalog->common,w.travel,*w.movement_exact,*w.catalog->travel,w.services,movement_,&party_,{}},original_(context_){}
Outcome<std::unique_ptr<NativeWorldPersonalTransportFundingRead>> NativeWorldPersonalTransportFundingRead::create(const WorldBundle& world){try{
    auto funding=take(NativeWorldPrivateTransportFundingRead::create(world));
    if(!world.catalog->travel||!world.catalog->interaction||!world.movement_exact||!world.services.actor_social_occupancy_world())required("PersonalFundingActualMovementAndPartySourcesRequired");
    auto occupancy=take(selected_native_actor_social_occupancy_version(world.catalog->identity));
    if(occupancy!=2)required("PersonalFundingOriginalPartyAcceptanceHistoryRequired");
    Integer travel=0;
    for(const auto& row:world.catalog->identity.module_versions)if(row.first=="travel"){
        if(travel||row.second!="5")required("PersonalFundingSelectedTravel5Required");travel=5;
    }
    if(travel!=5)required("PersonalFundingSelectedTravel5Required");
    return {true,std::unique_ptr<NativeWorldPersonalTransportFundingRead>(new NativeWorldPersonalTransportFundingRead(world,std::move(funding))),{}};
}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status NativeWorldPersonalTransportFundingRead::validate()const{return validate_task_personal_movement_funding(context_);}
Status validate_native_private_transport_funding_world(const WorldBundle& world){try{
    take(validate_native_private_transport_funding_layout(world));
    if(!world.private_transport_funding)return common_success();
    auto funding=take(NativeWorldPrivateTransportFundingRead::create(world));take(funding->validate());
    // 选中S1资金后继时增加稳定态反向门：缺同候选真实派发的开段不能保存。
    bool funded_travel=false;for(const auto& row:world.catalog->identity.module_versions)if(row.first=="travel"&&row.second=="5")funded_travel=true;
    if(funded_travel){auto movement=take(NativeWorldPersonalTransportFundingRead::create(world));take(movement->validate());}
    return common_success();
}catch(Failure& error){return {false,{},std::move(error.error)};}}
}
