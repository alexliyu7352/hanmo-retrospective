// 既有城防拓扑纯规则的typed实现；仅返回候选与真实资产/会战请求，不改世界、军团名册或库存。
#include "siege_model.hpp"
#include <algorithm>
#include <set>

namespace hanmo::resident {
namespace {
// 有序typed表按原键查值；查不到时返回空指针，不隐式补实体或字段。
template<class T> const T* value(const std::vector<std::pair<StableId,T>>& rows,const StableId& id) {
    for(const auto& row:rows) if(row.first==id) return &row.second;
    return nullptr;
}
// 候选账沿原插入顺序更新已有项，新键只追加一次。
template<class T> void put(std::vector<std::pair<StableId,T>>& rows,const StableId& id,T v) {
    for(auto& row:rows) if(row.first==id) {row.second=std::move(v);return;}
    rows.emplace_back(id,std::move(v));
}
// 只删除当前候选拥有的关联项，不接触公共库存或真实单位。
template<class T> void erase(std::vector<std::pair<StableId,T>>& rows,const StableId& id) {
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==id;}),rows.end());
}
// 定义与观察中的实体均从真实有限投影查找，不创建默认实体。
template<class T> const T* find(const std::vector<T>& rows,const StableId& id) {
    for(const auto& row:rows) if(row.id==id) return &row;
    return nullptr;
}
// 原规则中只有明确需要稳定仲裁的单位/节点键才排序。
template<class T> std::vector<StableId> ids(const std::vector<std::pair<StableId,T>>& rows) {
    std::vector<StableId> out;for(const auto& row:rows) out.push_back(row.first);
    std::sort(out.begin(),out.end());return out;
}
// 字符串集合仅检查成员关系，不重排作者或回执顺序。
bool has(const std::vector<StableId>& rows,const StableId& id) {return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 热态已是整数，范围仍按原规则逐项拒绝。
bool whole(Integer x,Integer low,Integer high) {return x>=low&&x<=high;}
// 原业务首错与路径完整返回，调用者丢弃失败候选。
Status fail(const StableId& code,const StableId& path={}) {return {false,{},{{code,path},ErrorKind::rule,{},{},{},{}}};}
// 带结果的失败同样不携带半成功状态。
template<class T> Outcome<T> error(const StableId& code,const StableId& path={}) {return {false,{},fail(code,path).error};}
// 成功只表示本纯规则完成，资源提交仍由唯一世界owner处理。
Status okay() {return {true,{},{}};}
// 查找双向拓扑边，未知路线返回空值供原规则报错。
const SiegeEdge* edge(const SiegeTopology& topology,const StableId& a,const StableId& b) {
    for(const auto& e:topology.edges) if((e.from==a&&e.to==b)||(e.from==b&&e.to==a)) return &e;
    return nullptr;
}
// 原定义校验顺序；只有冷目录校验提供战场/物品表，已校验热目录不重复复制作者表。
Status topology_valid(const SiegeTopology& t,const MilitaryBattleCatalog* catalog,const std::vector<StableId>* item_ids) {
    if(t.nodes.size()<4||t.nodes.size()>32) return fail("InvalidSiegeTopology");
    std::set<StableId> nodes;
    for(const auto& n:t.nodes) {
        if(!nodes.insert(n.id).second||!has({"approach","gate","side","wall","inner","supply"},n.kind)||!whole(n.max_hp,0,1000000)) return fail("InvalidSiegeNode");
        if(has({"gate","side","wall"},n.kind)&&n.max_hp<1) return fail("InvalidSiegeStructure");
        if(catalog&&!find(catalog->battlefields,n.battlefield_id)) return fail("UnknownSiegeBattlefield",n.id);
    }
    for(const auto& entry:std::vector<std::pair<StableId,StableId>>{{"attacker_entry_id",t.attacker_entry_id},{"defender_entry_id",t.defender_entry_id},{"inner_id",t.inner_id}})
        if(!nodes.count(entry.second)) return fail("UnknownSiegeNode",entry.first);
    if(t.attacker_entry_id==t.defender_entry_id||find(t.nodes,t.inner_id)->kind!="inner") return fail("InvalidSiegeEntrances");
    if(t.supply_node_ids.empty()) return fail("MissingSiegeSupplyPort");
    std::set<StableId> ports;
    for(const auto& id:t.supply_node_ids) if(!nodes.count(id)||find(t.nodes,id)->kind!="supply"||!ports.insert(id).second) return fail("InvalidSiegeSupplyPort");
    std::set<std::pair<StableId,StableId>> edges;
    for(const auto& e:t.edges) {
        if(!nodes.count(e.from)||!nodes.count(e.to)||e.from==e.to) return fail("InvalidSiegeEdge");
        if(!e.blocker_id.empty()&&(!nodes.count(e.blocker_id)||!has({"gate","side"},find(t.nodes,e.blocker_id)->kind))) return fail("UnknownSiegeBlocker");
        if(!edges.emplace(std::min(e.from,e.to),std::max(e.from,e.to)).second) return fail("DuplicateSiegeEdge");
    }
    std::set<StableId> seen{t.attacker_entry_id};
    for(std::size_t i=0;i<t.nodes.size();++i) for(const auto& e:t.edges) {
        if(seen.count(e.from)) seen.insert(e.to);
        if(seen.count(e.to)) seen.insert(e.from);
    }
    if(seen.size()!=nodes.size()) return fail("DisconnectedSiegeTopology");
    const auto& r=t.rules;
    for(const auto& p:std::vector<std::pair<StableId,Integer>>{{"hold_ticks",r.hold_ticks},{"blockade_units",r.blockade_units},{"starvation_ticks",r.starvation_ticks},{"max_ticks",r.max_ticks},{"gate_damage",r.gate_damage},{"repair_amount",r.repair_amount},{"engine_bonus",r.engine_bonus},{"engine_ticks",r.engine_ticks}})
        if(!whole(p.second,1,100000)) return fail("InvalidSiegeRule",p.first);
    if(!whole(r.surrender_morale,1,100)||!whole(r.wall_defense_bp,10000,30000)||(item_ids&&!has(*item_ids,r.engine_item_id))) return fail("InvalidSiegeRules");
    for(const auto& field:std::vector<std::pair<StableId,const std::vector<SiegeMaterial>*>>{{"engine_materials",&r.engine_materials},{"repair_materials",&r.repair_materials}}) {
        if(field.second->empty()) return fail("MissingSiegeMaterials",field.first);
        std::set<StableId> materials;
        for(const auto& m:*field.second) if((item_ids&&!has(*item_ids,m.item_id))||!materials.insert(m.item_id).second||!whole(m.quantity,1,100000)) return fail("InvalidSiegeMaterial",field.first);
    }
    return okay();
}
// 到城只认观察中的真实城内地点或同地图精确城点，预计抵达不能代替位置。
bool at_target(const SiegeObservation& o,const StableId& army_id) {
    const auto* a=find(o.armies,army_id);if(!a) return false;
    if(const auto* l=std::get_if<SettlementLocation>(&a->location)) return l->settlement_id==o.target_settlement_id;
    if(const auto* l=std::get_if<WildernessLocation>(&a->location)) return !o.world_map_id.empty()&&l->map_id==o.world_map_id&&l->point.x==o.target_point.x&&l->point.y==o.target_point.y;
    return false;
}
// 单位在等待真实节点会战回写时不可再行动；不凭占用标记修改兵额。
bool available(const SiegeRecord& s,const SiegeObservation& o,const StableId& id) {
    const auto* node=value(s.unit_nodes,id);const auto* u=find(o.units,id);
    return node&&u&&u->available&&!u->engaged&&u->fit_count>0&&u->morale>0&&!value(s.pending_nodes,*node);
}
// 有效守军即使正在会战仍占据节点，不能因engaged被当作消失。
bool side_at(const SiegeRecord& s,const SiegeObservation& o,const StableId& node,Integer side) {
    for(const auto& p:s.unit_nodes) {
        const auto* u=find(o.units,p.first);const auto* army=u?value(s.armies,u->army_id):nullptr;
        if(u&&army&&p.second==node&&*army==side&&u->fit_count>0&&u->available&&u->morale>0) return true;
    }
    return false;
}
// 敌方判断只查询当前真实部队，不使用模板兵额。
bool enemy_at(const SiegeRecord& s,const SiegeObservation& o,const StableId& node,Integer side) {return side_at(s,o,node,1-side);}
// 没有任何真实存活兵员才可认定一方耗尽；会战占用不等于全灭。
bool side_has_fit(const SiegeRecord& s,const SiegeObservation& o,Integer side) {
    for(const auto& p:s.unit_nodes) {const auto* u=find(o.units,p.first);const auto* army=u?value(s.armies,u->army_id):nullptr;if(u&&army&&*army==side&&u->fit_count>0) return true;}
    return false;
}
// 同方军团沿原表顺序返回，已撤离者不会继续收到撤军请求。
std::vector<StableId> side_armies(const SiegeRecord& s,Integer side) {
    std::vector<StableId> result;for(const auto& a:s.armies) if(a.second==side&&!has(s.withdrawn_army_ids,a.first)) result.push_back(a.first);return result;
}
// 最短图距离仅用于AI/退路仲裁，不能跳过关闭城门或敌军检查。
Integer distance(const SiegeTopology& t,const StableId& a,const StableId& b) {
    std::vector<std::pair<StableId,Integer>> queue{{a,0}};
    for(std::size_t i=0;i<queue.size();++i) {
        const auto id=queue[i].first;const auto steps=queue[i].second;if(id==b) return steps;
        for(const auto& n:t.nodes) if(!value(queue,n.id)&&edge(t,id,n.id)) queue.emplace_back(n.id,steps+1);
    }
    return 1000;
}
// 真实观察中未列材料视为零，不制造材料记录。
Integer stock(const SiegeGoods& goods,const StableId& army,const StableId& item) {const auto* g=value(goods,army);const auto* n=g?value(*g,item):nullptr;return n?*n:0;}
// 原材料预览按每条定义检查，合量占用由本轮reserve执行。
bool affordable(const SiegeGoods& goods,const StableId& army,const std::vector<SiegeMaterial>& costs) {
    for(const auto& c:costs) if(stock(goods,army,c.item_id)<c.quantity) return false;
    return true;
}
// 只写临时预算；正式公库消费由返回的资产请求统一执行。
void spend(SiegeGoods& goods,const StableId& army,const std::vector<SiegeMaterial>& costs) {
    auto ledger=*value(goods,army);for(const auto& c:costs) put(ledger,c.item_id,*value(ledger,c.item_id)-c.quantity);put(goods,army,std::move(ledger));
}
// 攻具声明必须指向本军真实公库和现有器械，省略即不用，不自动加伤害。
Outcome<std::optional<SiegeEngineUse>> engine_use(const SiegeTopology& t,const SiegeObservation& o,const StableId& id,const SiegeOrder& order) {
    if(!order.engine_use) return {true,{},{}};
    if(order.action!="assault") return error<std::optional<SiegeEngineUse>>("UnexpectedSiegeEngineUse",id);
    const auto& use=*order.engine_use;
    if(use.quantity!=1||(!use.field_order.empty()&&(use.field_order.size()!=3||!has(use.field_order,"inventory_id")||!has(use.field_order,"item_id")||!has(use.field_order,"quantity")))) return error<std::optional<SiegeEngineUse>>("InvalidSiegeEngineUse",id);
    const auto* u=find(o.units,id);const auto* a=u?find(o.armies,u->army_id):nullptr;
    if(!a||a->inventory_id.empty()||use.inventory_id!=a->inventory_id||use.item_id!=t.rules.engine_item_id) return error<std::optional<SiegeEngineUse>>("SiegeEngineSourceMismatch",id);
    if(stock(o.public_goods,a->id,use.item_id)<1) return error<std::optional<SiegeEngineUse>>("SiegeEngineUnavailable",id);
    return {true,use,{}};
}
// AI只从已扣材料和攻具占用的局部账挑本军可定位的器械。
std::optional<SiegeEngineUse> select_engine(const SiegeTopology& t,const SiegeObservation& o,const StableId& army,const SiegeGoods& ledger) {
    const auto* a=find(o.armies,army);if(!a||a->inventory_id.empty()||stock(ledger,army,t.rules.engine_item_id)<1) return {};
    return SiegeEngineUse{a->inventory_id,t.rules.engine_item_id,1,{}};
}
// 材料消费与攻具本轮独占先合量核足，再一次扣候选预算；器械本身不被消费。
Status reserve(SiegeGoods& ledger,const StableId& army,const SiegeOrderQuote& plan) {
    std::vector<std::pair<StableId,Integer>> required;
    for(const auto& c:plan.costs) {const auto* old=value(required,c.item_id);put(required,c.item_id,(old?*old:0)+c.quantity);}
    if(plan.engine_use) {const auto& use=*plan.engine_use;const auto* old=value(required,use.item_id);put(required,use.item_id,(old?*old:0)+use.quantity);}
    for(const auto& p:required) if(stock(ledger,army,p.first)<p.second) return fail("SiegeSharedStockInsufficient",army);
    if(!required.empty()) {auto goods=*value(ledger,army);for(const auto& p:required) put(goods,p.first,*value(goods,p.first)-p.second);put(ledger,army,std::move(goods));}
    return okay();
}
// 移动与封口共用通路检查，攻方没有绕过完整城门或交战节点的特权。
Status passage(const SiegeRecord& s,const SiegeTopology& t,const StableId& origin,const StableId& target,Integer side,const StableId& id) {
    const auto* e=edge(t,origin,target);if(!e) return fail("SiegeNodeNotAdjacent",id);
    if(side==0&&!e->blocker_id.empty()&&*value(s.node_hp,e->blocker_id)>0) return fail("SiegeGateClosed",id);
    if(value(s.pending_nodes,target)) return fail("SiegeNodeEngaged",id);
    return okay();
}
// 补给口必须可达且没有有效敌军或真实会战，预览与轮末复核完全共用。
Status blockade(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& id,const StableId& target) {
    const auto* u=find(o.units,id);const auto& origin=*value(s.unit_nodes,id);
    if(*value(s.armies,u->army_id)!=0||!has(t.supply_node_ids,target)) return fail("SiegeSupplyPortOutOfReach",id);
    if(origin!=target) {auto checked=passage(s,t,origin,target,0,id);if(!checked.ok) return checked;}
    else if(value(s.pending_nodes,target)) return fail("SiegeNodeEngaged",id);
    if(enemy_at(s,o,target,0)) return fail("SiegeSupplyPortContested",id);
    return okay();
}
// 单部行动解析保持原错误优先级；结构伤害不代表守军消失或已发生占领。
Outcome<SiegeOrderQuote> order_quote(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& id,const SiegeOrder& order) {
    if(!available(s,o,id)) return error<SiegeOrderQuote>("SiegeUnitUnavailable",id);
    for(const auto& field:order.field_order) if(!has({"action","node_id","engine_use"},field)) return error<SiegeOrderQuote>("UnexpectedSiegeOrderField",id);
    if(!has({"move","assault","repair","build_engine","blockade","wait","sortie"},order.action)) return error<SiegeOrderQuote>("UnknownSiegeAction",id);
    const auto& origin=*value(s.unit_nodes,id);const auto target=order.node_id.value_or(origin);const auto* node=find(t.nodes,target);
    if(!node) return error<SiegeOrderQuote>("UnknownSiegeNode",id);
    const auto& u=*find(o.units,id);const auto side=*value(s.armies,u.army_id);
    SiegeOrderQuote quote;quote.action=order.action;quote.node_id=target;
    if(value(s.engine_work,id)&&order.action!="wait") return error<SiegeOrderQuote>("SiegeEngineWorkPending",id);
    const auto use=engine_use(t,o,id,order);if(!use.ok) return {false,{},use.error};quote.engine_use=use.value;
    if(order.action=="move"||order.action=="sortie") {auto checked=passage(s,t,origin,target,side,id);if(!checked.ok) return {false,{},checked.error};}
    else if(order.action=="assault") {
        if(side!=0||!has({"gate","side","wall"},node->kind)||*value(s.node_hp,target)<=0||(origin!=target&&!edge(t,origin,target))) return error<SiegeOrderQuote>("SiegeStructureOutOfReach",id);
        Integer damage=t.rules.gate_damage+u.fit_count/50;if(use.value) damage+=t.rules.engine_bonus;
        // 显式工程资格只替换原始攻门公式；实际器械、墙防、材料和伤害截顶仍走原路径。
        if(o.engineering_preparation) {
            const auto& prepared=*o.engineering_preparation;
            const auto found=std::find_if(prepared.units.begin(),prepared.units.end(),[&](const auto& row){return row.unit_id==id;});
            if(found==prepared.units.end()||found->army_id!=u.army_id||found->fit_count!=u.fit_count) return error<SiegeOrderQuote>("MilitarySiegeEngineeringRosterMismatch",id);
            auto base=military_siege_engineering_assault_base({},u.fit_count,found->level,bool(use.value));
            if(!base.ok) return {false,{},base.error};damage=base.value;
        }
        for(const auto& wall:t.nodes) if(wall.kind=="wall"&&*value(s.node_hp,wall.id)>0&&edge(t,wall.id,target)) {damage=damage*10000/t.rules.wall_defense_bp;break;}
        quote.damage=std::max<Integer>(1,damage);
    } else if(order.action=="repair") {
        if(side!=1||!has({"gate","side","wall"},node->kind)||*value(s.node_hp,target)>=node->max_hp||(origin!=target&&!edge(t,origin,target))||enemy_at(s,o,target,side)||value(s.pending_nodes,target)) return error<SiegeOrderQuote>("SiegeRepairUnavailable",id);
        quote.costs=t.rules.repair_materials;
    } else if(order.action=="build_engine") {
        if(target!=origin) return error<SiegeOrderQuote>("SiegeEngineMustHold",id);
        quote.costs=t.rules.engine_materials;
    } else if(order.action=="blockade") {auto checked=blockade(s,t,o,id,target);if(!checked.ok) return {false,{},checked.error};}
    else if(order.action=="wait"&&target!=origin) return error<SiegeOrderQuote>("SiegeWaitMustHold",id);
    if(!affordable(o.public_goods,u.army_id,quote.costs)) return error<SiegeOrderQuote>("SiegeMaterialsInsufficient",id);
    return {true,std::move(quote),{}};
}
// 本轮报价只借玩家提交与先前保存AI；后生成的攻击集合会取消同轮修缮材料消费。
using Plans=std::vector<std::pair<StableId,SiegeOrderQuote>>;
Outcome<Plans> prepare_orders(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeOrders& orders) {
    auto checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    if(s.phase!="active") return error<Plans>("SiegeNotActive");
    if(actor!=o.controller_actor_id) return error<Plans>("SiegeCommanderUnauthorized");
    const auto* selected=value(o.control_sets,actor);const std::vector<StableId> empty;const auto& controlled=selected?*selected:empty;
    for(const auto& p:orders) if(!has(controlled,p.first)||!value(s.unit_nodes,p.first)) return error<Plans>("SiegeUnitUnauthorized",p.first);
    Plans plans;
    for(const auto& id:ids(s.unit_nodes)) {
        if(!available(s,o,id)) {if(value(orders,id)) return error<Plans>("SiegeUnitUnavailable",id);continue;}
        const auto* order=value(has(controlled,id)?orders:s.ai_plans,id);if(!order) return error<Plans>("MissingSiegeOrder",id);
        auto quote=order_quote(s,t,o,id,*order);if(!quote.ok) return {false,{},quote.error};plans.emplace_back(id,std::move(quote.value));
    }
    for(const auto& p:s.ai_plans) if(!value(s.unit_nodes,p.first)||has(controlled,p.first)) return error<Plans>("UnexpectedSiegeAIOrder",p.first);
    std::vector<StableId> attacked;for(const auto& p:plans) if(p.second.action=="assault") attacked.push_back(p.second.node_id);
    for(auto& p:plans) if(p.second.action=="repair"&&has(attacked,p.second.node_id)) {p.second.under_attack=true;p.second.costs.clear();}
    auto ledger=o.public_goods;
    for(const auto& id:ids(plans)) {checked=reserve(ledger,find(o.units,id)->army_id,*value(plans,id));if(!checked.ok) return {false,{},checked.error};}
    return {true,std::move(plans),{}};
}
// 到城加入只引用该军团已有存活单位；观察次序保留为原会话插入次序。
Status join_army(SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& army,Integer side) {
    if(!find(o.armies,army)||!at_target(o,army)) return fail("SiegeArmyNotAtTarget",army);
    const auto& entry=side==0?t.attacker_entry_id:t.defender_entry_id;bool found=false;
    for(const auto& u:o.units) if(u.army_id==army&&u.fit_count>0) {
        if(value(s.unit_nodes,u.id)) return fail("DuplicateSiegeUnit",u.id);
        s.unit_nodes.emplace_back(u.id,entry);found=true;
    }
    if(!found) return fail("SiegeArmyHasNoUnits",army);
    put(s.armies,army,side);return okay();
}
// 溃退沿现有边后撤一节点，优先合法真实来源，再按到本方入口距离及稳定节点ID选择。
StableId retreat_node(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& id,const StableId& contact,const StableId& origin) {
    const auto side=*value(s.armies,find(o.units,id)->army_id);const auto& entry=side==0?t.attacker_entry_id:t.defender_entry_id;
    std::vector<StableId> nodes;for(const auto& n:t.nodes) nodes.push_back(n.id);std::sort(nodes.begin(),nodes.end());
    StableId best;Integer score=10000;
    for(const auto& node:nodes) {
        const auto* e=edge(t,contact,node);
        if(node==contact||!e||value(s.pending_nodes,node)||enemy_at(s,o,node,side)) continue;
        if(side==0&&!e->blocker_id.empty()&&*value(s.node_hp,e->blocker_id)>0) continue;
        if(node==origin) return node;
        const auto d=distance(t,node,entry);if(d<score) {best=node;score=d;}
    }
    return best;
}
// 存活可用守军的平均士气决定协商窗口，不因日期替代个人立场。
Integer defender_morale(const SiegeRecord& s,const SiegeObservation& o) {
    Integer total=0,count=0;for(const auto& p:s.unit_nodes) {const auto& u=*find(o.units,p.first);if(*value(s.armies,u.army_id)==1&&u.fit_count>0&&u.available) {total+=u.morale;++count;}}
    return total/std::max<Integer>(1,count);
}
// 补粮须同时匹配物品、公库、组织和收粮军团；坏来源列表不被当作普通非粮。
Outcome<bool> classify_supply(const SiegeObservedReceipt& receipt,const std::vector<SiegeFoodSource>& destinations) {
    if(receipt.item_id.empty()||receipt.to_inventory_id.empty()||receipt.recipient_owner.id.empty()||receipt.recipient_owner.extra) return error<bool>("InvalidSiegeSupplyReceipt");
    std::set<StableId> inventories;const SiegeFoodSource* source=nullptr;
    for(const auto& d:destinations) {
        if(d.inventory_id.empty()||d.food_item_id.empty()||d.organization_id.empty()||!inventories.insert(d.inventory_id).second) return error<bool>("InvalidSiegeFoodSources");
        if(d.inventory_id==receipt.to_inventory_id) source=&d;
    }
    return {true,source&&receipt.recipient_owner.kind==Owner::Kind::organization&&receipt.recipient_owner.id==source->organization_id&&receipt.item_id==source->food_item_id&&receipt.recipient_army_id==source->recipient_army_id,{}};
}
// 战略终局列明真实待释放来源，未完成器械停止且不退材料，根层原子释放实际占用。
void release_terminal(SiegeRecord& s) {
    std::vector<StableId> sources;for(const auto& p:s.pending_nodes) sources.push_back(p.second.source_id);
    if(!sources.empty()) {
        SiegeEvent event;event.kind="siege_release_request";event.battle_source_ids=std::move(sources);event.army_ids=side_armies(s,0);
        auto defenders=side_armies(s,1);event.army_ids->insert(event.army_ids->end(),defenders.begin(),defenders.end());event.reason=s.result->kind;s.last_events.push_back(std::move(event));
    }
    s.pending_nodes.clear();s.engine_work.clear();s.ai_plans.clear();
}
}

