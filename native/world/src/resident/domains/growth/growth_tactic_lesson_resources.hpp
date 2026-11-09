// 战法教学只操作本次原Action预约的私人学费；不接公共军资，不以新账余额代替Common钱包。
#pragma once
#include "growth_tactic_lesson_codec.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonPayment {ResourceRecipe recipe;Integer provided_half_days=0,fee_paid=0,fee_remaining=0;};
// 检查所有钱包/库存，拒冒用同一Action预约到别处；关闭后只能为零，无Action也不能残留钱货。
Status validate_growth_tactic_lesson_reservation(const CommonStore&,const GrowthTacticLessonState&,const StableId& started_fact_id,Integer expected_remaining);
Outcome<ResourceRecipe> reserve_growth_tactic_lesson_recipe(const CommonStore&,const GrowthTacticLessonState&,const StableId& started_fact_id);
// 根据原已经记录的课时求唯一下一期；调用者不得传一个任意付款额或重复序号。
Outcome<GrowthTacticLessonPayment> pay_growth_tactic_lesson_recipe(const CommonStore&,const GrowthTacticLessonState&,const StableId& started_fact_id);
Outcome<GrowthTacticLessonPayment> release_growth_tactic_lesson_recipe(const CommonStore&,const GrowthTacticLessonState&,const StableId& started_fact_id);
}
