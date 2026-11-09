// 修缮冷史消费原资产前序和具名军籍历史；不从今天的船主、船员或经验倒填过去。
#pragma once
#include "naval_repair.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 每次观察只针对原 Common 中已有的开工/完工事实；外层按全 Naval 事件序调用。
Status validate_naval_repair_history_event(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const NavalAssetState& before,const StableId& kind,const StableId& source_id);
// Common全表只在冷验核本工单预留：不得夹带别的容器、钱袋或未冻结材料。
Status validate_naval_repair_reservations(const CommonStore&,const NavalState&);
// 原工单存在与否由真实开工头决定，已终止仍返回；完整 Naval3 历史必须先验通过。
Outcome<std::optional<NavalRepairActionSource>> query_naval_repair_action(const CommonStore&,const NavalState&,const NavalCatalog&,const StableId& action_id);
// 缓存只核可证明的原业务事实和当次Common实例区间；重放不借当前终态改写旧回执。
Status validate_naval_repair_cached_receipt(const CommonStore&,const NavalState&,const NavalCatalog&,const StableId& cache_key,const StableId& kind,const StableId& result_id,Integer result_tick,const NavalRepairReceipt&);
// 精确终态只读供原计划/对白验证成功或失败完成；必须在完整 Naval3 历史验证之后消费。
Outcome<NavalRepairReceipt> query_naval_repair_completion(const CommonStore&,const NavalState&,const NavalCatalog&,const StableId& result_id,const StableId& action_id,const StableId& actor_id);
}
