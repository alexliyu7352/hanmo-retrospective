// 实际已安装组件上的有限关联查询；无新count算法、位置解析、余额派生或职务授权。
#include "legacy_projection_store_internal.hpp"
#include "inventory_core.hpp"
#include <algorithm>
#include <new>

namespace hanmo::detail {
// 将原字符串数组按原序重组为typed值，不排序参与者。
static Value strings(const std::vector<std::string>& input) {
    Value::Array result; for(const auto& item:input) result.push_back(Value{item}); return Value{std::move(result)};
}
// 重组原人物记录，包括真正存在的可选值，缺值不补默认。
Value actor_record(const ActorState& actor) {
    auto row=actor.extras;
    row.emplace("id",Value{actor.id}); row.emplace("wallet_id",Value{actor.wallet_id}); row.emplace("inventory_id",Value{actor.inventory_id}); row.emplace("action_id",Value{actor.action_id});
    row.emplace("alive",Value{actor.alive}); row.emplace("available",Value{actor.available}); row.emplace("hp",Value{actor.hp}); row.emplace("fatigue",Value{actor.fatigue}); row.emplace("merit",Value{actor.merit}); row.emplace("location",actor.location);
    if(actor.prototype_id) row.emplace("prototype_id",Value{*actor.prototype_id});
    if(actor.display_name) row.emplace("display_name",Value{*actor.display_name});
    if(actor.birth_tick) row.emplace("birth_tick",Value{*actor.birth_tick});
    if(actor.created_tick) row.emplace("created_tick",Value{*actor.created_tick});
    return Value{std::move(row)};
}
// 钱包只保原余额及按action键的保留量，包括0。
Value wallet_record(const WalletState& wallet) {
    auto row=wallet.extras; Value::Dict reserved;
    for(const auto& [id,amount]:wallet.reserved) reserved.emplace(id,Value{amount});
    row.emplace("id",Value{wallet.id}); row.emplace("owner",wallet.owner); row.emplace("balance",Value{wallet.balance}); row.emplace("reserved",Value{std::move(reserved)});
    return Value{std::move(row)};
}
// 七字段与extras原位合并，start/remaining不解释成到期承诺。
Value activity_record(const LegacyActivity& activity) {
    auto row=activity.extras;
    row.emplace("id",Value{activity.id}); row.emplace("kind",Value{activity.kind}); row.emplace("actor_id",Value{activity.actor_id}); row.emplace("participants",strings(activity.participants));
    row.emplace("start_tick",Value{activity.start_tick}); row.emplace("remaining_ticks",Value{activity.remaining_ticks}); row.emplace("params",Value{activity.params}); return Value{std::move(row)};
}
// 任职运行行和作者定义各自重组，绝不把definition当授权。
Value office_record(const LegacyOffice& office) {
    auto row=office.extras;
    row.emplace("id",Value{office.id}); row.emplace("holder_actor_id",Value{office.holder_actor_id}); row.emplace("enabled",Value{office.enabled}); row.emplace("appointment_id",Value{office.appointment_id});
    return Value{Value::Dict{{"state",Value{std::move(row)}},{"definition",Value{office.definition}}}};
}
// 作者组织的目录领袖可以未激活，重组不生成运行人物。
Value organization_record(const OrganizationDefinition& organization) {
    auto row=organization.extras; row.emplace("id",Value{organization.id}); row.emplace("name_key",Value{organization.name_key}); row.emplace("kind",Value{organization.kind});
    if(organization.leader_actor) row.emplace("leader_actor",Value{*organization.leader_actor});
    return Value{std::move(row)};
}
// 表键与事实正文分开，允许Actor与Fact共用同一原字符串。
Value fact_record(const LegacyFact& fact) {
    auto row=fact.extras; row.emplace("kind",Value{fact.kind}); return Value{Value::Dict{{"id",Value{fact.id}},{"record",Value{std::move(row)}}}};
}
// 物品定义保存原重量来源和额外目录字段，不计算新容量。
Value item_record(const ItemDefinition& item) {
    auto row=item.extras; row.emplace("id",Value{item.id}); row.emplace("weight",Value{item.weight}); return Value{std::move(row)};
}
} // 重组只服务只读审计。

