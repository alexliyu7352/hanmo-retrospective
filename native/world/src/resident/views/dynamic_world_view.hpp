// 动态场所与合同页面只投影当前候选的有限可知值，不观察新知识、不推进时间或取随机。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
class EconomicSupplyPolicyRead;
struct OrganizationContractCommand;
// 组织动作仅在中央同源只读报价通过时输出；未绑定报价器的领域调用不公开可点击元数据。
Outcome<BoundaryValue> query_dynamic_place(const WorldRead&,const PreparedCatalog&,const OrganizationRead&,const StableId& observer,const StableId& place,Integer cursor,const std::function<bool(const OrganizationContractCommand&)>& can_accept_organization={},const EconomicSupplyPolicyRead* issued_supply=nullptr);
Outcome<BoundaryValue> query_dynamic_contract(const WorldRead&,const PreparedCatalog&,const OrganizationRead&,const StableId& observer,const StableId& task_id);
} // 页面游标是固定64行分页边界；越界、非分页边界和无权观察均明确拒绝。
