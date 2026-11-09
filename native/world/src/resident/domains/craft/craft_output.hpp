// 一次真实定质与实际入库分开：无空间只保原WIP，成功输出才实际关闭行动。
#pragma once
#include "craft_action_time.hpp"
namespace hanmo::resident {
class NativeCraftItemOutputRead final:public ItemOutputSourceRead {
 const CraftJobReadContext&x_;const ItemOutputSourceRead* other_;
public:
 explicit NativeCraftItemOutputRead(const CraftJobReadContext&x,const ItemOutputSourceRead*other=nullptr):x_(x),other_(other){}
 Outcome<ItemOutputSourceView> read_item_output(const ItemOutputKey&,ItemOutputKind)const override;
 Outcome<ItemTerminationSourceView> read_item_termination(const SourceRef&,Integer,Integer,const StableId&)const override;
};
// Historical creation only. This borrows the actual completed output and its
// original adopted owner/destination, never today's lot or container Owner.
// It does not authorize a later transfer or prove unchanged current title.
struct CraftOriginalItemCreation {
 const CraftJob* job=nullptr;
 const CraftCompletionResult* completion=nullptr;
 const ItemOutputAdoption* adoption=nullptr;
 const ItemInstance* instance=nullptr;
 const Owner* original_owner=nullptr;
 const StableId* original_inventory_id=nullptr;
};
Outcome<CraftOriginalItemCreation> read_original_craft_item_creation(
 const CraftJobReadContext&,const StableId& instance_id);
Outcome<bool> read_craft_output_ready(const CraftJobReadContext&,const StableId& job_id);
Outcome<std::optional<StableId>> materialize_craft_output_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,ItemInstancesState&,LotPropertiesState&,const CraftJobWriteContext&,const ItemOutputSourceRead&,const StableId& job_id);
}
