#pragma once
#include "transport_section_codec.hpp"
#include "transport_physical_table.hpp"
#include "transport_physical_ownership.hpp"
#include <optional>
namespace hanmo::resident {
// Temporary decode rows are moved once to each original owner table. These are
// not an independently serializable world, and never escape the codec context.
struct TransportPhysicalCodecContext::State:CheckpointAllocatedObject,movement_physical_fields::BasisRestoreRead {
 const CommonStore& common;
 TransportPhysicalCodecRoots roots;
 TransportPhysicalCodecSelection selected;
 MovementRecordCodecSelection record_selection;
 ExactArithmeticBudget budget;
 MovementBasisDecodeAllocation* original_basis_allocation=nullptr;
 bool reading=false,travel_seen=false,group_seen=false,manifest_seen=false,finished=false;
 std::optional<TransportPhysicalSection> write_section;
 MovementPhysicalBasisIndex writing_bases;
 TransportPaymentPhysicalIndex writing_payments;
 OrderedTable<MovementBasis> decoded_bases;
 transport_physical_table::DecodedRows<MovementBasis> pending_bases;
 struct TerrainCostKey {StableId key;};
 CheckpointVector<TerrainCostKey> decoded_terrain_keys;
 struct CompositionChange {StableId key;std::variant<TransportRegistrationSource,TransportUnitReleaseSource> source;};
 CheckpointVector<CompositionChange> decoded_composition;
 CheckpointVector<TransportPaymentReceipt> decoded_payments;
 struct DecodedPaymentOwner {std::size_t payment_index=0;TransportPaymentPhysicalOwner owner=TransportPaymentPhysicalOwner::movement_unit;CheckpointVector<char> owner_key;};
 CheckpointVector<DecodedPaymentOwner> decoded_payment_owners;
 // A null root cannot acquire rows. These empty tables only provide a zero-row
 // shape to the same field function; they never substitute an optional state.
 OrderedTable<GroupTransportExpenseSource> absent_group_expenses;
 #define K4_PENDING(T,N) transport_physical_table::DecodedRows<T> N
 K4_PENDING(SpatialWriterBinding,spatial_bindings);
 K4_PENDING(MovementPrefixState,prefixes);K4_PENDING(MovementUnitState,units);
 K4_PENDING(TravelExertionBasis,effort_bases);K4_PENDING(MovementExpenseSource,expenses);
 K4_PENDING(MovementUnitCloseSource,closes);K4_PENDING(TravelExertionSource,exertions);
 K4_PENDING(MovementUnitBoundarySource,boundaries);K4_PENDING(MovementBasisTransitionSource,transitions);
 K4_PENDING(PersonalMovementControlSource,personal_controls);K4_PENDING(MovementArrivalSource,arrivals);
 K4_PENDING(PersonalMovementTerminalSource,personal_terminals);
 K4_PENDING(PatrolFinalMotionSource,patrol_final_motions);K4_PENDING(PatrolCheckpointArrival2,patrol_checkpoint_arrivals);
 K4_PENDING(TerrainCreditDispositionSource,credits);K4_PENDING(ExactMovementContact,contacts);
 K4_PENDING(MovementContactInstallationSource,installations);K4_PENDING(MovementContactContinuationSource,continuations);
 K4_PENDING(PassageModifierAdoption,modifiers);
 K4_PENDING(CargoHandlingTerms,handling_terms);K4_PENDING(CargoHandlingExecution,handling_executions);
 K4_PENDING(CargoHandlingWorkResult,handling_work);K4_PENDING(CargoHandlingResult,handling_results);
 K4_PENDING(GroupTransportExpenseSource,group_expenses);
 K4_PENDING(TransportUnitState,transport_units);K4_PENDING(CarrierPosition,positions);
 K4_PENDING(GroupMovementJourney,journeys);K4_PENDING(GroupMovementBinding,group_bindings);
 K4_PENDING(GroupJourneyStep,steps);K4_PENDING(GroupJourneyTerminal,terminals);
 K4_PENDING(JourneyPurposeBinding,purposes);K4_PENDING(GroupMovementOpeningSource,openings);
 K4_PENDING(GroupMovementUnitTerms,unit_terms);K4_PENDING(GroupMovementControlSource,group_controls);
 K4_PENDING(GroupMilitarySupplyRebindSource,military_supply_rebinds);
 K4_PENDING(InventorySpatialBinding,assignments);K4_PENDING(TransportRegistrationSource,registrations);
 K4_PENDING(TransportUnitReleaseSource,releases);K4_PENDING(BoundaryCrossingReceipt,crossings);
 K4_PENDING(TransportEntryManifestSource,entry_manifests);
 #undef K4_PENDING
 State(const CommonStore&,TransportPhysicalCodecRoots,const TransportPhysicalCodecSelection&,bool,ExactArithmeticBudget,MovementBasisDecodeAllocation*);
 Outcome<MovementBasis> copy_original_basis(const StableId&)override;
 Status adopt_travel_rows();
 Status adopt_group_rows();
 Status adopt_manifest_rows();

};
namespace transport_decoded_transfer {
template<>struct OriginalRow<TransportPhysicalCodecContext::State::CompositionChange>:std::true_type {};
}
namespace transport_section_fields {
using State=TransportPhysicalCodecContext::State;
using movement_record_fields::field;
using movement_record_fields::value;
template<class C,class T>Status simple(C& c,const StableId&,T& row,State& state){return value(c,row,state.record_selection);}
template<class C,class T,class F>Status all(C& c,const StableId& name,OrderedTable<T>& table,transport_physical_table::DecodedRows<T>& decoded,F visit){return transport_physical_table::all(c,name,table,decoded,visit);}
// New physical arrays retain original table allocation owners before borrowing
// rows. Nested arrays share the same pending collection across all parents.
template<class C,class T,class F,class Filter>Status children(C& c,const StableId& name,OrderedTable<T>& table,transport_physical_table::DecodedRows<T>& decoded,F visit,Filter filter){return transport_physical_table::selected(c,name,table,decoded,visit,filter);}
}
}
