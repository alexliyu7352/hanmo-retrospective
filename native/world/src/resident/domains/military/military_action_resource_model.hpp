// Original successful C05/C07 result evidence. Never the enclosing hot draft.
#pragma once
#include "resident/storage/resource_execution_draft.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct MilitaryFifoTransferEffect1 {
 TransferLots request;InventoryView source_before,source_after,destination_before,destination_after;
 std::vector<ResourceLotFlowDraft> actual_flows;Integer next_instance_before=0,next_instance_after=0;
};
struct MilitaryWalletDeltaEffect1 {WalletDelta request;WalletView before,after;};
struct MilitaryWalletReservationEffect1 {SetReservation request;WalletView before,after;};
struct MilitaryWalletCreationEffect1 {CreateWallet request;WalletView after;};
struct MilitaryInventoryCreationEffect1 {CreateInventory request;InventoryView after;};
using MilitaryResourceOperationEffect1=std::variant<MilitaryFifoTransferEffect1,MilitaryWalletDeltaEffect1,MilitaryWalletReservationEffect1,MilitaryWalletCreationEffect1,MilitaryInventoryCreationEffect1>;
struct MilitaryFifoTransferEffectBuildView {
 const TransferLots& request;const InventoryView& source_before;const InventoryView& source_after;
 const InventoryView& destination_before;const InventoryView& destination_after;
 const std::vector<ResourceLotFlowDraft>& actual_flows;Integer next_instance_before,next_instance_after;
};
struct MilitaryWalletDeltaEffectBuildView {const WalletDelta& request;const WalletView& before;const WalletView& after;};
struct MilitaryWalletReservationEffectBuildView {const SetReservation& request;const WalletView& before;const WalletView& after;};
struct MilitaryWalletCreationEffectBuildView {const CreateWallet& request;const WalletView& after;};
struct MilitaryInventoryCreationEffectBuildView {const CreateInventory& request;const InventoryView& after;};
// Caller must reserve the actual fresh payload before this finite copy.
MilitaryWalletReservationEffect1 copy_military_wallet_reservation_effect(const MilitaryWalletReservationEffectBuildView&);
using MilitaryResourceOperationEffectBuildView=std::variant<MilitaryFifoTransferEffectBuildView,MilitaryWalletDeltaEffectBuildView,MilitaryWalletReservationEffectBuildView,MilitaryWalletCreationEffectBuildView,MilitaryInventoryCreationEffectBuildView>;
struct MilitaryReturnResourceOpening1 {InventoryView source_inventory;std::optional<InventoryView> destination_inventory;};
struct MilitaryReturnResourceOpeningBuildView {const InventoryView& source_inventory;const InventoryView* destination_inventory=nullptr;};
struct MilitaryActionResourceEffectBuildView {
 const CheckpointVector<MilitaryResourceOperationEffectBuildView>& operations;
 const std::optional<MilitaryReturnResourceOpening1>& return_opening;
};

struct MilitaryActionResourceEffect1;


struct MilitaryActionResourceEffect1 {
 CheckpointMemoryCharge copied_fields;
 std::vector<MilitaryResourceOperationEffect1> operations;
 std::optional<MilitaryReturnResourceOpening1> return_opening;
 MilitaryActionResourceEffect1()=default;
 explicit MilitaryActionResourceEffect1(const MilitaryActionResourceEffectBuildView&);
 MilitaryActionResourceEffect1(const MilitaryActionResourceEffect1&);
 MilitaryActionResourceEffect1(MilitaryActionResourceEffect1&& v):MilitaryActionResourceEffect1(static_cast<const MilitaryActionResourceEffect1&>(v)){}
 MilitaryActionResourceEffect1& operator=(MilitaryActionResourceEffect1 v){swap(v);return *this;}
 void swap(MilitaryActionResourceEffect1&)noexcept;
};
// Validates only these actual finite primitive effects, not title, service,
// original military recipe choice, occupation or authority at a past cut.
Status validate_military_action_resource_effect(const MilitaryActionResourceEffect1&);
}