// 世界军议复用同一可行动谓词，待回写节点与真实会战占用都禁止重复行动。
bool siege_unit_available(const SiegeRecord& s,const SiegeObservation& o,const StableId& unit) {return available(s,o,unit);}

// 世界观察复用同一守点谓词，engaged不抹去仍存活可用且有士气的守军。
bool siege_side_at(const SiegeRecord& s,const SiegeObservation& o,const StableId& node,Integer side) {return side_at(s,o,node,side);}

// 冷作者目录检查包括战场与材料的真实交叉引用，失败不安装热目录。
Status validate_siege_topology(const SiegeTopology& t,const MilitaryBattleCatalog& catalog,const std::vector<StableId>& item_ids) {return topology_valid(t,&catalog,&item_ids);}

// 纯会话检查按原GD首错顺序进行；强据点/物品目录关联由世界观察边界先行验证。
Status validate_siege(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o) {
    auto checked=topology_valid(t,nullptr,nullptr);if(!checked.ok) return checked;
    if(o.engineering_preparation) {
        const auto& prepared=*o.engineering_preparation;
        checked=validate_military_siege_engineering_preparation_shape(prepared);if(!checked.ok) return checked;
        // resolve内部生成下一轮AI可复用本次已冻结技艺；对外绑定器仍要求精确当前round。
        if(prepared.siege_id!=s.id||prepared.topology_id!=t.id||o.tick<prepared.from_tick||o.tick>prepared.to_tick||s.round<prepared.round||s.round>prepared.round+1)
            return fail("MilitarySiegeEngineeringObservationMismatch",s.id);
    }
    if(!has({"active","result","closed"},s.phase)) return fail("InvalidSiege");
    if(s.topology_id!=t.id) return fail("SiegeStrongholdMismatch");
    for(const auto& p:std::vector<std::pair<StableId,Integer>>{{"started_tick",s.started_tick},{"last_tick",s.last_tick},{"round",s.round},{"inner_hold_ticks",s.inner_hold_ticks},{"unsupplied_ticks",s.unsupplied_ticks}})
        if(!whole(p.second,0,2147483647)) return fail("InvalidSiegeTime",p.first);
    if(!whole(o.tick,s.last_tick,2147483647)) return fail("InvalidSiegeObservationTime");
    if(s.last_tick<s.started_tick||s.round!=s.last_tick-s.started_tick+1||s.inner_hold_ticks>s.last_tick-s.started_tick||s.unsupplied_ticks>s.last_tick-s.started_tick) return fail("InvalidSiegeTime");
    if(s.phase=="active"&&s.result) return fail("PrematureSiegeResult");
    if((s.phase=="result"||s.phase=="closed")&&(!s.result||!has({"occupied","surrendered","defended","truce","withdrawn","timeout","aborted"},s.result->kind)||!whole(s.result->tick,s.started_tick,o.tick)||!whole(s.result->winner_side,-1,1))) return fail("InvalidSiegeResult");
    if(!whole(o.defender_food,0,2147483647)) return fail("MissingSiegeObservation");
    for(const auto& control:o.control_sets) {
        std::set<StableId> seen;for(const auto& id:control.second) if(!find(o.units,id)||!seen.insert(id).second) return fail("InvalidSiegeControlSet");
    }
    std::set<StableId> departed;for(const auto& id:s.withdrawn_army_ids) if(!value(s.armies,id)||!departed.insert(id).second) return fail("InvalidSiegeWithdrawal");
    std::set<StableId> receipts;for(const auto& id:s.seen_receipt_ids) if(id.empty()||!receipts.insert(id).second||!find(o.receipts,id)) return fail("MissingSiegeReceipt",id);
    if(s.node_hp.size()!=t.nodes.size()) return fail("SiegeNodeMismatch");
    std::set<StableId> hp_ids;for(const auto& hp:s.node_hp) {const auto* node=find(t.nodes,hp.first);if(!node||!hp_ids.insert(hp.first).second||!whole(hp.second,0,node->max_hp)) return fail("InvalidSiegeDurability",hp.first);}
    std::set<StableId> army_ids;
    for(const auto& a:s.armies) {
        if(!whole(a.second,0,1)||!find(o.armies,a.first)||!army_ids.insert(a.first).second) return fail("UnknownObservedSiegeArmy",a.first);
        if(s.phase=="active"&&!has(s.withdrawn_army_ids,a.first)&&!at_target(o,a.first)) return fail("SiegeArmyNotAtTarget",a.first);
        const auto* goods=value(o.public_goods,a.first);if(!goods) return fail("UnknownSiegePublicGoods",a.first);
        for(const auto& g:*goods) if(!whole(g.second,0,2147483647)) return fail("InvalidSiegePublicGoods",a.first);
    }
    std::set<StableId> unit_ids;
    for(const auto& p:s.unit_nodes) {
        const auto* u=find(o.units,p.first);
        if(!u||!value(s.armies,u->army_id)||has(s.withdrawn_army_ids,u->army_id)||!find(t.nodes,p.second)||!unit_ids.insert(p.first).second) return fail("UnknownObservedSiegeUnit",p.first);
        if(!whole(u->fit_count,0,1000000)||!whole(u->morale,0,100)) return fail("InvalidObservedSiegeUnit",p.first);
    }
    for(const auto& p:s.engine_work) if(!value(s.unit_nodes,p.first)||!find(t.nodes,p.second.node_id)||!whole(p.second.remaining,1,t.rules.engine_ticks)) return fail("InvalidSiegeEngineWork",p.first);
    for(const auto& p:s.pending_nodes) {
        const auto& pending=p.second;if(!find(t.nodes,p.first)||pending.node_id!=p.first||pending.unit_ids.size()!=pending.origin_nodes.size()) return fail("InvalidSiegePendingBattle",p.first);
        for(const auto& id:pending.unit_ids) {const auto* current=value(s.unit_nodes,id);const auto* origin=value(pending.origin_nodes,id);if(!current||*current!=p.first||!origin||!find(t.nodes,*origin)) return fail("InvalidSiegePendingUnit",id);}
    }
    return okay();
}

