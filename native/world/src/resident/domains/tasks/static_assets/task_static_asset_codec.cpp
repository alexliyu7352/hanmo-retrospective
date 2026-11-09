// 独立静态货物段不碰旧Tasks或Task2 visitor；每层重键、未知字段和版本严格拒绝。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_static_asset_state.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident { namespace {
// 预约的kind与ID都必须保全，不能将合同预约压成同名Action字符串。
template<class A>Status reservation(A& a,ReservationRef& value){dynamic_codec::enumeration(a,"kind",value.kind,2);return a.field("id",value.id);}
template<class A>Status leg(A& a,TaskStaticAssetLeg& value){a.field("item_id",value.item_id);a.field("source_lot_id",value.source_lot_id);a.field("destination_lot_id",value.destination_lot_id);a.object("source_owner",[&](auto& nested){return dynamic_codec::owner(nested,value.source_owner);});a.object("destination_owner",[&](auto& nested){return dynamic_codec::owner(nested,value.destination_owner);});a.object("reservation",[&](auto& nested){return reservation(nested,value.reservation);});a.field("quantity",value.quantity);a.field("source_quantity_before",value.source_quantity_before);return a.field("source_quantity_after",value.source_quantity_after);}
template<class A>Status remainder(A& a,TaskStaticAssetRemainder& value){a.field("inventory_id",value.inventory_id);a.field("lot_id",value.lot_id);a.field("item_id",value.item_id);a.object("reservation",[&](auto& nested){return reservation(nested,value.reservation);});return a.field("quantity",value.quantity);}
template<class A>Status receipt(A& a,TaskStaticAssetReceipt& value){a.field("fact_id",value.fact_id);a.field("task_id",value.task_id);a.field("offer_id",value.offer_id);a.field("assignee_actor_id",value.assignee_actor_id);a.field("stage",value.stage);a.field("source_action_id",value.source_action_id);a.object("original_issuer",[&](auto& nested){return dynamic_codec::owner(nested,value.original_issuer);});a.object("from_container_owner",[&](auto& nested){return dynamic_codec::owner(nested,value.from_container_owner);});a.object("to_container_owner",[&](auto& nested){return dynamic_codec::owner(nested,value.to_container_owner);});a.field("from_inventory_id",value.from_inventory_id);a.field("to_inventory_id",value.to_inventory_id);a.field("tick",value.tick);a.sequence("legs",value.legs,[](auto& nested,auto& item){return leg(nested,item);});return a.sequence("retained_task_goods",value.retained_task_goods,[](auto& nested,auto& item){return remainder(nested,item);});}
template<class A>Status state(A& a,TaskStaticAssetState& value){a.field("version",value.version);if(value.version!=1)return a.reject("UnsupportedTaskStaticAssetVersion");return dynamic_codec::table(a,"receipts",value.receipts,[](auto& nested,auto& item){return receipt(nested,item);});}
// first-match解析之前遍历所有层；坏档不能把另一个Owner藏在重复字段后面。
Status unique(const BoundaryValue& value,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("TaskStaticAssetTooDeep");if(const auto* dict=value.as<BoundaryValue::Dict>()){if(dict->size()>CodecVisitor::max_items)return common_failure("TaskStaticAssetTooLarge");std::set<StableId> seen;for(const auto& field:*dict){if(!seen.insert(field.first).second)return common_failure("DuplicateTaskStaticAssetField",field.first);auto checked=unique(field.second,depth+1);if(!checked.ok)return checked;}}if(const auto* array=value.as<BoundaryValue::Array>()){if(array->size()>CodecVisitor::max_items)return common_failure("TaskStaticAssetTooLarge");for(const auto& item:*array){auto checked=unique(item,depth+1);if(!checked.ok)return checked;}}return common_success();}
}
void rebuild_task_static_asset_index(TaskStaticAssetState& value){value.observed_by.clear();for(const auto& row:value.receipts.rows()){const auto& receipt=row.second;value.observed_by[receipt.assignee_actor_id].push_back(row.first);}}
Status visit_task_static_assets_checkpoint(CodecVisitor& codec,TaskStaticAssetState& value){auto checked=state(codec,value);if(checked.ok&&codec.reading())rebuild_task_static_asset_index(value);return checked;}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_task_static_assets_fields(ProjectionWriter& writer,const TaskStaticAssetState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskStaticAssetState&>(value));
}
BoundaryValue encode_task_static_assets(const TaskStaticAssetState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_task_static_assets_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Outcome<TaskStaticAssetState> decode_task_static_assets(const BoundaryValue& value){auto checked=unique(value);if(!checked.ok)return {false,{},checked.error};TaskStaticAssetState out;dynamic_codec::JsonArchive codec(value);checked=state(codec,out);if(checked.ok)checked=codec.finish();if(!checked.ok)return {false,{},checked.error};rebuild_task_static_asset_index(out);return {true,std::move(out),{}};}
}
