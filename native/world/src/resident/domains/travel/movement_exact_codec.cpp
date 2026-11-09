// S1成对typed冷布局；variant严格判别，运行票据/read_versions不入档。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "spatial_carrier.hpp"
#include "passage_boundary_codec_fields.hpp"
#include "spatial_carrier_ref_codec_fields.hpp"
#include "movement_execution_ref_codec_fields.hpp"
#include "passage_requester_codec_fields.hpp"
#include "group_journey_funding_codec.hpp"
#include "../military/military_march_codec.hpp"
#include "group_transport_registration.hpp"
#include "group_movement.hpp"
#include "cargo_handling.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "../military/military_exact_sources.hpp"
#include "../military/military_exact_source_codec.hpp"
#include "../military/military_supply_exact_codec.hpp"
#include "../military/military_local_body_codec_fields.hpp"
#include "../military/military_local_motion_codec_fields.hpp"
#include "../military/military_spatial_chart.hpp"
#include "../military/military_spatial_handoff.hpp"
#include "movement_exact_detail.hpp"
#include "../../kernel/world_timeline_codec.hpp"
#include <set>
#include <type_traits>
namespace hanmo::resident { namespace movement_codec {
Status value(CodecVisitor&, SourceRef&);
Status value(CodecVisitor&, PointI&);
Status value(CodecVisitor&, SettlementLocation&);
Status value(CodecVisitor&, WildernessLocation&);
Status value(CodecVisitor&, FollowingLocation&);
Status value(CodecVisitor&, InArmyLocation&);
Status value(CodecVisitor&, CarriedLocation&);
Status value(CodecVisitor&, OnRouteLocation&);
Status value(CodecVisitor&, SubjectRef&);
Status value(CodecVisitor&, ActionCoreView&);
Status value(CodecVisitor&, SpatialCarrierRef&);
Status value(CodecVisitor&, MovementExecutionRef&);
Status value(CodecVisitor&, PassageRequester&);
Status value(CodecVisitor&, TransportUnitRef&);
Status value(CodecVisitor&, ActorCarrierRef&);
Status value(CodecVisitor&, PartyCarrierRef&);
Status value(CodecVisitor&, ArmyCarrierRef&);
Status value(CodecVisitor&, FleetCarrierRef&);
Status value(CodecVisitor&, TransportUnitCarrierRef&);
Status value(CodecVisitor&, PublicBudgetExpenseAuthority&);
Status value(CodecVisitor&, PrivateMandateExpenseAuthority&);
Status value(CodecVisitor&, SelfFundedExpenseAuthority&);
Status value(CodecVisitor&, HouseholdPurchaseExpenseAuthority&);
Status value(CodecVisitor&, ActorGroupInitiator&);
Status value(CodecVisitor&, HouseholdPolicyGroupInitiator&);
Status value(CodecVisitor&, TerrainTravelParams&);
Status value(CodecVisitor&, RoadTravelParams&);
Status value(CodecVisitor&, RoadTravelMotion&);
Status value(CodecVisitor&, RoadContactEndpoint&);
Status value(CodecVisitor&, RoadContactEndpoints&);
Status value(CodecVisitor&, ContactPoint&);
Status value(CodecVisitor&, ContactSegment&);
Status value(CodecVisitor&, TravelMotion&);
Status value(CodecVisitor&, ContactEndpoint&);
Status value(CodecVisitor&, ContactHit&);
Status value(CodecVisitor&, ExactTerrainCost&);
Status value(CodecVisitor&, OriginalMovementAnchor&);
Status value(CodecVisitor&, RoadFractionalAnchor&);
Status value(CodecVisitor&, TerrainContinuousAnchor&);
Status value(CodecVisitor&, PersonalMovementExecution&);
Status value(CodecVisitor&, GroupMovementExecution&);
Status value(CodecVisitor&, CustodyExitMovementExecution&);
Status value(CodecVisitor&, ShoreAssaultMovementExecution&);
Status value(CodecVisitor&, TerrainCostPiece&);
Status value(CodecVisitor&, TerrainCostBasis&);
Status value(CodecVisitor&, RoadMovementBasis&);
Status value(CodecVisitor&, RunningMovementMap&);
Status value(CodecVisitor&, HeldMovementMap&);
Status value(CodecVisitor&, MovementActive&);
Status value(CodecVisitor&, MovementHeld&);
Status value(CodecVisitor&, MovementClosed&);
Status value(CodecVisitor&, MovementPrefixState&);
Status value(CodecVisitor&, NoTerrainBudget&);
Status value(CodecVisitor&, TerrainUnitBudget&);
Status value(CodecVisitor&, MovementPaymentNotConsumed&);
Status value(CodecVisitor&, MovementPaymentConsumed&);
Status value(CodecVisitor&, MovementUnitOpen&);
Status value(CodecVisitor&, MovementUnitFinalized&);
Status value(CodecVisitor&, MovementActivitySegment&);
Status value(CodecVisitor&, MovementUnitState&);
Status value(CodecVisitor&, TerrainCreditDispositionSource&);
Status value(CodecVisitor&, ExactContactMotionWitness&);
Status value(CodecVisitor&, ExactMovementContact&);
Status value(CodecVisitor&, WorldMovementWriter&);
Status value(CodecVisitor&, BattleMovementWriter&);
Status value(CodecVisitor&, StableMovementWriter&);
Status value(CodecVisitor&, CarriedMovementWriter&);
Status value(CodecVisitor&, SpatialWriterBinding&);
Status value(CodecVisitor&, MovementArrivalSource&);
Status value(CodecVisitor&, MovementExpenseSource&);
Status value(CodecVisitor&, TravelExertionSource&);
Status value(CodecVisitor&, TravelExertionBasis&);
Status value(CodecVisitor&, PersonalMovementControlSource&);
Status value(CodecVisitor&, MovementContactContinuationSource&);
Status value(CodecVisitor&, MovementUnitBoundarySource&);
Status value(CodecVisitor&, MovementContactInstallationSource&);
Status value(CodecVisitor&, PersonalMovementTerminalSource&);
Status value(CodecVisitor&, MovementBasisTransitionSource&);
Status value(CodecVisitor&, MovementUnitCloseSource&);
Status value(CodecVisitor&, PatrolFinalMotionSource&);
Status value(CodecVisitor&, PatrolCheckpointArrival2&);
Status value(CodecVisitor&, ActorPassageRequester&);
Status value(CodecVisitor&, HouseholdPolicyPassageRequester&);
Status value(CodecVisitor&, PassageBoundaryRef&);
Status value(CodecVisitor&, BoundaryCrossingReceipt&);
Status value(CodecVisitor&, CarrierPosition&);
Status value(CodecVisitor&, TransportUnitState&);
Status value(CodecVisitor&, DirectGroupMovement&);
Status value(CodecVisitor&, CarriedGroupMovement&);
Status value(CodecVisitor&, GroupMovementBinding&);
Status value(CodecVisitor&, GroupMovementJourney&);
Status value(CodecVisitor&, GroupJourneyStep&);
Status value(CodecVisitor&, GroupJourneyTerminal&);
Status value(CodecVisitor&, InventorySpatialBinding&);
Status value(CodecVisitor&, TransportPopulationSource&);
Status value(CodecVisitor&, VesselManifestPlacement&);
Status value(CodecVisitor&, TransportManifestRead&);
Status value(CodecVisitor&, TransportInventoryManifest&);
Status value(CodecVisitor&, TransportPopulationManifest&);
Status value(CodecVisitor&, TransportEntryManifestSource&);
Status value(CodecVisitor&, TravelProfile&);
Status value(CodecVisitor&, GroupMotionProfile&);
Status value(CodecVisitor&, GroupMovementOpeningSource&);
Status value(CodecVisitor&, GroupMovementUnitTerms&);
Status value(CodecVisitor&, GroupExpensePrefixCause&);
Status value(CodecVisitor&, GroupExpenseContactCause&);
Status value(CodecVisitor&, GroupTransportExpenseSource&);
Status value(CodecVisitor&, GroupMovementControlSource&);
Status value(CodecVisitor&, TransportSelectionRegistration&);
Status value(CodecVisitor&, TransportRegistrationAuthority&);
Status value(CodecVisitor&, TransportRegistrationSource&);
Status value(CodecVisitor&, TransportUnitReleaseSource&);
Status value(CodecVisitor&, MilitaryActorChange&);
Status value(CodecVisitor&, MilitaryBattleResult&);
Status value(CodecVisitor&, MilitaryRosterReceipt&);
Status value(CodecVisitor&, MilitaryMemberEffectLink&);
Status value(CodecVisitor&, DynamicMilitaryBattleFact2&);
Status value(CodecVisitor&, DynamicMilitaryCasualtyFact2&);
Status value(CodecVisitor&, MilitarySupplyEffectSource2&);
Status value(CodecVisitor&, SignedSpatialRational&);
Status value(CodecVisitor&, MilitarySpatialPoint&);
Status value(CodecVisitor&, MilitarySpatialTranslation&);
Status value(CodecVisitor&, MilitarySpatialMatrix&);
Status value(CodecVisitor&, MilitarySpatialSegment&);
Status value(CodecVisitor&, MilitarySpatialPortal&);
Status value(CodecVisitor&, MilitarySpatialChart2&);
Status value(CodecVisitor&, MilitaryEntryConnectorDefinition&);
Status value(CodecVisitor&, HexPoint&);
Status value(CodecVisitor&, MilitaryOrientationHalfspace&);
Status value(CodecVisitor&, MilitaryEncounterOrientation&);
Status value(CodecVisitor&, LockedArenaChartDefinition&);
Status value(CodecVisitor&, EncounterChartDefinition&);
Status value(CodecVisitor&, MilitarySpatialGeometryDefinition2&);
Status value(CodecVisitor&, MilitaryLocalUnitRef&);
Status value(CodecVisitor&, MilitaryLocalVesselRef&);
Status value(CodecVisitor&, LocalMotionSegmentRef&);
Status value(CodecVisitor&, LocalHexNeighbor&);
Status value(CodecVisitor&, ReverseLocalMotionAtPrefix&);
Status value(CodecVisitor&, RepriceLocalMotionSuffix&);
Status value(CodecVisitor&, LocalPartialEdgeContinuation&);
Status value(CodecVisitor&, LocalEntryConnector&);
Status value(CodecVisitor&, LocalExitConnector&);
Status value(CodecVisitor&, WorkUnits&);
Status value(CodecVisitor&, LocalMotionSegment&);
Status value(CodecVisitor&, LocalHexMotionBasis2&);
Status value(CodecVisitor&, LocalMotionPrefixClose2&);
Status value(CodecVisitor&, LocalMotionCursor2&);
Status value(CodecVisitor&, MilitaryRouteCorrespondence2&);
Status value(CodecVisitor&, MilitarySpatialHandoff2&);
Status value(CodecVisitor&, CargoSelection&);
Status value(CodecVisitor&, TimeScaleRef&);
Status value(CodecVisitor&, ActionElapsedEntryRef&);
Status value(CodecVisitor&, CargoHandlingRule&);
Status value(CodecVisitor&, NamedCargoHandlingWorkers&);
Status value(CodecVisitor&, CohortCargoHandlingWorkers&);
Status value(CodecVisitor&, CargoHandlingTerms&);
Status value(CodecVisitor&, NamedCargoHandlingTiming&);
Status value(CodecVisitor&, CohortCargoHandlingTiming&);
Status value(CodecVisitor&, CargoHandlingExecution&);
Status value(CodecVisitor&, CargoHandlingWorkResult&);
Status value(CodecVisitor&, CargoHandlingResult&);
Status value(CodecVisitor&,MilitaryGroupOrderAcceptance&);
Status value(CodecVisitor&,GroupJourneyFunding&);
Status value(CodecVisitor&,InventoryEntryEvidence&);
Status value(CodecVisitor&,Owner&);
Status value(CodecVisitor&,ReservationRef&);
Status value(CodecVisitor&, CanonicalFraction&);
Status value(CodecVisitor&, WorldMoment&);
Status value(CodecVisitor&, HalfDayDuration&);
Status value(CodecVisitor&, MomentSpan&);
Status value(CodecVisitor&, Location&);
Status value(CodecVisitor&, GroupJourneyTerminalOutcome&);
Status value(CodecVisitor&, TerrainOriginalClosingPolicy&);
Status value(CodecVisitor&, CarriedLocation::CarrierKind&);
Status value(CodecVisitor&, ExactContactGeometryProfile&);
Status value(CodecVisitor&, CargoHandlingOperation&);
Status value(CodecVisitor&, CargoHandlingLaborMode&);
Status value(CodecVisitor&, CargoHandlingPhase&);
Status value(CodecVisitor&, CargoCohortServiceState&);
Status value(CodecVisitor&, CargoHandlingResultStatus&);
Status value(CodecVisitor&, LocalMotionPhase&);
Status value(CodecVisitor&, PersonalMovementControlOperation&);
Status value(CodecVisitor&, PersonalMovementTerminalKind&);
Status value(CodecVisitor&, SubjectKind&);
Status value(CodecVisitor&, TerrainCostPieceKind&);
Status value(CodecVisitor&, MovementCloseKind&);
Status value(CodecVisitor&, PassageBoundaryKind&);
Status value(CodecVisitor&, TransportUnitLifecycle&);
Status value(CodecVisitor&, GroupJourneyLifecycle&);
Status value(CodecVisitor&, GroupStepOutcome&);
Status value(CodecVisitor&, GroupControlOperation&);
inline Status value(CodecVisitor& c,StableId& v){return c.field("value",v);}
inline Status value(CodecVisitor& c,Integer& v){return c.field("value",v);}
inline Status value(CodecVisitor& c,bool& v){return c.field("value",v);}
inline Status value(CodecVisitor& c,double& v){return c.field("value",v);}
template<class A,class B> Status value(CodecVisitor&,std::pair<A,B>&);
template<class T> Status value(CodecVisitor&,std::vector<T>&);
template<class T> Status value(CodecVisitor&,std::optional<T>&);
template<class... T> Status value(CodecVisitor&,std::variant<T...>&);
template<class T> Status value(CodecVisitor& c,std::vector<T>& v){return c.sequence("items",v,[](CodecVisitor& x,T& row){return value(x,row);});}
template<class T> Status value(CodecVisitor& c,std::optional<T>& v){return c.optional("optional",v,[](CodecVisitor& x,T& row){return value(x,row);});}
template<std::size_t I=0,class V> bool construct_variant(V& v,Integer index){if constexpr(I<std::variant_size_v<V>){if(index==Integer(I)){v.template emplace<I>();return true;}return construct_variant<I+1>(v,index);}return false;}
template<class... T> Status value(CodecVisitor& c,std::variant<T...>& v){Integer tag=Integer(v.index());c.field("variant",tag);if(c.reading()&&c.status().ok&&!construct_variant(v,tag))return c.reject("InvalidMovementVariant");if(c.status().ok)return std::visit([&](auto& item){return value(c,item);},v);return c.status();}
template<class A,class B> Status value(CodecVisitor& c,std::pair<A,B>& v){c.object("first",[&](CodecVisitor& x){return value(x,v.first);});return c.object("second",[&](CodecVisitor& x){return value(x,v.second);});}
template<class T> Status field(CodecVisitor& c,const char* name,T& v){return c.object(name,[&](CodecVisitor& x){return value(x,v);});}
// 有序表保原键与记录字节；留存只走拥有图，解码重复键仍明确拒绝。
template <class T>
Status table(CodecVisitor &c, const char *name, OrderedTable<T> &v) {

  using Row = typename OrderedTable<T>::Row;
  auto emit = [](CodecVisitor &x, Row &row) {
    x.field("key", row.first);
    return field(x, "record", row.second);
  };
  if (!c.reading())
    return c.sequence(name, v.rows(), emit);
  std::vector<Row> rows;
  c.sequence(name, rows, emit);
  if (c.status().ok && !v.adopt_checkpoint_rows(std::move(rows)))
    return c.reject("DuplicateMovementRecord", name);
  return c.status();
}
Status value(CodecVisitor& c,MilitaryGroupOrderAcceptance& v){return visit_military_group_order_acceptance(c,v);}
Status value(CodecVisitor& c,GroupJourneyFunding& v){
    if(c.reading())v.emplace<TransportExpenseAuthority>();
    auto* original=std::get_if<TransportExpenseAuthority>(&v);
    if(!original)return c.reject("MilitaryMarchFundingRequiresGroupSuccessorLayout");
    return value(c,*original);
}
Status value(CodecVisitor& c,Location& v){return common_checkpoint::location(c,v);}
Status value(CodecVisitor& c,Owner& v){return common_checkpoint::owner(c,v);}
Status value(CodecVisitor& c,ReservationRef& v){return common_checkpoint::reservation(c,v,"reservation_id");}
Status value(CodecVisitor& c,InventoryEntryEvidence& v){return visit_inventory_entry_evidence(c,v);}
Status value(CodecVisitor& c,CanonicalFraction& v){EncodedFraction wire;if(!c.reading()){auto r=encode_canonical_fraction(v);if(!r.ok)return r.error.kind==ErrorKind::internal?Status{false,{},r.error}:c.reject(r.error.original.code);wire=std::move(r.value);}c.field("numerator",wire.numerator);c.field("denominator",wire.denominator);if(c.reading()&&c.status().ok){auto r=decode_canonical_fraction(wire);if(!r.ok)return {false,{},r.error};v=std::move(r.value);}return c.status();}
Status value(CodecVisitor& c,WorldMoment& v){return visit_world_moment2(c,v);}
Status value(CodecVisitor& c,HalfDayDuration& v){return field(c,"value",v.value);}
Status value(CodecVisitor& c,MomentSpan& v){field(c,"from",v.from);return field(c,"to",v.to);}
Status value(CodecVisitor& c,GroupJourneyTerminalOutcome& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>1)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=GroupJourneyTerminalOutcome(tag);return c.status();}
Status value(CodecVisitor& c,TerrainOriginalClosingPolicy& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>1)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=TerrainOriginalClosingPolicy(tag);return c.status();}
Status value(CodecVisitor& c,CarriedLocation::CarrierKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CarriedLocation::CarrierKind(tag);return c.status();}
Status value(CodecVisitor& c,ExactContactGeometryProfile& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=ExactContactGeometryProfile(tag);return c.status();}
Status value(CodecVisitor& c,CargoHandlingOperation& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>1)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CargoHandlingOperation(tag);return c.status();}
Status value(CodecVisitor& c,CargoHandlingLaborMode& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>1)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CargoHandlingLaborMode(tag);return c.status();}
Status value(CodecVisitor& c,CargoHandlingPhase& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>5)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CargoHandlingPhase(tag);return c.status();}
Status value(CodecVisitor& c,CargoCohortServiceState& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CargoCohortServiceState(tag);return c.status();}
Status value(CodecVisitor& c,CargoHandlingResultStatus& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=CargoHandlingResultStatus(tag);return c.status();}
Status value(CodecVisitor& c,LocalMotionPhase& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,PersonalMovementControlOperation& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=PersonalMovementControlOperation(tag);return c.status();}
Status value(CodecVisitor& c,PersonalMovementTerminalKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>1)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=PersonalMovementTerminalKind(tag);return c.status();}
Status value(CodecVisitor& c,SubjectKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=SubjectKind(tag);return c.status();}
Status value(CodecVisitor& c,TerrainCostPieceKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=TerrainCostPieceKind(tag);return c.status();}
Status value(CodecVisitor& c,MovementCloseKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>3)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=MovementCloseKind(tag);return c.status();}
Status value(CodecVisitor& c,PassageBoundaryKind& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>3)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=PassageBoundaryKind(tag);return c.status();}
Status value(CodecVisitor& c,TransportUnitLifecycle& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=TransportUnitLifecycle(tag);return c.status();}
Status value(CodecVisitor& c,GroupJourneyLifecycle& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>4)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=GroupJourneyLifecycle(tag);return c.status();}
Status value(CodecVisitor& c,GroupStepOutcome& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>3)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=GroupStepOutcome(tag);return c.status();}
Status value(CodecVisitor& c,GroupControlOperation& v){Integer tag=Integer(v);c.field("enum",tag);if(tag<0||tag>2)return c.reject("InvalidMovementEnum");if(c.reading()&&c.status().ok)v=GroupControlOperation(tag);return c.status();}
Status value(CodecVisitor& c,SourceRef& v){field(c,"domain",v.domain);field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
Status value(CodecVisitor& c,PointI& v){field(c,"x",v.x);field(c,"y",v.y);return c.status();}
Status value(CodecVisitor& c,SettlementLocation& v){field(c,"settlement_id",v.settlement_id);field(c,"place_id",v.place_id);return c.status();}
Status value(CodecVisitor& c,WildernessLocation& v){field(c,"map_id",v.map_id);field(c,"origin",v.origin);field(c,"point",v.point);return c.status();}
Status value(CodecVisitor& c,FollowingLocation& v){field(c,"party_id",v.party_id);return c.status();}
Status value(CodecVisitor& c,InArmyLocation& v){field(c,"army_id",v.army_id);return c.status();}
Status value(CodecVisitor& c,CarriedLocation& v){field(c,"carrier_kind",v.carrier_kind);field(c,"carrier_id",v.carrier_id);return c.status();}
Status value(CodecVisitor& c,OnRouteLocation& v){field(c,"edge_id",v.edge_id);field(c,"origin",v.origin);field(c,"progress",v.progress);return c.status();}
Status value(CodecVisitor& c,SubjectRef& v){return spatial_carrier_ref_codec_fields::subject_fields(c,v);}
Status value(CodecVisitor& c,ActionCoreView& v){field(c,"id",v.id);field(c,"kind",v.kind);field(c,"actor_id",v.actor_id);field(c,"participants",v.participants);field(c,"start_tick",v.start_tick);field(c,"remaining_ticks",v.remaining_ticks);return c.status();}
Status value(CodecVisitor& c,SpatialCarrierRef& v){return spatial_carrier_ref_codec_fields::fields(c,v);}
Status value(CodecVisitor& c,MovementExecutionRef& v){return movement_execution_ref_codec_fields::fields(c,v);}
Status value(CodecVisitor& c,PassageRequester& v){return passage_requester_codec_fields::fields(c,v);}
Status value(CodecVisitor& c,TransportUnitRef& v){return spatial_carrier_ref_codec_fields::transport_unit_fields(c,v);}
Status value(CodecVisitor& c,ActorCarrierRef& v){return spatial_carrier_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,PartyCarrierRef& v){return spatial_carrier_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,ArmyCarrierRef& v){return spatial_carrier_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,FleetCarrierRef& v){return spatial_carrier_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,TransportUnitCarrierRef& v){return spatial_carrier_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,PublicBudgetExpenseAuthority& v){field(c,"execution_ref",v.execution_ref);field(c,"payer_wallet_id",v.payer_wallet_id);return c.status();}
Status value(CodecVisitor& c,PrivateMandateExpenseAuthority& v){field(c,"mandate_id",v.mandate_id);field(c,"accepted_terms_revision",v.accepted_terms_revision);field(c,"authority_epoch",v.authority_epoch);field(c,"payer_wallet_id",v.payer_wallet_id);return c.status();}
Status value(CodecVisitor& c,SelfFundedExpenseAuthority& v){field(c,"funding_terms_id",v.funding_terms_id);field(c,"principal_actor_id",v.principal_actor_id);field(c,"payer_wallet_id",v.payer_wallet_id);return c.status();}
Status value(CodecVisitor& c,HouseholdPurchaseExpenseAuthority& v){field(c,"household_id",v.household_id);field(c,"accepted_policy_ref",v.accepted_policy_ref);field(c,"policy_revision",v.policy_revision);field(c,"purchase_plan_ref",v.purchase_plan_ref);field(c,"purchase_ref",v.purchase_ref);field(c,"phase",v.phase);return c.status();}
Status value(CodecVisitor& c,ActorGroupInitiator& v){field(c,"initiator_actor_id",v.initiator_actor_id);field(c,"authority_ref",v.authority_ref);return c.status();}
Status value(CodecVisitor& c,HouseholdPolicyGroupInitiator& v){field(c,"accepted_household_policy_ref",v.accepted_household_policy_ref);field(c,"policy_revision",v.policy_revision);field(c,"purchase_plan_ref",v.purchase_plan_ref);field(c,"purchase_ref",v.purchase_ref);field(c,"phase",v.phase);return c.status();}
Status value(CodecVisitor& c,TerrainTravelParams& v){field(c,"plan_id",v.plan_id);field(c,"map_id",v.map_id);field(c,"origin",v.origin);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"mode",v.mode);field(c,"route_index",v.route_index);field(c,"budget",v.budget);field(c,"travel_credit",v.travel_credit);field(c,"money_per_tick",v.money_per_tick);field(c,"fatigue_per_tick",v.fatigue_per_tick);field(c,"points",v.points);field(c,"destination",v.destination);field(c,"paused",v.paused);field(c,"encounter_id",v.encounter_id);field(c,"has_encounter_id",v.has_encounter_id);field(c,"has_met_actions",v.has_met_actions);field(c,"met_actions",v.met_actions);field(c,"field_order",v.field_order);return c.status();}
Status value(CodecVisitor& c,RoadTravelParams& v){field(c,"destination",v.destination);field(c,"via",v.via);field(c,"path",v.path);field(c,"edge_index",v.edge_index);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"returning",v.returning);field(c,"paused",v.paused);field(c,"reverse_first",v.reverse_first);field(c,"has_terrain",v.has_terrain);field(c,"has_paused",v.has_paused);field(c,"has_reverse_first",v.has_reverse_first);field(c,"has_encounter_id",v.has_encounter_id);field(c,"has_met_actions",v.has_met_actions);field(c,"encounter_id",v.encounter_id);field(c,"met_actions",v.met_actions);field(c,"field_order",v.field_order);return c.status();}
Status value(CodecVisitor& c,RoadTravelMotion& v){field(c,"actor_id",v.actor_id);field(c,"action_id",v.action_id);field(c,"edge_id",v.edge_id);field(c,"origin",v.origin);field(c,"start",v.start);field(c,"end",v.end);field(c,"moving",v.moving);field(c,"params",v.params);return c.status();}
Status value(CodecVisitor& c,RoadContactEndpoint& v){field(c,"actor_id",v.actor_id);field(c,"anchor",v.anchor);field(c,"trace",v.trace);return c.status();}
Status value(CodecVisitor& c,RoadContactEndpoints& v){field(c,"first",v.first);field(c,"second",v.second);return c.status();}
Status value(CodecVisitor& c,ContactPoint& v){field(c,"x",v.x);field(c,"y",v.y);return c.status();}
Status value(CodecVisitor& c,ContactSegment& v){field(c,"from",v.from);field(c,"to",v.to);field(c,"start_cost",v.start_cost);field(c,"end_cost",v.end_cost);field(c,"point_index",v.point_index);return c.status();}
Status value(CodecVisitor& c,TravelMotion& v){field(c,"actor_id",v.actor_id);field(c,"action_id",v.action_id);field(c,"map_id",v.map_id);field(c,"origin",v.origin);field(c,"params",v.params);field(c,"segments",v.segments);field(c,"budget",v.budget);field(c,"end_credit",v.end_credit);field(c,"reached_points",v.reached_points);field(c,"end",v.end);field(c,"moving",v.moving);field(c,"stationary",v.stationary);field(c,"stationary_location",v.stationary_location);field(c,"cost_piece_kinds",v.cost_piece_kinds);field(c,"actual_consumed_total",v.actual_consumed_total);field(c,"original_point_count",v.original_point_count);return c.status();}
Status value(CodecVisitor& c,ContactEndpoint& v){field(c,"actor_id",v.actor_id);field(c,"anchor",v.anchor);field(c,"trace",v.trace);return c.status();}
Status value(CodecVisitor& c,ContactHit& v){field(c,"a",v.a);field(c,"b",v.b);field(c,"time_num",v.time_num);field(c,"time_den",v.time_den);field(c,"first",v.first);field(c,"second",v.second);field(c,"road",v.road);return c.status();}
Status value(CodecVisitor& c,ExactTerrainCost& v){field(c,"value",v.value);return c.status();}
Status value(CodecVisitor& c,OriginalMovementAnchor& v){field(c,"location",v.location);field(c,"actual_source",v.actual_source);return c.status();}
Status value(CodecVisitor& c,RoadFractionalAnchor& v){field(c,"edge_id",v.edge_id);field(c,"origin",v.origin);field(c,"progress",v.progress);field(c,"actual_source",v.actual_source);return c.status();}
Status value(CodecVisitor& c,TerrainContinuousAnchor& v){field(c,"map_id",v.map_id);field(c,"origin",v.origin);field(c,"x_bits",v.x_bits);field(c,"y_bits",v.y_bits);field(c,"actual_prefix_close_source",v.actual_prefix_close_source);field(c,"geometry_profile_ref",v.geometry_profile_ref);return c.status();}
Status value(CodecVisitor& c,PersonalMovementExecution& v){return movement_execution_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,GroupMovementExecution& v){return movement_execution_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,CustodyExitMovementExecution& v){return movement_execution_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,ShoreAssaultMovementExecution& v){return movement_execution_ref_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,TerrainCostPiece& v){field(c,"kind",v.kind);field(c,"segment",v.segment);field(c,"cost_from",v.cost_from);field(c,"cost_to",v.cost_to);return c.status();}
Status value(CodecVisitor& c,TerrainCostBasis& v){field(c,"basis_ref",v.basis_ref);field(c,"original_inputs",v.original_inputs);field(c,"start_anchor",v.start_anchor);field(c,"end_anchor",v.end_anchor);field(c,"parameter_limit",v.parameter_limit);field(c,"pieces",v.pieces);field(c,"actual_consumed_total",v.actual_consumed_total);field(c,"reached_points",v.reached_points);field(c,"original_point_count",v.original_point_count);field(c,"original_end_credit",v.original_end_credit);field(c,"original_closing_policy",v.original_closing_policy);field(c,"continued",v.continued);field(c,"catalog_rule_ref",v.catalog_rule_ref);field(c,"original_unit_ref",v.original_unit_ref);return c.status();}
Status value(CodecVisitor& c,RoadMovementBasis& v){field(c,"basis_ref",v.basis_ref);field(c,"original_inputs",v.original_inputs);field(c,"start_anchor",v.start_anchor);field(c,"end_anchor",v.end_anchor);field(c,"exact_start_progress",v.exact_start_progress);field(c,"exact_end_progress",v.exact_end_progress);field(c,"continued",v.continued);field(c,"catalog_rule_ref",v.catalog_rule_ref);field(c,"original_unit_ref",v.original_unit_ref);return c.status();}
Status value(CodecVisitor& c,RunningMovementMap& v){field(c,"world_from",v.world_from);field(c,"cursor_at_from",v.cursor_at_from);field(c,"rate",v.rate);field(c,"budget_rate",v.budget_rate);field(c,"rate_source",v.rate_source);return c.status();}
Status value(CodecVisitor& c,HeldMovementMap& v){field(c,"held_cursor",v.held_cursor);field(c,"actual_hold_source",v.actual_hold_source);field(c,"effective_at",v.effective_at);field(c,"resume_rate",v.resume_rate);field(c,"resume_budget_rate",v.resume_budget_rate);field(c,"resume_rate_source",v.resume_rate_source);return c.status();}
Status value(CodecVisitor& c,MovementActive& v){return c.status();}
Status value(CodecVisitor& c,MovementHeld& v){field(c,"original_reason_source",v.original_reason_source);return c.status();}
Status value(CodecVisitor& c,MovementClosed& v){field(c,"original_result_ref",v.original_result_ref);field(c,"actual_anchor",v.actual_anchor);return c.status();}
Status value(CodecVisitor& c,MovementPrefixState& v){field(c,"id",v.id);field(c,"execution",v.execution);field(c,"basis",v.basis);field(c,"cursor",v.cursor);field(c,"activity_world_map",v.activity_world_map);field(c,"last_settled_at",v.last_settled_at);field(c,"unit_ref",v.unit_ref);field(c,"lifecycle",v.lifecycle);return c.status();}
Status value(CodecVisitor& c,NoTerrainBudget& v){return c.status();}
Status value(CodecVisitor& c,TerrainUnitBudget& v){field(c,"opening_credit",v.opening_credit);field(c,"funded",v.funded);field(c,"spent",v.spent);field(c,"discarded",v.discarded);field(c,"disposition_sources",v.disposition_sources);return c.status();}
Status value(CodecVisitor& c,MovementPaymentNotConsumed& v){return c.status();}
Status value(CodecVisitor& c,MovementPaymentConsumed& v){field(c,"actual_expense_source",v.actual_expense_source);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"amount",v.amount);return c.status();}
Status value(CodecVisitor& c,MovementUnitOpen& v){return c.status();}
Status value(CodecVisitor& c,MovementUnitFinalized& v){field(c,"original_close_source",v.original_close_source);return c.status();}
Status value(CodecVisitor& c,MovementActivitySegment& v){field(c,"span",v.span);field(c,"cursor_from",v.cursor_from);field(c,"cursor_to",v.cursor_to);field(c,"rate",v.rate);field(c,"budget_rate",v.budget_rate);field(c,"rate_source",v.rate_source);field(c,"basis_ref",v.basis_ref);return c.status();}
Status value(CodecVisitor& c,MovementUnitState& v){field(c,"id",v.id);field(c,"execution",v.execution);field(c,"ordinal",v.ordinal);field(c,"active_elapsed",v.active_elapsed);field(c,"budget",v.budget);field(c,"payment",v.payment);field(c,"effort_basis_refs",v.effort_basis_refs);field(c,"naval_finalized_source",v.naval_finalized_source);field(c,"status",v.status);field(c,"activity_spans",v.activity_spans);return c.status();}
Status value(CodecVisitor& c,TerrainCreditDispositionSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"basis_ref",v.basis_ref);field(c,"execution",v.execution);field(c,"cursor",v.cursor);field(c,"consumed",v.consumed);field(c,"before_credit",v.before_credit);field(c,"after_credit",v.after_credit);field(c,"discarded",v.discarded);field(c,"kind",v.kind);field(c,"original_close_source",v.original_close_source);field(c,"occurred_at",v.occurred_at);return c.status();}
Status value(CodecVisitor& c,ExactContactMotionWitness& v){field(c,"unit_ref",v.unit_ref);field(c,"basis_ref",v.basis_ref);field(c,"original_mapping",v.original_mapping);return c.status();}
Status value(CodecVisitor& c,ExactMovementContact& v){field(c,"id",v.id);field(c,"original_hit",v.original_hit);field(c,"window_from",v.window_from);field(c,"window_duration",v.window_duration);field(c,"occurred_at",v.occurred_at);field(c,"first_prefix_id",v.first_prefix_id);field(c,"second_prefix_id",v.second_prefix_id);field(c,"first_motion_source",v.first_motion_source);field(c,"second_motion_source",v.second_motion_source);field(c,"exact_road_fraction",v.exact_road_fraction);field(c,"geometry_profile",v.geometry_profile);return c.status();}
Status value(CodecVisitor& c,WorldMovementWriter& v){field(c,"prefix_id",v.prefix_id);return c.status();}
Status value(CodecVisitor& c,BattleMovementWriter& v){field(c,"session_id",v.session_id);field(c,"handoff_id",v.handoff_id);field(c,"generation",v.generation);return c.status();}
Status value(CodecVisitor& c,StableMovementWriter& v){field(c,"anchor",v.anchor);return c.status();}
Status value(CodecVisitor& c,CarriedMovementWriter& v){field(c,"carrier_ref",v.carrier_ref);return c.status();}
Status value(CodecVisitor& c,SpatialWriterBinding& v){field(c,"subject_kind",v.subject_kind);field(c,"subject_id",v.subject_id);field(c,"writer",v.writer);field(c,"binding_source",v.binding_source);return c.status();}
Status value(CodecVisitor& c,MovementArrivalSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"execution",v.execution);field(c,"occurred_at",v.occurred_at);field(c,"actual_anchor",v.actual_anchor);field(c,"basis_ref",v.basis_ref);field(c,"cursor",v.cursor);field(c,"reached_points",v.reached_points);field(c,"original_point_count",v.original_point_count);field(c,"destination_settlement_id",v.destination_settlement_id);return c.status();}
Status value(CodecVisitor& c,MovementExpenseSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"execution",v.execution);field(c,"occurred_at",v.occurred_at);field(c,"amount",v.amount);field(c,"balance_before",v.balance_before);field(c,"balance_after",v.balance_after);field(c,"original_cost_rule_ref",v.original_cost_rule_ref);return c.status();}
Status value(CodecVisitor& c,TravelExertionSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"execution",v.execution);field(c,"actual_span",v.actual_span);field(c,"active_elapsed",v.active_elapsed);field(c,"original_fatigue_per_unit",v.original_fatigue_per_unit);field(c,"actual_participants",v.actual_participants);field(c,"original_close_source",v.original_close_source);return c.status();}
Status exertion_basis_fields(CodecVisitor& c,TravelExertionBasis& v,Integer selected_travel_version){
 if(selected_travel_version!=4&&selected_travel_version!=5)return c.reject("UnsupportedExactTravelLayout");
 if(selected_travel_version==4){if(!c.reading()&&v.accepted_funding_plan)return c.reject("UnselectedPersonalFundingSource",v.id);if(c.reading())v.accepted_funding_plan.reset();}
 field(c,"id",v.id);field(c,"unit_ref",v.unit_ref);field(c,"execution",v.execution);field(c,"original_action",v.original_action);field(c,"opened_at",v.opened_at);field(c,"actual_participants",v.actual_participants);field(c,"original_fatigue_per_unit",v.original_fatigue_per_unit);field(c,"original_rule_source",v.original_rule_source);field(c,"actual_basis",v.actual_basis);field(c,"opened_cursor",v.opened_cursor);
 if(selected_travel_version==5)c.optional("accepted_funding_plan",v.accepted_funding_plan,[](CodecVisitor& x,MomentOwnerSource& source){field(x,"source_ref",source.source_ref);return x.field("source_schema",source.source_schema);});
 return c.status();
}
Status value(CodecVisitor& c,TravelExertionBasis& v){return exertion_basis_fields(c,v,4);}
// 出力依据沿已选版本保原字段；独立留存分派不进入编码或重复计数。
Status exertion_basis_table(CodecVisitor &c,
                            OrderedTable<TravelExertionBasis> &values,
                            Integer selected_travel_version) {
  auto visit = [selected_travel_version](CodecVisitor &x,
                                         TravelExertionBasis &record) {
    return exertion_basis_fields(x, record, selected_travel_version);
  };

  using Row = OrderedTable<TravelExertionBasis>::Row;
  auto emit = [&](CodecVisitor &x, Row &row) {
    x.field("key", row.first);
    return x.object(
        "record", [&](CodecVisitor &item) { return visit(item, row.second); });
  };
  if (!c.reading())
    return c.sequence("exertion_bases", values.rows(), emit);
  std::vector<Row> rows;
  c.sequence("exertion_bases", rows, emit);
  if (c.status().ok && !values.adopt_checkpoint_rows(std::move(rows)))
    return c.reject("DuplicateMovementRecord", "exertion_bases");
  return c.status();
}
Status value(CodecVisitor& c,PersonalMovementControlSource& v){field(c,"id",v.id);field(c,"actor_id",v.actor_id);field(c,"action_id",v.action_id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"basis_ref",v.basis_ref);field(c,"operation",v.operation);field(c,"occurred_at",v.occurred_at);field(c,"cursor",v.cursor);field(c,"original_execution_source",v.original_execution_source);return c.status();}
Status value(CodecVisitor& c,MovementContactContinuationSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"contact_ref",v.contact_ref);field(c,"closed_unit_ref",v.closed_unit_ref);field(c,"new_unit_ref",v.new_unit_ref);field(c,"actual_resolution_source",v.actual_resolution_source);field(c,"resolution_schema",v.resolution_schema);field(c,"occurred_at",v.occurred_at);field(c,"held",v.held);return c.status();}
Status value(CodecVisitor& c,MovementUnitBoundarySource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"basis_ref",v.basis_ref);field(c,"execution",v.execution);field(c,"occurred_at",v.occurred_at);field(c,"cursor",v.cursor);return c.status();}
Status value(CodecVisitor& c,MovementContactInstallationSource& v){field(c,"id",v.id);field(c,"contact_ref",v.contact_ref);field(c,"actual_business_source",v.actual_business_source);field(c,"business_source_schema",v.business_source_schema);field(c,"occurred_at",v.occurred_at);return c.status();}
Status value(CodecVisitor& c,PersonalMovementTerminalSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"actual_action",v.actual_action);field(c,"opening_source",v.opening_source);field(c,"actual_close_source",v.actual_close_source);field(c,"occurred_at",v.occurred_at);field(c,"kind",v.kind);return c.status();}
Status value(CodecVisitor& c,MovementBasisTransitionSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"execution",v.execution);field(c,"occurred_at",v.occurred_at);field(c,"old_basis_ref",v.old_basis_ref);field(c,"old_cursor",v.old_cursor);field(c,"new_basis_ref",v.new_basis_ref);field(c,"actual_anchor",v.actual_anchor);field(c,"original_reason_source",v.original_reason_source);return c.status();}
Status value(CodecVisitor& c,MovementUnitCloseSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"execution",v.execution);field(c,"kind",v.kind);field(c,"occurred_at",v.occurred_at);field(c,"cursor",v.cursor);field(c,"actual_anchor",v.actual_anchor);field(c,"actual_cause",v.actual_cause);return c.status();}
Status value(CodecVisitor& c,PatrolFinalMotionSource& v){field(c,"id",v.id);field(c,"prefix_id",v.prefix_id);field(c,"unit_ref",v.unit_ref);field(c,"basis_ref",v.basis_ref);field(c,"execution",v.execution);field(c,"occurred_at",v.occurred_at);field(c,"cursor",v.cursor);field(c,"actual_anchor",v.actual_anchor);field(c,"final_contact_source",v.final_contact_source);return c.status();}
Status value(CodecVisitor& c,PatrolCheckpointArrival2& v){field(c,"source_fact_id",v.source_fact_id);field(c,"journey_id",v.journey_id);field(c,"final_motion_source_ref",v.final_motion_source_ref);field(c,"actor_or_group_ref",v.actor_or_group_ref);field(c,"checkpoint_ref",v.checkpoint_ref);field(c,"arrived_at",v.arrived_at);return c.status();}
Status value(CodecVisitor& c,ActorPassageRequester& v){return passage_requester_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,HouseholdPolicyPassageRequester& v){return passage_requester_codec_fields::branch_fields(c,v);}
Status value(CodecVisitor& c,PassageBoundaryRef& v){return passage_boundary_codec_fields::fields(c,v);}
Status value(CodecVisitor& c,BoundaryCrossingReceipt& v){field(c,"receipt_id",v.receipt_id);field(c,"execution",v.execution);field(c,"carrier",v.carrier);field(c,"composition_revision",v.composition_revision);field(c,"manifest_revision",v.manifest_revision);field(c,"boundary",v.boundary);field(c,"entry_at",v.entry_at);field(c,"entry_ordinal",v.entry_ordinal);field(c,"decision_source_ref",v.decision_source_ref);field(c,"payment_refs",v.payment_refs);field(c,"effect_use_refs",v.effect_use_refs);field(c,"safe_exit_binding_ref",v.safe_exit_binding_ref);return c.status();}
Status value(CodecVisitor& c,CarrierPosition& v){field(c,"position_id",v.position_id);field(c,"carrier_ref",v.carrier_ref);field(c,"location",v.location);field(c,"source_ref",v.source_ref);field(c,"revision",v.revision);return c.status();}
Status value(CodecVisitor& c,TransportUnitState& v){field(c,"transport_unit_id",v.transport_unit_id);field(c,"subject",v.subject);field(c,"registration_source_ref",v.registration_source_ref);field(c,"composition_revision",v.composition_revision);field(c,"population_binding_refs",v.population_binding_refs);field(c,"cohort_allocation_refs",v.cohort_allocation_refs);field(c,"named_participants",v.named_participants);field(c,"vehicle_refs",v.vehicle_refs);field(c,"inventory_ids",v.inventory_ids);field(c,"custody_ref",v.custody_ref);field(c,"position_ref",v.position_ref);field(c,"active_journey_id",v.active_journey_id);field(c,"state",v.state);return c.status();}
Status value(CodecVisitor& c,DirectGroupMovement& v){field(c,"group_movement_id",v.group_movement_id);return c.status();}
Status value(CodecVisitor& c,CarriedGroupMovement& v){field(c,"vessel_binding_refs",v.vessel_binding_refs);field(c,"carrier_group_movement_id",v.carrier_group_movement_id);return c.status();}
Status value(CodecVisitor& c,GroupMovementBinding& v){field(c,"carrier",v.carrier);field(c,"position_ref",v.position_ref);field(c,"execution_owner",v.execution_owner);return c.status();}
Status value(CodecVisitor& c,GroupMovementJourney& v){field(c,"journey_id",v.journey_id);field(c,"carrier",v.carrier);field(c,"position_ref",v.position_ref);field(c,"composition_revision",v.composition_revision);field(c,"initiator",v.initiator);field(c,"sponsor",v.sponsor);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"expense_authority",v.expense_authority);field(c,"shipment_refs",v.shipment_refs);field(c,"route_profile",v.route_profile);field(c,"movement_execution",v.movement_execution);field(c,"started_at",v.started_at);field(c,"last_step_at",v.last_step_at);field(c,"state",v.state);field(c,"stop_reason",v.stop_reason);field(c,"expense_source_refs",v.expense_source_refs);field(c,"movement_source_refs",v.movement_source_refs);field(c,"movement_prefix_ref",v.movement_prefix_ref);return c.status();}
Status value(CodecVisitor& c,GroupJourneyStep& v){field(c,"id",v.id);field(c,"journey_id",v.journey_id);field(c,"ordinal",v.ordinal);field(c,"span",v.span);field(c,"from_anchor",v.from_anchor);field(c,"to_anchor",v.to_anchor);field(c,"actual_progress",v.actual_progress);field(c,"cost_refs",v.cost_refs);field(c,"exertion_refs",v.exertion_refs);field(c,"contact_refs",v.contact_refs);field(c,"outcome",v.outcome);return c.status();}
Status value(CodecVisitor& c,GroupJourneyTerminal& v){field(c,"journey_id",v.journey_id);field(c,"actual_terminal_source",v.actual_terminal_source);field(c,"source_schema",v.source_schema);field(c,"occurred_at",v.occurred_at);field(c,"outcome",v.outcome);return c.status();}
Status value(CodecVisitor& c,InventorySpatialBinding& v){field(c,"inventory_id",v.inventory_id);field(c,"placement",v.placement);field(c,"source_ref",v.source_ref);field(c,"revision",v.revision);return c.status();}
Status value(CodecVisitor& c,TransportPopulationSource& v){field(c,"source_ref",v.source_ref);field(c,"cohort_id",v.cohort_id);field(c,"persons",v.persons);field(c,"working_crew",v.working_crew);field(c,"named_actor_ids",v.named_actor_ids);return c.status();}
Status value(CodecVisitor& c,VesselManifestPlacement& v){field(c,"vessel_id",v.vessel_id);field(c,"placement_source",v.placement_source);field(c,"named_actor_ids",v.named_actor_ids);field(c,"inventory_ids",v.inventory_ids);field(c,"population_sources",v.population_sources);field(c,"persons",v.persons);field(c,"weight",v.weight);field(c,"maximum_persons",v.maximum_persons);field(c,"maximum_weight",v.maximum_weight);return c.status();}
Status value(CodecVisitor& c,TransportManifestRead& v){field(c,"carrier",v.carrier);field(c,"manifest_revision",v.manifest_revision);field(c,"composition_revision",v.composition_revision);field(c,"at_tick",v.at_tick);field(c,"actual_anchor",v.actual_anchor);field(c,"source_refs",v.source_refs);field(c,"named_actor_refs",v.named_actor_refs);field(c,"vehicle_refs",v.vehicle_refs);field(c,"inventory_refs",v.inventory_refs);field(c,"civilian_population_refs",v.civilian_population_refs);field(c,"military_force_refs",v.military_force_refs);field(c,"vessel_placements",v.vessel_placements);field(c,"persons",v.persons);field(c,"working_crew",v.working_crew);field(c,"weight",v.weight);return c.status();}
Status value(CodecVisitor& c,TransportInventoryManifest& v){field(c,"entry_evidence",v.entry_evidence);return c.status();}
Status value(CodecVisitor& c,TransportPopulationManifest& v){field(c,"population_ref",v.population_ref);field(c,"population_revision",v.population_revision);field(c,"actual",v.actual);return c.status();}
Status value(CodecVisitor& c,TransportEntryManifestSource& v){field(c,"id",v.id);field(c,"execution",v.execution);field(c,"entry_ordinal",v.entry_ordinal);field(c,"boundary",v.boundary);field(c,"occurred_at",v.occurred_at);field(c,"manifest",v.manifest);field(c,"inventory_sources",v.inventory_sources);field(c,"population_sources",v.population_sources);return c.status();}
Status value(CodecVisitor& c,TravelProfile& v){field(c,"mode",v.mode);field(c,"payer_wallet_id",v.payer_wallet_id);field(c,"load_weight",v.load_weight);field(c,"naval_budget",v.naval_budget);return c.status();}
Status value(CodecVisitor& c,GroupMotionProfile& v){if(v.original_load_total||v.original_load_people)return c.reject("MilitarySupplyProfileRequiresGroup7");field(c,"actual_rule_source",v.actual_rule_source);field(c,"source_schema",v.source_schema);field(c,"original_profile",v.original_profile);field(c,"additional_cost_per_unit",v.additional_cost_per_unit);field(c,"fare_payee_wallet_id",v.fare_payee_wallet_id);field(c,"fare_destination_id",v.fare_destination_id);return c.status();}
Status value(CodecVisitor& c,GroupMovementOpeningSource& v){if(c.reading()){v.carrier_origin.emplace<TransportUnitRef>();v.military_order.reset();}auto* original=std::get_if<TransportUnitRef>(&v.carrier_origin);if(!original||v.military_order)return c.reject("MilitaryGroupOpeningRequiresSuccessorLayout");field(c,"id",v.id);field(c,"journey_id",v.journey_id);field(c,"transport_unit_ref",*original);field(c,"composition_revision",v.composition_revision);field(c,"occurred_at",v.occurred_at);field(c,"original_anchor",v.original_anchor);field(c,"motion_profile",v.motion_profile);field(c,"manifest_sources",v.manifest_sources);field(c,"named_participants",v.named_participants);return c.status();}
Status value(CodecVisitor& c,GroupMovementUnitTerms& v){field(c,"unit_ref",v.unit_ref);field(c,"journey_id",v.journey_id);field(c,"motion_profile",v.motion_profile);field(c,"fare_amount",v.fare_amount);field(c,"actual_start_source",v.actual_start_source);return c.status();}
Status value(CodecVisitor& c,GroupExpensePrefixCause& v){field(c,"first_actual_span",v.first_actual_span);return c.status();}
Status value(CodecVisitor& c,GroupExpenseContactCause& v){field(c,"actual_contact",v.actual_contact);return c.status();}
Status value(CodecVisitor& c,GroupTransportExpenseSource& v){field(c,"id",v.id);field(c,"journey_id",v.journey_id);field(c,"prefix_ref",v.prefix_ref);field(c,"unit_ref",v.unit_ref);field(c,"occurred_at",v.occurred_at);field(c,"actual_cause",v.actual_cause);field(c,"original_payment_receipt",v.original_payment_receipt);return c.status();}
Status value(CodecVisitor& c,GroupMovementControlSource& v){field(c,"id",v.id);field(c,"journey_id",v.journey_id);field(c,"operation",v.operation);field(c,"authority_source",v.authority_source);field(c,"occurred_at",v.occurred_at);field(c,"actual_anchor",v.actual_anchor);return c.status();}
Status value(CodecVisitor& c,TransportSelectionRegistration& v){field(c,"selection_id",v.selection_id);field(c,"departure_anchor_ref",v.departure_anchor_ref);field(c,"release_allocation_ids",v.release_allocation_ids);return c.status();}
Status value(CodecVisitor& c,TransportRegistrationAuthority& v){field(c,"business_source",v.business_source);field(c,"subject",v.subject);field(c,"initiator",v.initiator);field(c,"accepted_scope_revision",v.accepted_scope_revision);field(c,"labor_until_tick",v.labor_until_tick);field(c,"accepted_selection_ids",v.accepted_selection_ids);field(c,"named_participants",v.named_participants);field(c,"vehicle_refs",v.vehicle_refs);field(c,"inventory_ids",v.inventory_ids);field(c,"custody_ref",v.custody_ref);return c.status();}
Status value(CodecVisitor& c,TransportRegistrationSource& v){field(c,"id",v.id);field(c,"transport_unit_id",v.transport_unit_id);field(c,"accepted_scope",v.accepted_scope);field(c,"occurred_at",v.occurred_at);field(c,"departure_anchor",v.departure_anchor);field(c,"selections",v.selections);field(c,"actual_vehicle_takeover_refs",v.actual_vehicle_takeover_refs);return c.status();}
Status value(CodecVisitor& c,TransportUnitReleaseSource& v){field(c,"id",v.id);field(c,"transport_unit_id",v.transport_unit_id);field(c,"handover_site_id",v.handover_site_id);field(c,"receiver_custody_ref",v.receiver_custody_ref);field(c,"composition_revision",v.composition_revision);field(c,"occurred_at",v.occurred_at);field(c,"actual_anchor",v.actual_anchor);field(c,"accepted_handover_source",v.accepted_handover_source);field(c,"released_composition",v.released_composition);field(c,"cargo_handover_refs",v.cargo_handover_refs);field(c,"participant_release_refs",v.participant_release_refs);field(c,"vehicle_release_refs",v.vehicle_release_refs);field(c,"obligation_disposition_refs",v.obligation_disposition_refs);field(c,"population_binding_ids",v.population_binding_ids);return c.status();}
Status value(CodecVisitor& c,MilitaryActorChange& v){field(c,"actor_id",v.actor_id);field(c,"status",v.status);field(c,"captor_army_id",v.captor_army_id);field(c,"garrison_id",v.garrison_id);field(c,"army_id",v.army_id);field(c,"field_order",v.field_order);return c.status();}
Status value(CodecVisitor& c,MilitaryBattleResult& v){field(c,"winner_side",v.winner_side);field(c,"reason",v.reason);field(c,"field_order",v.field_order);return c.status();}
Status value(CodecVisitor& c,MilitaryRosterReceipt& v){field(c,"unit_id",v.unit_id);field(c,"fit",v.fit);field(c,"wounded",v.wounded);field(c,"dead",v.dead);field(c,"captured",v.captured);field(c,"deserted",v.deserted);field(c,"member_ids",v.member_ids);field(c,"morale",v.morale);field(c,"captors",v.captors);field(c,"field_order",v.field_order);return c.status();}
Status value(CodecVisitor& c,MilitaryMemberEffectLink& v){field(c,"actor_id",v.actor_id);field(c,"effect_index",v.effect_index);field(c,"purpose",v.purpose);field(c,"effect_source",v.effect_source);return c.status();}
Status value(CodecVisitor& c,DynamicMilitaryBattleFact2& v){field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"effective_at",v.effective_at);field(c,"started_at",v.started_at);field(c,"effect_group_ref",v.effect_group_ref);field(c,"army_ids",v.army_ids);field(c,"unit_ids",v.unit_ids);field(c,"result",v.result);field(c,"source_ref",v.source_ref);field(c,"contact_source_ref",v.contact_source_ref);field(c,"winner_side",v.winner_side);field(c,"final",v.final);return c.status();}
Status value(CodecVisitor& c,DynamicMilitaryCasualtyFact2& v){field(c,"id",v.id);field(c,"effective_at",v.effective_at);field(c,"session_ref",v.session_ref);field(c,"effect_group_ref",v.effect_group_ref);field(c,"original_casualty_rule_ref",v.original_casualty_rule_ref);field(c,"roster_receipts",v.roster_receipts);field(c,"actor_changes",v.actor_changes);field(c,"member_effect_links",v.member_effect_links);return c.status();}
Status value(CodecVisitor& c,MilitarySupplyEffectSource2& v){return visit_exact_military_supply_source(c,v);}
Status value(CodecVisitor& c,SignedSpatialRational& v){field(c,"sign",v.sign);field(c,"magnitude",v.magnitude);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialPoint& v){field(c,"x",v.x);field(c,"y",v.y);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialTranslation& v){field(c,"b1",v.b1);field(c,"b2",v.b2);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialMatrix& v){field(c,"a11",v.a11);field(c,"a12",v.a12);field(c,"a21",v.a21);field(c,"a22",v.a22);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialSegment& v){field(c,"from",v.from);field(c,"to",v.to);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialPortal& v){field(c,"portal_id",v.portal_id);field(c,"world_entry_exit_source_kind",v.world_entry_exit_source_kind);field(c,"world_boundary_ref",v.world_boundary_ref);field(c,"local_boundary_segment",v.local_boundary_segment);field(c,"allowed_carrier_kinds",v.allowed_carrier_kinds);field(c,"orientation_mapping",v.orientation_mapping);field(c,"connector_path_ref",v.connector_path_ref);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialChart2& v){field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"geometry_definition_ref",v.geometry_definition_ref);field(c,"world_geometry_ref",v.world_geometry_ref);field(c,"world_length_unit_ref",v.world_length_unit_ref);field(c,"local_axis_unit_ref",v.local_axis_unit_ref);field(c,"matrix",v.matrix);field(c,"translation",v.translation);field(c,"domain_polygon",v.domain_polygon);field(c,"portal_rows",v.portal_rows);field(c,"creation_source_ref",v.creation_source_ref);field(c,"rule_version",v.rule_version);return c.status();}
Status value(CodecVisitor& c,MilitaryEntryConnectorDefinition& v){field(c,"id",v.id);field(c,"chart_ref",v.chart_ref);field(c,"portal_id",v.portal_id);field(c,"path",v.path);field(c,"entered_cell_costs",v.entered_cell_costs);field(c,"original_path_source",v.original_path_source);return c.status();}
Status value(CodecVisitor& c,HexPoint& v){field(c,"q",v.q);field(c,"r",v.r);return c.status();}
Status value(CodecVisitor& c,MilitaryOrientationHalfspace& v){field(c,"normal",v.normal);field(c,"maximum",v.maximum);return c.status();}
Status value(CodecVisitor& c,MilitaryEncounterOrientation& v){field(c,"id",v.id);field(c,"tangent_region",v.tangent_region);field(c,"matrix",v.matrix);field(c,"local_entry",v.local_entry);return c.status();}
Status value(CodecVisitor& c,LockedArenaChartDefinition& v){field(c,"matrix",v.matrix);field(c,"translation",v.translation);return c.status();}
Status value(CodecVisitor& c,EncounterChartDefinition& v){field(c,"orientation_choices",v.orientation_choices);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialGeometryDefinition2& v){field(c,"id",v.id);field(c,"world_geometry_ref",v.world_geometry_ref);field(c,"world_length_unit_ref",v.world_length_unit_ref);field(c,"local_axis_unit_ref",v.local_axis_unit_ref);field(c,"domain_polygon",v.domain_polygon);field(c,"portal_rows",v.portal_rows);field(c,"author_mode",v.author_mode);field(c,"actual_definition_source",v.actual_definition_source);return c.status();}
Status value(CodecVisitor& c,MilitaryLocalUnitRef& v){return military_local_body_codec::value(c,v);}
Status value(CodecVisitor& c,MilitaryLocalVesselRef& v){return military_local_body_codec::value(c,v);}
Status value(CodecVisitor& c,LocalMotionSegmentRef& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalHexNeighbor& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,ReverseLocalMotionAtPrefix& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,RepriceLocalMotionSuffix& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalPartialEdgeContinuation& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalEntryConnector& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalExitConnector& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,WorkUnits& v){field(c,"value",v.value);return c.status();}
Status value(CodecVisitor& c,LocalMotionSegment& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalHexMotionBasis2& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalMotionPrefixClose2& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,LocalMotionCursor2& v){return military_local_motion_fields::value(c,v);}
Status value(CodecVisitor& c,MilitaryRouteCorrespondence2& v){field(c,"old_segment_ordinal",v.old_segment_ordinal);field(c,"new_segment_ordinal",v.new_segment_ordinal);field(c,"old_parameter_from",v.old_parameter_from);field(c,"old_parameter_to",v.old_parameter_to);field(c,"new_parameter_from",v.new_parameter_from);field(c,"new_parameter_to",v.new_parameter_to);field(c,"old_from",v.old_from);field(c,"old_to",v.old_to);field(c,"new_from",v.new_from);field(c,"new_to",v.new_to);field(c,"chart_ref",v.chart_ref);field(c,"original_parameter_source",v.original_parameter_source);return c.status();}
Status value(CodecVisitor& c,MilitarySpatialHandoff2& v){field(c,"id",v.id);field(c,"carrier_ref",v.carrier_ref);field(c,"from_binding",v.from_binding);field(c,"to_binding",v.to_binding);field(c,"old_execution_ref",v.old_execution_ref);field(c,"new_execution_ref",v.new_execution_ref);field(c,"at",v.at);field(c,"entry_or_exit_source_ref",v.entry_or_exit_source_ref);field(c,"chart_ref",v.chart_ref);field(c,"portal_id",v.portal_id);field(c,"old_basis_ref",v.old_basis_ref);field(c,"old_cursor",v.old_cursor);field(c,"new_anchor",v.new_anchor);field(c,"remaining_route_correspondence",v.remaining_route_correspondence);field(c,"cost_and_work_disposition_refs",v.cost_and_work_disposition_refs);field(c,"closed_writer_source_ref",v.closed_writer_source_ref);return c.status();}
Status value(CodecVisitor& c,CargoSelection& v){field(c,"inventory_id",v.inventory_id);field(c,"lot_id",v.lot_id);field(c,"quantity",v.quantity);return c.status();}
Status value(CodecVisitor& c,TimeScaleRef& v){field(c,"definition_id",v.definition_id);field(c,"version",v.version);field(c,"units_per_half_day",v.units_per_half_day);return c.status();}
Status value(CodecVisitor& c,ActionElapsedEntryRef& v){field(c,"action_ref",v.action_ref);return c.status();}
Status value(CodecVisitor& c,CargoHandlingRule& v){field(c,"id",v.id);field(c,"version",v.version);field(c,"allowed_carrier_kinds",v.allowed_carrier_kinds);field(c,"allowed_operations",v.allowed_operations);field(c,"allowed_business_kinds",v.allowed_business_kinds);field(c,"allowed_cargo_classes",v.allowed_cargo_classes);field(c,"handling_half_days",v.handling_half_days);field(c,"required_workers",v.required_workers);field(c,"labor_modes",v.labor_modes);field(c,"required_capability_purpose_ref",v.required_capability_purpose_ref);field(c,"facility_requirement_ref",v.facility_requirement_ref);field(c,"cost_rule_ref",v.cost_rule_ref);field(c,"interruption_rule_ref",v.interruption_rule_ref);return c.status();}
Status value(CodecVisitor& c,NamedCargoHandlingWorkers& v){field(c,"lead_actor_id",v.lead_actor_id);field(c,"participant_actor_ids",v.participant_actor_ids);return c.status();}
Status value(CodecVisitor& c,CohortCargoHandlingWorkers& v){field(c,"allocation_request_refs",v.allocation_request_refs);return c.status();}
Status value(CodecVisitor& c,CargoHandlingTerms& v){field(c,"terms_id",v.terms_id);field(c,"business_source",v.business_source);field(c,"batch_ordinal",v.batch_ordinal);field(c,"operation",v.operation);field(c,"carrier",v.carrier);field(c,"source_inventory_id",v.source_inventory_id);field(c,"destination_inventory_id",v.destination_inventory_id);field(c,"cargo",v.cargo);field(c,"matching_owners",v.matching_owners);field(c,"source_reservation",v.source_reservation);field(c,"destination_owner",v.destination_owner);field(c,"handling_rule_ref",v.handling_rule_ref);field(c,"accepted_rule_version",v.accepted_rule_version);field(c,"actual_site_or_anchor_ref",v.actual_site_or_anchor_ref);field(c,"carrier_composition_source_ref",v.carrier_composition_source_ref);field(c,"source_custody_ref",v.source_custody_ref);field(c,"receiver_acceptance_source_ref",v.receiver_acceptance_source_ref);field(c,"expense_authority_ref",v.expense_authority_ref);field(c,"cost_terms_ref",v.cost_terms_ref);field(c,"labor_selection",v.labor_selection);field(c,"facility_claim_ref",v.facility_claim_ref);field(c,"accepted_at",v.accepted_at);field(c,"actual_start_cost_sources",v.actual_start_cost_sources);return c.status();}
Status value(CodecVisitor& c,NamedCargoHandlingTiming& v){field(c,"action_ref",v.action_ref);field(c,"action_elapsed_source_ref",v.action_elapsed_source_ref);return c.status();}
Status value(CodecVisitor& c,CohortCargoHandlingTiming& v){field(c,"scale_ref",v.scale_ref);field(c,"required_units",v.required_units);field(c,"credited_units",v.credited_units);field(c,"last_settled_at",v.last_settled_at);field(c,"participation_source_refs",v.participation_source_refs);field(c,"state",v.state);return c.status();}
Status value(CodecVisitor& c,CargoHandlingExecution& v){field(c,"execution_id",v.execution_id);field(c,"terms_ref",v.terms_ref);field(c,"business_source",v.business_source);field(c,"batch_ordinal",v.batch_ordinal);field(c,"phase",v.phase);field(c,"started_at",v.started_at);field(c,"timing",v.timing);field(c,"labor_binding_refs",v.labor_binding_refs);field(c,"facility_claim_ref",v.facility_claim_ref);field(c,"progress_source_revision",v.progress_source_revision);field(c,"work_result_ref",v.work_result_ref);field(c,"terminal_result_ref",v.terminal_result_ref);field(c,"waiting_cause_source_ref",v.waiting_cause_source_ref);return c.status();}
Status value(CodecVisitor& c,CargoHandlingWorkResult& v){field(c,"result_id",v.result_id);field(c,"execution_ref",v.execution_ref);field(c,"terms_ref",v.terms_ref);field(c,"service_started_at",v.service_started_at);field(c,"service_completed_at",v.service_completed_at);field(c,"timing_source_ref",v.timing_source_ref);field(c,"labor_coverage_source_refs",v.labor_coverage_source_refs);field(c,"facility_use_source_refs",v.facility_use_source_refs);field(c,"cost_source_refs",v.cost_source_refs);field(c,"rule_ref",v.rule_ref);field(c,"rule_version",v.rule_version);return c.status();}
Status value(CodecVisitor& c,CargoHandlingResult& v){field(c,"result_id",v.result_id);field(c,"execution_ref",v.execution_ref);field(c,"terms_ref",v.terms_ref);field(c,"business_source",v.business_source);field(c,"batch_ordinal",v.batch_ordinal);field(c,"status",v.status);field(c,"occurred_at",v.occurred_at);field(c,"work_result_ref",v.work_result_ref);field(c,"actual_transfer_source_refs",v.actual_transfer_source_refs);field(c,"actual_resource_effect_refs",v.actual_resource_effect_refs);field(c,"actual_lot_flow_refs",v.actual_lot_flow_refs);field(c,"carrier_manifest_change_refs",v.carrier_manifest_change_refs);field(c,"original_business_result_refs",v.original_business_result_refs);field(c,"released_labor_source_refs",v.released_labor_source_refs);field(c,"released_claim_source_refs",v.released_claim_source_refs);field(c,"reason_source_ref",v.reason_source_ref);return c.status();}
} // 冷布局不调用业务、不发款、不恢复临时票据。
Status visit_transport_expense_authority(CodecVisitor& c,TransportExpenseAuthority& v){return movement_codec::value(c,v);}
Status visit_transport_expense_authority(CodecVisitor &c, std::string_view name,
                                         TransportExpenseAuthority &v) {
  return c.object(name, [&](CodecVisitor &child) {
    return visit_transport_expense_authority(child, v);
  });
}
Status visit_movement_execution_ref(CodecVisitor& c,const char* name,MovementExecutionRef& v){return movement_codec::field(c,name,v);}
Status visit_spatial_carrier_ref(CodecVisitor& c,const char* name,SpatialCarrierRef& v){return movement_codec::field(c,name,v);}
Status visit_passage_boundary_ref(CodecVisitor& c,const char* name,PassageBoundaryRef& v){return movement_codec::field(c,name,v);}
Status visit_passage_requester(CodecVisitor& c,const char* name,PassageRequester& v){return movement_codec::field(c,name,v);}
Status visit_patrol_checkpoint_sources(CodecVisitor& c,MovementExactState& state){using movement_codec::table;table(c,"final_motion_sources",state.patrol_final_motions);table(c,"arrivals",state.patrol_checkpoint_arrivals);return c.status();}
Status visit_movement_exact_checkpoint(CodecVisitor& c,MovementExactState& state,Integer selected_travel_version){if(!state.passage_modifier_adoptions.empty())return c.reject("FinalTravelPhysicalSectionRequired");if(selected_travel_version!=4&&selected_travel_version!=5)return c.reject("UnsupportedExactTravelLayout");using movement_codec::table;
table(c,"personal_controls",state.personal_controls);
table(c,"contact_continuations",state.contact_continuations);
table(c,"unit_boundaries",state.unit_boundaries);
table(c,"contact_installations",state.contact_installations);
table(c,"personal_terminals",state.personal_terminals);
table(c,"prefixes",state.prefixes);
table(c,"units",state.units);
table(c,"spatial_bindings",state.spatial_bindings);
table(c,"contacts",state.contacts);
table(c,"credit_dispositions",state.credit_dispositions);
table(c,"arrivals",state.arrivals);
table(c,"expenses",state.expenses);
table(c,"exertions",state.exertions);
movement_codec::exertion_basis_table(c,state.exertion_bases,selected_travel_version);
table(c,"unit_closes",state.unit_closes);
table(c,"basis_transitions",state.basis_transitions);
visit_cargo_handling_payloads(c,state.cargo_handling);return c.status();}
Status visit_spatial_carrier_checkpoint(CodecVisitor& c,SpatialCarrierState& state){if(!state.journey_purpose_bindings.empty())return c.reject("FinalGroupPhysicalSectionRequired");using movement_codec::table;
table(c,"transport_units",state.transport_units);
table(c,"positions",state.positions);
table(c,"bindings",state.bindings);
table(c,"journeys",state.journeys);
table(c,"steps",state.steps);
table(c,"terminals",state.terminals);
table(c,"inventory_bindings",state.inventory_bindings);
table(c,"crossings",state.crossings);
return c.status();}
Status visit_group_movement_execution_checkpoint(CodecVisitor& c,GroupMovementExecutionState& state){if(!state.military_supply_rebinds.empty())return c.reject("MilitarySupplyRebindRequiresGroup7");using movement_codec::table;table(c,"openings",state.openings);table(c,"unit_terms",state.unit_terms);table(c,"expenses",state.expenses);table(c,"controls",state.controls);table(c,"entry_manifests",state.entry_manifests);return c.status();}
Status visit_transport_registration_checkpoint(CodecVisitor& c,TransportRegistrationState& state){using movement_codec::table;table(c,"registrations",state.registrations);table(c,"releases",state.releases);return c.status();}
// 军事三表入口已成套归 military_exact_source_codec.cpp；此处不另定义同名旧写者。
Status visit_military_spatial_handoff_payloads(CodecVisitor& c,MilitarySpatialHandoffState& state){using movement_codec::table;table(c,"spatial_charts",state.charts);table(c,"spatial_handoffs",state.handoffs);return c.status();}
Status visit_military_spatial_definition2(CodecVisitor& c,MilitarySpatialGeometryDefinition2& value){return movement_codec::value(c,value);}
Status visit_cargo_handling_payloads(CodecVisitor& c,CargoHandlingState& state){using movement_codec::table;table(c,"cargo_handling_terms",state.terms);table(c,"cargo_handling_executions",state.executions);table(c,"cargo_handling_work_results",state.work_results);table(c,"cargo_handling_results",state.results);return c.status();}
Status visit_cargo_handling_rule(CodecVisitor& c,CargoHandlingRule& value){return movement_codec::value(c,value);}
namespace movement_codec {
Status successor(CodecVisitor& c,GroupMovementJourney& v){field(c,"journey_id",v.journey_id);field(c,"carrier",v.carrier);field(c,"position_ref",v.position_ref);field(c,"composition_revision",v.composition_revision);field(c,"initiator",v.initiator);field(c,"sponsor",v.sponsor);field(c,"payer_wallet_id",v.payer_wallet_id);c.object("expense_authority",[&](auto& x){return visit_group_journey_funding(x,v.expense_authority);});field(c,"shipment_refs",v.shipment_refs);field(c,"route_profile",v.route_profile);field(c,"movement_execution",v.movement_execution);field(c,"started_at",v.started_at);field(c,"last_step_at",v.last_step_at);field(c,"state",v.state);field(c,"stop_reason",v.stop_reason);field(c,"expense_source_refs",v.expense_source_refs);field(c,"movement_source_refs",v.movement_source_refs);field(c,"movement_prefix_ref",v.movement_prefix_ref);return c.status();}
Status successor(CodecVisitor& c,GroupMovementOpeningSource& v){field(c,"id",v.id);field(c,"journey_id",v.journey_id);field(c,"carrier_origin",v.carrier_origin);field(c,"composition_revision",v.composition_revision);field(c,"occurred_at",v.occurred_at);field(c,"original_anchor",v.original_anchor);field(c,"motion_profile",v.motion_profile);field(c,"manifest_sources",v.manifest_sources);field(c,"named_participants",v.named_participants);field(c,"military_order",v.military_order);return c.status();}
// 后继载荷表仍用原有限分支；留存不编码，读回保原重复键拒绝。
template <class T>
Status successor_table(CodecVisitor &c, const char *name, OrderedTable<T> &v) {

  using Row = typename OrderedTable<T>::Row;
  auto emit = [](CodecVisitor &x, Row &row) {
    x.field("key", row.first);
    return x.object("record",
                    [&](auto &item) { return successor(item, row.second); });
  };
  if (!c.reading())
    return c.sequence(name, v.rows(), emit);
  std::vector<Row> rows;
  c.sequence(name, rows, emit);
  if (c.status().ok && !v.adopt_checkpoint_rows(std::move(rows)))
    return c.reject("DuplicateMovementRecord", name);
  return c.status();
}
}
Status visit_spatial_carrier_checkpoint(CodecVisitor& c,SpatialCarrierState& s,Integer version){if(!s.journey_purpose_bindings.empty())return c.reject("FinalGroupPhysicalSectionRequired");if(version==4||version==5)return visit_spatial_carrier_checkpoint(c,s);if(version!=6)return c.reject("UnsupportedGroupTransportLayout");using movement_codec::table;table(c,"transport_units",s.transport_units);table(c,"positions",s.positions);table(c,"bindings",s.bindings);movement_codec::successor_table(c,"journeys",s.journeys);table(c,"steps",s.steps);table(c,"terminals",s.terminals);table(c,"inventory_bindings",s.inventory_bindings);table(c,"crossings",s.crossings);return c.status();}
Status visit_group_movement_execution_checkpoint(CodecVisitor& c,GroupMovementExecutionState& s,Integer version){if(!s.military_supply_rebinds.empty())return c.reject("MilitarySupplyRebindRequiresGroup7");if(version==4||version==5)return visit_group_movement_execution_checkpoint(c,s);if(version!=6)return c.reject("UnsupportedGroupTransportLayout");using movement_codec::table;movement_codec::successor_table(c,"openings",s.openings);table(c,"unit_terms",s.unit_terms);table(c,"expenses",s.expenses);table(c,"controls",s.controls);table(c,"entry_manifests",s.entry_manifests);return c.status();}
} // 外层完成全部领域引用校验后才publish恢复世界。