// 据已校验据点与到城观察建立原会话，不生成兵额、材料或宏观位置。
Outcome<SiegeRecord> create_siege(const SiegeCreateRequest& request,const SiegeTopology& t,const SiegeObservation& o) {
    auto checked=topology_valid(t,nullptr,nullptr);if(!checked.ok) return {false,{},checked.error};
    if(request.id.empty()||request.campaign_id.empty()) return error<SiegeRecord>("InvalidSiegeIdentity");
    if(!whole(o.tick,0,2147483646)) return error<SiegeRecord>("InvalidSiegeObservationTime");
    SiegeRecord s;s.id=request.id;s.campaign_id=request.campaign_id;s.stronghold_id=request.stronghold_id;s.topology_id=t.id;s.started_tick=o.tick;s.last_tick=o.tick;
    for(const auto& n:t.nodes) s.node_hp.emplace_back(n.id,n.max_hp);
    for(Integer side:{0,1}) {
        const auto& armies=side==0?request.attacker_army_ids:request.defender_army_ids;if(armies.empty()) return error<SiegeRecord>("MissingSiegeArmy");
        for(const auto& id:armies) {if(value(s.armies,id)) return error<SiegeRecord>("DuplicateSiegeArmy");checked=join_army(s,t,o,id,side);if(!checked.ok) return {false,{},checked.error};}
    }
    checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    auto ai=make_siege_ai(s,t,o);if(!ai.ok) return {false,{},ai.error};s.ai_plans=std::move(ai.value);return {true,std::move(s),{}};
}

