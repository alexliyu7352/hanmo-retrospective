// 本人手记只投影原授权和真实收付款；金额由本人另行输入，不以最大可付额替本人决定。
#pragma once
#include "task_personal_advance.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_personal_advances(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalAdvanceState&,Integer cursor=0);
}
