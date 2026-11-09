// 原Birth可先于完整Relief史验证；这里只按本域具名接受/关闭前缀读互斥，绝不回调World或Birth。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident {
// 非空边界只允许真实原family_child_care_started；消费方已核该start.payload.tick。
Status validate_original_care_not_replaced(const CommonStore&,const ChildCareReliefState&,const StableId& child_actor_id,Integer tick,const StableId& original_start_fact={});
}