// 单部预览只认当前控制范围，正式命令与AI仍复用同一解析路径。
Outcome<SiegeOrderQuote> preview_siege_order(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const StableId& id,const SiegeOrder& order) {
    auto checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    if(s.phase!="active") return error<SiegeOrderQuote>("SiegeNotActive");
    const auto* controlled=value(o.control_sets,actor);if(actor!=o.controller_actor_id||!controlled||!has(*controlled,id)) return error<SiegeOrderQuote>("SiegeUnitUnauthorized",id);
    return order_quote(s,t,o,id,order);
}

// 整套预览会校验已保存AI与共享材料/器械额度，不向玩家泄露AI下一步命令。
Status validate_siege_orders(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeOrders& orders) {
    auto checked=prepare_orders(s,t,o,actor,orders);return checked.ok?okay():Status{false,{},checked.error};
}

// AI按稳定单位ID和作者节点/行动顺序选择公开状态最高分，不读取未提交玩家计划。
Outcome<SiegeOrders> make_siege_ai(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o) {
    auto checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    SiegeOrders orders;auto goods=o.public_goods;const auto* controlled=value(o.control_sets,o.controller_actor_id);
    for(const auto& id:ids(s.unit_nodes)) {
        if(!available(s,o,id)||(controlled&&has(*controlled,id))) continue;
        SiegeOrder choice;choice.action="wait";choice.node_id=*value(s.unit_nodes,id);Integer best=-1;
        const auto& army=find(o.units,id)->army_id;const auto side=*value(s.armies,army);
        for(const auto& node:t.nodes) for(const auto& action:side==0?std::vector<StableId>{"assault","move","blockade"}:std::vector<StableId>{"repair","sortie"}) {
            SiegeOrder candidate;candidate.action=action;candidate.node_id=node.id;
            if(action=="assault") candidate.engine_use=select_engine(t,o,army,goods);
            auto offer=order_quote(s,t,o,id,candidate);if(!offer.ok) continue;
            auto test=goods;if(!reserve(test,army,offer.value).ok) continue;
            Integer score=0;
            if(action=="assault") score=has({"gate","side"},node.kind)?35:20;
            else if(action=="repair") score=40;
            else if(action=="sortie") score=enemy_at(s,o,node.id,side)?30:-1;
            else if(action=="blockade") score=18;
            else score=node.id==t.inner_id?50:15-distance(t,node.id,t.inner_id);
            if(score>best) {best=score;choice=std::move(candidate);}
        }
        orders.emplace_back(id,choice);auto selected=order_quote(s,t,o,id,choice);if(!selected.ok) return {false,{},selected.error};
        checked=reserve(goods,army,selected.value);if(!checked.ok) return {false,{},checked.error};
    }
    return {true,std::move(orders),{}};
}

