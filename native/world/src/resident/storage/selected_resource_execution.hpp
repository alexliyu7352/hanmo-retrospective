// X精确原语的纯capture核；逐全frame验证未选集保真，不重执行任何资源writer。
#pragma once
#include "resource_execution_draft.hpp"
#include <span>
namespace hanmo::resident {
struct SelectedUniqueItemTransferRead {
 const ResourceInventoryExecutionDraft* source=nullptr;
 const ResourceInventoryExecutionDraft* destination=nullptr;
 const ResourceLotFlowDraft* flow=nullptr;
 const LotView* original=nullptr;
 const LotView* delivered=nullptr;
};
// Shared sale/custody/return proof for one actual unique item. It preserves all
// unselected rows by borrowed comparison and never clones an inventory.
Outcome<SelectedUniqueItemTransferRead> read_selected_unique_item_transfer(
 const TransferSelectedLots&,const ResourceOperationExecutionDraft&);
// The legacy FIFO primitive creates new lot IDs, including whole-lot moves.
// This borrowed read proves its actual captured effects, not title, capacity,
// a historical source cut, or a selected unique-item transfer.
struct OriginalFifoLotTransferRead {
 const ResourceInventoryExecutionDraft* source=nullptr;
 const ResourceInventoryExecutionDraft* destination=nullptr;
 const std::vector<ResourceLotFlowDraft>* actual_flows=nullptr;
};
// Borrowed exact operation evidence. An explicitly versioned successful
// original result may own these values; this view owns no hot capture, world
// identity, recipe, operation index, allocation, or historical title claim.
struct OriginalFifoLotTransferEvidenceView {
 const InventoryView& source_before;
 const InventoryView& source_after;
 const InventoryView& destination_before;
 const InventoryView& destination_after;
 const std::vector<ResourceLotFlowDraft>& actual_flows;
 Integer next_instance_before=0,next_instance_after=0;
};
Status validate_original_fifo_lot_transfer(const TransferLots&,const OriginalFifoLotTransferEvidenceView&);
Status validate_original_wallet_delta(const WalletDelta&,const WalletView& before,const WalletView& after);
Status validate_original_wallet_reservation_change(const SetReservation&,const WalletView& before,const WalletView& after);
Status validate_original_wallet_creation(const CreateWallet&,const WalletView& after);
Status validate_original_inventory_creation(const CreateInventory&,const InventoryView& after);
Outcome<OriginalFifoLotTransferRead> read_original_fifo_lot_transfer(
 const TransferLots&,const ResourceOperationExecutionDraft&);
// The original C05 FIFO reservation uses a genuine nonempty reservation and
// live positive original lots. Quantity0 preserves its real revision increment.
// Selected Item/property lots require their original selected primitive instead.
struct OriginalFifoLotReservationEvidenceView {
 const InventoryView& before;
 const InventoryView& after;
 const std::vector<ResourceLotFlowDraft>& actual_flows;
 Integer next_instance_before=0,next_instance_after=0;
};
struct OriginalFifoLotReservationRead {
 const ResourceInventoryExecutionDraft* inventory=nullptr;
 const std::vector<ResourceLotFlowDraft>* actual_flows=nullptr;
};
Status validate_original_fifo_lot_reservation(const ReserveLots&,const OriginalFifoLotReservationEvidenceView&);
Outcome<OriginalFifoLotReservationRead> read_original_fifo_lot_reservation(
 const ReserveLots&,const ResourceOperationExecutionDraft&);
// Finite original companion primitives. These compare the actual captured
// frames only; the original cash recipe still owns payment policy and order.
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_delta(
 const WalletDelta&,const ResourceOperationExecutionDraft&);
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_reservation_change(
 const SetReservation&,const ResourceOperationExecutionDraft&);
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_creation(
 const CreateWallet&,const ResourceOperationExecutionDraft&);
Outcome<const ResourceInventoryExecutionDraft*> read_original_inventory_creation(
 const CreateInventory&,const ResourceOperationExecutionDraft&);
bool is_selected_resource_operation(const ResourceOperation&);
bool same_resource_lot_value(const LotView&,const LotView&);
bool same_original_wallet_frame(const WalletView&,const WalletView&);
bool same_original_inventory_frame(const InventoryView&,const InventoryView&);
struct CommonResourceCodecContext;
// 仅显式冷重建调用：值仍逐字段严核，空序只等于原Common writer的canonical序。
bool same_resource_lot_value_cold(const LotView&,const LotView&,const CommonResourceCodecContext&);
bool same_original_inventory_frame_cold(const InventoryView&,const InventoryView&,const CommonResourceCodecContext&);
// Verifies actual original rows/ordered flows by borrowed comparison. It does
// not clone/replay inventories or manufacture a second operation result.
// Full Item semantics, title, source binding and protected consumers stay with
// their original selected owners.
// 仅借实际原资源帧，热 draft 与明确持久效果共用同一核验正文。
// 不重建 draft/recipe，不分配副本，也不凭这份视图授予历史产权或付款许可。
struct SelectedResourceExecutionEvidenceView {
 std::span<const ResourceWalletExecutionDraft> wallets;
 std::span<const ResourceInventoryExecutionDraft> inventories;
 std::span<const ResourceLotFlowDraft> lot_flows;
 Integer next_instance_before=0,next_instance_after=0;
};
// 冷来源只借既有完整帧与原七字段流；不先构造拥有型 draft 来调用纯核。
struct SelectedInventoryEvidenceBorrow {
 const StableId& inventory_id;
 const InventoryView* before;
 const InventoryView& after;
};
struct SelectedLotFlowEvidenceBorrow {
 ResourceLotFlowDraft::Kind kind;
 const StableId& source_inventory_id;
 const StableId& source_lot_id;
 const StableId& destination_inventory_id;
 const StableId& destination_lot_id;
 const StableId& item_id;
 Integer quantity;
};
struct SelectedResourceEvidenceBorrow {
 std::span<const ResourceWalletExecutionDraft> wallets;
 std::span<const SelectedInventoryEvidenceBorrow> inventories;
 std::span<const SelectedLotFlowEvidenceBorrow> lot_flows;
 Integer next_instance_before=0,next_instance_after=0;
 // 热原语默认nullptr；冷调用者必须传实际锁定的Common资源布局，不能自行猜轴。
 const CommonResourceCodecContext* cold_resources=nullptr;
};
// 仅 TransferSelectedLots 的借用参数；保原 owner、预约与选择序，不扩其他原语。
struct SelectedTransferParametersBorrow {
 const StableId& source_id;
 const StableId& destination_id;
 std::span<const CargoSelection> selections;
 std::span<const Owner> owners;
 const ReservationRef& reservation;
 const Owner* new_owner=nullptr;
};
Status validate_selected_resource_evidence(const SelectedTransferParametersBorrow&,
                                          const SelectedResourceEvidenceBorrow&);
// 仅借原 ConsumeSelectedLots 参数，完整消耗核仍复用同一 selected 正文。
struct SelectedConsumeParametersBorrow {
 const StableId& inventory_id;
 std::span<const CargoSelection> selections;
 std::span<const Owner> owners;
 const ReservationRef& reservation;
};
Status validate_selected_resource_evidence(const SelectedConsumeParametersBorrow&,
                                          const SelectedResourceEvidenceBorrow&);
Status validate_selected_resource_evidence(const ReserveSelectedLots&,const SelectedResourceExecutionEvidenceView&);
Status validate_selected_resource_evidence(const TransferSelectedLots&,const SelectedResourceExecutionEvidenceView&);
Status validate_selected_resource_evidence(const ConsumeSelectedLots&,const SelectedResourceExecutionEvidenceView&);
Status validate_selected_resource_evidence(const ResourceOperation&,const SelectedResourceExecutionEvidenceView&);
Status validate_selected_resource_execution(const ResourceOperation&,const ResourceOperationExecutionDraft&);
}
