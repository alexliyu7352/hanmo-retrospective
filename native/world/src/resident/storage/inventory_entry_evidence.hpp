// Common原owner只在真实运输entry捕获。证据嵌S1原entry，不创建库存变动历史或当前资源副账。
#pragma once
#include "common_codec.hpp"
#include "resident/kernel/source_ref.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
struct ItemInstancesState;struct LotPropertiesState;struct ItemServiceCatalog;class ItemOutputSourceRead;class ProjectionWriter;class CodecVisitor;
struct InventoryEntryLotEvidence {
 StableId lot_id,item_id;Integer quantity=0,item_definition_weight=0;
 std::optional<StableId> item_instance_id;std::optional<LotPropertiesRef> lot_properties_ref;
};
struct InventoryEntryEvidence {
 Integer source_schema=1;SourceRef entry_source;StableId inventory_id;Integer inventory_revision=0;
 WorldMoment occurred_at;WorldId world_id{};StableId item_catalog_digest;std::vector<InventoryEntryLotEvidence> lots;
};
// 受信S1原writer从本次真实movement entry及实际manifest产生；不是命令或调用者输入数量。
struct InventoryEntryAdoptionScope {SourceRef entry_source;WorldMoment occurred_at;WorldId world_id{};std::vector<StableId> actual_inventory_ids;};
class InventoryEntryOriginalSourceRead {public:virtual ~InventoryEntryOriginalSourceRead()=default;
 virtual Outcome<InventoryEntryAdoptionScope> read_original_inventory_entry_scope(const SourceRef&)const=0;
};
struct InventoryEntryItemSources {const ItemInstancesState& instances;const LotPropertiesState& properties;const ItemServiceCatalog& catalog;const ItemOutputSourceRead& outputs;};
Outcome<InventoryEntryEvidence> capture_inventory_entry_evidence(const CommonStore&,const CommonCatalog&,const InventoryEntryAdoptionScope&,const StableId& inventory_id,Integer expected_revision,const InventoryEntryItemSources*);
// 冷校验从原entry反读独立几何/ordinal/主体及实际接受库存范围；不拿今天的lot或余额回填过去。
Status validate_inventory_entry_evidence(const CommonStore&,const CommonCatalog&,const InventoryEntryEvidence&,const InventoryEntryOriginalSourceRead&,const InventoryEntryItemSources*);
Outcome<Integer> inventory_entry_evidence_weight(const InventoryEntryEvidence&);
BoundaryValue encode_inventory_entry_evidence(const InventoryEntryEvidence&,const CommonResourceCodecContext&);
Outcome<InventoryEntryEvidence> decode_inventory_entry_evidence(const BoundaryValue&,const CommonResourceCodecContext&);
Status emit_inventory_entry_evidence_fields(ProjectionWriter&,const InventoryEntryEvidence&,const CommonResourceCodecContext&);
Status emit_inventory_entry_evidence(ProjectionWriter&,const StableId& name,const InventoryEntryEvidence&,const CommonResourceCodecContext&);
Status visit_inventory_entry_evidence(CodecVisitor&,InventoryEntryEvidence&);
}
