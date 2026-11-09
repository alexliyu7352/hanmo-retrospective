// 将已完成的整批人物承诺DTO转成类型树；任何错误由上游整批门先返回。
#include "actor_commitments_result.hpp"
namespace hanmo::detail {
namespace {
// 已声明七字段恢复原值；未请求的行动扩展不进入这个有限查询结果。
Value activity_value(const LegacyActivity& action) {
    Value::Array participants;
    for(const auto& id:action.participants) participants.push_back(Value{id});
    return Value{Value::Dict{{"id",Value{action.id}},{"kind",Value{action.kind}},{"actor_id",Value{action.actor_id}},
        {"participants",Value{std::move(participants)}},{"start_tick",Value{action.start_tick}},
        {"remaining_ticks",Value{action.remaining_ticks}},{"params",Value{action.params}}}};
}
// 组织是原目录定义；可选字段缺失不填null或假领袖。
Value organization_value(const OrganizationDefinition& organization) {
    Value::Dict row=organization.extras;
    row.emplace("id",Value{organization.id}); row.emplace("name_key",Value{organization.name_key});
    row.emplace("kind",Value{organization.kind});
    if(organization.leader_actor) row.emplace("leader_actor",Value{*organization.leader_actor});
    return Value{std::move(row)};
}
// 返回物理批次的完整owner和扩展，不给额外字段另造包装层。
Value lot_value(const Lot& lot) {
    Value::Dict row=lot.extras;
    row.emplace("id",Value{lot.id}); row.emplace("item_id",Value{lot.item_id});
    row.emplace("quantity",Value{lot.quantity}); row.emplace("owner",lot.owner);
    row.emplace("reserved_by",Value{lot.reserved_by});
    return Value{std::move(row)};
}
} // 编码帮助函数不计算新领域答案。

// 输入已按合同排序；此处保持query、人物、原批次与参与者次序。
Value actor_commitments_value(const std::vector<ActorCommitmentsResult>& results) {
    Value::Array queries;
    for(const auto& query:results) {
        Value::Array actors;
        for(const auto& actor:query.actors) {
            Value::Array money,goods,offices;
            for(const auto& row:actor.wallet_reservations)
                money.push_back(Value{Value::Dict{{"wallet_id",Value{row.wallet_id}},{"owner",row.owner},{"amount",Value{row.amount}}}});
            for(const auto& row:actor.goods_reservations)
                goods.push_back(Value{Value::Dict{{"inventory_id",Value{row.inventory_id}},{"container_owner",row.container_owner},
                    {"lot_index",Value{static_cast<std::int64_t>(row.lot_index)}},{"lot",lot_value(row.lot)}}});
            for(const auto& row:actor.offices)
                offices.push_back(Value{Value::Dict{{"office_id",Value{row.office_id}},{"enabled",Value{row.enabled}},
                    {"appointment_id",Value{row.appointment_id}},{"appointment_resolved",Value{row.appointment_resolved}},
                    {"organization_id",Value{row.organization_id}},{"organization",organization_value(row.organization)}}});
            actors.push_back(Value{Value::Dict{{"actor_id",Value{actor.actor_id}},{"location",actor.location},
                {"wallet_id",Value{actor.wallet_id}},{"inventory_id",Value{actor.inventory_id}},
                {"current_activity",actor.current_activity?activity_value(*actor.current_activity):Value{}},
                {"wallet_reservations",Value{std::move(money)}},{"goods_reservations",Value{std::move(goods)}},
                {"offices",Value{std::move(offices)}}}});
        }
        queries.push_back(Value{Value::Dict{{"query_id",Value{query.query_id}},{"actors",Value{std::move(actors)}}}});
    }
    return Value{std::move(queries)};
}
} // 输出与store完全值隔离。
