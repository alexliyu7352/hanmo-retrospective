// 私人雇用只是原S5合同的本人办理投影；不登记第二份Task、工资或接受事实。
#pragma once
#include "resident/domains/estate/employment/estate_employment.hpp"
namespace hanmo::resident {
using TaskEmploymentPreview=std::function<Outcome<EstateEmploymentQuote>(const StableId&,const EstateEmploymentCommand&)>;
// 两个游标分别限本人合同和当前会面工种；调用者提供同一候选的纯读及原生业务报价。
Outcome<BoundaryValue> query_task_employment(const CommonStore&,const CommonCatalog&,
    const InteractionState&,const EstateEmploymentRead&,const TaskEmploymentPreview&,
    Integer cursor=0,Integer policy_cursor=0);
}
