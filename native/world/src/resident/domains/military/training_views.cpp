#include "resident/domains/military/military_model.hpp"
// 驻军训练与军阵题面的有限投影；不报价、不推进课时，不返回作者解法、种子或完整军事状态。
#include "resident/domains/military/training_rules.hpp"
#include "resident/domains/military/training_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>

namespace hanmo::resident {
namespace {
using Value=BoundaryValue;
using Dict=Value::Dict;
using Array=Value::Array;

// 唯一训练事实按已有实例ID查找；缺记录不猜空闲，也不从临时action重建永久账本。
const MilitaryTrainingFact* training_fact(const MilitaryState& state,const StableId& action_id) {
    for(const auto& fact:state.training_facts)if(fact.id==action_id)return &fact;
    return nullptr;
}

// 简单坐标只复制原5×4题面的整数，不将抽象棋盘映射到真实军团坐标。
Value point_value(const HexPoint& point) {
    return Value{Array{Value{point.q},Value{point.r}}};
}

// 数组保持作者插入序；目标格不在这里重新排序或计算可达性。
Value point_values(const std::vector<HexPoint>& points) {
    Array values;
    values.reserve(points.size());
    for(const auto& point:points)values.push_back(point_value(point));
    return Value{std::move(values)};
}

// 原GD先追加公开text再删除text_key，因而保留其他字段顺序并把text放在最后。
FieldOrder public_goal_order(const FieldOrder& source_order) {
    if(source_order.empty())return {};
    FieldOrder keys;
    keys.reserve(source_order.size());
    for(const auto& key:source_order)if(key!="text_key")keys.push_back(key);
    keys.push_back("text");
    return keys;
}

// 供粮目标只暴露既有公开格与说明，不附带评分结果、最佳路径或源摘要。
Value supply_goal(const DrillSupplyGoal& goal) {
    return boundary::ordered(boundary::obj({
        {"group_id",Value{goal.group_id}},
        {"point",point_value(goal.point)},
        {"guard_cells",point_values(goal.guard_cells)},
        {"text",Value{goal.text}}
    }),public_goal_order(goal.field_order));
}

// 通路目标沿作者明确的格集合显示；不会把字面目标解释成新世界通行权限。
Value passage_goal(const DrillPassageGoal& goal) {
    return boundary::ordered(boundary::obj({
        {"group_id",Value{goal.group_id}},
        {"cells",point_values(goal.cells)},
        {"text",Value{goal.text}}
    }),public_goal_order(goal.field_order));
}

// 掩护格对保留原对内与数组顺序，不用相邻关系猜测作者配对。
Value screen_goal(const DrillScreenGoal& goal) {
    Array pairs;
    pairs.reserve(goal.pairs.size());
    for(const auto& pair:goal.pairs)pairs.push_back(boundary::ordered(boundary::obj({
        {"front",point_value(pair.front)},
        {"rear",point_value(pair.rear)}
    }),pair.field_order));
    return boundary::ordered(boundary::obj({
        {"front_group_id",Value{goal.front_group_id}},
        {"ranged_group_id",Value{goal.ranged_group_id}},
        {"pairs",Value{std::move(pairs)}},
        {"text",Value{goal.text}}
    }),public_goal_order(goal.field_order));
}

// 骑兵目标仅投影原公开不利格，不读取实际军团兵种属性或疲劳。
Value cavalry_goal(const DrillCavalryGoal& goal) {
    return boundary::ordered(boundary::obj({
        {"group_id",Value{goal.group_id}},
        {"bad_cells",point_values(goal.bad_cells)},
        {"text",Value{goal.text}}
    }),public_goal_order(goal.field_order));
}

// 实例已经由唯一规则叶验证；这里只按已确认移动取最后位置，未确认草案仍单独显示。
Value public_drill(const DrillDefinition& definition,const DrillState& challenge) {
    if(challenge.phase=="completed")return boundary::obj({
        {"ok",Value{true}},
        {"phase",Value{StableId{"completed"}}},
        {"result",training_codec::encode(*challenge.result)}
    });
    Array groups;
    groups.reserve(definition.groups.size());
    for(const auto& group:definition.groups) {
        HexPoint position=group.origin;
        for(const auto& move:challenge.moves)if(move.group_id==group.id)position=move.target;
        groups.push_back(boundary::obj({
            {"id",Value{group.id}},
            {"kind",Value{group.kind}},
            {"text",Value{group.text}},
            {"position",point_value(position)},
            {"targets",point_values(group.targets)}
        }));
    }
    auto objectives=boundary::ordered(boundary::obj({
        {"supply",supply_goal(definition.supply)},
        {"passage",passage_goal(definition.passage)},
        {"screen",screen_goal(definition.screen)},
        {"cavalry",cavalry_goal(definition.cavalry)}
    }),definition.objectives_field_order);
    return boundary::obj({
        {"ok",Value{true}},
        {"phase",Value{StableId{"active"}}},
        {"width",Value{Integer{5}}},
        {"height",Value{Integer{4}}},
        {"step",Value{Integer(challenge.moves.size()+1)}},
        {"prompt",Value{definition.prompt}},
        {"groups",Value{std::move(groups)}},
        {"objectives",std::move(objectives)},
        {"pending",challenge.pending?training_codec::encode(*challenge.pending):Value{std::monostate{}}}
    });
}

// 读到不完整运行来源时返回明确失败，而非生成active=false掩盖损坏的训练链。
Value invalid_active(const StableId& code) {
    return boundary::obj({{"ok",Value{false}},{"code",Value{code}}});
}
}

// 与原study_activity_input军事分支相同：只取当前行动、永久课时和真实钱包预留标量。
BoundaryValue military_training_active_view(const CommonStore& common,const MilitaryState& state) {
    const auto& actor_id=common.state().metadata.player_actor_id;
    const auto* actor=common.actor(actor_id);
    if(!actor)return invalid_active("UnknownActor");
    const auto* action=common.action(actor->action_id);
    if(!action||action->kind!="military_train")return boundary::obj({{"active",Value{false}}});
    const auto* fact=training_fact(state,action->id);
    if(!fact)return invalid_active("InvalidMilitaryTrainingAction");
    const auto* wallet=common.wallet(fact->terms.payer_wallet_id);
    if(!wallet)return invalid_active("MilitaryTrainingReservationMismatch");
    Integer reserved=0;
    for(const auto& row:wallet->reserved)if(row.action_id==action->id){reserved=row.amount;break;}
    std::vector<StableId> scopes;
    if(fact->drill)scopes.push_back("training");
    return boundary::obj({
        {"active",Value{true}},
        {"actor_id",Value{actor_id}},
        {"action_id",Value{action->id}},
        {"kind",Value{action->kind}},
        {"remaining_ticks",Value{action->remaining_ticks}},
        {"completed_lessons",Value{Integer(fact->lessons.size())}},
        {"paid",Value{fact->terms.fee-reserved}},
        {"reserved",Value{reserved}},
        {"challenge_scopes",boundary::string_array(scopes)}
    });
}

// 手动题面答完或首课冻结后，只要原本人训练仍started，原Main的自动推进窗口就保持阻止。
bool military_manual_window(const CommonRead& common,const MilitaryState& state,const StableId& actor_id) {
    const auto* actor=common.actor(actor_id);
    const auto* action=actor?common.action(actor->action_id):nullptr;
    if(!action||action->kind!="military_train"||action->actor_id!=actor_id)return false;
    const auto* fact=training_fact(state,action->id);
    return fact&&fact->phase=="started"&&fact->actor_id==actor_id&&fact->terms.drill&&fact->terms.drill->mode=="manual";
}

// 原查询允许本人已完成历史实例；身份先于绑定校验，不新增当前行动、存活或可用性门槛。
Outcome<BoundaryValue> military_drill_view(const CommonRead& common,const MilitaryState& state,const MilitaryCatalog& catalog,const StableId& actor_id,const StableId& action_id) {
    (void)common;
    const auto* fact=training_fact(state,action_id);
    if(!fact||fact->actor_id!=actor_id)return {false,{},common_failure("TrainingDrillActorMismatch").error};
    auto valid=validate_training_drill_binding(catalog,*fact);
    if(!valid.ok)return {false,{},valid.error};
    if(!fact->drill)return {false,{},common_failure("TrainingDrillNotOffered").error};
    if(!fact->terms.drill)return {false,{},common_failure("InvalidTrainingDrillTerms").error};
    const DrillDefinition* definition=nullptr;
    for(const auto& candidate:catalog.drill_challenges)if(candidate.id==fact->terms.drill->challenge_id){definition=&candidate;break;}
    if(!definition)return {false,{},common_failure("MissingTrainingDrill").error};
    try {
        auto view=public_drill(*definition,fact->drill->challenge);
        auto& fields=std::get<Dict>(view.data);
        fields.emplace_back("instance_phase",Value{fact->drill->phase});
        fields.emplace_back("performance_bp",fact->drill->performance_bp?Value{*fact->drill->performance_bp}:Value{std::monostate{}});
        fields.emplace_back("group_source",training_codec::encode(fact->terms.drill->group_source));
        return {true,std::move(view),{}};
    }catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}
}
} // 训练资格、结算、随机和题目合法性仍在原唯一owner；本文件不执行任何规则写入。
