#include "resident/kernel/catalog_section_observation.hpp"
#include "native_transport_catalog.hpp"
#include "native_transport_funding.hpp"
#include "native_item_craft_state.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/travel/transport_manifest_history.hpp"
#include "resident/domains/travel/transport_physical_ownership.hpp"
#include "resident/domains/travel/spatial_initial_state.hpp"
#include "resident/domains/local_economy/economic_inventory_placement.hpp"
#include "world_bundle.hpp"
#include "resident/checkpoint/common_codec_selection.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
namespace {
struct Failure {RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]]void fail(const char* code,const StableId& path={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,path,kind).error};}
// 有限作者字段必须实际出现且只含version，不从世界历史推断选择。
Integer author_version(const BoundaryValue& dynamic,const char* key){
    const BoundaryValue* value=nullptr;
    for(const auto& row:boundary::object(dynamic,"scenario/dynamic_world"))if(row.first==key){
        if(value)fail("DuplicateTransportAuthorSelection",key);value=&row.second;
    }
    if(!value)return 0;
    boundary::fields(*value,{"version"},{},key);
    if(boundary::object(*value,key).size()!=1)fail("InvalidTransportAuthorSelection",key);
    const auto out=boundary::get_int(*value,"version",key);
    if(out<=0)fail("UnsupportedTransportAuthorVersion",key,ErrorKind::unsupported_capability);
    return out;
}
// 版本组合仅约束物理来源，不能据此授予军务、承运或空间业务权限。
void versions(const NativeTransportSectionVersions& v,bool funding){
    if(v.travel!=0&&v.travel!=4&&v.travel!=5)fail("UnsupportedTravelPhysicalVersion","travel",ErrorKind::unsupported_capability);
    if(v.group!=0&&v.group!=4&&v.group!=5&&v.group!=6)fail("UnsupportedGroupTransportPhysicalVersion","group_transport",ErrorKind::unsupported_capability);
    if(v.manifest!=0&&v.manifest!=2)fail("UnsupportedTransportManifestPhysicalVersion","transport_manifest_bindings",ErrorKind::unsupported_capability);
    // 暂定：原 Manifest2 也可承载静置箱，仍必须真实 travel 与 Common K 轴。
    if((v.group&&!v.manifest)||((v.group||v.manifest)&&(!v.travel||!v.carrier_location)))fail("TransportSelectedSourcePairRequired");
    if((v.travel==5)!=funding)fail("PersonalFundingTravel5PairRequired");
    if(v.carrier_location&&!v.travel)fail("CarrierLocationExactTravelRequired");
}
NativeTransportSectionVersions author(const BoundaryValue& scenario){
    NativeTransportSectionVersions out;const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return out;
    out.travel=author_version(*dynamic,"travel");out.group=author_version(*dynamic,"group_transport");
    out.manifest=author_version(*dynamic,"transport_manifest_bindings");
    const auto carrier=author_version(*dynamic,"carrier_location");
    if(carrier!=0&&carrier!=1)fail("UnsupportedCarrierLocationVersion","carrier_location",ErrorKind::unsupported_capability);
    out.carrier_location=carrier==1;
    if(out.travel||out.group||out.manifest||out.carrier_location){
        auto profile=take(decode_exact_time_profile(scenario));
        if(!profile||!exact_complete_selected(*profile))fail("TransportCompleteProfileRequired",{},ErrorKind::unsupported_capability);
    }
    return out;
}
// 唯一Common K轴标记同其余两原段一起显式登记；无空根推断或默认升级。
void append(CatalogIdentity& identity,const NativeTransportSectionVersions& v){
    for(const auto& row:identity.module_versions)if(row.first=="travel"||row.first=="group_transport"||row.first=="transport_manifest_bindings"||row.first=="carrier_location")fail("DuplicateTransportModuleSelection",row.first);
    versions(v,take(selected_native_private_transport_funding_version(identity))==1);
    if(v.travel)identity.module_versions.emplace_back("travel",std::to_string(v.travel));
    if(v.group)identity.module_versions.emplace_back("group_transport",std::to_string(v.group));
    if(v.manifest)identity.module_versions.emplace_back("transport_manifest_bindings",std::to_string(v.manifest));
    if(v.carrier_location)identity.module_versions.emplace_back("carrier_location","1");
}
// 新局只读原 S0 初始时间；不接受请求 Tick，也不从冷档补造时刻。
class InitialTransportClock final:public CurrentMomentRead {
    const CommonStore& common_;const WorldTimelineState& timeline_;
public:
    InitialTransportClock(const CommonStore& c,const WorldTimelineState& t):common_(c),timeline_(t){}
    Outcome<WorldMoment> read_current_moment()const override {
        if(common_.tick()!=0||common_.revision()!=0||
           timeline_.boundary_state!=PeriodBoundaryState::ready||
           timeline_.opening_receipt_ref||timeline_.close_receipt_ref)
            return {false,{},common_failure("TransportOriginalFreshClockRequired").error};
        return {true,{common_.tick(),timeline_.sub_half_day},{}};
    }
};
}
Outcome<NativeTransportSectionVersions> selected_native_transport_versions(const CatalogIdentity& identity){try{
    NativeTransportSectionVersions out;bool travel=false,group=false,manifest=false,carrier=false;
    for(const auto& row:identity.module_versions){
        if(row.first=="travel"){
            if(travel||(row.second!="4"&&row.second!="5"))fail("TransportModuleVersionMismatch",row.first);travel=true;out.travel=row.second=="4"?4:5;
        }else if(row.first=="group_transport"){
            if(group||(row.second!="4"&&row.second!="5"&&row.second!="6"))fail("TransportModuleVersionMismatch",row.first);group=true;out.group=row.second=="4"?4:row.second=="5"?5:6;
        }else if(row.first=="transport_manifest_bindings"){
            if(manifest||row.second!="2")fail("TransportModuleVersionMismatch",row.first);manifest=true;out.manifest=2;
        }else if(row.first=="carrier_location"){
            if(carrier||row.second!="1")fail("TransportModuleVersionMismatch",row.first);carrier=true;out.carrier_location=true;
        }
    }
    versions(out,take(selected_native_private_transport_funding_version(identity))==1);return {true,out,{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status append_native_transport_modules(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    append(identity,author(scenario));return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status prepare_native_transport_selection(const BoundaryValue& scenario,PreparedCatalog& catalog,CommonCatalog& common){try{
    if(catalog.common.get()!=&common)fail("TransportSameCatalogCommonRequired");
    append(catalog.identity,author(scenario));
    // 此处只取P06已登记的能力位；实际base的唯一版本仍由World领域表选择。
    const auto selected=take(select_common_codec(5,catalog.identity));
    // Static X/1 does not alter the P06 physical field axes.
    const auto resources=take(selected_native_item_craft_versions(catalog.identity));
    common.selected_resource_operations_enabled=resources.selected_resource_operations==1;
    common.item_instances_enabled=selected.features.item_instances;
    common.lot_properties_enabled=selected.features.lot_properties;
    common.domain_activity_reservations_enabled=selected.features.domain_activity;
    common.carrier_location_enabled=selected.features.carrier_location;
    return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_transport_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 158, "PreparedCatalog", "validate_native_transport_catalog", &catalog);
try{
    const auto selected=take(selected_native_transport_versions(catalog.identity));
    if(!catalog.common)fail("TransportCommonCatalogRequired",{},ErrorKind::unsupported_capability);
    const auto flags=take(select_common_codec(5,catalog.identity)).features;
    const auto resources=take(selected_native_item_craft_versions(catalog.identity));
    if(catalog.common->selected_resource_operations_enabled!=(resources.selected_resource_operations==1))fail("SelectedResourceCommonCatalogMismatch");
    if(catalog.common->item_instances_enabled!=flags.item_instances||catalog.common->lot_properties_enabled!=flags.lot_properties||catalog.common->domain_activity_reservations_enabled!=flags.domain_activity||catalog.common->carrier_location_enabled!=flags.carrier_location)fail("CommonResourceSelectedCatalogMismatch");
    if(selected.travel&&(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.travel||!catalog.actor_foundation))fail("TransportCompleteOriginalCatalogRequired",{},ErrorKind::unsupported_capability);
    return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<TransportPhysicalCodecSelection> native_transport_physical_selection(const PreparedCatalog& catalog){try{
    take(validate_native_transport_catalog(catalog));const auto v=take(selected_native_transport_versions(catalog.identity));
    if(!v.travel)fail("ExactTransportNotSelected",{},ErrorKind::unsupported_capability);
    TransportPhysicalCodecSelection out;out.travel_version=v.travel;out.group_transport_version=v.group==6?7:v.group;out.transport_manifest_bindings_version=v.manifest;
    out.common_resources={catalog.common->item_instances_enabled,catalog.common->lot_properties_enabled,catalog.common->domain_activity_reservations_enabled,catalog.common->carrier_location_enabled};
    out.carrier_location_enabled=v.carrier_location;out.typed_reservations=catalog.dynamic_profile==DynamicProfile::local_society_v1;
    out.domain_activity_reservations=catalog.common->domain_activity_reservations_enabled;return {true,out,{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status initialize_native_transport_roots(const PreparedCatalog& catalog,const CommonStore& original,InitialWorldSeed& seed){try{
    take(validate_native_transport_catalog(catalog));const auto v=take(selected_native_transport_versions(catalog.identity));
    if(seed.movement_exact||seed.spatial_carriers||seed.group_movement_executions||seed.transport_registrations||seed.economic_transport_expenses)fail("TransportInitialRootsAlreadyPresent");
    if(!v.travel)return common_success();
    if(original.tick()!=0||original.revision()!=0||!original.state().history.empty()||!original.state().results.empty()||!original.state().actions.empty())fail("TransportOriginalFreshWorldRequired");
    if(!seed.travel||!seed.actor_foundation||!seed.world_timeline||!catalog.travel||!catalog.actor_foundation)
        fail("TransportOriginalInitialSourcesRequired");
    auto movement=checkpoint_make_shared<MovementExactState>();
    auto carriers=checkpoint_make_shared<SpatialCarrierState>();
    auto groups=checkpoint_make_shared<GroupMovementExecutionState>();
    auto registrations=checkpoint_make_shared<TransportRegistrationState>();
    InitialTransportClock clock(original,*seed.world_timeline);
    MotionExactAuthorSelection selected{v.travel,v.group,v.manifest};
    for(const auto& row:catalog.identity.module_versions){
        if(row.first=="cargo_handling"){if(selected.cargo_handling_schema||row.second!="1")fail("UnsupportedCargoHandlingVersion");selected.cargo_handling_schema=1;}
        else if(row.first=="patrol_checkpoint_sources"){if(selected.patrol_checkpoint_schema||row.second!="2")fail("UnsupportedPatrolCheckpointVersion");selected.patrol_checkpoint_schema=2;}
        else if(row.first=="private_transport_funding"){if(selected.private_transport_funding_schema||row.second!="1")fail("UnsupportedPrivateTransportFundingVersion");selected.private_transport_funding_schema=1;}
    }
    take(initialize_exact_spatial_state_in_candidate(original,*catalog.common,*seed.travel,
        *catalog.travel,*seed.actor_foundation,*catalog.actor_foundation,catalog.family.get(),clock,
        selected,*movement,carriers.get(),groups.get(),registrations.get()));
    if(v.manifest){
        if(!catalog.local_economy||!catalog.estate||!catalog.growth)fail("FixedInventoryOriginalCatalogsRequired");
        EconomicFixedInventoryPlacementContext input{original,*catalog.common,*catalog.local_economy,
            *catalog.estate,*catalog.growth,*catalog.travel};
        NativeEconomicFixedInventoryPlacementRead fixed(input);
        take(install_original_fixed_inventory_bindings_in_candidate(original,*catalog.common,
            *catalog.travel,*carriers,*registrations,fixed));
    }
    if(!v.group)take(validate_stationary_inventory_transport_roots(*movement,*carriers,*groups,*registrations,v.manifest==2));
    seed.movement_exact=std::move(movement);seed.spatial_carriers=std::move(carriers);
    seed.group_movement_executions=std::move(groups);seed.transport_registrations=std::move(registrations);
    seed.economic_transport_expenses=checkpoint_make_shared<const EconomicTransportExpenseState>();return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_native_transport_layout(const WorldBundle& world){try{
    if(!world.catalog)fail("TransportWorldCatalogRequired");take(validate_native_transport_catalog(*world.catalog));
    const auto selected=take(selected_native_transport_versions(world.catalog->identity));
    if(!selected.travel){
        if(world.movement_exact||world.spatial_carriers||world.group_movement_executions||world.transport_registrations||world.economic_transport_expenses)fail("UnexpectedExactTransportRoots");return common_success();
    }
    if(!world.movement_exact||!world.spatial_carriers||!world.group_movement_executions||!world.transport_registrations||!world.economic_transport_expenses)fail("SelectedExactTransportRootsRequired");
    if(!selected.group)take(validate_stationary_inventory_transport_roots(*world.movement_exact,*world.spatial_carriers,*world.group_movement_executions,*world.transport_registrations,selected.manifest==2));
    return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<TransportPhysicalCodecRoots>> native_transport_physical_roots(WorldBundle& world){
    auto layout=validate_native_transport_layout(world);if(!layout.ok)return {false,{},std::move(layout.error)};
    auto selected=selected_native_transport_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(!selected.value.travel)return {true,std::nullopt,{}};
    return {true,TransportPhysicalCodecRoots{world.travel,*world.movement_exact,selected.value.manifest?&*world.spatial_carriers:nullptr,selected.value.manifest?&*world.group_movement_executions:nullptr,selected.value.manifest?&*world.transport_registrations:nullptr,&*world.economic_transport_expenses},{}};
}
Status restore_native_transport_derived_values_in_candidate(WorldBundle& world,ExactArithmeticBudget budget){
    auto layout=validate_native_transport_layout(world);if(!layout.ok)return layout;
    auto selected=selected_native_transport_versions(world.catalog->identity);
    if(!selected.ok)return {false,{},std::move(selected.error)};
    // An actual empty immutable entry collection has no totals to restore.
    // This does not stand in for the selected full source/owner validator.
    if(!selected.value.group||world.group_movement_executions->entry_manifests.empty())return common_success();
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if(!world.has_active_read_phase()){
        auto entered=world.enter_phase_read_context("transport_decoded_derived_values","motion",{now.value,now.value},now.value);
        if(!entered.ok)return {false,{},std::move(entered.error)};
        phase=std::move(entered.value);
    }
    auto owners=world.complete_exact_time_bindings();if(!owners.ok)return {false,{},std::move(owners.error)};
    if(!owners.value)return common_failure("TransportRestoreOriginalOwningLeaseRequired",{},ErrorKind::unsupported_capability);
    const auto& bindings=owners.value->reads();
    if(!bindings.transport_inventory_entries||!bindings.transport_manifest_history)
        return common_failure("TransportRestoreOriginalSourceReadersRequired",{},ErrorKind::unsupported_capability);
    // The actual pure hydrator checks all five original root addresses before
    // adopting any derived value. Its failure discards the whole candidate.
    // Stamp invalidation is after the real read/adoption, never before it.
    struct Invalidate {WorldBundle& world;~Invalidate(){world.note_write_boundary();}} invalidate{world};
    return hydrate_transport_entry_manifest_derived_values(world.common,*world.spatial_carriers,*world.movement_exact,
        *world.group_movement_executions,*world.transport_registrations,*bindings.transport_inventory_entries,
        *bindings.transport_manifest_history,budget);
}
}
