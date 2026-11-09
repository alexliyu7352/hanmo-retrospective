// 历史组织合同授权核验只读原始任职、任职事实与冻结条款；不把当前持有人当成历史签发者。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
// 恢复与整候选验证检查同一历史grant覆盖范围、份额、委派和收款账户，不要求现任继续任职。
Status validate_organization_contract_authority(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const AffairsState&,const TaskState&);
} // 接单起点来自已持久NeedClaim并与冻结期限反算交叉验证，不增加第二份历史或资金状态。
