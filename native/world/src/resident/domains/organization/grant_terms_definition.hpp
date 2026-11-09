// 原 Grant 条款正文的纯校验；无 Office/资源存在检查，不授任何运行权。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
// 原作者初始 Grant 允许无 valid_until，保留旧种子的同一接受域和错误码。
Status validate_authored_grant_terms_definition(const Grant &);
// 原动态 issue/amend 要求有限窗口及原有列表上限/唯一性。
Status validate_runtime_grant_terms_definition(const Grant &, bool require_id);
} // namespace hanmo::resident
