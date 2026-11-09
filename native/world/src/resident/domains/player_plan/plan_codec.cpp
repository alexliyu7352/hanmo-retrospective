// 独立计划冷codec逐字段解码闭合类型，不允许任意命令或未知字段进入常驻状态。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "plan_codec.hpp"
#include "plan_lifecycle.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
namespace {
using namespace boundary;
// 本地交互只允许普通场所三元组，禁止路遇或叙事输入偷偷扩张权限。
InteractionContext context(const BoundaryValue& value){fields(value,{"place_id","provider_actor_id","role_id"},{},"plan/context");InteractionContext result;result.place_id=get_string(value,"place_id","plan/context");result.provider_actor_id=get_string(value,"provider_actor_id","plan/context");result.role_id=get_string(value,"role_id","plan/context");return result;}
// 固定三元组只输出原提供者与地点，不携带对白隐式权限。
Status emit_plan_context_fields(ProjectionWriter& w,const InteractionContext& value){w.field("place_id",value.place_id);w.field("provider_actor_id",value.provider_actor_id);return w.field("role_id",value.role_id);}
// 来源枚举固定版本内含义；不存在的kind不能按作者已知来源默认恢复。
PlayerPlanSource source(const BoundaryValue& value){fields(value,{"kind","source_id","observed_tick"},{},"plan/source");auto kind=get_int(value,"kind","plan/source");if(kind<0||kind>4)fail("InvalidPlayerPlanSource","plan/source");return {static_cast<PlayerPlanSourceKind>(kind),get_string(value,"source_id","plan/source"),get_int(value,"observed_tick","plan/source")};}
// 来源输出有限枚举和观察时刻，不包含远方私密状态。
Status emit_plan_source_fields(ProjectionWriter& w,const PlayerPlanSource& value){w.field("kind",static_cast<Integer>(value.kind));w.field("source_id",value.source_id);return w.field("observed_tick",value.observed_tick);}
// 同一原报价全部实质条款均参与保存；缺值不能用零静默补齐。
PlayerPlanTerms terms(const BoundaryValue& value){
    fields(value,{"provider_actor_id","role_id","place_id","payer_wallet_id","recipient_wallet_id","mode","skill_id","qualification_key","challenge_kind","challenge_id","challenge_mode","duration","cost","wage","hp_gain","fatigue_delta","xp_limit","target_level"},{"settlement_policy","challenge_source_sha256"},"plan/terms");PlayerPlanTerms result;
    result.provider_actor_id=get_string(value,"provider_actor_id","plan/terms");
    result.role_id=get_string(value,"role_id","plan/terms");
    result.place_id=get_string(value,"place_id","plan/terms");
    result.payer_wallet_id=get_string(value,"payer_wallet_id","plan/terms");
    result.recipient_wallet_id=get_string(value,"recipient_wallet_id","plan/terms");
    result.mode=get_string(value,"mode","plan/terms");
    result.skill_id=get_string(value,"skill_id","plan/terms");
    result.qualification_key=get_string(value,"qualification_key","plan/terms");
    result.challenge_kind=get_string(value,"challenge_kind","plan/terms");
    result.challenge_id=get_string(value,"challenge_id","plan/terms");
    result.challenge_mode=get_string(value,"challenge_mode","plan/terms");
    result.duration=get_int(value,"duration","plan/terms");
    result.cost=get_int(value,"cost","plan/terms");
    result.wage=get_int(value,"wage","plan/terms");
    result.hp_gain=get_int(value,"hp_gain","plan/terms");
    result.fatigue_delta=get_int(value,"fatigue_delta","plan/terms");
    result.xp_limit=get_int(value,"xp_limit","plan/terms");
    result.target_level=get_int(value,"target_level","plan/terms");
    if(auto* entry=value.find("settlement_policy"))result.settlement_policy=string(*entry,"plan/terms/settlement_policy");
    if(auto* entry=value.find("challenge_source_sha256"))result.challenge_source_sha256=string(*entry,"plan/terms/challenge_source_sha256");
    return result;
}
// 固定七种目标的参数分别解码，不把用户kind映射为任意原命令。
PlayerPlanStepData payload(const StableId& kind,const BoundaryValue& value){
    const StableId path="plan/params";
    if(kind=="move_local"){fields(value,{"place_id"},{},path);return PlayerPlanMove{get_string(value,"place_id",path)};}
    if(kind=="travel"){fields(value,{"destination"},{"via"},path);PlayerPlanTravel result;const auto& target=field(value,"destination",path);if(target.as<std::string>())result.request.destination=string(target,path);else{fields(target,{"x","y"},{},path+"/destination");result.request.destination=PointI{get_int(target,"x",path),get_int(target,"y",path)};}if(auto* via=value.find("via")){result.request.via=strings(*via,path+"/via");result.request.has_via=true;}return result;}
    if(kind=="work"){fields(value,{"context"},{},path);return PlayerPlanWork{context(field(value,"context",path))};}
    if(kind=="course"){fields(value,{"context","skill_id","ticks","challenge_mode"},{},path);return PlayerPlanCourse{context(field(value,"context",path)),get_string(value,"skill_id",path),get_int(value,"ticks",path),get_string(value,"challenge_mode",path)};}
    if(kind=="rest"){fields(value,{"estate_id","duration"},{},path);return PlayerPlanRest{get_string(value,"estate_id",path),get_int(value,"duration",path)};}
    if(kind=="continue_action"){fields(value,{"action_id"},{},path);return PlayerPlanContinue{get_string(value,"action_id",path)};}
    if(kind=="task_reminder"){fields(value,{"task_id","place_id"},{},path);return PlayerPlanTaskReminder{get_string(value,"task_id",path),get_string(value,"place_id",path)};}
    fail("UnsupportedPlayerPlanStep",path);return PlayerPlanMove{};
}
// phase只接受闭合集合，不能把未知状态当成未启动再次收费。
PlayerPlanStepPhase phase(const StableId& value){for(Integer n=0;n<=4;++n)if(player_plan_phase_name(static_cast<PlayerPlanStepPhase>(n))==value)return static_cast<PlayerPlanStepPhase>(n);fail("InvalidPlayerPlanPhase","plan/phase");return PlayerPlanStepPhase::queued;}
// 读取完整步骤边界并保留各原引用的存在位。
PlayerPlanStep step(const BoundaryValue& value){
    fields(value,{"step_id","kind","params","source","authorized","phase","stop_reason"},{"terms","action_id","start_result_id","completion_result_id","started_tick","completed_tick"},"plan/step");PlayerPlanStep result;
    result.id=get_string(value,"step_id","plan/step");result.data=payload(get_string(value,"kind","plan/step"),field(value,"params","plan/step"));result.source=source(field(value,"source","plan/step"));result.authorized=get_bool(value,"authorized","plan/step");result.phase=phase(get_string(value,"phase","plan/step"));result.stop_reason=get_string(value,"stop_reason","plan/step");
    if(auto* item=value.find("terms"))result.terms=terms(*item);
    if(auto* item=value.find("action_id"))result.action_id=string(*item,"plan/step/action_id");
    if(auto* item=value.find("start_result_id"))result.start_result_id=string(*item,"plan/step/start_result_id");
    if(auto* item=value.find("completion_result_id"))result.completion_result_id=string(*item,"plan/step/completion_result_id");
    if(auto* item=value.find("started_tick"))result.started_tick=integer(*item,"plan/step/started_tick");
    if(auto* item=value.find("completed_tick"))result.completed_tick=integer(*item,"plan/step/completed_tick");
    return result;
}
// 只输出持久步骤的明确字段，真实结果不在这里复制。
Status emit_plan_step_fields(ProjectionWriter& w,const PlayerPlanStep& value){
    w.field("step_id",value.id);w.field("kind",player_plan_step_kind(value.data));
    w.object("params",[&](auto& c){return emit_player_plan_step_data_fields(c,value.data);});
    w.object("source",[&](auto& c){return emit_plan_source_fields(c,value.source);});
    w.field("authorized",value.authorized);w.field("phase",player_plan_phase_name(value.phase));w.field("stop_reason",value.stop_reason);
    if(value.terms)w.object("terms",[&](auto& c){return emit_player_plan_terms_fields(c,*value.terms);});
    if(value.action_id)w.field("action_id",*value.action_id);
    if(value.start_result_id)w.field("start_result_id",*value.start_result_id);
    if(value.completion_result_id)w.field("completion_result_id",*value.completion_result_id);
    if(value.started_tick)w.field("started_tick",*value.started_tick);
    if(value.completed_tick)w.field("completed_tick",*value.completed_tick);
    return w.status();
}
// 预算字段全部必填，缺消费值不能被当作未花费。
PlayerPlanBudget budget(const BoundaryValue& value,Integer version){
 if(version==1)fields(value,{"max_additional_ticks","max_gross_spend","used_ticks","gross_spend","approved_tick"},{},"plan/budget");
 else fields(value,{"max_additional_ticks","max_gross_spend","used_ticks","gross_spend","approved_tick","exact_used_duration"},{},"plan/budget");
 PlayerPlanBudget result{get_int(value,"max_additional_ticks","plan/budget"),get_int(value,"max_gross_spend","plan/budget"),get_int(value,"used_ticks","plan/budget"),get_int(value,"gross_spend","plan/budget"),get_int(value,"approved_tick","plan/budget"),{}};
 if(version==2){auto decoded=exact_fields::decode_json<HalfDayDuration>(field(value,"exact_used_duration","plan/budget"),[](auto& c,auto& duration){return c.object("value",[&](auto& row){return exact_fields::fraction(row,duration.value);});});if(!decoded.ok){DecodeFailure error(decoded.error.original.code,decoded.error.original.path);error.error=std::move(decoded.error);throw error;}result.exact_used_duration=std::move(decoded.value);}return result;
}
// /1固定原五字段；/2追加唯一精确累计，不能按字段存在猜版本。
Status emit_plan_budget_fields(ProjectionWriter& w,const PlayerPlanBudget& value,Integer version){
    w.field("max_additional_ticks",value.max_additional_ticks);w.field("max_gross_spend",value.max_gross_spend);w.field("used_ticks",value.used_ticks);w.field("gross_spend",value.gross_spend);w.field("approved_tick",value.approved_tick);
    if(version!=2)return w.status();
    if(!value.exact_used_duration)return w.reject("MissingPlayerPlanExactBudget","plan/budget");
    return w.object("exact_used_duration",[&](auto& child){
        auto result=exact_fields::guarded<Done>([&]{
            DynamicProjectionArchive archive(child);
            exact_fields::take(archive.object("value",[&](auto& row){return exact_fields::fraction(row,const_cast<CanonicalFraction&>(value.exact_used_duration->value));}));
            return Done{};
        });
        return result.ok?common_success():Status{false,{},result.error};
    });
}
// BoundaryValue 仅在原公开 encode 调用要求时构建，shape 路径直接借同 emit。
template<class T,class F>BoundaryValue encode_plan_fields(const T& value,F emit){
    BoundaryValue result;BoundaryProjectionWriter writer(result);auto status=writer.object("",[&](auto& child){return emit(child,value);});
    if(status.ok)status=writer.finish();
    if(!status.ok){DecodeFailure error(status.error.original.code,status.error.original.path);error.error=status.error;throw error;}
    return result;
}

}
// 逐字段输出已知同源报价，供显示、比较与严格保存。
Status emit_player_plan_terms_fields(ProjectionWriter& w,const PlayerPlanTerms& value){
    w.field("provider_actor_id",value.provider_actor_id);w.field("role_id",value.role_id);w.field("place_id",value.place_id);w.field("payer_wallet_id",value.payer_wallet_id);w.field("recipient_wallet_id",value.recipient_wallet_id);w.field("mode",value.mode);w.field("skill_id",value.skill_id);w.field("qualification_key",value.qualification_key);w.field("challenge_kind",value.challenge_kind);w.field("challenge_id",value.challenge_id);w.field("challenge_mode",value.challenge_mode);w.field("duration",value.duration);w.field("cost",value.cost);w.field("wage",value.wage);w.field("hp_gain",value.hp_gain);w.field("fatigue_delta",value.fatigue_delta);w.field("xp_limit",value.xp_limit);w.field("target_level",value.target_level);
    if(value.settlement_policy)w.field("settlement_policy",*value.settlement_policy);
    if(value.challenge_source_sha256)w.field("challenge_source_sha256",*value.challenge_source_sha256);
    return w.status();
}
BoundaryValue encode_player_plan_terms(const PlayerPlanTerms& value){return encode_plan_fields(value,emit_player_plan_terms_fields);}
// 每个variant只有自己的闭合参数，不输出通用参数包。
Status emit_player_plan_step_data_fields(ProjectionWriter& w,const PlayerPlanStepData& data){
    return std::visit([&](const auto& value)->Status{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,PlayerPlanMove>)return w.field("place_id",value.place_id);
        else if constexpr(std::is_same_v<T,PlayerPlanTravel>){
            if(const auto* id=std::get_if<StableId>(&value.request.destination))w.field("destination",*id);
            else {const auto& point=std::get<PointI>(value.request.destination);w.object("destination",[&](auto& c){c.field("x",point.x);return c.field("y",point.y);});}
            if(value.request.has_via)w.sequence("via",value.request.via,[](auto& c,const auto& id){return c.field("",id);});return w.status();
        }else if constexpr(std::is_same_v<T,PlayerPlanWork>)return w.object("context",[&](auto& c){return emit_plan_context_fields(c,value.context);});
        else if constexpr(std::is_same_v<T,PlayerPlanCourse>){w.object("context",[&](auto& c){return emit_plan_context_fields(c,value.context);});w.field("skill_id",value.skill_id);w.field("ticks",value.ticks);return w.field("challenge_mode",value.challenge_mode);}
        else if constexpr(std::is_same_v<T,PlayerPlanRest>){w.field("estate_id",value.estate_id);return w.field("duration",value.duration);}
        else if constexpr(std::is_same_v<T,PlayerPlanContinue>)return w.field("action_id",value.action_id);
        else {w.field("task_id",value.task_id);return w.field("place_id",value.place_id);}
    },data);
}
BoundaryValue encode_player_plan_step_data(const PlayerPlanStepData& value){return encode_plan_fields(value,emit_player_plan_step_data_fields);}
// 编辑命令拒绝自称来源或已执行字段，由原native报价另行补足。
Outcome<PlayerPlanEdit> decode_player_plan_edit(const StableId& kind,const BoundaryValue& value){using namespace boundary;try{const StableId path="player_plan";
    if(kind=="player_plan_add"){fields(value,{"kind","params","authorized"},{"terms"},path);PlayerPlanAppend result;result.data=payload(get_string(value,"kind",path),field(value,"params",path));result.authorized=get_bool(value,"authorized",path);if(auto* item=value.find("terms"))result.terms=terms(*item);return {true,result,{}};}
    if(kind=="player_plan_remove"){fields(value,{"step_id"},{},path);return {true,PlayerPlanRemove{get_string(value,"step_id",path)},{}};}
    if(kind=="player_plan_reorder"){fields(value,{"step_ids"},{},path);return {true,PlayerPlanReorder{strings(field(value,"step_ids",path),path)},{}};}
    if(kind=="player_plan_clear"){fields(value,{},{},path);return {true,PlayerPlanClear{},{}};}
    if(kind=="player_plan_pause"){fields(value,{},{},path);return {true,PlayerPlanPause{},{}};}
    if(kind=="player_plan_authorize"){fields(value,{"step_id","authorized"},{"terms"},path);PlayerPlanAuthorize result{get_string(value,"step_id",path),get_bool(value,"authorized",path),{}};if(auto* item=value.find("terms"))result.terms=terms(*item);return {true,result,{}};}
    return {false,{},common_failure("UnsupportedPlayerPlanCommand",path).error};
}catch(const DecodeFailure& error){return {false,{},error.error};}}
// 严格新版本结构解码后立即核计划形状，失败不返回半成品。
Outcome<PlayerPlanState> decode_player_plan_state(const BoundaryValue& value){using namespace boundary;try{fields(value,{"version"},{"current"},"player_plan");PlayerPlanState result;result.version=get_int(value,"version","player_plan");if(result.version!=1&&result.version!=2)fail("PlayerPlanVersionMismatch","player_plan/version");if(auto* item=value.find("current")){if(result.version==1)fields(*item,{"plan_id","actor_id","created_tick","steps","stop_reason"},{"budget"},"player_plan/current");else fields(*item,{"plan_id","actor_id","created_tick","steps","stop_reason","approved_scope_revision"},{"budget"},"player_plan/current");PlayerPlan plan;if(result.version==2)plan.approved_scope_revision=get_int(*item,"approved_scope_revision","player_plan/current");plan.id=get_string(*item,"plan_id","plan");plan.actor_id=get_string(*item,"actor_id","plan");plan.created_tick=get_int(*item,"created_tick","plan");plan.stop_reason=get_string(*item,"stop_reason","plan");const auto& rows=array(field(*item,"steps","plan"),"plan/steps");if(rows.size()>player_plan_max_steps)fail("PlayerPlanStepLimit","plan/steps");for(const auto& row:rows)plan.steps.push_back(step(row));if(auto* b=item->find("budget"))plan.budget=budget(*b,result.version);result.current=std::move(plan);}auto checked=validate_player_plan_shape(result);if(!checked.ok)return {false,{},checked.error};return {true,std::move(result),{}};}catch(const DecodeFailure& error){return {false,{},error.error};}}
// 旧profile不调用此函数，新profile明确保存当前计划或空状态。
Status emit_player_plan_state_fields(ProjectionWriter& w,const PlayerPlanState& state){
    if(state.version==2){auto valid=validate_player_plan_shape(state);if(!valid.ok)return valid;}
    w.field("version",state.version);
    if(state.current){const auto& plan=*state.current;w.object("current",[&](auto& c){
        c.field("plan_id",plan.id);c.field("actor_id",plan.actor_id);c.field("created_tick",plan.created_tick);
        c.sequence("steps",plan.steps,[](auto& out,const auto& step){return out.object("",[&](auto& row){return emit_plan_step_fields(row,step);});});
        c.field("stop_reason",plan.stop_reason);
        if(plan.budget)c.object("budget",[&](auto& row){return emit_plan_budget_fields(row,*plan.budget,state.version);});
        if(state.version==2)c.field("approved_scope_revision",plan.approved_scope_revision);
        return c.status();
    });}
    return w.status();
}
BoundaryValue encode_player_plan_state(const PlayerPlanState& state){return encode_plan_fields(state,emit_player_plan_state_fields);}

} // 解码只返回未采纳typed候选，世界安装前还须验证所有原行动和回执引用。
