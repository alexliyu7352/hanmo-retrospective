// 新profile逐字段检查点不复活驱动许可，严格版本和引用校验在发布前完成。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "plan_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
namespace hanmo::resident {
namespace {
// 可选字段保留存在位，空ID由形状validator拒绝。
template<class T>Status optional(CodecVisitor& c,const StableId& name,std::optional<T>& value){return c.optional(name,value,[](auto& row,auto& item){return row.field("value",item);});}
// 在分配前先核有限项数，损坏存档不能制造超长计划或路线。
template<class T,class F>Status bounded(CodecVisitor& c,const StableId& name,std::vector<T>& values,Integer maximum,F visit){return c.object(name,[&](auto& row){Integer count=static_cast<Integer>(values.size());row.field("count",count);if(!row.status().ok)return row.status();if(count<0||count>maximum)return row.reject("PlayerPlanCollectionLimit",name);if(row.reading())values.resize(static_cast<std::size_t>(count));for(auto& value:values){auto result=row.object("entry",[&](auto& entry){return visit(entry,value);});if(!result.ok)return result;}return row.status();});}
// 上下文只遍历本profile允许的普通场所三元组。
Status context(CodecVisitor& c,InteractionContext& value){c.field("place_id",value.place_id);c.field("provider_actor_id",value.provider_actor_id);return c.field("role_id",value.role_id);}
// 报价只保存稳定条款；动态资格和钱包仍从当前世界重新读取。
Status terms(CodecVisitor& c,PlayerPlanTerms& value){
    c.field("provider_actor_id",value.provider_actor_id);
    c.field("role_id",value.role_id);
    c.field("place_id",value.place_id);
    c.field("payer_wallet_id",value.payer_wallet_id);
    c.field("recipient_wallet_id",value.recipient_wallet_id);
    c.field("mode",value.mode);
    c.field("skill_id",value.skill_id);
    c.field("qualification_key",value.qualification_key);
    c.field("challenge_kind",value.challenge_kind);
    c.field("challenge_id",value.challenge_id);
    c.field("challenge_mode",value.challenge_mode);
    c.field("duration",value.duration);
    c.field("cost",value.cost);
    c.field("wage",value.wage);
    c.field("hp_gain",value.hp_gain);
    c.field("fatigue_delta",value.fatigue_delta);
    c.field("xp_limit",value.xp_limit);
    c.field("target_level",value.target_level);
    optional(c,"settlement_policy",value.settlement_policy);
    optional(c,"challenge_source_sha256",value.challenge_source_sha256);
    return c.status();
}
// variant编号固定七种动作，新增任意命令kind均明确拒绝。
Status payload(CodecVisitor& c,PlayerPlanStepData& data){Integer kind=data.index();c.field("kind",kind);if(kind<0||kind>6)return c.reject("UnsupportedPlayerPlanStep");if(c.reading()){switch(kind){case 0:data=PlayerPlanMove{};break;case 1:data=PlayerPlanTravel{};break;case 2:data=PlayerPlanWork{};break;case 3:data=PlayerPlanCourse{};break;case 4:data=PlayerPlanRest{};break;case 5:data=PlayerPlanContinue{};break;case 6:data=PlayerPlanTaskReminder{};break;}}
    return std::visit([&](auto& value)->Status{using T=std::decay_t<decltype(value)>;
    if constexpr(std::is_same_v<T,PlayerPlanMove>)return c.field("place_id",value.place_id);
    else if constexpr(std::is_same_v<T,PlayerPlanTravel>){auto& request=value.request;Integer target=request.destination.index();c.field("destination_kind",target);if(target<0||target>1)return c.reject("InvalidPlayerPlanDestination");if(c.reading()){if(target==0)request.destination=StableId{};else request.destination=PointI{};}if(target==0)c.field("destination",std::get<StableId>(request.destination));else{auto& point=std::get<PointI>(request.destination);c.field("x",point.x);c.field("y",point.y);}c.field("has_via",request.has_via);return bounded(c,"via",request.via,player_plan_max_steps,[](auto& row,auto& id){return row.field("id",id);});}
    else if constexpr(std::is_same_v<T,PlayerPlanWork>)return c.object("context",[&](auto& row){return context(row,value.context);});
    else if constexpr(std::is_same_v<T,PlayerPlanCourse>){c.object("context",[&](auto& row){return context(row,value.context);});c.field("skill_id",value.skill_id);c.field("ticks",value.ticks);return c.field("challenge_mode",value.challenge_mode);}
    else if constexpr(std::is_same_v<T,PlayerPlanRest>){c.field("estate_id",value.estate_id);return c.field("duration",value.duration);}
    else if constexpr(std::is_same_v<T,PlayerPlanContinue>)return c.field("action_id",value.action_id);
    else {c.field("task_id",value.task_id);return c.field("place_id",value.place_id);}
},data);}
// 边界的存在位全部保存；缺完成回执的活动行动仍必须由原领域继续。
Status step(CodecVisitor& c,PlayerPlanStep& value){c.field("step_id",value.id);c.object("params",[&](auto& row){return payload(row,value.data);});Integer source_kind=static_cast<Integer>(value.source.kind);c.field("source_kind",source_kind);if(source_kind<0||source_kind>4)return c.reject("InvalidPlayerPlanSource");if(c.reading())value.source.kind=static_cast<PlayerPlanSourceKind>(source_kind);c.field("source_id",value.source.source_id);c.field("observed_tick",value.source.observed_tick);c.field("authorized",value.authorized);c.optional("terms",value.terms,terms);Integer phase=static_cast<Integer>(value.phase);c.field("phase",phase);if(phase<0||phase>4)return c.reject("InvalidPlayerPlanPhase");if(c.reading())value.phase=static_cast<PlayerPlanStepPhase>(phase);
    optional(c,"action_id",value.action_id);
    optional(c,"start_result_id",value.start_result_id);
    optional(c,"completion_result_id",value.completion_result_id);
    optional(c,"started_tick",value.started_tick);
    optional(c,"completed_tick",value.completed_tick);
    return c.field("stop_reason",value.stop_reason);
}
// 保存的是上一明确批准批次的范围和实际消耗，没有enabled/run/permit位。
Status budget(CodecVisitor& c,PlayerPlanBudget& value,Integer version){c.field("max_additional_ticks",value.max_additional_ticks);c.field("max_gross_spend",value.max_gross_spend);c.field("used_ticks",value.used_ticks);c.field("gross_spend",value.gross_spend);c.field("approved_tick",value.approved_tick);if(version==2){if(c.reading())value.exact_used_duration.emplace();if(!value.exact_used_duration)return c.reject("MissingPlayerPlanExactBudget");return c.object("exact_used_duration",[&](auto& row){return visit_half_day_duration2(row,*value.exact_used_duration);});}if(c.reading())value.exact_used_duration.reset();return c.status();}
}
// 完整typed遍历后再核形状，非法版本或边界不安装为当前计划。
Status visit_player_plan_checkpoint(CodecVisitor& c,PlayerPlanState& state){c.field("version",state.version);if(state.version!=1&&state.version!=2)return c.reject("PlayerPlanVersionMismatch");c.optional("current",state.current,[&](auto& row,auto& plan){row.field("plan_id",plan.id);row.field("actor_id",plan.actor_id);row.field("created_tick",plan.created_tick);bounded(row,"steps",plan.steps,player_plan_max_steps,step);row.field("stop_reason",plan.stop_reason);row.optional("budget",plan.budget,[&](auto& field,auto& value){return budget(field,value,state.version);});if(state.version==2)row.field("approved_scope_revision",plan.approved_scope_revision);else if(row.reading())plan.approved_scope_revision=0;return row.status();});if(!c.status().ok)return c.status();auto checked=validate_player_plan_shape(state);if(!checked.ok)return c.reject(checked.error.original.code,checked.error.original.path);return c.status();}
} // 读档暂停由中央在已核世界安装时设置，本codec不改变保存的实际步骤事实。
