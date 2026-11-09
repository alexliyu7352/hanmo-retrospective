// 本人回应profile只由所选原生情景启用；组合包能力并集不能悄悄改写旧档布局。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct TaskOfferResponseState;
class CommonStore;
// 纯读选定情景，不补状态、不修改作者字段；未知profile和缺失原生基础均拒绝。
Outcome<bool> decode_task_offer_response_profile(const BoundaryValue& selected_scenario,const std::vector<StableId>& required_capabilities);
// 新调用方显式保版本；旧bool入口不将v2误登记为v1，全部既有能力版本仍由原总合同核准。
Outcome<Integer> decode_task_offer_response_profile_version(const BoundaryValue&,const std::vector<StableId>& required_capabilities);
// 初始化与冷恢复共用同一存在性门：profile、唯一marker及独立域必须同时一致。
Status validate_task_offer_response_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& module_versions,const TaskOfferResponseState* state);
// 未启用回应时不能携带该命令的旧回执或历史；不改变共同缓存/历史的原编码与其它kind。
Status validate_task_offer_response_history(const CommonStore&,bool enabled);
Status validate_task_offer_response_version_layout(Integer registered_version,const std::vector<std::pair<StableId,StableId>>&,const TaskOfferResponseState*);
Status validate_task_offer_response_version_history(const CommonStore&,Integer registered_version);
}
