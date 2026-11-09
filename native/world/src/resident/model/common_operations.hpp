// 共同资源变更是有类型、按序执行的recipe；规则报价留在各域，所有资产仍落同一候选。
#pragma once
#include "common_model.hpp"
namespace hanmo::resident {
struct WalletDelta { StableId wallet_id;Integer delta=0;bool require_spendable=true; };
struct SetReservation { StableId wallet_id;ReservationRef action_id;Integer amount=0;bool erase=false; };
struct AddLot { StableId inventory_id,item_id;Integer quantity=0;Owner owner;ReservationRef reservation;std::optional<StableId> item_instance_id;std::optional<LotPropertiesRef> lot_properties_ref; };
struct ReserveLots { StableId inventory_id,item_id;Integer quantity=0;std::vector<Owner> owners;ReservationRef action_id; };
struct TransferLots { StableId source_id,destination_id,item_id;Integer quantity=0;std::vector<Owner> owners;ReservationRef reservation;std::optional<Owner> new_owner; };
struct ConsumeLots { StableId inventory_id,item_id;Integer quantity=0;std::vector<Owner> owners;ReservationRef reservation; };
struct ReleaseActionResources { StableId action_id; };
// 合同/组织活动只释放自己的预留种类；action收尾不触碰它们。
struct ReleaseReservationResources { ReservationRef reservation; };
struct CreateWallet { WalletView wallet; };
struct CreateInventory { InventoryView inventory; };
struct LotDelta { StableId lot_id;Integer delta=0; };
struct AdjustLots { StableId inventory_id;std::vector<LotDelta> lots;bool bump_revision=true; };
struct SetInventoryCapacity { StableId inventory_id;Integer capacity=0; };
// 家庭继任只转移已核定产权；金额、数量与任务来源仍由原领域规则决定。
struct SetWalletOwner { StableId wallet_id;Owner owner; };
struct SetInventoryOwner { StableId inventory_id;Owner owner; };
struct SetLotOwner { StableId inventory_id,lot_id;Owner owner;bool bump_revision=true; };
// 整编仅整体搬移已精确报价的无预留公产批次，保留原lot身份和元数据。
struct MoveIntegrationLot { StableId source_id,destination_id,lot_id;Integer expected_quantity=0;Owner expected_owner,new_owner; };
// X/1：精确选择按稳定输入序执行，整批转移保ID，部分拆分继承所有性质。
struct ReserveSelectedLots { StableId inventory_id;std::vector<CargoSelection> selections;std::vector<Owner> owners;ReservationRef reservation; };
struct TransferSelectedLots { StableId source_id,destination_id;std::vector<CargoSelection> selections;std::vector<Owner> owners;ReservationRef reservation;std::optional<Owner> new_owner; };
struct ConsumeSelectedLots { StableId inventory_id;std::vector<CargoSelection> selections;std::vector<Owner> owners;ReservationRef reservation; };
using ResourceOperation=std::variant<WalletDelta,SetReservation,AddLot,ReserveLots,TransferLots,ConsumeLots,ReleaseActionResources,CreateWallet,CreateInventory,SetInventoryCapacity,AdjustLots,SetWalletOwner,SetInventoryOwner,SetLotOwner,ReleaseReservationResources,MoveIntegrationLot,ReserveSelectedLots,TransferSelectedLots,ConsumeSelectedLots>;
struct ResourceRecipe { std::vector<ResourceOperation> operations;std::vector<ReadVersion> expected_versions; };
// Finite synchronous input ownership only. The sole original Common primitive
// still decides all release semantics and owns its independent target writes.
Status apply_original_action_resource_release_in_candidate(CommonWrite&,const StableId&);
// optional字段表示实际写入；零数值、false、空action_id均为有效更新，不能被当缺省。
struct ActorMutation { enum class LocationMode { replace,wilderness_point,route_progress,route_contact };StableId actor_id;std::optional<Location> location;std::optional<StableId> action_id;std::optional<bool> alive,available;std::optional<Integer> hp,fatigue,merit,injury;LocationMode location_mode=LocationMode::replace;std::optional<ActorSkills> skills{};std::optional<ActorCombat> combat{}; };
struct ActionMutation { enum class Kind { insert,patch,erase };Kind kind=Kind::patch;ActionCoreView action;std::optional<Integer> remaining_ticks;std::optional<StableId> action_kind;std::optional<std::vector<StableId>> participants{}; };
// id由调用者next_id分配或沿用action_id；append仅登记，不重复分配，也不复制业务payload。
struct FactDraft { StableId id,kind,domain; };
} // recipe错误必须丢弃整个recipe的改动；外围Txn仍是整世界唯一原子屏障。
