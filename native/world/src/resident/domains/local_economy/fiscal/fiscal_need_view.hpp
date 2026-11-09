// Need引用只附在原财政合法页；没有任意need_id/fact_id前台查询入口。
#pragma once
#include "resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
namespace hanmo::resident {
// 完整财政/Need校验后，在候选最终身份重建时调用；只派生ID与状态，不保存冷态。
void bind_fiscal_need_references(FiscalSettlementIndex&,const AffairsState&);
}
