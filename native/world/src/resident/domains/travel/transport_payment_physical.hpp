// Physical placement only. The one runtime S5 collection remains the owner of
// payment records; final V has no independent payment section.
#pragma once
#include "group_movement.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
enum class TransportPaymentPhysicalOwner {movement_unit,boundary_crossing,handling_terms,handling_work_result};
struct TransportPaymentPhysicalBinding {
 const TransportPaymentReceipt* payment=nullptr;
 TransportPaymentPhysicalOwner owner=TransportPaymentPhysicalOwner::movement_unit;
 const StableId* owner_id=nullptr;
};
struct TransportPaymentPhysicalIndex {
 CheckpointVector<TransportPaymentPhysicalBinding> bindings;
 const TransportPaymentPhysicalBinding* find_payment(const StableId&)const noexcept;
};
// Requires actual immutable source/cold validation before publication; this
// additionally proves every original receipt has exactly one physical owner.
// All views borrow the same frozen candidate. No amount/time matching is used
// to invent a missing source reference.
Outcome<TransportPaymentPhysicalIndex> index_transport_payment_physical_owners(const CommonStore&,const MovementExactState&,const SpatialCarrierState*,const GroupMovementExecutionState*,const EconomicTransportExpenseState&,ExactArithmeticBudget={});
}
