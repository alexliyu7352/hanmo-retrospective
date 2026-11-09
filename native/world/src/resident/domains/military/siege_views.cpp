// 只读城防入口重用正式观察与单部预览，打开页面不会出AI、扣材料或改变半日游标。
#include "siege_views.hpp"
#include "siege_codec.hpp"
#include "siege_integration_decisions.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 有限DTO逐字段组装，绝不通过导出整态再删隐藏字段。
template<class T>const T* get(const std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>const T* pair(const std::vector<std::pair<StableId,T>>& rows,const StableId& id){for(auto& r:rows)if(r.first==id)return &r.second;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
V strings(const std::vector<StableId>& rows){A out;for(auto& x:rows)out.push_back(V{x});return V{std::move(out)};}
template<class T>V pairs(const std::vector<std::pair<StableId,T>>& rows){D out;for(auto& p:rows)out.emplace_back(p.first,V{p.second});return V{std::move(out)};}
Outcome<V> fail(const StableId& code="InvalidSiegeViewSource"){return {false,{},common_failure(code).error};}
V failure(const RuleError& e){return V{D{{"ok",V{false}},{"code",V{e.original.code}},{"path",V{e.original.path}}}};}
V identity(const CommonStore& c,const SiegeRecord& s){return V{D{{"actor_id",V{c.state().metadata.player_actor_id}},{"revision",V{c.revision()}},{"siege_id",V{s.id}},{"round",V{s.round}},{"phase",V{s.phase}}}};}
Outcome<V> base(const CommonStore& c){if(c.revision()<0||c.revision()>2147483647||!c.actor(c.state().metadata.player_actor_id))return fail();return {true,V{D{}},{}};}
// 形状只守卫原页面会读的字段，正式世界校验仍归领域稳定边界。
bool shape(const SiegeRecord& s){if(!has({"active","result","closed"},s.phase)||s.round<0||s.round>2147483647||s.inner_hold_ticks<0||s.inner_hold_ticks>2147483647)return false;for(auto& p:s.node_hp)if(p.second<0||p.second>2147483647)return false;for(auto& p:s.armies)if(p.second<0||p.second>1)return false;for(const auto& event:s.last_events)if(event.kind=="siege_damage"&&(!event.unit_id||!event.node_id||!event.damage||!event.remaining_hp))return false;return true;}
V supply(const Outcome<MilitaryProvisions>& q){if(!q.ok)return failure(q.error);auto& p=q.value;return V{D{{"ok",V{true}},{"food_item_id",V{p.food_item_id}},{"mouth_count",V{p.mouth_count}},{"required",V{p.required}},{"available",V{p.available}},{"full_ticks",V{p.full_ticks}},{"shortage",V{p.shortage}}}};}
// 单部报价保原字段次序，空器械不输出、草案不透露敌军已保存计划。
V quote(const Outcome<SiegeOrderQuote>& q){if(!q.ok)return failure(q.error);A costs;for(auto& cost:q.value.costs)costs.push_back(siege_codec::encode(cost));D out{{"ok",V{true}},{"action",V{q.value.action}},{"node_id",V{q.value.node_id}},{"costs",V{std::move(costs)}}};if(q.value.engine_use)out.emplace_back("engine_use",siege_codec::encode(*q.value.engine_use));if(q.value.damage)out.emplace_back("damage",V{*q.value.damage});return V{std::move(out)};}
// 有限候选只含可控攻军的本军公共未预留器械；正式目标和整轮合量继续由原军议复核。
V engine_choices(const SiegeRecord& siege,const SiegeTopology& topology,const SiegeObservation& obs,const StableId& player){
    D choices;
    for(const auto& position:siege.unit_nodes){
        SiegeOrder hold;hold.action="wait";hold.node_id=position.second;
        if(!preview_siege_order(siege,topology,obs,player,position.first,hold).ok)continue;
        const auto* unit=get(obs.units,position.first);
        const auto* side=unit?pair(siege.armies,unit->army_id):nullptr;
        if(!side||*side!=0)continue;
        const auto* army=get(obs.armies,unit->army_id);
        const auto* stock=pair(obs.public_goods,unit->army_id);
        const auto* quantity=stock?pair(*stock,topology.rules.engine_item_id):nullptr;
        A rows;
        if(army&&!army->inventory_id.empty()&&quantity&&*quantity>0){
            SiegeEngineUse use;use.inventory_id=army->inventory_id;use.item_id=topology.rules.engine_item_id;use.quantity=1;
            rows.push_back(V{D{{"engine_use",siege_codec::encode(use)},{"available_quantity",V{*quantity}}}});
        }
        choices.emplace_back(position.first,V{std::move(rows)});
    }
    return V{std::move(choices)};
}
// 只复制已有结构破坏回执的展示字段，不暴露战略请求或从库存推测发动过器械。
V last_damage_events(const SiegeRecord& siege){
    A events;
    for(const auto& event:siege.last_events){
        if(event.kind!="siege_damage"||!event.unit_id||!event.node_id||!event.damage||!event.remaining_hp)continue;
        events.push_back(V{D{{"unit_id",V{*event.unit_id}},{"node_id",V{*event.node_id}},{"damage",V{*event.damage}},{"remaining_hp",V{*event.remaining_hp}}}});
    }
    return V{std::move(events)};
}
// 整编有限投影只列本围城的冻结条款、必需军官与永久本人决定；政策预览绝不冒充决定来源。
V integration_view(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const SiegeRecord& siege,const CommonPresentation& presentation,const MilitaryIntegrationAccess* access){
    if(!cat.siege_integration.enabled)return V{D{{"enabled",V{false}}}};
    const auto& integration=s.siege_integration;
    SiegeIntegrationReadContext context{c,s,cat,a,integration,cat.siege_integration};
    const auto player=c.state().metadata.player_actor_id;
    SiegeIntegrationPropose request;request.siege_id=siege.id;request.terms=cat.siege_integration.default_terms;
    auto proposed=preview_siege_integration(context,player,SiegeIntegrationCommand{request});
    D out{{"enabled",V{true}},{"proposal_quote",proposed.ok?V{D{{"ok",V{true}}}}:failure(proposed.error)},{"terms",encode_siege_integration_terms(request.terms)}};
    const auto* attempt=current_siege_integration_attempt(integration,siege.id);
    const auto* proposal=attempt?siege_integration_proposal(integration,attempt->proposal_id):nullptr;
    if(!proposal&&proposed.ok&&proposed.value.proposal)proposal=&*proposed.value.proposal;
    if(!proposal)return V{std::move(out)};
    if(attempt){out.emplace_back("proposal",encode_siege_integration_proposal(*proposal));out.emplace_back("status",V{attempt->status});out.emplace_back("reason",V{attempt->reason});out.emplace_back("completion_source_id",V{attempt->completion_source_id});}
    else {out.emplace_back("offer",encode_siege_integration_proposal(*proposal));out.emplace_back("status",V{StableId{"draft"}});}
    A officers;
    for(const auto& officer:proposal->required_officers){
        const SiegeOfficerDecision* decision=nullptr;
        if(attempt)for(const auto& id:attempt->decision_source_ids){const auto* candidate=get(integration.decisions,id);if(candidate&&candidate->actor_id==officer.actor_id){decision=candidate;break;}}
        const auto title=presentation.display_name(officer.actor_id);
        D row{{"actor_id",V{officer.actor_id}},{"actor_name",V{title.ok?title.value:officer.actor_id}},{"army_id",V{officer.army_id}},{"unit_ids",strings(officer.unit_ids)},{"formation_source_id",V{officer.formation_source_id}},{"decision",V{decision?decision->decision:StableId{"pending"}}}};
        A appointments;for(const auto& appointment:officer.appointment_sources)appointments.push_back(V{D{{"office_id",V{appointment.id}},{"appointment_id",V{appointment.appointment_id}},{"enabled",V{appointment.enabled}}}});row.emplace_back("appointment_sources",V{std::move(appointments)});
        if(decision){row.emplace_back("decision_source_id",V{decision->id});row.emplace_back("actor_action_source_id",V{decision->actor_action_source_id});row.emplace_back("reason",V{decision->policy_reason.empty()?decision->legal_basis:decision->policy_reason});row.emplace_back("decision_tick",V{decision->tick});}
        else if(attempt&&attempt->status=="pending"){
            auto pending=preview_siege_integration_npc(context,proposal->id,officer.actor_id);
            if(!pending.ok)row.emplace_back("reason",V{pending.error.original.code});
            else if(pending.value.decision=="wait")row.emplace_back("reason",V{pending.value.reason});
        }
        officers.push_back(V{std::move(row)});
    }
    out.emplace_back("officers",V{std::move(officers)});
    if(attempt&&attempt->status=="completed"){
        const auto* completed=get(s.integration_facts,attempt->completion_source_id);
        if(completed&&completed->proposal_id==proposal->id&&completed->proposal_key==proposal->proposal_key){
            A custody;for(const auto& row:completed->custody)custody.push_back(V{D{{"member_id",V{row.member_id}},{"from_army_id",V{row.from_army_id}},{"to_army_id",V{row.to_army_id}}}});
            out.emplace_back("completed_transfer",V{D{{"id",V{completed->id}},{"tick",V{completed->tick}},{"recipient_army_id",V{completed->recipient_army_id}},{"recipient_units_before",strings(completed->recipient_units_before)},{"recipient_units_after",strings(completed->recipient_units_after)},{"recipient_members_before",strings(completed->recipient_members_before)},{"recipient_members_after",strings(completed->recipient_members_after)},{"closed_campaign_ids",strings(completed->closed_campaign_ids)},{"population_receipt_ids",strings(completed->population_receipt_ids)},{"custody",V{std::move(custody)}}}});
        }
    }
    const auto* recipient=get(s.armies,proposal->recipient.id);const auto* inventory=recipient?c.inventory(recipient->inventory_id):nullptr;
    auto weight=inventory?c.inventory_weight(inventory->id):Outcome<Integer>{false,{},common_failure("UnknownInventory").error};
    if(inventory&&weight.ok){out.emplace_back("receiving_inventory_id",V{inventory->id});out.emplace_back("receiving_weight",V{weight.value});out.emplace_back("receiving_capacity",V{inventory->capacity});}
    if(attempt&&attempt->status=="pending"){
        const MilitaryIntegrationAccess unavailable;
        auto assets=preview_military_integration(c,s,cat,a,integration,*proposal,access?*access:unavailable);
        if(!assets.ok)out.emplace_back("asset_quote",failure(assets.error));
        else {Integer money=0;A wallets,lots;for(const auto& wallet:assets.value.transfer.wallets){money+=wallet.amount;wallets.push_back(V{D{{"source_wallet_id",V{wallet.source_wallet_id}},{"destination_wallet_id",V{wallet.destination_wallet_id}},{"amount",V{wallet.amount}},{"source_before",V{wallet.source_before}},{"source_after",V{wallet.source_after}},{"destination_before",V{wallet.destination_before}},{"destination_after",V{wallet.destination_after}}}});}for(const auto& lot:assets.value.transfer.lots)lots.push_back(V{D{{"source_inventory_id",V{lot.source_inventory_id}},{"destination_inventory_id",V{lot.destination_inventory_id}},{"lot_id",V{lot.lot_id}},{"item_id",V{lot.item_id}},{"quantity",V{lot.quantity}}}});out.emplace_back("asset_quote",V{D{{"ok",V{true}},{"receiving_weight_before",V{assets.value.receiving_weight_before}},{"receiving_weight_after",V{assets.value.receiving_weight_after}},{"receiving_capacity",V{assets.value.receiving_capacity}},{"money",V{money}},{"wallets",V{std::move(wallets)}},{"lots",V{std::move(lots)}}}});}
    }
    return V{std::move(out)};
}
// 命令内容交原规则判断；这里只将错误类型映为必拒的typed值，保留资格错误先于内容错误。
SiegeOrder draft_order(const V& value){SiegeOrder out;for(auto& p:*value.as<D>())out.field_order.push_back(p.first);if(auto x=value.find("action");x&&x->as<StableId>())out.action=*x->as<StableId>();if(auto x=value.find("node_id"))out.node_id=x->as<StableId>()?*x->as<StableId>():StableId{};if(auto x=value.find("engine_use")){SiegeEngineUse use;use.quantity=0;if(auto object=x->as<D>()){for(auto& p:*object)use.field_order.push_back(p.first);auto inventory=x->find("inventory_id"),item=x->find("item_id"),quantity=x->find("quantity");if(inventory&&inventory->as<StableId>()&&item&&item->as<StableId>()&&quantity&&quantity->as<Integer>()){use.inventory_id=*inventory->as<StableId>();use.item_id=*item->as<StableId>();use.quantity=*quantity->as<Integer>();}}out.engine_use=use;}return out;}
}
// 当前选择器未命中立即返回当前页头，不额外枚举库存或观察其他会话。
Outcome<BoundaryValue> military_siege_page(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const CommonCatalog& common,const MilitaryAccess& a,const StableId& selected,const MilitaryIntegrationAccess* integration_access){auto checked=base(c);if(!checked.ok)return checked;auto player=c.state().metadata.player_actor_id;auto id=selected.empty()?siege_for_actor(s,player):selected;if(id.empty())return {true,V{D{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{c.revision()}},{"siege_id",V{StableId{}}}}},{}};auto siege=get(s.sieges,id);if(!siege||!shape(*siege)||siege->phase=="closed")return fail();if(cat.siege_integration.enabled&&siege_for_actor(s,player)!=id)return fail("SiegeCommanderUnauthorized");auto fort=get(cat.strongholds,siege->stronghold_id);if(c.tick()<0||c.tick()>2147483647||!fort||fort->topology_id!=siege->topology_id)return fail();auto observation=observe_siege_orders_world(c,s,cat,a,*siege);if(!observation.ok)return fail();auto& obs=observation.value;auto topology=get(cat.siege_topologies,siege->topology_id);if(!topology)return fail();std::vector<StableId> pending;for(auto& p:siege->pending_nodes)pending.push_back(p.first);D shown{{"id",V{siege->id}},{"stronghold_id",V{siege->stronghold_id}},{"topology_id",V{siege->topology_id}},{"phase",V{siege->phase}},{"round",V{siege->round}},{"unit_nodes",pairs(siege->unit_nodes)},{"node_hp",pairs(siege->node_hp)},{"armies",pairs(siege->armies)},{"inner_hold_ticks",V{siege->inner_hold_ticks}},{"blockaded",V{siege->blockaded}},{"pending_node_ids",strings(pending)},{"result",siege->result?V{D{{"kind",V{siege->result->kind}}}}:V{D{}}}};D units,names;CommonPresentation presentation(c.state(),common);for(auto& p:siege->unit_nodes){auto unit=get(obs.units,p.first);auto source=get(s.military_units,p.first);if(!unit||!source)return fail();D row{{"army_id",V{unit->army_id}},{"fit_count",V{unit->fit_count}},{"morale",V{unit->morale}}};if(obs.engineering_preparation){const auto& prepared=obs.engineering_preparation->units;auto q=std::find_if(prepared.begin(),prepared.end(),[&](const auto& x){return x.unit_id==p.first;});if(q==prepared.end())return fail("MilitarySiegeEngineeringUnitRequired");row.emplace_back("fortification_level",V{q->level});}units.emplace_back(p.first,V{std::move(row)});auto title=presentation.display_name(source->commander_actor_id);if(!title.ok)return fail();auto actor=c.state().actors.find(source->commander_actor_id);auto definition=actor?common.actor_names.find(actor->prototype_id.value_or(actor->id)):nullptr;names.emplace_back(p.first,V{definition?title.value+"部":p.first});}V own_supply{D{}};auto own=active_army_for_actor(s,player);if(own&&own->commander_actor_id==player)own_supply=supply(preview_army_supply(c,s,cat,own->id));Integer side=-1,friendly=0;A others;std::vector<StableId> withdrawals;for(auto& p:siege->armies){auto army=get(obs.armies,p.first);if(!army)return fail();if(army->commander_actor_id==player)side=p.second;}for(auto& p:siege->armies){if(has(siege->withdrawn_army_ids,p.first))continue;auto army=get(s.armies,p.first);if(!army)return fail();if(army->commander_actor_id==player){withdrawals.push_back(p.first);continue;}auto title=presentation.display_name(army->commander_actor_id);auto camp=get(cat.garrisons,army->garrison_id);if(!title.ok||!camp)return fail();auto stock=pair(obs.public_goods,army->id);auto quantity=stock?pair(*stock,camp->food_item_id):nullptr;others.push_back(V{D{{"commander_name",V{title.value}},{"food_item_id",V{camp->food_item_id}},{"quantity",V{quantity?*quantity:Integer(0)}}}});if(p.second==side)++friendly;}D page{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{c.revision()}},{"siege_id",V{id}},{"identity",identity(c,*siege)},{"siege",V{std::move(shown)}},{"units",V{std::move(units)}},{"unit_names",V{std::move(names)}},{"own_supply_quote",std::move(own_supply)},{"other_supply_rows",V{std::move(others)}},{"friendly_reinforcement_count",V{friendly}},{"defender_food",V{obs.defender_food}},{"withdrawal_army_ids",strings(withdrawals)},{"engine_choices",engine_choices(*siege,*topology,obs,player)},{"last_damage_events",last_damage_events(*siege)}};if(cat.siege_integration.enabled)page.emplace_back("integration",integration_view(c,s,cat,a,*siege,presentation,integration_access));return {true,V{std::move(page)},{}};}
// 同一来源身份的批量草案只观察一次，按请求原序输出且在首成功模式立即停止。
Outcome<BoundaryValue> military_siege_draft(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const V& expected,const A& requests,bool first_ok){auto checked=base(c);if(!checked.ok)return checked;auto player=c.state().metadata.player_actor_id;auto current=siege_for_actor(s,player);auto object=expected.as<D>();if(!object||object->size()!=5)return fail();auto actor=expected.find("actor_id"),revision=expected.find("revision"),id=expected.find("siege_id"),round=expected.find("round"),phase=expected.find("phase");if(!actor||!actor->as<StableId>()||!revision||!revision->as<Integer>()||!id||!id->as<StableId>()||!round||!round->as<Integer>()||!phase||!phase->as<StableId>())return fail();if(current.empty())return fail("SiegeViewChanged");auto siege=get(s.sieges,current);if(!siege||!shape(*siege))return fail();auto actual=identity(c,*siege);if(*actor->as<StableId>()!=player||*revision->as<Integer>()!=c.revision()||*id->as<StableId>()!=siege->id||*round->as<Integer>()!=siege->round||*phase->as<StableId>()!=siege->phase)return fail("SiegeViewChanged");A quotes;if(requests.empty())return {true,V{D{{"ok",V{true}},{"identity",actual},{"quotes",V{quotes}}}},{}};auto fort=get(cat.strongholds,siege->stronghold_id);if(c.tick()<0||c.tick()>2147483647||!fort||fort->topology_id!=siege->topology_id)return fail();auto observation=observe_siege_orders_world(c,s,cat,a,*siege);auto topology=get(cat.siege_topologies,siege->topology_id);if(!observation.ok||!topology)return fail();for(std::size_t i=0;i<requests.size();++i){auto row=requests[i].as<D>();auto unit=requests[i].find("unit_id"),order=requests[i].find("order");if(!row||row->size()!=2||!unit||!unit->as<StableId>()||!order||!order->as<D>())return fail();auto quoted=preview_siege_order(*siege,*topology,observation.value,player,*unit->as<StableId>(),draft_order(*order));quotes.push_back(V{D{{"index",V{Integer(i)}},{"unit_id",V{*unit->as<StableId>()}},{"quote",quote(quoted)}}});if(first_ok&&quoted.ok)break;}return {true,V{D{{"ok",V{true}},{"identity",actual},{"quotes",V{std::move(quotes)}}}},{}};}
}
