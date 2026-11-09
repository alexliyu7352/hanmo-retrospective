#pragma once
#include "military_formation_reservation_model.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct ArmyFormationTerms;
class MilitaryFormationReservationCapture {
 CheckpointMemoryCharge opening_memory_{0};
 const CommonStore* common_;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_;
 std::size_t first_;
 Integer selected_schema_=1;
 bool opening_captured_=false;
 std::optional<InventoryView> empty_inventory_opening_;
 std::optional<MilitaryFormationReservations1> effect_;
public:
 explicit MilitaryFormationReservationCapture(const CommonStore&,Integer selected_schema=1);
 MilitaryFormationReservationCapture(const MilitaryFormationReservationCapture&)=delete;
 MilitaryFormationReservationCapture(MilitaryFormationReservationCapture&&)=default;
 MilitaryFormationReservationCapture& operator=(MilitaryFormationReservationCapture&&)=delete;
 // Original reserve writer: after preview, before its first write.
 Status capture_original_opening(const CommonWrite&,const ArmyFormationTerms&);
 Status capture_original_reservation(const CommonWrite&);
 const std::optional<MilitaryFormationReservations1>& effect()const noexcept{return effect_;}
};
Outcome<std::optional<MilitaryFormationReservationCapture>> begin_military_formation_reservation_capture(const CommonStore&,Integer selected_schema);
struct MilitaryCatalog;struct DynamicMilitaryActionFact2;
// Proves the original accepted recipe/effects and their own original frames.
// Does not replace independent roster, body, authority, spatial or service reads.
Status validate_original_formation_acceptance_reservations(const MilitaryCatalog&,const DynamicMilitaryActionFact2&,Integer selected_schema);
}
