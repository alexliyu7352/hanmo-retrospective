// 静态任务真实货物实交和返还的独立凭据；旧Task和Task2现金段的字节不追加字段。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include <map>
namespace hanmo::resident {
inline constexpr const char* task_static_asset_fact_kind="task_static_asset_transfer";
// 每腿对应原TransferLots实际拆出的一个新批次，保原产权和预约，不形成第二库存。
struct TaskStaticAssetLeg {
    StableId item_id,source_lot_id,destination_lot_id;
    Owner source_owner,destination_owner;
    ReservationRef reservation;
    Integer quantity=0,source_quantity_before=0,source_quantity_after=0;
};
// 终态仍留有的Task产权逐批冻结；后来才清空不能抹去当时未结的货物责任。
struct TaskStaticAssetRemainder {
    StableId inventory_id,lot_id,item_id;
    ReservationRef reservation;
    Integer quantity=0;
};
struct TaskStaticAssetReceipt {
    StableId fact_id,task_id,offer_id,assignee_actor_id,stage,source_action_id;
    Owner original_issuer,from_container_owner,to_container_owner;
    StableId from_inventory_id,to_inventory_id;
    Integer tick=0;
    std::vector<TaskStaticAssetLeg> legs;
    std::vector<TaskStaticAssetRemainder> retained_task_goods;
};
// 只索引本人承包的实交；委托人身份不自动产生远方办理知识，索引不编码。
struct TaskStaticAssetState {Integer version=1;OrderedTable<TaskStaticAssetReceipt> receipts;CheckpointMap<StableId,std::vector<StableId>> observed_by;};
// 从原收据重建派生索引，冷档不得输入第二份可伪造的知情表。
void rebuild_task_static_asset_index(TaskStaticAssetState&);
// 显式段严格同序JSON/typed编码；无原收据的旧终态不能由解码器补写历史。
Status visit_task_static_assets_checkpoint(CodecVisitor&,TaskStaticAssetState&);
BoundaryValue encode_task_static_assets(const TaskStaticAssetState&);
Outcome<TaskStaticAssetState> decode_task_static_assets(const BoundaryValue&);
}
