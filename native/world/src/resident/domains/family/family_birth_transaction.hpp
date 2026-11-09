// 生育域独立调用的事务护栏；World/NPC已有整个候选时显式使用in_candidate入口，不能嵌套保存点。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident::birth_detail {
// 原址恢复领域账，保证已绑定的预约和历史只读服务始终观察同一个对象。
template<class Write> auto with_savepoint(CommonStore& common,FamilyBirthState& state,Write write)->decltype(write()) {
    using Result=decltype(write());
    auto mark=common.begin_savepoint();if(!mark.ok)return Result{false,{},mark.error};
    auto original=state;
    auto fail=[&](const RuleError& error)->Result{state=std::move(original);auto rolled=common.rollback_savepoint(mark.value);return {false,{},rolled.ok?error:rolled.error};};
    auto result=write();if(!result.ok)return fail(result.error);
    auto committed=common.commit_savepoint(mark.value);if(!committed.ok)return fail(committed.error);
    return result;
}
}