// 半日结算依次应用旧报价、器械工期、真实接敌、封口与补粮，最后判终局并保存下一轮AI。
Outcome<SiegeResolution> resolve_siege(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeOrders& orders,Integer finish_tick) {
    auto prepared=prepare_orders(s,t,o,actor,orders);if(!prepared.ok) return {false,{},prepared.error};
    if(s.phase!="active"||finish_tick!=s.last_tick+1||o.tick!=s.last_tick) return error<SiegeResolution>("SiegeInvalidStep");
    const auto& plans=prepared.value;SiegeResolution result;result.siege=s;auto& next=result.siege;next.last_events.clear();auto goods=o.public_goods;
    for(const auto& id:ids(plans)) {
        const auto& plan=*value(plans,id);const auto& army=find(o.units,id)->army_id;
        if(!affordable(goods,army,plan.costs)) return error<SiegeResolution>("SiegeSharedStockInsufficient",army);
        spend(goods,army,plan.costs);
        for(const auto& cost:plan.costs) result.asset_effects.push_back({"consume",army,cost.item_id,cost.quantity,next.id+":"+std::to_string(finish_tick)+":"+id});
        if(plan.action=="move"||plan.action=="sortie") put(next.unit_nodes,id,plan.node_id);
        else if(plan.action=="assault") {
            const auto old=*value(next.node_hp,plan.node_id);const auto damage=std::min(old,*plan.damage);put(next.node_hp,plan.node_id,old-damage);
            SiegeEvent e;e.kind="siege_damage";e.unit_id=id;e.node_id=plan.node_id;e.damage=damage;e.remaining_hp=old-damage;next.last_events.push_back(std::move(e));
        } else if(plan.action=="repair") {
            if(!plan.under_attack) put(next.node_hp,plan.node_id,std::min(find(t.nodes,plan.node_id)->max_hp,*value(next.node_hp,plan.node_id)+t.rules.repair_amount));
            else {SiegeEvent e;e.kind="siege_repair_blocked";e.unit_id=id;e.node_id=plan.node_id;next.last_events.push_back(std::move(e));}
        } else if(plan.action=="build_engine") put(next.engine_work,id,SiegeEngineWork{*value(next.unit_nodes,id),t.rules.engine_ticks,{}});
    }
    // 工期沿原插入顺序推进；本轮开工也立刻消费一个半日。
    std::vector<StableId> works;for(const auto& p:next.engine_work) works.push_back(p.first);
    for(const auto& id:works) {
        const auto work=*value(next.engine_work,id);
        if(!available(next,o,id)||*value(next.unit_nodes,id)!=work.node_id) {
            erase(next.engine_work,id);SiegeEvent e;e.kind="siege_engine_interrupted";e.unit_id=id;next.last_events.push_back(std::move(e));continue;
        }
        auto remaining=work;--remaining.remaining;put(next.engine_work,id,remaining);
        if(remaining.remaining==0) {
            result.asset_effects.push_back({"produce",find(o.units,id)->army_id,t.rules.engine_item_id,1,next.id+":"+std::to_string(finish_tick)+":"+id+":engine"});erase(next.engine_work,id);
        }
    }
    // 接敌节点按作者顺序，参战单位与来源按稳定ID形成不可重排的永久事实输入。
    for(const auto& node:t.nodes) if(side_at(next,o,node.id,0)&&side_at(next,o,node.id,1)&&!value(next.pending_nodes,node.id)) {
        SiegeBattleRequest request;request.source_id=next.id+":"+std::to_string(finish_tick)+":"+node.id;request.node_id=node.id;request.battlefield_id=node.battlefield_id;
        for(const auto& id:ids(next.unit_nodes)) if(*value(next.unit_nodes,id)==node.id&&available(next,o,id)) {
            request.unit_ids.push_back(id);request.origin_nodes.emplace_back(id,*value(s.unit_nodes,id));
        }
        put(next.pending_nodes,node.id,request);result.battle_requests.push_back(std::move(request));
    }
    std::vector<StableId> covered,blockers;
    for(const auto& p:plans) if(p.second.action=="blockade"&&available(next,o,p.first)&&blockade(next,t,o,p.first,p.second.node_id).ok) {covered.push_back(p.second.node_id);blockers.push_back(p.first);}
    next.blockaded=Integer(blockers.size())>=t.rules.blockade_units;
    for(const auto& port:t.supply_node_ids) if(!has(covered,port)) next.blockaded=false;
    bool supplied=false;
    for(const auto& receipt:o.receipts) {
        if(receipt.kind!="military_delivery"||receipt.stronghold_id!=next.stronghold_id||has(next.seen_receipt_ids,receipt.id)) continue;
        if(!whole(receipt.tick,0,finish_tick)||!whole(receipt.quantity,1,2147483647)) return error<SiegeResolution>("InvalidSiegeSupplyReceipt",receipt.id);
        if(receipt.tick<next.started_tick) continue;
        next.seen_receipt_ids.push_back(receipt.id);
        if(receipt.recipient_owner.kind!=Owner::Kind::organization) return error<SiegeResolution>("InvalidSiegeSupplyReceipt",receipt.id);
        auto delivered=classify_supply(receipt,o.defender_food_sources);if(!delivered.ok) return error<SiegeResolution>(delivered.error.original.code,receipt.id);
        if(delivered.value) supplied=true;
    }
    next.unsupplied_ticks=next.blockaded&&o.defender_food==0&&!supplied?next.unsupplied_ticks+1:0;
    const bool occupying=side_at(next,o,t.inner_id,0)&&!side_at(next,o,t.inner_id,1)&&!value(next.pending_nodes,t.inner_id);
    next.inner_hold_ticks=occupying?next.inner_hold_ticks+1:0;
    if(!side_has_fit(next,o,0)&&next.pending_nodes.empty()) {
        next.phase="result";next.result=SiegeResult{"defended",1,finish_tick,{},{},{}};
        SiegeEvent e;e.kind="withdrawal_request";e.army_ids=side_armies(next,0);next.last_events.push_back(std::move(e));
    } else if(next.inner_hold_ticks>=t.rules.hold_ticks&&o.defender_commander_out) {
        next.phase="result";next.result=SiegeResult{"occupied",0,finish_tick,{},{},{}};
        SiegeEvent e;e.kind="occupation_request";e.stronghold_id=next.stronghold_id;e.campaign_id=next.campaign_id;next.last_events.push_back(std::move(e));
    } else if(finish_tick-next.started_tick>=t.rules.max_ticks) {
        next.phase="result";next.result=SiegeResult{"timeout",1,finish_tick,{},{},{}};
        SiegeEvent e;e.kind="withdrawal_request";e.army_ids=side_armies(next,0);next.last_events.push_back(std::move(e));
    }
    if(next.phase!="active") {release_terminal(next);result.battle_requests.clear();}
    next.last_tick=finish_tick;++next.round;
    auto future=o;future.tick=finish_tick;future.public_goods=std::move(goods);
    for(const auto& effect:result.asset_effects) if(effect.kind=="produce") {
        auto ledger=*value(future.public_goods,effect.army_id);put(ledger,effect.item_id,stock(future.public_goods,effect.army_id,effect.item_id)+effect.quantity);put(future.public_goods,effect.army_id,std::move(ledger));
    }
    auto ai=make_siege_ai(next,t,future);if(!ai.ok) return {false,{},ai.error};next.ai_plans=next.phase=="active"?std::move(ai.value):SiegeOrders{};
    result.events=next.last_events;return {true,std::move(result),{}};
}

