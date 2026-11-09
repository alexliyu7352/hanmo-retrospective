// Actual original C05 acceptance reserve result, not a second resource ledger.
#pragma once
#include "military_action_resource_model.hpp"
namespace hanmo::resident {
struct MilitaryInventoryReservationEffect1 {
 ReserveLots request;InventoryView before,after;
 std::vector<ResourceLotFlowDraft> actual_flows;
 Integer next_instance_before=0,next_instance_after=0;
};
struct MilitaryInventoryReservationEffectBuildView {
 const ReserveLots& request;const InventoryView& before;const InventoryView& after;
 const std::vector<ResourceLotFlowDraft>& actual_flows;
 Integer next_instance_before,next_instance_after;
};
struct MilitaryFormationReservationsBuildView {
 const CheckpointVector<MilitaryInventoryReservationEffectBuildView>& inventory_reservations;
 MilitaryWalletReservationEffectBuildView wallet_reservation;
 const InventoryView* empty_inventory_opening=nullptr;
};
struct MilitaryFormationReservations1;


struct MilitaryFormationReservations1 {
 CheckpointMemoryCharge copied_fields;
 std::vector<MilitaryInventoryReservationEffect1> inventory_reservations;
 MilitaryWalletReservationEffect1 wallet_reservation;
 // /2 only: true accepted inventory when the original recipe reserves no goods.
 std::optional<InventoryView> empty_inventory_opening;
 MilitaryFormationReservations1()=default;
 explicit MilitaryFormationReservations1(const MilitaryFormationReservationsBuildView&);
 MilitaryFormationReservations1(const MilitaryFormationReservations1&);
 MilitaryFormationReservations1(MilitaryFormationReservations1&& v):MilitaryFormationReservations1(static_cast<const MilitaryFormationReservations1&>(v)){}
 MilitaryFormationReservations1& operator=(MilitaryFormationReservations1 v){swap(v);return *this;}
 void swap(MilitaryFormationReservations1&)noexcept;
};
Status validate_military_formation_reservations(const MilitaryFormationReservations1&);
}