namespace hanmo {
// count保原整批decode及库存存在性首序，数值循环只调用唯一inventory_count。
Result<std::vector<CountRow>> ProjectionStore::query_count_batch(const std::string& bytes) const {
    if(!impl_) return {false,{}, {"NotInstalled","/runtime"}};
    const auto queries=detail::decode_queries(bytes,impl_->input_sha256);
    if(!queries.ok) return {false,{},queries.error};
    for(const auto& query:queries.value) if(!impl_->inventories.count(query.inventory_id)) return {false,{}, {"UnknownInventory",query.inventory_id}};
    std::vector<CountRow> result;
    for(const auto& query:queries.value) {
        const auto* inventory=detail::component<detail::InventoryState>(impl_->inventories,query.inventory_id);
        if(!inventory) return {false,{}, {"ProjectionComponentMissing",query.inventory_id}};
        const auto count=detail::inventory_count(*inventory,query.item_id,query.owners,query.reservation,query.inventory_id);
        if(!count.ok) return {false,{},count.error};
        result.push_back({query.id,count.value});
    }
    return {true,std::move(result),{}};
}

// 先核profile和整批人物，再从真实组件关联；任何失败均不带部分行或授权结论。
Result<std::vector<ActorCommitmentsResult>> ProjectionStore::actor_commitments(const std::string& bytes) const {
    if(!impl_) return {false,{}, {"NotInstalled","/runtime"}};
    if(impl_->profile!=detail::legacy_profile) return {false,{}, {"UnsupportedProjectionProfile","/runtime/profile"}};
    const auto queries=detail::decode_actor_queries(bytes,impl_->input_sha256);
    if(!queries.ok) return {false,{},queries.error};
    for(const auto& query:queries.value) for(const auto& id:query.actor_ids) if(!impl_->actors.count(id)) return {false,{}, {"UnknownActor",id}};
    try {
        std::vector<ActorCommitmentsResult> results;
        for(const auto& query:queries.value) {
            ActorCommitmentsResult result; result.query_id=query.query_id;
            auto ids=query.actor_ids; std::sort(ids.begin(),ids.end());
            for(const auto& id:ids) {
                const auto* actor=detail::component<detail::ActorState>(impl_->actors,id);
                if(!actor) return {false,{}, {"ProjectionComponentMissing",id}};
                ActorCommitmentRow row; row.actor_id=id; row.wallet_id=actor->wallet_id; row.inventory_id=actor->inventory_id; row.location=actor->location;
                if(!actor->action_id.empty()) {
                    const auto* activity=detail::component<detail::LegacyActivity>(impl_->activities,actor->action_id);
                    if(!activity) return {false,{}, {"ProjectionComponentMissing",actor->action_id}};
                    row.current_activity=*activity;
                    // 输出只声明七原字段；活动额外字段留组件审计，不借关联口dump。
                    row.current_activity->extras.clear();
                    for(const auto& [wallet_id,entity]:impl_->wallets) {
                        const auto* wallet=entity.view().try_get<detail::WalletState>();
                        if(!wallet) return {false,{}, {"ProjectionComponentMissing",wallet_id}};
                        const auto held=wallet->reserved.find(actor->action_id);
                        if(held!=wallet->reserved.end()) row.wallet_reservations.push_back({wallet_id,wallet->owner,held->second});
                    }
                    for(const auto& [inventory_id,entity]:impl_->inventories) {
                        const auto* inventory=entity.view().try_get<detail::InventoryState>();
                        if(!inventory) return {false,{}, {"ProjectionComponentMissing",inventory_id}};
                        for(std::size_t index=0;index<inventory->lots.size();++index) {
                            const auto& lot=inventory->lots[index];
                            if(lot.reserved_by==actor->action_id) row.goods_reservations.push_back({inventory_id,inventory->owner,index,lot});
                        }
                    }
                }
                for(const auto& [office_id,entity]:impl_->offices) {
                    const auto* office=entity.view().try_get<detail::LegacyOffice>();
                    if(!office) return {false,{}, {"ProjectionComponentMissing",office_id}};
                    if(office->holder_actor_id!=id) continue;
                    const auto source=office->definition.find("organization_id");
                    const auto* organization_id=source==office->definition.end()?nullptr:std::get_if<std::string>(&source->second.data);
                    if(!organization_id) return {false,{}, {"ProjectionComponentMissing",office_id}};
                    const auto* organization=detail::component<detail::OrganizationDefinition>(impl_->organizations,*organization_id);
                    if(!organization) return {false,{}, {"ProjectionComponentMissing",*organization_id}};
                    row.offices.push_back({office_id,office->appointment_id,*organization_id,office->enabled,!office->appointment_id.empty() && impl_->facts.count(office->appointment_id)!=0,*organization});
                }
                result.actors.push_back(std::move(row));
            }
            results.push_back(std::move(result));
        }
        return {true,std::move(results),{}};
    } catch(const std::bad_alloc&) { return {false,{}, {"ProjectionResourceLimit","/queries"}}; }
}
} // 有限关联不改变原行动、资金、货权或时间。
