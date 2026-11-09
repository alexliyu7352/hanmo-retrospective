// 条件模型只保存作者类型；读取端口借用真实社会/任务状态，不持第二份权威数据。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
struct Condition {
    enum class Kind { flag,party_member,actor_at,task_state,relation_min,all,any,offer_declined_before,treaty_personal_passage };
    Kind kind=Kind::all;
    StableId flag,actor_id,place_id,offer_id,state,from_actor,to_actor;
    bool equals=false; Integer trust=0; std::vector<Condition> terms;
};
// 三个回调只能纯读取当前候选；任务可重试判定由其唯一owner实现。
struct ConditionAccess {
    std::function<Location(const StableId&)> resolve_location;
    std::function<bool(const StableId&,const StableId&)> party_contains;
    std::function<StableId(const StableId&,const StableId&)> task_status;
    std::function<std::optional<Integer>(const StableId&,const StableId&)> relation_trust;
    // 仅当前真实会面接收人的本人回应历史；读取错误不能降成false。
    std::function<Outcome<bool>(const StableId&,const StableId&)> offer_declined_before;
    std::function<Outcome<bool>(const StableId&,const StableId&,const StableId&)> treaty_personal_passage;
};
struct InteractionState;
Outcome<bool> evaluate_condition(const CommonRead&,const InteractionState&,const ConditionAccess&,const StableId&,const Condition&);
Outcome<bool> all_conditions(const CommonRead&,const InteractionState&,const ConditionAccess&,const StableId&,const std::vector<Condition>&);
Status visit_condition_checkpoint(CodecVisitor&,Condition&);
}
