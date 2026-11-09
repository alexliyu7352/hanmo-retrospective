// 派生引用只能附着原合法账户页，不能成为任意工资/家庭查询入口。
#pragma once
#include "resident/domains/local_economy/views/local_economy_view.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
namespace hanmo::resident {
// 完整验证后于候选最终身份重建，派生索引不写入银行。
void bind_cohort_wage_need_references(LocalEconomyIndex&,const AffairsState&);
}