// 援军只凭真实到城回执及当前同方授权加入，边界重算AI但绝不增造兵员或物资。
Outcome<SiegeRecord> join_siege(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& army,Integer side,const StableId& receipt_id) {
    if(s.phase!="active"||value(s.armies,army)||!whole(side,0,1)) return error<SiegeRecord>("SiegeReinforcementInvalid");
    const auto* receipt=find(o.receipts,receipt_id);
    if(!receipt||receipt->kind!="campaign_arrival"||receipt->army_id!=army||receipt->stronghold_id!=s.stronghold_id||!whole(receipt->tick,s.started_tick,o.tick)) return error<SiegeRecord>("SiegeArrivalReceiptRequired");
    if(has(s.seen_receipt_ids,receipt_id)) return error<SiegeRecord>("SiegeArrivalAlreadyUsed");
    const auto* joining=find(o.armies,army);const StableId organization=joining?joining->organization_id:StableId{};
    const auto* allies=value(o.side_allies,std::to_string(side));bool permitted=allies&&has(*allies,organization);
    for(const auto& member:side_armies(s,side)) {const auto* a=find(o.armies,member);if(a&&a->organization_id==organization) permitted=true;}
    if(!permitted) return error<SiegeRecord>("SiegeReinforcementSideUnauthorized");
    auto next=s;auto checked=join_army(next,t,o,army,side);if(!checked.ok) return {false,{},checked.error};next.seen_receipt_ids.push_back(receipt_id);
    auto ai=make_siege_ai(next,t,o);if(!ai.ok) return {false,{},ai.error};next.ai_plans=std::move(ai.value);return {true,std::move(next),{}};
}

