// 原个人开战隐含半日的中央短栈协调；未在完整Personal2条件配对前启用。
#pragma once
#include "resident/domains/growth/growth_personal_condition_advance_proof.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct AdvanceDetails;
// 实际调用唯一advance一次，只有其完整尾成功才返回凭据；不收外传tick或成功旗标。
// details仅成功后赋值，错误仍由原外层整个候选回滚；不替代原开战前后preview。
Outcome<std::unique_ptr<GrowthConditionDuelAdvanceProof>> advance_world_for_personal_condition(
    WorldBundle&,const std::vector<StableId>& hold,AdvanceDetails&);
}
