// v1e §2.5：仅注册原 family_personal_decision/1
// 身体用途，不代来源、控制或收知。
#pragma once
#include "authored_origin.hpp"
namespace hanmo::resident {
// 真实 director_family_autonomy/1 已选且原 character_expression binding
// 完整才可采用。 安装仍须同 Prepared 的 Family/Director/Condition 完整 required
// 门，不由本函数替代。
Status
register_original_family_personal_decision_purpose(ActorFoundationCatalog &);
} // namespace hanmo::resident