// 节点会战只凭真实已回写收据释放；退败幸存者须有现存合法退路，不能静默删人。
Outcome<SiegeRecord> finish_siege_battle(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& node,const StableId& receipt_id) {
    const auto* pending=value(s.pending_nodes,node);if(!pending) return error<SiegeRecord>("SiegeBattleNotPending");
    const auto* receipt=find(o.receipts,receipt_id);
    if(!receipt||receipt->kind!="military_battle"||receipt->phase!="completed"||receipt->source_id!=pending->source_id||!whole(receipt->winner_side,-1,1)) return error<SiegeRecord>("SiegeBattleReceiptMismatch");
    if(!whole(receipt->tick,s.started_tick,o.tick)) return error<SiegeRecord>("SiegeBattleReceiptTime");
    auto next=s;
    for(const auto& id:pending->unit_ids) {
        const auto* u=find(o.units,id);if(!u) return error<SiegeRecord>("UnknownObservedSiegeUnit",id);
        const auto* origin=value(pending->origin_nodes,id);if(!origin) return error<SiegeRecord>("InvalidSiegePendingUnit",id);
        if(receipt->winner_side<0) put(next.unit_nodes,id,*origin);
        else {
            const auto* side=value(next.armies,u->army_id);if(!side) return error<SiegeRecord>("UnknownObservedSiegeUnit",id);
            if(*side!=receipt->winner_side) {
                const auto destination=retreat_node(s,t,o,id,node,*origin);
                if(destination.empty()&&u->fit_count>0) return error<SiegeRecord>("SiegeRetreatUnresolved",id);
                put(next.unit_nodes,id,destination.empty()?node:destination);
            }
        }
    }
    erase(next.pending_nodes,node);next.seen_receipt_ids.push_back(receipt_id);
    auto ai=make_siege_ai(next,t,o);if(!ai.ok) return {false,{},ai.error};next.ai_plans=std::move(ai.value);return {true,std::move(next),{}};
}

