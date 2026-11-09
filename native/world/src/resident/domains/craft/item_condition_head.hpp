// Immutable adoption of the original S9 condition chain, never another balance.
#pragma once
#include "item_condition_contract.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
// Current-head output owns only its actual copied fields. This ephemeral
// envelope is not another Item state, saved head row or authority source.
class OwnedItemConditionHead {
 CheckpointCopyLease memory_;
 ItemConditionHead value_;
public:
 OwnedItemConditionHead()=default;
 explicit OwnedItemConditionHead(const ItemConditionHeadBuildView&);
 OwnedItemConditionHead(const OwnedItemConditionHead&)=delete;
 OwnedItemConditionHead&operator=(const OwnedItemConditionHead&)=delete;
 OwnedItemConditionHead(OwnedItemConditionHead&&)noexcept;
 OwnedItemConditionHead&operator=(OwnedItemConditionHead&&)noexcept;
 void swap(OwnedItemConditionHead&)noexcept;
 const ItemConditionHead& value()const& noexcept{return value_;}
 const ItemConditionHead& value()const&&=delete;
};
struct ItemConditionAtHead {
 const ItemInstance* original_instance=nullptr;
 const ItemOutputAdoption* original_creation=nullptr;
 Integer instance_version=0,durability=0,max_durability=0;
};
// Current capture validates the original full instance history. Historical
// consumption validates only the adopted prefix, so a RepairCompleted reader
// cannot recurse into the condition result that consumes that same completion.
Outcome<OwnedItemConditionHead> read_current_item_condition_head(const ItemConditionReadContext&,const StableId&);
Outcome<ItemConditionAtHead> read_item_condition_at_head(const ItemConditionReadContext&,const ItemConditionHead&);
// Exact accepted Common boundary: includes every original condition source
// before that real header, excludes later same-M sources, and validates only
// the adopted prefix. Full condition coverage remains the publication gate.
Outcome<ItemConditionAtHead> read_item_condition_before_common_source(const ItemConditionReadContext&,const ItemConditionHead&,const SourceRef& actual_boundary,const WorldMoment&);
}
