// S7 domain alias only; Common owns the one original transfer implementation.
#pragma once
#include "resident/storage/unique_item_transfer.hpp"
namespace hanmo::resident::repair_service_detail {
using AppliedRepairItemTransfer=AppliedUniqueItemTransfer;
// This adapter grants no acceptance/title/custody permission. Those original
// Repair sources must already be verified by the actual business writer.
Outcome<AppliedRepairItemTransfer> apply_original_repair_item_transfer(
 CommonStore&,const CargoSelection& actual_selection,const StableId& actual_destination,
 const StableId& instance_id,const Owner& actual_legal_owner,const ReservationRef& actual_reservation);
}