// 真实指挥者可撤本军或交涉；只返回后续请求，归属、公产和宏观路线始终由根层原子回写。
Outcome<SiegeResolution> strategic_siege(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeStrategicCommand& command) {
    auto checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    if(s.phase!="active") return error<SiegeResolution>("SiegeNotActive");
    SiegeResolution result;result.siege=s;auto& next=result.siege;std::vector<SiegeEvent> events;
    if(command.kind=="withdraw") {
        const auto* side=value(s.armies,command.army_id);const auto* army=find(o.armies,command.army_id);
        if(!side||has(s.withdrawn_army_ids,command.army_id)||!army||army->commander_actor_id!=actor) return error<SiegeResolution>("SiegeArmyCommandRequired");
        const auto& entry=*side==0?t.attacker_entry_id:t.defender_entry_id;
        for(const auto& p:s.unit_nodes) {const auto& u=*find(o.units,p.first);if(u.army_id==command.army_id&&u.fit_count>0&&(p.second!=entry||!available(s,o,p.first))) return error<SiegeResolution>("SiegeRetreatRouteRequired");}
        for(const auto& field:command.field_order) if(!has({"kind","army_id"},field)) return error<SiegeResolution>("UnexpectedSiegeStrategicField",field);
        if(!command.unexpected_fields.empty()) return error<SiegeResolution>("UnexpectedSiegeStrategicField",command.unexpected_fields.front());
        if(!command.terms.empty()) return error<SiegeResolution>("UnexpectedSiegeStrategicField","terms");
        std::vector<StableId> departed;for(const auto& p:next.unit_nodes) if(find(o.units,p.first)->army_id==command.army_id) departed.push_back(p.first);
        for(const auto& id:departed) {erase(next.unit_nodes,id);erase(next.engine_work,id);}
        next.withdrawn_army_ids.push_back(command.army_id);SiegeEvent event;event.kind="withdrawal_request";event.army_ids=std::vector<StableId>{command.army_id};events.push_back(std::move(event));
        if(side_armies(next,0).empty()) {next.phase="result";next.result=SiegeResult{"withdrawn",1,o.tick,{},{},{}};}
    } else if(command.kind=="negotiate") {
        if(!s.pending_nodes.empty()) return error<SiegeResolution>("SiegeBattlePending");
        if(!has(o.authorized_actor_ids,actor)||o.defender_actor_id.empty()) return error<SiegeResolution>("SiegeNegotiatorRequired");
        if(!has({"safe_departure","integrate","truce"},command.terms)) return error<SiegeResolution>("UnknownSiegeTerms");
        if(!whole(o.willingness,0,100)||o.willingness<50) return error<SiegeResolution>("SiegeTermsRejected");
        if(command.terms!="truce"&&(s.unsupplied_ticks<t.rules.starvation_ticks||defender_morale(s,o)>=t.rules.surrender_morale)) return error<SiegeResolution>("SiegeNegotiationNotReady");
        for(const auto& field:command.field_order) if(!has({"kind","terms"},field)) return error<SiegeResolution>("UnexpectedSiegeStrategicField",field);
        if(!command.unexpected_fields.empty()) return error<SiegeResolution>("UnexpectedSiegeStrategicField",command.unexpected_fields.front());
        if(!command.army_id.empty()) return error<SiegeResolution>("UnexpectedSiegeStrategicField","army_id");
        // 新能力的整编只提出真实交接请求；逐将决定和资产办理齐备以前仍保留原围城与军令。
        const bool integration=command.terms=="integrate"&&o.negotiated_integration;
        if(!integration) {next.phase="result";next.result=SiegeResult{command.terms=="truce"?"truce":"surrendered",command.terms=="truce"?-1:0,o.tick,command.terms,{},{}};}
        SiegeEvent event;event.kind=integration?"integration_request":command.terms=="truce"?"truce_request":"occupation_request";event.stronghold_id=s.stronghold_id;event.campaign_id=s.campaign_id;event.terms=command.terms;event.defender_actor_id=o.defender_actor_id;events.push_back(std::move(event));
        if(command.terms=="safe_departure") {auto departing=side_armies(s,1);std::sort(departing.begin(),departing.end());SiegeEvent departure;departure.kind="safe_departure_request";departure.army_ids=std::move(departing);events.push_back(std::move(departure));}
    } else return error<SiegeResolution>("UnknownSiegeStrategicCommand");
    next.last_events=std::move(events);if(next.phase!="active") release_terminal(next);
    auto ai=make_siege_ai(next,t,o);if(!ai.ok) return {false,{},ai.error};next.ai_plans=next.phase=="active"?std::move(ai.value):SiegeOrders{};
    result.events=next.last_events;return {true,std::move(result),{}};
}

// 安全边界因军令失效或停战中止，只列应释放的真实来源与军团，不补偿或搬移资产。
Outcome<SiegeResolution> terminate_siege(const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& reason) {
    auto checked=validate_siege(s,t,o);if(!checked.ok) return {false,{},checked.error};
    if(!has({"authority_lost","responsible_actor_lost","truce","cancelled"},reason)) return error<SiegeResolution>("UnknownSiegeTermination");
    if(s.phase!="active") return error<SiegeResolution>("SiegeNotActive");
    SiegeResolution result;result.siege=s;auto& next=result.siege;std::vector<StableId> releases;for(const auto& pending:next.pending_nodes) releases.push_back(pending.second.source_id);
    next.phase="result";next.result=SiegeResult{"aborted",-1,o.tick,{},reason,{}};next.pending_nodes.clear();next.engine_work.clear();next.ai_plans.clear();
    SiegeEvent event;event.kind="siege_release_request";event.battle_source_ids=std::move(releases);event.army_ids=side_armies(s,0);auto defenders=side_armies(s,1);event.army_ids->insert(event.army_ids->end(),defenders.begin(),defenders.end());event.reason=reason;
    next.last_events={std::move(event)};result.events=next.last_events;return {true,std::move(result),{}};
}
}
