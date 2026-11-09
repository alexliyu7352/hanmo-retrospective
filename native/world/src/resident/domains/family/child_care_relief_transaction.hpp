// 独立调用保持原址状态，预约与资格回调始终观察同一对象；外层World候选不嵌套保存点。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident::care_relief_detail {
template<class Write> auto with_savepoint(CommonStore& common,ChildCareReliefState& state,Write write)->decltype(write()){
    using Result=decltype(write());auto mark=common.begin_savepoint();if(!mark.ok)return Result{false,{},mark.error};auto original=state;
    auto fail=[&](const RuleError& error)->Result{state=std::move(original);auto rolled=common.rollback_savepoint(mark.value);return {false,{},rolled.ok?error:rolled.error};};
    auto result=write();if(!result.ok)return fail(result.error);auto committed=common.commit_savepoint(mark.value);if(!committed.ok)return fail(committed.error);return result;
}
}
