// 真实编组/货物容量及位置统一读口；军队、船和人口账仍保原owner。
#pragma once
#include "passage_model.hpp"
#include "group_journey_funding.hpp"
#include "journey_purpose_model.hpp"
namespace hanmo::resident {
struct TransportPopulationSource { SourceRef source_ref; StableId cohort_id; Integer persons=0,working_crew=0; std::vector<StableId> named_actor_ids; };
struct VesselManifestPlacement { StableId vessel_id; SourceRef placement_source; std::vector<StableId> named_actor_ids,inventory_ids; std::vector<SourceRef> population_sources; Integer persons=0,weight=0,maximum_persons=0,maximum_weight=0; };
struct TransportManifestRead {
    SpatialCarrierRef carrier; Integer manifest_revision=0,composition_revision=0,at_tick=0;
    MovementAnchor actual_anchor; std::vector<SourceRef> source_refs;
    std::vector<StableId> named_actor_refs,vehicle_refs,inventory_refs;
    std::vector<SourceRef> civilian_population_refs,military_force_refs;
    std::vector<VesselManifestPlacement> vessel_placements;
    Integer persons=0,working_crew=0,weight=0; std::shared_ptr<const ReadStamp> read_stamp;
};
struct CarrierPosition { StableId position_id; SpatialCarrierRef carrier_ref; Location location; SourceRef source_ref; Integer revision=0; };
enum class TransportUnitLifecycle { active,handover_pending,dissolved };
struct TransportUnitState {
    StableId transport_unit_id; SubjectRef subject; SourceRef registration_source_ref; Integer composition_revision=0;
    std::vector<SourceRef> population_binding_refs,cohort_allocation_refs;
    std::vector<StableId> named_participants,vehicle_refs,inventory_ids;
    StableId custody_ref,position_ref; std::optional<StableId> active_journey_id;
    TransportUnitLifecycle state=TransportUnitLifecycle::active;
};
struct DirectGroupMovement { StableId group_movement_id; };
struct CarriedGroupMovement { std::vector<SourceRef> vessel_binding_refs; std::optional<StableId> carrier_group_movement_id; };
struct GroupMovementBinding { SpatialCarrierRef carrier; StableId position_ref; std::variant<DirectGroupMovement,CarriedGroupMovement> execution_owner; };
enum class GroupJourneyLifecycle { travelling,paused,blocked,arrived,cancelled };
struct GroupMovementJourney {
    StableId journey_id; SpatialCarrierRef carrier; StableId position_ref; Integer composition_revision=0;
    GroupInitiator initiator; SubjectRef sponsor; StableId payer_wallet_id;
    GroupJourneyFunding expense_authority; std::vector<SourceRef> shipment_refs;
    StableId route_profile; GroupMovementExecution movement_execution;
    WorldMoment started_at,last_step_at;
    GroupJourneyLifecycle state=GroupJourneyLifecycle::travelling; std::optional<SourceRef> stop_reason;
    std::vector<SourceRef> expense_source_refs,movement_source_refs; StableId movement_prefix_ref;
};
enum class GroupStepOutcome { progressed,arrived,blocked,interrupted };
struct GroupJourneyStep { StableId id,journey_id; Integer ordinal=0; MomentSpan span; MovementAnchor from_anchor,to_anchor; CanonicalFraction actual_progress; std::vector<SourceRef> cost_refs,exertion_refs,contact_refs; GroupStepOutcome outcome=GroupStepOutcome::progressed; };
// One terminal binding to the actual arrival/cancel source. It carries no second
// position or delivered-goods result, and is absent while paused or in contact.
enum class GroupJourneyTerminalOutcome { arrived,cancelled };
struct GroupJourneyTerminal { StableId journey_id; SourceRef actual_terminal_source; Integer source_schema=1; WorldMoment occurred_at; GroupJourneyTerminalOutcome outcome=GroupJourneyTerminalOutcome::arrived; };
struct InventorySpatialBinding { StableId inventory_id; std::variant<SpatialCarrierRef,StableId> placement; SourceRef source_ref; Integer revision=0; };
struct SpatialCarrierState {
    OrderedTable<TransportUnitState> transport_units; OrderedTable<CarrierPosition> positions;
    OrderedTable<GroupMovementBinding> bindings; OrderedTable<GroupMovementJourney> journeys;
    OrderedTable<GroupJourneyStep> steps; OrderedTable<GroupJourneyTerminal> terminals; OrderedTable<InventorySpatialBinding> inventory_bindings;
    OrderedTable<BoundaryCrossingReceipt> crossings;
    OrderedTable<JourneyPurposeBinding> journey_purpose_bindings;
};
// 真provider分别给原存在、来源及容量；缺能力和真实空分开，接口无成功默认值。
struct TransportCapacitySource { SourceRef source_ref; Integer organization_maximum_weight=0,physical_maximum_weight=0; };
class SpatialCarrierSources {
public:
    virtual ~SpatialCarrierSources()=default;
    virtual Outcome<TransportPopulationSource> population(const SourceRef&) const=0;
    virtual Outcome<TransportManifestRead> military_manifest(const SpatialCarrierRef&) const=0;
    virtual Outcome<TransportCapacitySource> carrier_capacity(const SpatialCarrierRef&) const=0;
    virtual Outcome<Integer> vehicle_capacity(const StableId&) const=0;
    virtual Outcome<MovementAnchor> original_actor_anchor(const StableId&) const=0;
    virtual Outcome<MovementAnchor> original_site_anchor(const StableId&) const=0;
    virtual Outcome<Integer> selected_lot_weight(const CargoSelection&) const=0;
    virtual Status verify_registration_source(const TransportUnitState&) const=0;
    virtual Status verify_initiator(const GroupInitiator&,const TransportUnitRef&,const SubjectRef&,const SourceRef& accepted_business) const=0;
    virtual Outcome<WorldMoment> current_moment()const=0;
    virtual Outcome<std::shared_ptr<const ReadStamp>> capture_manifest_reads(const TransportManifestRead&)const=0;
};
StableId spatial_carrier_key(const SpatialCarrierRef&);
Outcome<SpatialWriterBinding> read_resolved_spatial_writer(const MovementExactState&,const SpatialCarrierRef&);
Outcome<MovementAnchor> read_carrier_movement_anchor(const MovementExactState&,const SpatialCarrierRef&,ExactArithmeticBudget budget={});
Outcome<bool> same_spatial_anchor(const MovementAnchor&,const MovementAnchor&,ExactArithmeticBudget budget={});
// Position equality only; actual source identities are validated by their owners.
Outcome<bool> same_actual_movement_position(const MovementAnchor&,const MovementAnchor&,ExactArithmeticBudget budget={});
// Only the two existing authored site source kinds are accepted.
Outcome<MovementAnchor> read_original_spatial_site_anchor(const CommonCatalog&,const TravelCatalog&,const SourceRef&);
Outcome<TransportManifestRead> read_transport_manifest(const CommonRead&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,const SpatialCarrierRef&,ExactArithmeticBudget budget={});
enum class CargoCapacityOperation { load,unload,retain };
struct CargoCapacityRequest { SpatialCarrierRef carrier; std::vector<CargoSelection> selection; CargoCapacityOperation operation=CargoCapacityOperation::retain; Integer expected_manifest_revision=0; };
struct CargoCapacityQuote { SpatialCarrierRef carrier; Integer used_weight=0,selected_weight=0,maximum_weight=0,available_weight=0,carrier_revision=0; std::vector<ReadVersion> inventory_read_versions; std::vector<VesselManifestPlacement> placement; };
Outcome<CargoCapacityQuote> quote_cargo_capacity(const CommonRead&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,const CargoCapacityRequest&,ExactArithmeticBudget budget={});
struct SameSiteReachability { MovementAnchor resolved_site; SourceRef required_handling_rule; };
struct JourneyRequiredReachability { MovementAnchor origin_anchor,destination_anchor; std::vector<StableId> required_transfer_sites; };
struct BlockedReachability { StableId visible_reason_code; };
using StorageReachability=std::variant<SameSiteReachability,JourneyRequiredReachability,BlockedReachability>;
Outcome<MovementAnchor> read_inventory_spatial_anchor(const CommonRead&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,const StableId&,ExactArithmeticBudget budget={});
Outcome<StorageReachability> read_storage_reachability(const CommonRead&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,const StableId&,const StableId&,const SourceRef& handling_rule,ExactArithmeticBudget budget={});
Status validate_spatial_carriers(const CommonRead&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources&,ExactArithmeticBudget budget={});
Status visit_spatial_carrier_checkpoint(CodecVisitor&,SpatialCarrierState&);
Status visit_spatial_carrier_checkpoint(CodecVisitor&,SpatialCarrierState&,Integer selected_group_version);
} // 构造manifest只求和真实原source，查询不装货、不拆人口、不获准入。
