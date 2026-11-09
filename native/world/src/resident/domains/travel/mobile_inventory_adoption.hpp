#pragma once
#include "mobile_inventory_model.hpp"
#include "group_transport_registration.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct TravelCatalog;class ProjectionWriter;class ScopedCheckpointJsonDecode;
struct MobileInventoryAdoptionContext {
    const CommonStore& common;const CommonCatalog& common_catalog;
    const TravelCatalog& travel_catalog;const SpatialCarrierState& carriers;
    const MovementExactState& movement;const TransportRegistrationState& registrations;
    const SpatialCarrierSources& spatial_sources;
    // 暂定：未选择编组时允许借空指针；装配者仍须核原选择与反向覆盖。
    // 真实登记或交接历史一旦需要原业务，读取必须显式拒绝缺源，不能冒成功。
    const TransportRegistrationBusinessRead* original_business = nullptr;
    const CurrentMomentRead& clock;ExactArithmeticBudget budget={};

    // 新装配借用原业务指针，不持有另一份业务状态；生命周期由同 World owner 保证。
    MobileInventoryAdoptionContext(
        const CommonStore& common_value, const CommonCatalog& common_catalog_value,
        const TravelCatalog& travel_catalog_value, const SpatialCarrierState& carriers_value,
        const MovementExactState& movement_value,
        const TransportRegistrationState& registrations_value,
        const SpatialCarrierSources& spatial_sources_value,
        const TransportRegistrationBusinessRead* original_business_value,
        const CurrentMomentRead& clock_value, ExactArithmeticBudget budget_value = {})
        : common(common_value), common_catalog(common_catalog_value),
          travel_catalog(travel_catalog_value), carriers(carriers_value),
          movement(movement_value), registrations(registrations_value),
          spatial_sources(spatial_sources_value), original_business(original_business_value),
          clock(clock_value), budget(budget_value) {}

    // 保留原引用式调用顺序，委托同一构造路径，不要求旧调用者改传替身。
    MobileInventoryAdoptionContext(
        const CommonStore& common_value, const CommonCatalog& common_catalog_value,
        const TravelCatalog& travel_catalog_value, const SpatialCarrierState& carriers_value,
        const MovementExactState& movement_value,
        const TransportRegistrationState& registrations_value,
        const SpatialCarrierSources& spatial_sources_value,
        const TransportRegistrationBusinessRead& original_business_value,
        const CurrentMomentRead& clock_value, ExactArithmeticBudget budget_value = {})
        : MobileInventoryAdoptionContext(common_value, common_catalog_value,
              travel_catalog_value, carriers_value, movement_value, registrations_value,
              spatial_sources_value, &original_business_value, clock_value, budget_value) {}
};
// This finite implementation supports explicitly authored portable containers.
// A vehicle branch requires its real vehicle takeover/physical provider and is
// rejected here, rather than treating an arbitrary inventory as a vehicle.
Outcome<MobileInventoryAdoptionView> capture_mobile_inventory_adoption(
    const MobileInventoryAdoptionContext&,const StableId& inventory_id,
    const SourceRef& actual_plan_source,const WorldMoment& actual_plan_at);
// The S5 owner first validates its original plan payload/M. This reader uses the
// plan's actual Common append cut, including later same-M registrations/releases.
Outcome<MobileInventoryAdoptionView> read_mobile_inventory_adoption_before_source(
    const MobileInventoryAdoptionContext&,const StableId& inventory_id,
    const SourceRef& actual_plan_source,const WorldMoment& actual_plan_at);
Status validate_mobile_inventory_adoption(const MobileInventoryAdoptionContext&,
    const MobileInventoryAdoptionView&,const SourceRef& actual_plan_source,
    const WorldMoment& actual_plan_at);
Outcome<MovementAnchor> read_mobile_inventory_adoption_anchor(
    const MobileInventoryAdoptionContext&,const MobileInventoryAdoptionView&,
    const SourceRef& actual_plan_source,const WorldMoment& actual_plan_at);
Outcome<Integer> read_mobile_container_maximum_weight(
    const CommonCatalog&,const TravelCatalog&,const MomentOwnerSource& actual_container_source);
struct MobileInventoryBindingHistory {
 StableId inventory_id;MovementAnchor stationary_or_departure_anchor;
 MomentOwnerSource actual_binding_source;std::optional<TransportUnitRef> carrying_unit;
};
// Same retained registration/release walk as plan adoption. This general
// observation may be a real service or handover; caller validates its M.
Outcome<MobileInventoryBindingHistory> read_mobile_inventory_binding_before_source(
 const MobileInventoryAdoptionContext&,const StableId& inventory_id,
 const SourceRef& actual_observation,const WorldMoment& actual_at);
// Called only by real selected initial construction, never by decode or plan.
// Existing binding/state/history rejects instead of repairing a loaded world.
Status initialize_mobile_inventory_bindings_in_candidate(const CommonStore&,
    const CommonCatalog&,const TravelCatalog&,SpatialCarrierState&,
    const TransportRegistrationState&,const CurrentMomentRead&);
Status validate_mobile_inventory_definitions(const CommonCatalog&,const TravelCatalog&);
Status validate_mobile_inventory_bindings(const MobileInventoryAdoptionContext&);
Outcome<OrderedTable<MobileInventoryDefinition>> decode_mobile_inventory_definitions(const BoundaryValue&);
Status visit_mobile_inventory_adoption(CodecVisitor&,MobileInventoryAdoptionView&);
Status visit_mobile_inventory_adoption(CodecVisitor&,std::string_view,MobileInventoryAdoptionView&);
Outcome<MobileInventoryAdoptionView> decode_mobile_inventory_adoption(const BoundaryValue&);
Outcome<MobileInventoryAdoptionView> decode_mobile_inventory_adoption(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Outcome<BoundaryValue> encode_mobile_inventory_adoption(const MobileInventoryAdoptionView&);
Status emit_mobile_inventory_adoption(ProjectionWriter&,std::string_view,const MobileInventoryAdoptionView&);
Status emit_mobile_inventory_definition(ProjectionWriter&,std::string_view,const MobileInventoryDefinition&);
}
