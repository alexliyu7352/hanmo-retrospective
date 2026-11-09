// 两个S9段独立且成对存读；冷读只恢复原来源，不物化或补品质。
#pragma once
#include "item_service.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class ProjectionWriter;
BoundaryValue encode_item_instances(const ItemInstancesState&,Integer selected_version=1);
Outcome<ItemInstancesState> decode_item_instances(const BoundaryValue&,Integer selected_version=1);
Status visit_item_instances(CodecVisitor&,ItemInstancesState&,Integer selected_version=1);
BoundaryValue encode_item_condition_result(const ItemConditionResultView&);
Outcome<ItemConditionResultView> decode_item_condition_result(const BoundaryValue&);
Status visit_item_condition_result(CodecVisitor&,ItemConditionResultView&);
Status emit_item_condition_result(ProjectionWriter&,const ItemConditionResultView&);
BoundaryValue encode_lot_properties(const LotPropertiesState&);
Outcome<LotPropertiesState> decode_lot_properties(const BoundaryValue&);
Status visit_lot_properties(CodecVisitor&,LotPropertiesState&);
Outcome<ItemServiceCatalog> decode_item_service_catalog(const BoundaryValue&);
Outcome<ItemServiceCatalog> decode_item_service_catalog(const BoundaryValue&,const CommonCatalog&);
BoundaryValue encode_item_service_catalog(const ItemServiceCatalog&);
Status visit_item_service_catalog(CodecVisitor&,ItemServiceCatalog&);
Status emit_item_instances(ProjectionWriter&,const ItemInstancesState&,Integer selected_version=1);
Status emit_lot_properties(ProjectionWriter&,const LotPropertiesState&);
Status emit_item_service_catalog(ProjectionWriter&,const ItemServiceCatalog&);
}
